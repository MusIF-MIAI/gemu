#include "utest.h"
#include "../binimage.h"

#include <stdio.h>
#include <string.h>

/*
 * Edge-case unit tests for the binimage unified binary format module.
 *
 * These tests are intentionally distinct from the round-trip and bad-magic
 * cases already covered by tests/image.c.  Each test is self-contained and
 * uses tmpfile() for I/O so no external files are needed.
 *
 * Header layout (for hand-built buffers):
 *   [0..3]  magic   'G','E','1','2'
 *   [4]     version 0x01
 *   [5]     flags   0x00
 *   [6..7]  origin  big-endian uint16
 *   [8..9]  entry   big-endian uint16
 *   [10..11] len    big-endian uint16
 *   [12..]  image   `len` bytes
 */


/* 1. Round-trip with a non-zero origin and entry and a multi-byte image.
 *    Verifies that origin, entry, and len are preserved exactly and that
 *    every image byte is reproduced without corruption. */
UTEST(binimage_edge, roundtrip_nonzero_origin)
{
    uint8_t  in[8]   = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
    uint8_t  out[64];
    uint16_t origin  = 0xFFFF;
    uint16_t entry   = 0xFFFF;
    uint16_t len     = 0xFFFF;
    int      rc;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, (FILE *)NULL);

    rc = binimage_write(tmp, 0x0200, 0x0204, in, (uint16_t)sizeof(in));
    ASSERT_EQ(rc, BINIMAGE_OK);

    rewind(tmp);

    rc = binimage_read(tmp, &origin, &entry, out, sizeof(out), &len);
    ASSERT_EQ(rc, BINIMAGE_OK);

    fclose(tmp);

    ASSERT_EQ((int)origin, 0x0200);
    ASSERT_EQ((int)entry,  0x0204);
    ASSERT_EQ((int)len,    (int)sizeof(in));
    ASSERT_EQ(memcmp(in, out, sizeof(in)), 0);
}


/* 2. Zero-length image: write then read must succeed and report len == 0. */
UTEST(binimage_edge, zero_length_image)
{
    uint8_t  buf[8];
    uint16_t origin = 0xFFFF;
    uint16_t entry  = 0xFFFF;
    uint16_t len    = 0xFFFF;
    int      rc;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, (FILE *)NULL);

    rc = binimage_write(tmp, 0x0100, 0x0100, NULL, 0);
    ASSERT_EQ(rc, BINIMAGE_OK);

    rewind(tmp);

    rc = binimage_read(tmp, &origin, &entry, buf, sizeof(buf), &len);
    ASSERT_EQ(rc, BINIMAGE_OK);

    fclose(tmp);

    ASSERT_EQ((int)len, 0);
}


/* 3. Bad magic: a buffer whose first four bytes are not 'G','E','1','2'
 *    must cause binimage_read to return BINIMAGE_E_MAGIC.
 *    (Note: this test is a different construction from image.c's
 *    binimage_rejects_bad_magic which uses 'X','X','X','X'.) */
UTEST(binimage_edge, bad_magic_returns_error)
{
    /* Valid-looking header except magic is 'B','A','D','!' */
    uint8_t hdr[16] = {
        'B', 'A', 'D', '!',   /* bad magic */
        0x01,                  /* version */
        0x00,                  /* flags */
        0x00, 0x00,            /* origin = 0 */
        0x00, 0x00,            /* entry  = 0 */
        0x00, 0x04,            /* len    = 4 */
        0x01, 0x02, 0x03, 0x04 /* image */
    };
    uint8_t  out[16];
    uint16_t o, e, l;
    int      rc;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, (FILE *)NULL);

    fwrite(hdr, 1, sizeof(hdr), tmp);
    rewind(tmp);

    rc = binimage_read(tmp, &o, &e, out, sizeof(out), &l);
    ASSERT_EQ(rc, BINIMAGE_E_MAGIC);

    fclose(tmp);
}


/* 4. Bad version: valid magic but version byte = 0x02 must yield
 *    BINIMAGE_E_VERSION. */
UTEST(binimage_edge, bad_version_returns_error)
{
    uint8_t hdr[12] = {
        'G', 'E', '1', '2',   /* correct magic */
        0x02,                  /* unsupported version */
        0x00,                  /* flags */
        0x00, 0x00,            /* origin */
        0x00, 0x00,            /* entry */
        0x00, 0x00             /* len = 0 */
    };
    uint8_t  out[8];
    uint16_t o, e, l;
    int      rc;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, (FILE *)NULL);

    fwrite(hdr, 1, sizeof(hdr), tmp);
    rewind(tmp);

    rc = binimage_read(tmp, &o, &e, out, sizeof(out), &l);
    ASSERT_EQ(rc, BINIMAGE_E_VERSION);

    fclose(tmp);
}


/* 5. Truncated header: only 6 bytes present, binimage_read must return
 *    BINIMAGE_E_TRUNCATED. */
UTEST(binimage_edge, truncated_header_returns_error)
{
    uint8_t  partial[6] = { 'G', 'E', '1', '2', 0x01, 0x00 };
    uint8_t  out[8];
    uint16_t o, e, l;
    int      rc;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, (FILE *)NULL);

    fwrite(partial, 1, sizeof(partial), tmp);
    rewind(tmp);

    rc = binimage_read(tmp, &o, &e, out, sizeof(out), &l);
    ASSERT_EQ(rc, BINIMAGE_E_TRUNCATED);

    fclose(tmp);
}


/* 6. Buffer too small: write an image of 10 bytes, then read with bufcap=4
 *    must return BINIMAGE_E_TOOBIG. */
UTEST(binimage_edge, buffer_too_small_returns_error)
{
    uint8_t in[10]  = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    uint8_t out[4];
    uint16_t o, e, l;
    int      rc;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, (FILE *)NULL);

    rc = binimage_write(tmp, 0x0000, 0x0000, in, (uint16_t)sizeof(in));
    ASSERT_EQ(rc, BINIMAGE_OK);

    rewind(tmp);

    rc = binimage_read(tmp, &o, &e, out, sizeof(out), &l);
    ASSERT_EQ(rc, BINIMAGE_E_TOOBIG);

    fclose(tmp);
}


/* 7. Range error on write: origin=0xFFFE with len=4 would exceed 0x10000,
 *    so binimage_write must return BINIMAGE_E_RANGE. */
UTEST(binimage_edge, write_range_overflow_returns_error)
{
    uint8_t img[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    int     rc;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, (FILE *)NULL);

    rc = binimage_write(tmp, 0xFFFE, 0xFFFE, img, 4);
    ASSERT_EQ(rc, BINIMAGE_E_RANGE);

    fclose(tmp);
}


/* 8. binimage_strerror: must return a non-NULL pointer for every defined
 *    result code, including BINIMAGE_OK and all six negative codes. */
UTEST(binimage_edge, strerror_nonnull_for_all_codes)
{
    const char *s;

    s = binimage_strerror(BINIMAGE_OK);
    ASSERT_NE(s, (const char *)NULL);

    s = binimage_strerror(BINIMAGE_E_IO);
    ASSERT_NE(s, (const char *)NULL);

    s = binimage_strerror(BINIMAGE_E_MAGIC);
    ASSERT_NE(s, (const char *)NULL);

    s = binimage_strerror(BINIMAGE_E_VERSION);
    ASSERT_NE(s, (const char *)NULL);

    s = binimage_strerror(BINIMAGE_E_TRUNCATED);
    ASSERT_NE(s, (const char *)NULL);

    s = binimage_strerror(BINIMAGE_E_TOOBIG);
    ASSERT_NE(s, (const char *)NULL);

    s = binimage_strerror(BINIMAGE_E_RANGE);
    ASSERT_NE(s, (const char *)NULL);
}
