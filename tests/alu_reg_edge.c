/**
 * @file  tests/alu_reg_edge.c
 * @brief Edge-case unit tests for alu_reg.c — GE-120/GE-130 register ALU helpers.
 *
 * No UTEST_MAIN here — tests/main.c provides it.
 *
 * Complements tests/alu_reg.c; does NOT duplicate any existing test case.
 *
 * Source evidence:
 *   alu_reg.h  — authoritative CC tables, carry conventions, SR/SL address rules.
 *   alu_cc.h   — ALU_CC_* enum values.
 *   cpu_6.txt §5.6.4 (LR/STR/CMR/AMR/SMR), §5.5.1.3-4 (MVQ/CMQ), §5.5.4 (SR/SL).
 */

#include "utest.h"
#include "../ge.h"
#include "../alu_cc.h"
#include "../alu_reg.h"

/* =========================================================================
 * LR / STR — addressing at a different memory region
 * Distinct from alu_reg.c's 0x100-region round-trip.
 * =========================================================================*/

/*
 * Place known bytes in mem[0x60..0x61] and load them with alu_lr.
 * Then store a different value at 0x62 and verify the two raw bytes.
 * Exercises: big-endian byte order for both load and store.
 */
UTEST(alu_reg_edge, lr_str_low_page)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    /* Write raw bytes directly — tests that alu_lr reads them big-endian. */
    g.mem[0x60] = 0x12;
    g.mem[0x61] = 0x34;

    uint16_t r = 0x0000;
    alu_lr(&g, &r, 0x60);
    ASSERT_EQ(r, (uint16_t)0x1234);

    /* Store 0xABCD at 0x62: MSB must land at 0x62, LSB at 0x63. */
    alu_str(&g, 0xABCD, 0x62);
    ASSERT_EQ(g.mem[0x62], (uint8_t)0xAB);
    ASSERT_EQ(g.mem[0x63], (uint8_t)0xCD);
}

/* =========================================================================
 * AMR — plain add, no overflow, using values distinct from alu_reg.c
 * 0x0001+0x0002 is already tested; use 0x0010+0x0005 instead.
 * Expected: result=0x0015, cc=1 (nonzero, no overflow).
 * =========================================================================*/
UTEST(alu_reg_edge, amr_plain_nonzero)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    uint16_t r = 0x0010;
    alu_amr(&g, &r, 0x0005);
    ASSERT_EQ(r, (uint16_t)0x0015);
    /* No overflow, result != 0 → FA04=0,FA05=1 → cc=1 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
    ASSERT_EQ(g.URPE, (uint8_t)0);
}

/* =========================================================================
 * SMR — specific complement boundary: 0x0003 - 0x0008
 * Result = 0xFFFB (two's complement of 5); negative → cc=1.
 * alu_reg.c uses 0x0003-0x000A; use 0x0003-0x0008 for a distinct test point.
 * =========================================================================*/
UTEST(alu_reg_edge, smr_small_negative)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    uint16_t r = 0x0003;
    alu_smr(&g, &r, 0x0008);
    /* 3 - 8 = -5 = 0xFFFB in 16-bit two's complement */
    ASSERT_EQ(r, (uint16_t)0xFFFB);
    /* result < 0 → FA04=0,FA05=1 → cc=1 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
    ASSERT_EQ(g.URPE, (uint8_t)1); /* borrow occurred */
}

/* =========================================================================
 * SR — found at the last position in the field
 * alu_reg.c covers offset 0 and offset 2; cover offset == len-1 (last byte).
 * Expected: r7 = field + (len-1) + 1 = field + len.
 * (Same result as not-found, but via a match — tests the boundary path.)
 * =========================================================================*/
UTEST(alu_reg_edge, sr_found_last)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    /* field at 0x400, len=4: 0x01 0x02 0x03 0x99; model=0x99 at offset 3. */
    g.mem[0x400] = 0x01;
    g.mem[0x401] = 0x02;
    g.mem[0x402] = 0x03;
    g.mem[0x403] = 0x99;

    uint16_t r7 = 0x0000;
    alu_sr(&g, &r7, 0x400, 4, 0x99);
    /* found at offset 3: r7 = 0x400 + 3 + 1 = 0x404 */
    ASSERT_EQ(r7, (uint16_t)0x404);
}

/* =========================================================================
 * SR — field of length 1: not-found
 * Covers the single-byte degenerate field, not-found case.
 * Expected: r7 = field + 1.
 * =========================================================================*/
UTEST(alu_reg_edge, sr_single_byte_not_found)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x500] = 0xAA;

    uint16_t r7 = 0x0000;
    alu_sr(&g, &r7, 0x500, 1, 0xBB); /* 0xBB not in field */
    /* not found: r7 = 0x500 + 1 = 0x501 */
    ASSERT_EQ(r7, (uint16_t)0x501);
}

/* =========================================================================
 * SR — field of length 1: found
 * Counterpart to the single-byte not-found above.
 * Expected: r7 = field + 0 + 1 = field + 1.
 * Note: result address is the same as not-found for a 1-byte field,
 * confirming the formula holds at the boundary.
 * =========================================================================*/
UTEST(alu_reg_edge, sr_single_byte_found)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x500] = 0xAA;

    uint16_t r7 = 0x0000;
    alu_sr(&g, &r7, 0x500, 1, 0xAA); /* match at offset 0 */
    /* found at offset 0: r7 = 0x500 + 0 + 1 = 0x501 */
    ASSERT_EQ(r7, (uint16_t)0x501);
}

/* =========================================================================
 * CMQ — two-byte field: first field is greater due to second digit
 * {0xF1, 0xF5} vs {0xF1, 0xF3}: first digits equal (1==1), second digits
 * differ (5>3), so first > second → cc=3.
 * alu_reg.c's cmq_first_greater uses first-digit dominance; this tests
 * tie-breaking on the second digit (confirming left-to-right lexicographic
 * comparison continues past the first equal byte).
 * =========================================================================*/
UTEST(alu_reg_edge, cmq_tiebreak_second_digit_greater)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    /*
     * Field a (rightmost byte at 0x301): 0xF1 at 0x300, 0xF5 at 0x301
     * Field b (rightmost byte at 0x311): 0xF1 at 0x310, 0xF3 at 0x311
     * Digit sequence a: [1, 5]   b: [1, 3]
     * First digits equal; second digit a(5) > b(3) → a > b → cc=3.
     */
    g.mem[0x300] = 0xF1;
    g.mem[0x301] = 0xF5;
    g.mem[0x310] = 0xF1;
    g.mem[0x311] = 0xF3;

    alu_cmq(&g, 0x301, 0x311, 2);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}

/* =========================================================================
 * MVQ — single byte: digit nibble moved, zone nibble preserved in dst
 * dst byte = 0x50 (zone=5, digit=0), src byte = 0x73 (zone=7, digit=3).
 * After MVQ: dst zone 5 preserved, digit 3 copied → 0x53.
 * CC: result digit is 3 (nonzero) → cc=1.
 * =========================================================================*/
UTEST(alu_reg_edge, mvq_single_byte_zone_preserved)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x600] = 0x50; /* dst: zone=5, digit=0 */
    g.mem[0x610] = 0x73; /* src: zone=7, digit=3 */

    alu_mvq(&g, 0x600, 0x610, 1);

    /* dst zone nibble (5) must be preserved; digit nibble from src (3). */
    ASSERT_EQ(g.mem[0x600], (uint8_t)0x53);
    /* result nonzero (digit=3) → cc=1 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
    /* src byte must be unchanged */
    ASSERT_EQ(g.mem[0x610], (uint8_t)0x73);
}
