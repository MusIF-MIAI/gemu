#include "utest.h"
#include "../ge.h"

/*
 * Edge-case and complementary tests for the direct binary-load helpers.
 *
 * Deliberately avoids duplicating the cases already in image.c:
 *   - image.load_image_places_bytes  (3 bytes at 0x0400, checks mem_written[0x0400])
 *   - image.load_image_range_error   (4 bytes at 0xFFFE -> -1)
 *   - image.runs_to_hlt_at_nonzero_origin  (execution path)
 *
 * All six tests here are structurally new.
 */

/* 1. Non-zero origin: first AND last bytes placed and marked written. */
UTEST(load_edge, nonzero_origin_first_and_last_written)
{
    struct ge g;
    uint8_t img[4] = { 0xAA, 0xBB, 0xCC, 0xDD };

    ge_init(&g);
    ge_clear(&g);

    ASSERT_EQ(ge_load_image(&g, img, sizeof(img), 0x0500), 0);
    ASSERT_EQ((int)g.mem[0x0500], 0xAA);
    ASSERT_EQ((int)g.mem[0x0501], 0xBB);
    ASSERT_EQ((int)g.mem[0x0502], 0xCC);
    ASSERT_EQ((int)g.mem[0x0503], 0xDD);
    ASSERT_EQ((int)g.mem_written[0x0500], 1);
    ASSERT_EQ((int)g.mem_written[0x0503], 1);
}

/* 2. Range check: overrun returns -1; exact-fit (ends at 0x10000) returns 0. */
UTEST(load_edge, range_exact_fit_and_overrun)
{
    struct ge g;
    uint8_t img[4] = { 0, 0, 0, 0 };

    ge_init(&g);
    ge_clear(&g);

    /* 0xFFFE + 4 = 0x10002: overrun */
    ASSERT_EQ(ge_load_image(&g, img, sizeof(img), 0xFFFE), -1);

    /* 0xFFFC + 4 = 0x10000: exactly fills the space, must succeed */
    ASSERT_EQ(ge_load_image(&g, img, sizeof(img), 0xFFFC), 0);
}

/* 3. ge_seed_segment_bases: identity bases for all 8 segment registers. */
UTEST(load_edge, seed_segment_bases_identity)
{
    struct ge g;
    int n;

    ge_init(&g);
    ge_clear(&g);

    ge_seed_segment_bases(&g);

    for (n = 0; n < 8; n++) {
        uint16_t base = (uint16_t)(n << 12);
        uint8_t  hi   = (uint8_t)(base >> 8);
        uint8_t  lo   = (uint8_t)(base & 0xff);

        ASSERT_EQ((int)g.mem[240 + 2 * n],     (int)hi);
        ASSERT_EQ((int)g.mem[240 + 2 * n + 1], (int)lo);
    }
}

/* 4. Re-seed after clobber restores the N=1 base (0x10, 0x00). */
UTEST(load_edge, seed_segment_bases_restores_after_clobber)
{
    struct ge g;

    ge_init(&g);
    ge_clear(&g);

    /* Corrupt the N=1 entry (mem[242..243]) */
    g.mem[242] = 0xFF;
    g.mem[243] = 0xFF;

    ge_seed_segment_bases(&g);

    /* N=1 -> base 0x1000 -> hi=0x10, lo=0x00 */
    ASSERT_EQ((int)g.mem[242], 0x10);
    ASSERT_EQ((int)g.mem[243], 0x00);
}

/* 5. ge_enter seeds rPO and places the machine in the alpha state. */
UTEST(load_edge, enter_seeds_po_and_alpha_state)
{
    struct ge g;

    ge_init(&g);
    ge_clear(&g);

    ge_enter(&g, 0x0300);

    ASSERT_EQ((int)g.rPO, 0x0300);
    ASSERT_EQ((int)g.rSO, 0xe2);
    ASSERT_EQ((int)g.rSA, 0xe2);
}

/* 6. Zero-size load returns 0 and writes nothing. */
UTEST(load_edge, zero_size_load_writes_nothing)
{
    struct ge g;
    uint8_t img[1] = { 0xAA };

    ge_init(&g);
    ge_clear(&g);

    ASSERT_EQ(ge_load_image(&g, img, 0, 0x0200), 0);
    ASSERT_EQ((int)g.mem_written[0x0200], 0);
}
