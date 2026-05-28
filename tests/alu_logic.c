/**
 * @file  tests/alu_logic.c
 * @brief Unit tests for the GE-120/130 logical ALU primitives (alu_logic.h).
 *
 * Covers: MVC (non-overlap and overlap), MVI, NC, OC, XC (+ CC),
 *         NI, XI (+ CC), CMC (equal/low/high), CI (equal/low/high),
 *         TL (translate via small table), TM (all-zero / non-zero).
 *
 * No UTEST_MAIN here — tests/main.c provides it.
 *
 * CC encoding reminder (from the GE-130 manual §5.5.3.x, §5.6.3.x):
 *   cc = (FA04 << 1) | FA05
 *
 *   Compare (CMC/CI):  cc=1 less, cc=2 equal, cc=3 greater
 *   XC/XI/TM:         cc=2 all-zero result, cc=3 non-zero result
 */

#include "utest.h"
#include "../ge.h"
#include "../alu_logic.h"

/* ------------------------------------------------------------------ */
/* MVC – Move Characters                                               */
/* ------------------------------------------------------------------ */

UTEST(logic, mvc_no_overlap)
{
    struct ge g;
    ge_init(&g);

    /* Write a source pattern at 0x200 */
    g.mem[0x200] = 0x11;
    g.mem[0x201] = 0x22;
    g.mem[0x202] = 0x33;

    alu_mvc(&g, 0x300, 0x200, 3);

    ASSERT_EQ(g.mem[0x300], (uint8_t)0x11);
    ASSERT_EQ(g.mem[0x301], (uint8_t)0x22);
    ASSERT_EQ(g.mem[0x302], (uint8_t)0x33);

    /* Source must be unchanged */
    ASSERT_EQ(g.mem[0x200], (uint8_t)0x11);
    ASSERT_EQ(g.mem[0x201], (uint8_t)0x22);
    ASSERT_EQ(g.mem[0x202], (uint8_t)0x33);
}

UTEST(logic, mvc_overlap_dst_after_src)
{
    struct ge g;
    ge_init(&g);

    /*
     * dst > src, overlapping by 2 bytes.
     * src: [0x100] = 0xAA, [0x101] = 0xBB, [0x102] = 0xCC
     * Move 3 bytes from 0x100 → 0x101.
     * Left-to-right:
     *   [0x101] = mem[0x100] = 0xAA
     *   [0x102] = mem[0x101] = 0xAA  (already overwritten)
     *   [0x103] = mem[0x102] = 0xAA  (already overwritten)
     * Expected: [0x101..0x103] = 0xAA, 0xAA, 0xAA
     */
    g.mem[0x100] = 0xAA;
    g.mem[0x101] = 0xBB;
    g.mem[0x102] = 0xCC;

    alu_mvc(&g, 0x101, 0x100, 3);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0xAA); /* source first byte untouched */
    ASSERT_EQ(g.mem[0x101], (uint8_t)0xAA);
    ASSERT_EQ(g.mem[0x102], (uint8_t)0xAA);
    ASSERT_EQ(g.mem[0x103], (uint8_t)0xAA);
}

UTEST(logic, mvc_overlap_dst_before_src)
{
    struct ge g;
    ge_init(&g);

    /*
     * dst < src by 1, 3 bytes.
     * [0x100] = 0x11, [0x101] = 0x22, [0x102] = 0x33
     * Move 3 bytes from 0x101 → 0x100.
     * Left-to-right:
     *   [0x100] = mem[0x101] = 0x22
     *   [0x101] = mem[0x102] = 0x33
     *   [0x102] = mem[0x103] = 0x00 (unwritten memory is 0)
     */
    g.mem[0x100] = 0x11;
    g.mem[0x101] = 0x22;
    g.mem[0x102] = 0x33;
    g.mem[0x103] = 0x00;

    alu_mvc(&g, 0x100, 0x101, 3);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x22);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x33);
    ASSERT_EQ(g.mem[0x102], (uint8_t)0x00);
}

/* ------------------------------------------------------------------ */
/* MVI – Move Immediate                                                */
/* ------------------------------------------------------------------ */

UTEST(logic, mvi_basic)
{
    struct ge g;
    ge_init(&g);

    alu_mvi(&g, 0x400, 0xAB);
    ASSERT_EQ(g.mem[0x400], (uint8_t)0xAB);

    alu_mvi(&g, 0x400, 0x00);
    ASSERT_EQ(g.mem[0x400], (uint8_t)0x00);
}

/* ------------------------------------------------------------------ */
/* NC – AND Characters (no CC)                                         */
/* ------------------------------------------------------------------ */

UTEST(logic, nc_basic)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xFF;
    g.mem[0x101] = 0xAA;
    g.mem[0x200] = 0x0F;
    g.mem[0x201] = 0x55;

    alu_nc(&g, 0x100, 0x200, 2);

    /* 0xFF & 0x0F = 0x0F, 0xAA & 0x55 = 0x00 */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0x0F);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x00);

    /* operand2 unchanged */
    ASSERT_EQ(g.mem[0x200], (uint8_t)0x0F);
    ASSERT_EQ(g.mem[0x201], (uint8_t)0x55);
}

/* ------------------------------------------------------------------ */
/* OC – OR Characters (no CC)                                          */
/* ------------------------------------------------------------------ */

UTEST(logic, oc_basic)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xF0;
    g.mem[0x101] = 0x00;
    g.mem[0x200] = 0x0F;
    g.mem[0x201] = 0xAA;

    alu_oc(&g, 0x100, 0x200, 2);

    /* 0xF0 | 0x0F = 0xFF, 0x00 | 0xAA = 0xAA */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0xFF);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0xAA);

    ASSERT_EQ(g.mem[0x200], (uint8_t)0x0F);
    ASSERT_EQ(g.mem[0x201], (uint8_t)0xAA);
}

/* ------------------------------------------------------------------ */
/* XC – Exclusive-OR Characters (sets CC)                              */
/* ------------------------------------------------------------------ */

UTEST(logic, xc_nonzero_result)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xFF;
    g.mem[0x101] = 0x00;
    g.mem[0x200] = 0x0F;
    g.mem[0x201] = 0x00;

    alu_xc(&g, 0x100, 0x200, 2);

    /* 0xFF ^ 0x0F = 0xF0, 0x00 ^ 0x00 = 0x00 → result not all-zero */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0xF0);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x00);
    /* cc=3: result non-zero */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}

UTEST(logic, xc_all_zero_result)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xAB;
    g.mem[0x101] = 0xCD;
    g.mem[0x200] = 0xAB;
    g.mem[0x201] = 0xCD;

    alu_xc(&g, 0x100, 0x200, 2);

    /* 0xAB ^ 0xAB = 0x00, 0xCD ^ 0xCD = 0x00 → all-zero */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x00);
    /* cc=2: result all-zero */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
}

/* ------------------------------------------------------------------ */
/* NI – AND Immediate (no CC)                                          */
/* ------------------------------------------------------------------ */

UTEST(logic, ni_basic)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x300] = 0xF5;
    alu_ni(&g, 0x300, 0x0F);
    /* 0xF5 & 0x0F = 0x05 */
    ASSERT_EQ(g.mem[0x300], (uint8_t)0x05);
}

/* ------------------------------------------------------------------ */
/* XI – Exclusive-OR Immediate (sets CC)                               */
/* ------------------------------------------------------------------ */

UTEST(logic, xi_nonzero)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x300] = 0x55;
    alu_xi(&g, 0x300, 0xAA);
    /* 0x55 ^ 0xAA = 0xFF */
    ASSERT_EQ(g.mem[0x300], (uint8_t)0xFF);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}

UTEST(logic, xi_zero)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x300] = 0xCC;
    alu_xi(&g, 0x300, 0xCC);
    /* 0xCC ^ 0xCC = 0x00 */
    ASSERT_EQ(g.mem[0x300], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
}

/* ------------------------------------------------------------------ */
/* CMC – Compare Characters (sets CC)                                  */
/* ------------------------------------------------------------------ */

UTEST(logic, cmc_equal)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x41;
    g.mem[0x101] = 0x42;
    g.mem[0x200] = 0x41;
    g.mem[0x201] = 0x42;

    alu_cmc(&g, 0x100, 0x200, 2);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);

    /* fields unmodified */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0x41);
    ASSERT_EQ(g.mem[0x200], (uint8_t)0x41);
}

UTEST(logic, cmc_less)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x41;
    g.mem[0x101] = 0x40; /* 'A', '@' */
    g.mem[0x200] = 0x41;
    g.mem[0x201] = 0x42; /* 'A', 'B' */

    /* op1 < op2 because [0x101]=0x40 < [0x201]=0x42 */
    alu_cmc(&g, 0x100, 0x200, 2);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

UTEST(logic, cmc_greater)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x41;
    g.mem[0x101] = 0xFF;
    g.mem[0x200] = 0x41;
    g.mem[0x201] = 0x42;

    /* op1 > op2 because [0x101]=0xFF > [0x201]=0x42 */
    alu_cmc(&g, 0x100, 0x200, 2);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}

/* ------------------------------------------------------------------ */
/* CI – Compare Immediate (sets CC)                                    */
/* ------------------------------------------------------------------ */

UTEST(logic, ci_equal)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x55;
    alu_ci(&g, 0x100, 0x55);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
}

UTEST(logic, ci_less)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x10;
    alu_ci(&g, 0x100, 0x20); /* mem < imm */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

UTEST(logic, ci_greater)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xF0;
    alu_ci(&g, 0x100, 0x20); /* mem > imm */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}

/* ------------------------------------------------------------------ */
/* TL – Translate                                                      */
/* ------------------------------------------------------------------ */

UTEST(logic, tl_basic)
{
    struct ge g;
    uint16_t table;
    uint8_t i;

    ge_init(&g);

    /*
     * Build a small identity table at 0x0000 (a multiple of 256 per
     * the manual's requirement for the table base), then override a
     * few entries to create a known mapping:
     *   0x41 ('A') → 0x61 ('a')
     *   0x42 ('B') → 0x62 ('b')
     *   0x43 ('C') → 0x63 ('c')
     */
    table = 0x0000;
    for (i = 0; ; i++) {
        g.mem[table + i] = i;
        if (i == 255) break;
    }
    g.mem[table + 0x41] = 0x61;
    g.mem[table + 0x42] = 0x62;
    g.mem[table + 0x43] = 0x63;

    /* Source field at 0x200: 'A', 'B', 'C', 0x00 */
    g.mem[0x200] = 0x41;
    g.mem[0x201] = 0x42;
    g.mem[0x202] = 0x43;
    g.mem[0x203] = 0x00;

    alu_tl(&g, 0x200, 4, table);

    ASSERT_EQ(g.mem[0x200], (uint8_t)0x61);
    ASSERT_EQ(g.mem[0x201], (uint8_t)0x62);
    ASSERT_EQ(g.mem[0x202], (uint8_t)0x63);
    ASSERT_EQ(g.mem[0x203], (uint8_t)0x00); /* identity for 0x00 */
}

/* ------------------------------------------------------------------ */
/* TM – Test under Mask (sets CC)                                      */
/* ------------------------------------------------------------------ */

UTEST(logic, tm_all_zero)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xF0; /* bits 7-4 set, 3-0 clear */
    alu_tm(&g, 0x100, 0x0F); /* mask selects bits 3-0 only */

    /* 0xF0 & 0x0F = 0x00 → cc=2 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
    /* memory must NOT be modified */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0xF0);
}

UTEST(logic, tm_nonzero)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xAA; /* 1010 1010 */
    alu_tm(&g, 0x100, 0xFF); /* mask = all bits */

    /* 0xAA & 0xFF = 0xAA ≠ 0 → cc=3 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
    ASSERT_EQ(g.mem[0x100], (uint8_t)0xAA);
}

UTEST(logic, tm_partial_match)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x55; /* 0101 0101 */
    alu_tm(&g, 0x100, 0x80); /* test bit 7 only — it is 0 */

    /* 0x55 & 0x80 = 0x00 → cc=2 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);

    alu_tm(&g, 0x100, 0x01); /* test bit 0 — it is 1 */
    /* 0x55 & 0x01 = 0x01 → cc=3 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}
