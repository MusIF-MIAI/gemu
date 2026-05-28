/*
 * binimage.c — read/write the GE-120 unified binary format.
 *
 * See binimage.h for the header layout. All multi-byte header fields are
 * big-endian (machine-native order). Standalone: no gemu/gasm/gdis headers.
 */

#include "binimage.h"

int binimage_write(FILE *fp, uint16_t origin, uint16_t entry,
                   const uint8_t *img, uint16_t len)
{
    uint8_t hdr[BINIMAGE_HDR_SIZE];

    if (!fp)
        return BINIMAGE_E_IO;
    if ((uint32_t)origin + (uint32_t)len > 0x10000u)
        return BINIMAGE_E_RANGE;

    hdr[0]  = BINIMAGE_MAGIC0;
    hdr[1]  = BINIMAGE_MAGIC1;
    hdr[2]  = BINIMAGE_MAGIC2;
    hdr[3]  = BINIMAGE_MAGIC3;
    hdr[4]  = BINIMAGE_VERSION;
    hdr[5]  = 0x00;                       /* flags */
    hdr[6]  = (uint8_t)(origin >> 8);
    hdr[7]  = (uint8_t)(origin & 0xff);
    hdr[8]  = (uint8_t)(entry >> 8);
    hdr[9]  = (uint8_t)(entry & 0xff);
    hdr[10] = (uint8_t)(len >> 8);
    hdr[11] = (uint8_t)(len & 0xff);

    if (fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr))
        return BINIMAGE_E_IO;
    if (len && fwrite(img, 1, len, fp) != (size_t)len)
        return BINIMAGE_E_IO;

    return BINIMAGE_OK;
}

int binimage_read(FILE *fp, uint16_t *origin, uint16_t *entry,
                  uint8_t *buf, size_t bufcap, uint16_t *len)
{
    uint8_t hdr[BINIMAGE_HDR_SIZE];
    uint16_t o, e, n;

    if (!fp)
        return BINIMAGE_E_IO;

    if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr))
        return BINIMAGE_E_TRUNCATED;

    if (hdr[0] != BINIMAGE_MAGIC0 || hdr[1] != BINIMAGE_MAGIC1 ||
        hdr[2] != BINIMAGE_MAGIC2 || hdr[3] != BINIMAGE_MAGIC3)
        return BINIMAGE_E_MAGIC;

    if (hdr[4] != BINIMAGE_VERSION)
        return BINIMAGE_E_VERSION;

    o = (uint16_t)((hdr[6]  << 8) | hdr[7]);
    e = (uint16_t)((hdr[8]  << 8) | hdr[9]);
    n = (uint16_t)((hdr[10] << 8) | hdr[11]);

    if ((uint32_t)o + (uint32_t)n > 0x10000u)
        return BINIMAGE_E_RANGE;
    if ((size_t)n > bufcap)
        return BINIMAGE_E_TOOBIG;

    if (n && fread(buf, 1, n, fp) != (size_t)n)
        return BINIMAGE_E_TRUNCATED;

    if (origin) *origin = o;
    if (entry)  *entry  = e;
    if (len)    *len    = n;

    return BINIMAGE_OK;
}

const char *binimage_strerror(int code)
{
    switch (code) {
        case BINIMAGE_OK:
            return "ok";
        case BINIMAGE_E_IO:
            return "I/O error";
        case BINIMAGE_E_MAGIC:
            return "bad magic (not a GE12 unified image)";
        case BINIMAGE_E_VERSION:
            return "unsupported format version";
        case BINIMAGE_E_TRUNCATED:
            return "truncated header or image";
        case BINIMAGE_E_TOOBIG:
            return "image larger than buffer";
        case BINIMAGE_E_RANGE:
            return "origin + length exceeds 64 KiB";
        default:
            return "unknown error";
    }
}
