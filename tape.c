/*
 * tape.c — functional MTC/MTH magnetic tape on a standard connector (3/4).
 *
 * A length-prefixed record image (see tape.h). A read TPER transfers the record
 * at the current position into CPU memory; like the disk, each logical byte is
 * nibble-split for the connector's binary read mode (the channel packs the low
 * nibbles of two presented bytes back into one memory byte).
 *
 * Scope (functional MVP): READ and the motion control orders (rewind / forward
 * space / backspace) work on the record stream. WRITE / erase / write-tape-mark
 * are placeholders pending the real MTC order codes and the connector output
 * micro-flow (descriptive volume not yet extracted) — see the TODOs.
 *
 * Multi-unit controller semantics (a read/write busies all the controller's
 * units while a rewind frees the others) are documented in the plan but not yet
 * modelled: this is a single reel on one unit. TODO.
 */

#include "tape.h"
#include "ge.h"
#include "connector34.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TAPE_IMAGE_MAX (1u << 20)   /* 1 MiB reel image cap */

/* TODO: real CPER order codes from the MTC 163-173 descriptive volume. */
enum tape_order {
    TAPE_ORD_READ      = 0x40,
    TAPE_ORD_WRITE     = 0x42,
    TAPE_ORD_REWIND    = 0x20,
    TAPE_ORD_BACKSPACE = 0x24,
    TAPE_ORD_FORWARD   = 0x28,
    TAPE_ORD_ERASE     = 0x2C,
    TAPE_ORD_WTM       = 0x30,
};

struct tape_ctx {
    uint8_t              connector;
    uint8_t              unit;
    uint8_t             *image;
    size_t               nbytes;
    size_t               pos;        /* byte offset of the next record */
    size_t               prev_pos;   /* for a single-level backspace */
    struct ge_std_device dev;
};

static int tape_claims(void *opaque, struct std_unitname un)
{
    struct tape_ctx *t = (struct tape_ctx *)opaque;
    return un.connector == t->connector && un.unit == t->unit;
}

/* Length of the record at `pos` (0 = tape mark), or -1 at end of medium. */
static long record_len(struct tape_ctx *t, size_t pos)
{
    if (pos + 2 > t->nbytes)
        return -1;
    return ((long)t->image[pos] << 8) | t->image[pos + 1];
}

static std_reaction tape_command(struct ge *ge, void *opaque,
                                 struct std_unitname un, uint8_t order)
{
    struct tape_ctx *t = (struct tape_ctx *)opaque;
    (void)ge; (void)un;

    switch (order) {
    case TAPE_ORD_REWIND:
        t->prev_pos = t->pos;
        t->pos = 0;
        return STD_ACCEPTED_END;
    case TAPE_ORD_FORWARD: {
        long len = record_len(t, t->pos);
        if (len < 0)
            return STD_NOT_POSSIBLE;        /* at end of medium */
        t->prev_pos = t->pos;
        t->pos += 2 + (size_t)len;
        return STD_ACCEPTED_END;
    }
    case TAPE_ORD_BACKSPACE:
        t->pos = t->prev_pos;               /* single-level backspace (MVP) */
        return STD_ACCEPTED_END;
    case TAPE_ORD_WRITE:
    case TAPE_ORD_ERASE:
    case TAPE_ORD_WTM:
        return STD_ACCEPTED_NO_END;         /* TODO: output path not yet wired */
    case TAPE_ORD_READ:
        return STD_ACCEPTED_NO_END;         /* the read transfer does the work */
    default:
        return STD_ACCEPTED_NO_END;
    }
}

static std_reaction tape_transfer(struct ge *ge, void *opaque,
                                  struct std_unitname un, int dir,
                                  uint8_t *buf, uint16_t *len, uint16_t cap)
{
    struct tape_ctx *t = (struct tape_ctx *)opaque;
    (void)ge; (void)un;

    if (dir != 0) {
        /* WRITE: connector output path not yet wired in the core. TODO. */
        *len = 0;
        return STD_NOT_POSSIBLE;
    }

    long rl = record_len(t, t->pos);
    if (rl < 0) {
        *len = 0;
        return STD_NOT_POSSIBLE;             /* end of medium */
    }
    if (rl == 0) {
        /* Tape mark: a zero-length read. Step over it. */
        t->prev_pos = t->pos;
        t->pos += 2;
        *len = 0;
        return STD_ACCEPTED_END;
    }

    const uint8_t *rec = t->image + t->pos + 2;
    uint16_t n = 0;
    for (long i = 0; i < rl && (size_t)(t->pos + 2 + i) < t->nbytes
                       && n + 2 <= cap; i++) {
        buf[n++] = (uint8_t)(rec[i] >> 4);
        buf[n++] = (uint8_t)(rec[i] & 0x0F);
    }
    *len = n;

    t->prev_pos = t->pos;
    t->pos += 2 + (size_t)rl;
    return STD_ACCEPTED_END;
}

int tape_register(struct ge *ge, const char *image_path,
                  uint8_t connector, uint8_t unit)
{
    struct tape_ctx *t = calloc(1, sizeof(*t));
    if (!t)
        return -1;

    t->connector = connector;
    t->unit      = unit;
    t->image     = calloc(1, TAPE_IMAGE_MAX);
    if (!t->image) {
        free(t);
        return -1;
    }
    t->nbytes   = 0;
    t->pos      = 0;
    t->prev_pos = 0;

    if (image_path) {
        FILE *f = fopen(image_path, "rb");
        if (f) {
            t->nbytes = fread(t->image, 1, TAPE_IMAGE_MAX, f);
            fclose(f);
            ge_log(LOG_PERI, "tape: loaded %zu bytes from %s\n",
                   t->nbytes, image_path);
        } else {
            ge_log(LOG_ERR, "tape: cannot open %s (blank reel)\n", image_path);
        }
    }

    t->dev.name     = "MTC tape";
    t->dev.ctx      = t;
    t->dev.claims   = tape_claims;
    t->dev.command  = tape_command;
    t->dev.transfer = tape_transfer;
    t->dev.tick     = NULL;

    if (connector34_init(ge) != 0)
        return -1;
    return connector34_attach(ge, &t->dev, connector);
}
