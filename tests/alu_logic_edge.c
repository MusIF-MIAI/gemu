/**
 * @file  tests/alu_logic_edge.c
 * @brief Edge-case tests for GE-120/130 logical ALU primitives (alu_logic.h).
 *
 * Covers cases NOT in tests/alu_logic.c:
 *   - MVC left-to-right propagation (dst = src+1, len=4)
 *   - XC: equal 3-byte (cc=2) and differing 3-byte (cc=3)
 *   - CMC: fields agree on byte 0 but differ at byte 1 (stop-at-first-diff)
 *   - OC / NC: 3-byte results, operand-B unchanged, no CC written
 *   - TL: table[i]=i+1 at 256-aligned address 0x0100
 *   - TM: zero mask (all-selected-zero → cc=2), memory write guard
 *   - CI: memory byte not modified for all three compare outcomes
 *   - XI: 0x00 ^ 0x00 → result 0x00, cc=2
 *
 * No UTEST_MAIN here — tests/main.c provides it.
 *
 * CC encoding (GE-130 manual §5.5.3.x / §5.6.3.x):
 *   cc = (FA04 << 1) | FA05
 *   CMC/CI compare:  cc=1 less, cc=2 equal, cc=3 greater
 *   XC/XI:           cc=2 all-zero result, cc=3 non-zero result
 *   TM:              cc=2 all-selected-zero, cc=3 at-least-one-set
 */

#include "utest.h"
#include "../ge.h"
#include "../alu_logic.h"
#include "../alu_cc.h"

/* ------------------------------------------------------------------ */
/* MVC – left-to-right propagation (dst = src+1)                       */
/* ------------------------------------------------------------------ */

/*
 * dst = src+1, len = 4.  Because the copy is strictly left-to-right,
 * each destination byte is read from a source location that was already
 * overwritten with the original mem[src] value.  The initial 0xAA at
 * 0x50 therefore propagates into all four destination bytes.
 *
 *   step 0: mem[0x51] = mem[0x50] = 0xAA
 *   step 1: mem[0x52] = mem[0x51] = 0xAA  (0x51 already overwritten)
 *   step 2: mem[0x53] = mem[0x52] = 0xAA  (0x52 already overwritten)
 *   step 3: mem[0x54] = mem[0x53] = 0xAA  (0x53 already overwritten)
 *
 * Expected: mem[0x50]=0xAA (untouched), mem[0x51..0x54] all 0xAA.
 */
UTEST(alu_logic_edge, mvc_propagate_left_to_right)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0xAA;
    g.mem[0x51] = 0x11;
    g.mem[0x52] = 0x22;
    g.mem[0x53] = 0x33;
    g.mem[0x54] = 0x44;

    alu_mvc(&g, 0x51, 0x50, 4);

    ASSERT_EQ(g.mem[0x50], (uint8_t)0xAA); /* original source byte untouched */
    ASSERT_EQ(g.mem[0x51], (uint8_t)0xAA);
    ASSERT_EQ(g.mem[0x52], (uint8_t)0xAA);
    ASSERT_EQ(g.mem[0x53], (uint8_t)0xAA);
    ASSERT_EQ(g.mem[0x54], (uint8_t)0xAA);
}

/* ------------------------------------------------------------------ */
/* XC – equal and differing multi-byte fields                          */
/* ------------------------------------------------------------------ */

/*
 * Three-byte XC where both fields are identical: result is all-zero,
 * cc must be 2.  The existing xc_all_zero_result uses 2-byte fields;
 * this uses 3 bytes to confirm the all-zero check spans the full length.
 */
UTEST(alu_logic_edge, xc_equal_three_bytes_cc2)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x100] = 0x12;
    g.mem[0x101] = 0x34;
    g.mem[0x102] = 0x56;
    g.mem[0x200] = 0x12;
    g.mem[0x201] = 0x34;
    g.mem[0x202] = 0x56;

    alu_xc(&g, 0x100, 0x200, 3);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x102], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
}

/*
 * Three-byte XC where the fields differ on the last byte only.
 * The result is not all-zero, so cc must be 3 even when the first two
 * result bytes happen to be 0x00.
 */
UTEST(alu_logic_edge, xc_differ_last_byte_cc3)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x100] = 0xAB;
    g.mem[0x101] = 0xCD;
    g.mem[0x102] = 0x01;
    g.mem[0x200] = 0xAB;
    g.mem[0x201] = 0xCD;
    g.mem[0x202] = 0x00;

    alu_xc(&g, 0x100, 0x200, 3);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x102], (uint8_t)0x01);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}

/* ------------------------------------------------------------------ */
/* CMC – stop-at-first-differing-byte                                  */
/* ------------------------------------------------------------------ */

/*
 * Three-byte CMC where byte 0 is equal but byte 1 differs.
 * The comparison must stop at byte 1 and report that result; byte 2
 * is irrelevant.  Two sub-cases:
 *   a) op1[1] < op2[1] → cc=1
 *   b) op1[1] > op2[1] → cc=3
 * Neither operand field is modified.
 */
UTEST(alu_logic_edge, cmc_differ_at_second_byte_less)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x100] = 0x41; /* equal first byte */
    g.mem[0x101] = 0x10; /* smaller second byte */
    g.mem[0x102] = 0xFF; /* should not affect outcome */
    g.mem[0x200] = 0x41; /* equal first byte */
    g.mem[0x201] = 0x20; /* larger second byte */
    g.mem[0x202] = 0x00; /* should not affect outcome */

    alu_cmc(&g, 0x100, 0x200, 3);

    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* op1 < op2 */

    /* operands must be unmodified */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0x41);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x10);
    ASSERT_EQ(g.mem[0x102], (uint8_t)0xFF);
    ASSERT_EQ(g.mem[0x200], (uint8_t)0x41);
    ASSERT_EQ(g.mem[0x201], (uint8_t)0x20);
    ASSERT_EQ(g.mem[0x202], (uint8_t)0x00);
}

UTEST(alu_logic_edge, cmc_differ_at_second_byte_greater)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x100] = 0x41; /* equal first byte */
    g.mem[0x101] = 0x80; /* larger second byte */
    g.mem[0x102] = 0x00; /* should not affect outcome */
    g.mem[0x200] = 0x41; /* equal first byte */
    g.mem[0x201] = 0x30; /* smaller second byte */
    g.mem[0x202] = 0xFF; /* should not affect outcome */

    alu_cmc(&g, 0x100, 0x200, 3);

    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3); /* op1 > op2 */

    /* operands must be unmodified */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0x41);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x80);
    ASSERT_EQ(g.mem[0x102], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x200], (uint8_t)0x41);
    ASSERT_EQ(g.mem[0x201], (uint8_t)0x30);
    ASSERT_EQ(g.mem[0x202], (uint8_t)0xFF);
}

/* ------------------------------------------------------------------ */
/* OC / NC – 3-byte multi-byte results; operand-B unchanged; no CC    */
/* ------------------------------------------------------------------ */

/*
 * OC three bytes: result written into operand A, operand B untouched.
 * No CC assertion is made (the manual says "qualitative result: it is
 * not interested").
 */
UTEST(alu_logic_edge, oc_three_bytes_result_and_b_unchanged)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x100] = 0xF0;
    g.mem[0x101] = 0x0F;
    g.mem[0x102] = 0xAA;
    g.mem[0x200] = 0x0F;
    g.mem[0x201] = 0xF0;
    g.mem[0x202] = 0x55;

    alu_oc(&g, 0x100, 0x200, 3);

    /* 0xF0|0x0F=0xFF, 0x0F|0xF0=0xFF, 0xAA|0x55=0xFF */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0xFF);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0xFF);
    ASSERT_EQ(g.mem[0x102], (uint8_t)0xFF);

    /* operand B unmodified */
    ASSERT_EQ(g.mem[0x200], (uint8_t)0x0F);
    ASSERT_EQ(g.mem[0x201], (uint8_t)0xF0);
    ASSERT_EQ(g.mem[0x202], (uint8_t)0x55);
}

/*
 * NC three bytes: result written into operand A, operand B untouched.
 * No CC assertion is made.
 */
UTEST(alu_logic_edge, nc_three_bytes_result_and_b_unchanged)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x100] = 0xFF;
    g.mem[0x101] = 0xAA;
    g.mem[0x102] = 0x0F;
    g.mem[0x200] = 0x0F;
    g.mem[0x201] = 0x55;
    g.mem[0x202] = 0xF0;

    alu_nc(&g, 0x100, 0x200, 3);

    /* 0xFF&0x0F=0x0F, 0xAA&0x55=0x00, 0x0F&0xF0=0x00 */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0x0F);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x102], (uint8_t)0x00);

    /* operand B unmodified */
    ASSERT_EQ(g.mem[0x200], (uint8_t)0x0F);
    ASSERT_EQ(g.mem[0x201], (uint8_t)0x55);
    ASSERT_EQ(g.mem[0x202], (uint8_t)0xF0);
}

/* ------------------------------------------------------------------ */
/* TL – translate via table[i]=i+1 at 0x0100                          */
/* ------------------------------------------------------------------ */

/*
 * Build a 256-byte table at 0x0100 (a multiple of 256) where entry i
 * maps to i+1.  Entry 0xFF wraps to 0x00 per uint8_t arithmetic.
 * Translate a 3-byte field and verify each byte is incremented.
 */
UTEST(alu_logic_edge, tl_increment_table)
{
    struct ge g;
    uint16_t table;
    uint16_t i;

    ge_init(&g);
    ge_clear(&g);

    table = 0x0100; /* 256-aligned */
    for (i = 0; i < 256; i++) {
        g.mem[table + i] = (uint8_t)(i + 1); /* wraps: 0xFF+1 = 0x00 */
    }

    /* Source field at 0x0300: three distinct byte values */
    g.mem[0x0300] = 0x00;
    g.mem[0x0301] = 0x7F;
    g.mem[0x0302] = 0xFF;

    alu_tl(&g, 0x0300, 3, table);

    ASSERT_EQ(g.mem[0x0300], (uint8_t)0x01); /* 0x00+1 = 0x01 */
    ASSERT_EQ(g.mem[0x0301], (uint8_t)0x80); /* 0x7F+1 = 0x80 */
    ASSERT_EQ(g.mem[0x0302], (uint8_t)0x00); /* 0xFF+1 wraps to 0x00 */

    /* table bytes must be unmodified */
    ASSERT_EQ(g.mem[table + 0x00], (uint8_t)0x01);
    ASSERT_EQ(g.mem[table + 0x7F], (uint8_t)0x80);
    ASSERT_EQ(g.mem[table + 0xFF], (uint8_t)0x00);
}

/* ------------------------------------------------------------------ */
/* TM – zero mask and write-guard                                      */
/* ------------------------------------------------------------------ */

/*
 * A zero mask means no bits are selected; the masked result is always
 * 0x00, so cc must be 2 regardless of the byte value.  The byte at
 * addr must not be modified by TM in either branch.
 */
UTEST(alu_logic_edge, tm_zero_mask_always_cc2)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x400] = 0xFF; /* all bits set */
    alu_tm(&g, 0x400, 0x00); /* mask = 0 selects nothing */

    /* 0xFF & 0x00 = 0x00 → cc=2 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);

    /* memory must NOT be modified */
    ASSERT_EQ(g.mem[0x400], (uint8_t)0xFF);
}

/*
 * Verify that TM does not write to memory for both cc=2 and cc=3 cases.
 * Uses a different address from tm_all_zero/tm_nonzero in alu_logic.c.
 */
UTEST(alu_logic_edge, tm_no_memory_write_either_branch)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    /* cc=2 branch: selected bits all zero */
    g.mem[0x500] = 0x0F;
    alu_tm(&g, 0x500, 0xF0); /* 0x0F & 0xF0 = 0x00 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
    ASSERT_EQ(g.mem[0x500], (uint8_t)0x0F); /* byte unchanged */

    /* cc=3 branch: at least one selected bit set */
    g.mem[0x501] = 0x0F;
    alu_tm(&g, 0x501, 0x0F); /* 0x0F & 0x0F = 0x0F */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
    ASSERT_EQ(g.mem[0x501], (uint8_t)0x0F); /* byte unchanged */
}

/* ------------------------------------------------------------------ */
/* CI – memory byte not modified for any compare outcome               */
/* ------------------------------------------------------------------ */

/*
 * The existing ci_less/ci_equal/ci_greater in alu_logic.c do NOT assert
 * that the memory byte is unchanged after the compare.  These tests add
 * that guard for all three CC branches.
 */
UTEST(alu_logic_edge, ci_mem_unchanged_all_branches)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    /* less */
    g.mem[0x600] = 0x10;
    alu_ci(&g, 0x600, 0x20);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
    ASSERT_EQ(g.mem[0x600], (uint8_t)0x10); /* unchanged */

    /* equal */
    g.mem[0x601] = 0x55;
    alu_ci(&g, 0x601, 0x55);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
    ASSERT_EQ(g.mem[0x601], (uint8_t)0x55); /* unchanged */

    /* greater */
    g.mem[0x602] = 0xF0;
    alu_ci(&g, 0x602, 0x20);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
    ASSERT_EQ(g.mem[0x602], (uint8_t)0xF0); /* unchanged */
}

/* ------------------------------------------------------------------ */
/* XI – zero XOR zero → result 0x00, cc=2                             */
/* ------------------------------------------------------------------ */

/*
 * XOR of a zero byte with zero immediate: result is 0x00, cc=2.
 * This is distinct from the existing xi_zero (which uses 0xCC ^ 0xCC)
 * and confirms the cc=2 path for the degenerate all-zero case.
 * Also confirms that the result byte (0x00) is written back.
 */
UTEST(alu_logic_edge, xi_zero_xor_zero_cc2)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x700] = 0x00;
    alu_xi(&g, 0x700, 0x00); /* 0x00 ^ 0x00 = 0x00 */

    ASSERT_EQ(g.mem[0x700], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
}
