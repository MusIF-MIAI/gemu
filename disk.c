/*
 * disk.c — functional DSS 156-157 disk pack on a standard connector (3/4).
 *
 * A flat sectored image (SECTOR_BYTES logical bytes per sector). A read TPER
 * transfers the current sector to CPU memory: each logical byte is split into
 * its high then low nibble for presentation, because the connector's binary
 * read mode (order-block z=0) packs the low nibble of two presented bytes into
 * one memory byte (verified against the channel-1 datapath). So presenting
 * (b>>4, b&0x0F) reconstructs b in memory.
 *
 * Scope (functional MVP): READ of the current sector works end to end. Sector
 * addressing (SEEK) and WRITE are placeholders pending the real DSS order codes
 * and geometry (descriptive volume not yet extracted) — see the TODOs.
 */

#include "disk.h"
#include "ge.h"
#include "connector34.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* TODO: real geometry from the DSS 156-157 descriptive volume. */
#define SECTOR_BYTES   64
#define DEFAULT_SECTORS 64

/* TODO: real CPER order codes from the DSS 156-157 descriptive volume. */
enum disk_order {
    DISK_ORD_SEEK   = 0x10,
    DISK_ORD_READ   = 0x40,
    DISK_ORD_WRITE  = 0x42,
    DISK_ORD_FORMAT = 0x60,
};

struct disk_ctx {
    uint8_t            connector;
    uint8_t            unit;
    uint8_t           *image;
    size_t             nbytes;
    uint32_t           nsectors;
    uint32_t           current_sector;
    struct ge_std_device dev;
};

static int disk_claims(void *opaque, struct std_unitname un)
{
    struct disk_ctx *d = (struct disk_ctx *)opaque;
    return un.connector == d->connector && un.unit == d->unit;
}

static std_reaction disk_command(struct ge *ge, void *opaque,
                                 struct std_unitname un, uint8_t order)
{
    struct disk_ctx *d = (struct disk_ctx *)opaque;
    (void)ge; (void)un;

    switch (order) {
    case DISK_ORD_SEEK:
        /* TODO: decode the target cylinder/head/sector from the real order
         * block once the DSS format is known; for now SEEK is accepted as a
         * no-op (the read transfers current_sector). */
        return STD_ACCEPTED_NO_END;
    case DISK_ORD_READ:
    case DISK_ORD_WRITE:
    case DISK_ORD_FORMAT:
        return STD_ACCEPTED_NO_END;
    default:
        return STD_ACCEPTED_NO_END;   /* unknown orders accepted, no effect */
    }
}

static std_reaction disk_transfer(struct ge *ge, void *opaque,
                                  struct std_unitname un, int dir,
                                  uint8_t *buf, uint16_t *len, uint16_t cap)
{
    struct disk_ctx *d = (struct disk_ctx *)opaque;
    (void)ge; (void)un;

    if (d->current_sector >= d->nsectors) {
        *len = 0;
        return STD_NOT_POSSIBLE;            /* addressed past end of pack */
    }

    const uint8_t *sec = d->image + (size_t)d->current_sector * SECTOR_BYTES;

    if (dir == 0) {
        /* READ: nibble-split each logical byte for the packing channel. */
        uint16_t n = 0;
        for (int i = 0; i < SECTOR_BYTES && n + 2 <= cap; i++) {
            buf[n++] = (uint8_t)(sec[i] >> 4);
            buf[n++] = (uint8_t)(sec[i] & 0x0F);
        }
        *len = n;
        return STD_ACCEPTED_END;
    }

    /* dir == 1 (WRITE): the connector output path is not yet wired in the core
     * (only the channel-1 input transfer is). TODO with the output micro-flow. */
    *len = 0;
    return STD_NOT_POSSIBLE;
}

int disk_register(struct ge *ge, const char *image_path,
                  uint8_t connector, uint8_t unit)
{
    struct disk_ctx *d = calloc(1, sizeof(*d));
    if (!d)
        return -1;

    d->connector      = connector;
    d->unit           = unit;
    d->nsectors       = DEFAULT_SECTORS;
    d->nbytes         = (size_t)d->nsectors * SECTOR_BYTES;
    d->current_sector = 0;
    d->image          = calloc(1, d->nbytes);
    if (!d->image) {
        free(d);
        return -1;
    }

    if (image_path) {
        FILE *f = fopen(image_path, "rb");
        if (f) {
            size_t got = fread(d->image, 1, d->nbytes, f);
            fclose(f);
            ge_log(LOG_PERI, "disk: loaded %zu bytes from %s\n", got, image_path);
        } else {
            ge_log(LOG_ERR, "disk: cannot open %s (blank pack)\n", image_path);
        }
    }

    d->dev.name     = "DSS disk";
    d->dev.ctx      = d;
    d->dev.claims   = disk_claims;
    d->dev.command  = disk_command;
    d->dev.transfer = disk_transfer;
    d->dev.tick     = NULL;

    if (connector34_init(ge) != 0)
        return -1;
    return connector34_attach(ge, &d->dev, connector);
}
