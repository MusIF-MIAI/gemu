/**
 * @file  tests/alu_bin_edge.c
 * @brief Edge-case unit tests for alu_bin: AB, SB, AD helpers.
 *
 * No UTEST_MAIN here — tests/main.c provides it.
 *
 * Covers overflow, two's-complement negative results, operand-length
 * mismatch (zero-extension), and zone-clearing in decimal add.  These
 * cases are intentionally distinct from those in tests/alu_bin.c.
 *
 * Operand convention:
 *   addr points to the RIGHTMOST (least-significant) byte of the field.
 *   Multi-byte fields are big-endian: MSB at addr-len+1, LSB at addr.
 *
 * CC values used (from alu_bin.h / alu_cc.h):
 *   AB: 0=zero no-ovf, 1=nonzero no-ovf, 2=overflow+zero, 3=overflow+nonzero
 *   SB: 0=impossible,  1=result<0,        2=result==0,     3=result>0
 *   AD: 0=result zero, 1=result nonzero
 */

#include "utest.h"
#include "../ge.h"
#include "../alu_bin.h"
#include "../alu_cc.h"

/* ================================================================== */
/* AB – Add Binary edge cases                                          */
/* ================================================================== */

/* AB carry/overflow producing zero: 0xFF + 0x01 = 0x100, low byte 0x00.
 * Carry escapes the 1-byte field → overflow.  Partial result is zero → cc=2.
 */
UTEST(alu_bin_edge, ab_overflow_to_zero)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0xFF;
    g.mem[0x60] = 0x01;

    alu_ab(&g, 0x50, 1, 0x60, 1);

    ASSERT_EQ(g.mem[0x50], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2); /* overflow + zero */
}

/* AB carry/overflow producing nonzero: 0xFF + 0x03 = 0x102, low byte 0x02.
 * Carry escapes → overflow.  Partial result 0x02 is nonzero → cc=3.
 */
UTEST(alu_bin_edge, ab_overflow_nonzero)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0xFF;
    g.mem[0x60] = 0x03;

    alu_ab(&g, 0x50, 1, 0x60, 1);

    ASSERT_EQ(g.mem[0x50], (uint8_t)0x02);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3); /* overflow + nonzero */
}

/* AB plain add, no carry: 0x05 + 0x03 = 0x08, cc=1 (nonzero, no overflow). */
UTEST(alu_bin_edge, ab_plain_nonzero)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0x05;
    g.mem[0x60] = 0x03;

    alu_ab(&g, 0x50, 1, 0x60, 1);

    ASSERT_EQ(g.mem[0x50], (uint8_t)0x08);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* nonzero, no overflow */
}

/* ================================================================== */
/* SB – Subtract Binary edge cases                                     */
/* ================================================================== */

/* SB negative result: 0x03 - 0x08 = -5.
 * Stored as two's complement in 1 byte: 256 - 5 = 0xFB.  cc=1 (result < 0).
 */
UTEST(alu_bin_edge, sb_negative_twos_complement)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0x03;
    g.mem[0x60] = 0x08;

    alu_sb(&g, 0x50, 1, 0x60, 1);

    ASSERT_EQ(g.mem[0x50], (uint8_t)0xFB); /* -5 mod 256 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* result < 0 */
}

/* SB zero result: 0x07 - 0x07 = 0x00.  cc=2 (result == 0). */
UTEST(alu_bin_edge, sb_zero)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0x07;
    g.mem[0x60] = 0x07;

    alu_sb(&g, 0x50, 1, 0x60, 1);

    ASSERT_EQ(g.mem[0x50], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2); /* result == 0 */
}

/* SB positive result: 0x08 - 0x03 = 0x05.  cc=3 (result > 0). */
UTEST(alu_bin_edge, sb_positive)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0x08;
    g.mem[0x60] = 0x03;

    alu_sb(&g, 0x50, 1, 0x60, 1);

    ASSERT_EQ(g.mem[0x50], (uint8_t)0x05);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3); /* result > 0 */
}

/* ================================================================== */
/* AB – operand-length mismatch (zero-extension)                       */
/* ================================================================== */

/* AB with 2-byte A and 1-byte B: B is zero-extended to the left.
 * A = 0x0005 (MSB at 0x50 = 0x00, LSB at 0x51 = 0x05).
 * B = 0x03   (at 0x60, 1-byte; zero-extended to 0x0003).
 * Expected: 0x0005 + 0x0003 = 0x0008 → mem[0x50]=0x00, mem[0x51]=0x08.
 * No carry out of the 2-byte field → cc=1.
 */
UTEST(alu_bin_edge, ab_len_mismatch_b_shorter)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0x00; /* A MSB */
    g.mem[0x51] = 0x05; /* A LSB */
    g.mem[0x60] = 0x03; /* B: 1-byte */

    alu_ab(&g, 0x51, 2, 0x60, 1);

    ASSERT_EQ(g.mem[0x51], (uint8_t)0x08);
    ASSERT_EQ(g.mem[0x50], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* nonzero, no overflow */
}

/* ================================================================== */
/* AD – Add Decimal zone-clearing with EBCDIC sign zone (0xF)          */
/* ================================================================== */

/* AD clears zone nibble in result: operands carry zone 0xF (EBCDIC sign zone).
 * A = 0xF5 (zone=0xF, digit=5), B = 0xF3 (zone=0xF, digit=3).
 * Zones are ignored during arithmetic; result digit = 5+3 = 8.
 * Result stored with zone cleared to 0x00: mem[0x50] == 0x08.  cc=1.
 */
UTEST(alu_bin_edge, ad_zone_cleared_sign_zone)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x50] = 0xF5; /* zone=0xF, digit=5 */
    g.mem[0x60] = 0xF3; /* zone=0xF, digit=3 */

    alu_ad(&g, 0x50, 1, 0x60, 1);

    ASSERT_EQ(g.mem[0x50], (uint8_t)0x08); /* digit only, zone cleared */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* nonzero */
}
