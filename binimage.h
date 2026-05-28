#ifndef BINIMAGE_H
#define BINIMAGE_H
/*
 * binimage.h — the GE-120 "unified binary format".
 *
 * A small big-endian header (machine-native byte order) followed by a flat
 * machine-code image. This is the single interchange format shared by the
 * toolchain:
 *
 *   gasm  (assembler)     emits it
 *   gdis  (disassembler)  emits it (depunched .cap / --bin -> loadable image)
 *   gemu  (emulator)      loads it (positional `./ge prog.bin`)
 *
 * The same header is what a minimal real-machine card-reader bootstrap parses
 * to place the image at `origin` and jump to `entry`.
 *
 * Layout (12-byte header, then `length` image bytes):
 *
 *   off  size  field     notes
 *   0    4     magic     'G','E','1','2'  (0x47 0x45 0x31 0x32)
 *   4    1     version   BINIMAGE_VERSION
 *   5    1     flags     reserved, must be 0
 *   6    2     origin    load address           (big-endian)
 *   8    2     entry     initial PO             (big-endian)
 *   10   2     length    image byte count       (big-endian)
 *   12   ...   image     flat machine code, `length` bytes
 *
 * The header is metadata only: it is never placed in emulated/real memory.
 *
 * This file is standalone (only <stdio.h>/<stdint.h>/<stddef.h>); it includes
 * no gemu/gasm/gdis header so every tool can compile binimage.c directly.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define BINIMAGE_MAGIC0   0x47u   /* 'G' */
#define BINIMAGE_MAGIC1   0x45u   /* 'E' */
#define BINIMAGE_MAGIC2   0x31u   /* '1' */
#define BINIMAGE_MAGIC3   0x32u   /* '2' */
#define BINIMAGE_VERSION  0x01u
#define BINIMAGE_HDR_SIZE 12

/* Result codes (0 = success). */
enum {
    BINIMAGE_OK          =  0,
    BINIMAGE_E_IO        = -1,   /* read/write error                       */
    BINIMAGE_E_MAGIC     = -2,   /* bad magic                              */
    BINIMAGE_E_VERSION   = -3,   /* unsupported version                    */
    BINIMAGE_E_TRUNCATED = -4,   /* header/image shorter than `length`     */
    BINIMAGE_E_TOOBIG    = -5,   /* image larger than caller's buffer      */
    BINIMAGE_E_RANGE     = -6    /* origin + length would exceed 0x10000   */
};

/*
 * Write a unified-format file: the 12-byte header followed by `len` image
 * bytes. `fp` must be opened in binary write mode. Returns BINIMAGE_OK or a
 * negative error code.
 */
int binimage_write(FILE *fp, uint16_t origin, uint16_t entry,
                   const uint8_t *img, uint16_t len);

/*
 * Read a unified-format file. On success *origin, *entry and *len are filled
 * and up to *len image bytes are copied into buf (which must hold >= the
 * stored length; bufcap is the caller's capacity). Returns BINIMAGE_OK or a
 * negative error code. `fp` must be opened in binary read mode.
 */
int binimage_read(FILE *fp, uint16_t *origin, uint16_t *entry,
                  uint8_t *buf, size_t bufcap, uint16_t *len);

/* Human-readable message for a binimage_* result code. */
const char *binimage_strerror(int code);

#endif /* BINIMAGE_H */
