/*
 * capcat - append self-addressed scatter cards to an existing GE-120 .cap deck.
 *
 * The base deck is parsed with cap_load(), so only the hex-token card dump is
 * preserved; the ASCII hole-art half of the original capture is intentionally
 * omitted in the output. The appended cards use the funktionalcpu-family
 * self-addressed binary record format:
 *
 *   cols  0.. 7  per-deck constant prefix
 *   col      8   LL = payload length - 1
 *   cols  9..10  big-endian load address
 *   cols 11..    payload bytes (COLBIN)
 *   col     79   optional sequence/check byte
 *
 * Overlay inputs are either:
 *   - unified GE12 images (origin/length taken from the header), or
 *   - raw files specified as 0xADDR:path, which are loaded contiguously at ADDR.
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binimage.h"
#include "../cap.h"
#include "../transcode.h"

#define MAX_OVERLAYS 32
#define CARD_COLS 80

struct overlay {
    const char *path;
    uint16_t origin;
    uint16_t len;
    uint8_t data[65536];
};

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [options] base.cap overlay...\n"
            "\n"
            "Append one or more overlays as self-addressed scatter cards to base.cap.\n"
            "Overlay forms:\n"
            "  image.bin        unified GE12 image (origin taken from header)\n"
            "  0xADDR:file.bin  raw flat file loaded contiguously at ADDR\n"
            "\n"
            "Options:\n"
            "  -o FILE          output .cap path (required)\n"
            "  --payload N      payload bytes per appended card (default 64, max 68)\n"
            "  --prefix \"HH HH ...\"  force the 8-byte program-card prefix\n"
            "  -h, --help       show this help\n",
            argv0);
}

static uint16_t colbin_encode_byte(uint8_t b)
{
    uint16_t col = 0;

    if (b & 0x80) col |= (uint16_t)(1u << 0);
    if (b & 0x40) col |= (uint16_t)(1u << 1);
    if (b & 0x20) col |= (uint16_t)(1u << 2);
    if (b & 0x10) col |= (uint16_t)(1u << 3);
    if (b & 0x08) col |= (uint16_t)(1u << 6);
    if (b & 0x04) col |= (uint16_t)(1u << 7);
    if (b & 0x02) col |= (uint16_t)(1u << 8);
    if (b & 0x01) col |= (uint16_t)(1u << 9);

    return col;
}

static int detect_prefix(struct cap_deck *d, uint8_t out[8])
{
    struct prefix_count {
        uint8_t p[8];
        int count;
    } tab[1024];
    int ntab = 0;
    int best = -1;
    int nc = cap_num_cards(d);

    for (int i = 0; i < nc; i++) {
        int ncols = cap_card_ncols(d, i);
        const uint16_t *cols = cap_card_columns(d, i);
        uint8_t bytes[80];
        int found = -1;

        if (ncols < 11 || !cols)
            continue;

        for (int k = 0; k < 80 && k < ncols; k++)
            bytes[k] = transcode_column(cols[k], TC_COLBIN);

        for (int j = 0; j < ntab; j++) {
            if (memcmp(tab[j].p, bytes, 8) == 0) {
                found = j;
                break;
            }
        }
        if (found >= 0) {
            tab[found].count++;
            if (best < 0 || tab[found].count > tab[best].count)
                best = found;
            continue;
        }
        if (ntab >= (int)(sizeof(tab) / sizeof(tab[0])))
            continue;
        memcpy(tab[ntab].p, bytes, 8);
        tab[ntab].count = 1;
        if (best < 0)
            best = ntab;
        ntab++;
    }

    if (best < 0)
        return -1;
    memcpy(out, tab[best].p, 8);
    return tab[best].count;
}

static int parse_prefix_arg(const char *arg, uint8_t prefix[8])
{
    char tmp[128];
    char *tok;
    int k = 0;

    strncpy(tmp, arg, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    tok = strtok(tmp, " ,");
    while (tok && k < 8) {
        prefix[k++] = (uint8_t)strtoul(tok, NULL, 16);
        tok = strtok(NULL, " ,");
    }

    return k == 8 ? 0 : -1;
}

static int load_overlay_spec(const char *spec, struct overlay *ov)
{
    const char *colon = strchr(spec, ':');
    FILE *fp;

    memset(ov, 0, sizeof(*ov));
    ov->path = spec;

    if (colon && spec[0] == '0' && tolower((unsigned char)spec[1]) == 'x') {
        char addrbuf[32];
        size_t alen = (size_t)(colon - spec);
        size_t got;

        if (alen >= sizeof(addrbuf))
            return -1;
        memcpy(addrbuf, spec, alen);
        addrbuf[alen] = '\0';
        ov->origin = (uint16_t)strtoul(addrbuf, NULL, 0);
        ov->path = colon + 1;
        fp = fopen(ov->path, "rb");
        if (!fp)
            return -1;
        got = fread(ov->data, 1, sizeof(ov->data) - ov->origin, fp);
        fclose(fp);
        ov->len = (uint16_t)got;
        return ov->len ? 0 : -1;
    }

    fp = fopen(spec, "rb");
    if (!fp)
        return -1;
    if (binimage_read(fp, &ov->origin, NULL, ov->data, sizeof(ov->data),
                      &ov->len) != BINIMAGE_OK) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int write_card(FILE *out, int card_no, const uint16_t cols[CARD_COLS])
{
    fprintf(out, "Card n. %d\n", card_no);
    for (int i = 0; i < CARD_COLS; i++) {
        fprintf(out, "%04X", cols[i] & 0x1FFFu);
        fputc((i == CARD_COLS - 1) ? '\n' : ' ', out);
    }
    return ferror(out) ? -1 : 0;
}

static int append_overlay(FILE *out, int *card_no, const uint8_t prefix[8],
                          const struct overlay *ov, int payload_len)
{
    static const uint8_t seq_cycle[8] = { 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00 };
    uint16_t addr = ov->origin;
    uint16_t off = 0;
    int seq = 0;

    while (off < ov->len) {
        uint16_t cols[CARD_COLS] = {0};
        int chunk = ov->len - off;

        if (chunk > payload_len)
            chunk = payload_len;

        for (int i = 0; i < 8; i++)
            cols[i] = colbin_encode_byte(prefix[i]);
        cols[8] = colbin_encode_byte((uint8_t)(chunk - 1));
        cols[9] = colbin_encode_byte((uint8_t)(addr >> 8));
        cols[10] = colbin_encode_byte((uint8_t)(addr & 0xFF));
        for (int i = 0; i < chunk; i++)
            cols[11 + i] = colbin_encode_byte(ov->data[off + i]);
        cols[79] = colbin_encode_byte(seq_cycle[seq & 7]);

        if (write_card(out, *card_no, cols) != 0)
            return -1;

        (*card_no)++;
        seq++;
        addr = (uint16_t)(addr + chunk);
        off = (uint16_t)(off + chunk);
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *base_path = NULL;
    const char *out_path = NULL;
    const char *overlay_specs[MAX_OVERLAYS];
    struct overlay overlays[MAX_OVERLAYS];
    uint8_t prefix[8];
    int payload_len = 64;
    int have_prefix = 0;
    int noverlays = 0;
    struct cap_deck *base = NULL;
    FILE *out = NULL;
    int card_no = 1;
    int rc = 1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            out_path = argv[++i];
        } else if (!strcmp(argv[i], "--payload") && i + 1 < argc) {
            payload_len = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--prefix") && i + 1 < argc) {
            if (parse_prefix_arg(argv[++i], prefix) != 0) {
                fprintf(stderr, "capcat: --prefix needs 8 hex bytes\n");
                return 2;
            }
            have_prefix = 1;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "capcat: unknown option '%s'\n", argv[i]);
            usage(argv[0]);
            return 2;
        } else if (!base_path) {
            base_path = argv[i];
        } else {
            if (noverlays >= MAX_OVERLAYS) {
                fprintf(stderr, "capcat: too many overlays\n");
                return 2;
            }
            overlay_specs[noverlays++] = argv[i];
        }
    }

    if (!base_path || !out_path || noverlays == 0) {
        usage(argv[0]);
        return 2;
    }
    if (payload_len < 1 || payload_len > 68) {
        fprintf(stderr, "capcat: --payload must be in 1..68\n");
        return 2;
    }

    base = cap_load(base_path);
    if (!base) {
        fprintf(stderr, "capcat: cannot parse base deck '%s'\n", base_path);
        goto done;
    }
    if (!have_prefix && detect_prefix(base, prefix) < 0) {
        fprintf(stderr, "capcat: cannot auto-detect a program-card prefix from '%s'\n",
                base_path);
        goto done;
    }

    for (int i = 0; i < noverlays; i++) {
        if (load_overlay_spec(overlay_specs[i], &overlays[i]) != 0) {
            fprintf(stderr, "capcat: cannot load overlay '%s'\n", overlay_specs[i]);
            goto done;
        }
    }

    out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "capcat: cannot write '%s'\n", out_path);
        goto done;
    }

    for (int i = 0; i < cap_num_cards(base); i++) {
        uint16_t cols[CARD_COLS] = {0};
        const uint16_t *src = cap_card_columns(base, i);
        int ncols = cap_card_ncols(base, i);

        if (!src || ncols <= 0)
            continue;
        if (ncols > CARD_COLS)
            ncols = CARD_COLS;
        memcpy(cols, src, (size_t)ncols * sizeof(cols[0]));
        if (write_card(out, card_no++, cols) != 0) {
            fprintf(stderr, "capcat: write failed while copying base deck\n");
            goto done;
        }
    }

    for (int i = 0; i < noverlays; i++) {
        if (append_overlay(out, &card_no, prefix, &overlays[i], payload_len) != 0) {
            fprintf(stderr, "capcat: write failed while appending '%s'\n",
                    overlay_specs[i]);
            goto done;
        }
    }

    rc = 0;

done:
    if (out)
        fclose(out);
    if (base)
        cap_free(base);
    return rc;
}
