#include "utest.h"
#include "../ge.h"
#include "../binimage.h"

#include <stdio.h>
#include <string.h>

/*
 * Unified binary format + direct binary-load path (Phase A).
 *
 * Covers:
 *   - binimage_write / binimage_read round-trip (the shared format module),
 *   - ge_load_image placing bytes at a non-zero origin with parity primed,
 *   - ge_enter + run reaching HLT for an image loaded away from address 0.
 *
 * The program bytes are built by hand here (gasm-produced fixtures are exercised
 * separately by the toolchain CI); this keeps the test self-contained.
 */

/* binimage_write then binimage_read returns the same origin/entry/length and
 * image bytes. */
UTEST(image, binimage_roundtrip)
{
    uint8_t in[5]  = { 0x0a, 0x00, 0x47, 0xf0, 0x03 };
    uint8_t out[64];
    uint16_t origin = 0, entry = 0, len = 0;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, NULL);

    ASSERT_EQ(binimage_write(tmp, 0x0300, 0x0302, in, sizeof(in)), BINIMAGE_OK);
    rewind(tmp);
    ASSERT_EQ(binimage_read(tmp, &origin, &entry, out, sizeof(out), &len), BINIMAGE_OK);
    fclose(tmp);

    ASSERT_EQ((int)origin, 0x0300);
    ASSERT_EQ((int)entry,  0x0302);
    ASSERT_EQ((int)len,    (int)sizeof(in));
    ASSERT_EQ(memcmp(in, out, sizeof(in)), 0);
}

/* A bad magic is rejected. */
UTEST(image, binimage_rejects_bad_magic)
{
    uint8_t junk[16] = { 'X','X','X','X', 1, 0, 0,0, 0,0, 0,0 };
    uint8_t out[16];
    uint16_t o, e, l;

    FILE *tmp = tmpfile();
    ASSERT_NE(tmp, NULL);
    fwrite(junk, 1, sizeof(junk), tmp);
    rewind(tmp);
    ASSERT_EQ(binimage_read(tmp, &o, &e, out, sizeof(out), &l), BINIMAGE_E_MAGIC);
    fclose(tmp);
}

/* ge_load_image places the bytes at origin, primes parity, marks them written. */
UTEST(image, load_image_places_bytes)
{
    struct ge g;
    uint8_t img[3] = { 0x11, 0x22, 0x33 };

    ge_init(&g);
    ge_clear(&g);

    ASSERT_EQ(ge_load_image(&g, img, sizeof(img), 0x0400), 0);
    ASSERT_EQ((int)g.mem[0x0400], 0x11);
    ASSERT_EQ((int)g.mem[0x0401], 0x22);
    ASSERT_EQ((int)g.mem[0x0402], 0x33);
    ASSERT_EQ((int)g.mem_written[0x0400], 1);
    /* parity primed: a read of these cells must not raise MEM CHECK */
    ASSERT_EQ((int)g.mem_check, 0);
}

/* ge_load_image refuses an image that would not fit. */
UTEST(image, load_image_range_error)
{
    struct ge g;
    uint8_t img[4] = { 0, 0, 0, 0 };

    ge_init(&g);
    ge_clear(&g);
    /* origin 0xFFFE + 4 bytes overruns the 64 KiB space */
    ASSERT_EQ(ge_load_image(&g, img, sizeof(img), 0xFFFE), -1);
}

/* Full direct-load path: an image at a non-zero origin, entered at its entry
 * point, executes and halts. Program at 0x0300:
 *   MVI 'A', 0x0350   (92 41 03 50)
 *   HLT               (0a 00)
 */
UTEST(image, runs_to_hlt_at_nonzero_origin)
{
    struct ge g;
    uint8_t prog[6] = { 0x92, 0x41, 0x03, 0x50, 0x0a, 0x00 };

    ge_init(&g);
    ge_clear(&g);
    ASSERT_EQ(ge_load_image(&g, prog, sizeof(prog), 0x0300), 0);
    ge_start(&g);
    ge_enter(&g, 0x0300);

    int halted = 0;
    for (int i = 0; i < 200; i++) {
        ge_run_cycle(&g);
        if (g.halted) { halted = 1; break; }
    }

    ASSERT_EQ(halted, 1);
    ASSERT_EQ((int)g.mem[0x0350], 0x41);   /* the MVI ran */
    ASSERT_EQ((int)g.mem_check, 0);        /* no spurious parity fault */
}
