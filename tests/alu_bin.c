/**
 * @file  tests/alu_bin.c
 * @brief Unit tests for alu_bin: AB, SB, AD, SD helpers.
 *
 * No UTEST_MAIN here — tests/main.c provides it.
 *
 * Operand convention in all tests:
 *   addr points to the RIGHTMOST (least-significant) byte of the field.
 *   Multi-byte fields are big-endian: MSB at addr-len+1, LSB at addr.
 *
 * Tests cover: simple ops, carry/borrow across bytes, overflow, zero result,
 * negative result (SB), and unequal operand lengths.
 */

#include "utest.h"
#include "../ge.h"
#include "../alu_bin.h"

/* ================================================================== */
/* AB – Add Binary                                                      */
/* ================================================================== */

/* Simple 1-byte add: 0x10 + 0x20 = 0x30, non-zero, no overflow */
UTEST(bin, ab_simple_no_carry)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x10;   /* A: 1-byte field at addr 0x100 */
    g.mem[0x200] = 0x20;   /* B: 1-byte field at addr 0x200 */

    alu_ab(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x30);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* non-zero, no overflow */
}

/* 1-byte add resulting in zero: 0x00 + 0x00 = 0x00 */
UTEST(bin, ab_zero_result)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x00;
    g.mem[0x200] = 0x00;

    alu_ab(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)0); /* zero, no overflow */
}

/* 2-byte add with carry between bytes: 0x00FF + 0x0001 = 0x0100 */
/* Field at addr 0x101 (MSB=0x00 at 0x100, LSB=0xFF at 0x101)    */
/* + 0x0001 (MSB=0x00 at 0x201, LSB=0x01 at 0x202) — but both    */
/* fields 2-bytes, so B at addr 0x201:                             */
/*   mem[0x200]=0x00, mem[0x201]=0x01 => field value 0x0001        */
UTEST(bin, ab_carry_across_bytes)
{
    struct ge g;
    ge_init(&g);

    /* A = 0x00FF: MSB at 0x100, LSB at 0x101 */
    g.mem[0x100] = 0x00;
    g.mem[0x101] = 0xFF;

    /* B = 0x0001: MSB at 0x200, LSB at 0x201 */
    g.mem[0x200] = 0x00;
    g.mem[0x201] = 0x01;

    alu_ab(&g, 0x101, 2, 0x201, 2);

    /* Expected: 0x0100 */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0x01);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* non-zero, no overflow */
}

/* 1-byte overflow: 0xFF + 0x01 = 0x00 with carry lost → overflow+zero */
UTEST(bin, ab_overflow_zero)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xFF;
    g.mem[0x200] = 0x01;

    alu_ab(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2); /* overflow + zero */
}

/* 1-byte overflow: 0xFF + 0x02 = 0x01 with carry lost → overflow+non-zero */
UTEST(bin, ab_overflow_nonzero)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0xFF;
    g.mem[0x200] = 0x02;

    alu_ab(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x01);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3); /* overflow + non-zero */
}

/* Unequal lengths: A is 2 bytes, B is 1 byte.
 * A = 0x0100 (at 0x101), B = 0x01 (at 0x200).
 * B is zero-extended to 2 bytes: 0x0001.
 * Expected: 0x0100 + 0x0001 = 0x0101.
 */
UTEST(bin, ab_unequal_len_b_shorter)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x01;
    g.mem[0x101] = 0x00;

    g.mem[0x200] = 0x01; /* 1-byte B */

    alu_ab(&g, 0x101, 2, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x01);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x01);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* non-zero */
}

/* Unequal lengths: A is 1 byte, B is 2 bytes (L2 > L1 → treat as L2=L1).
 * A = 0x10 (at 0x100), B-low = 0x05 (at 0x201), B-high = 0xAB (at 0x200).
 * Only the low byte of B participates: 0x10 + 0x05 = 0x15.
 */
UTEST(bin, ab_unequal_len_b_longer)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x10;   /* A: 1-byte */

    g.mem[0x200] = 0xAB;   /* B: 2-byte, MSB (should be ignored) */
    g.mem[0x201] = 0x05;   /* B: 2-byte, LSB                     */

    alu_ab(&g, 0x100, 1, 0x201, 2);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x15);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* non-zero */
}

/* Second operand is not altered */
UTEST(bin, ab_b_not_modified)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x01;
    g.mem[0x200] = 0x02;

    alu_ab(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x200], (uint8_t)0x02); /* B unchanged */
}

/* ================================================================== */
/* SB – Subtract Binary                                                */
/* ================================================================== */

/* Simple positive result: 0x05 - 0x03 = 0x02, cc=3 (positive) */
UTEST(bin, sb_positive_result)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x05;
    g.mem[0x200] = 0x03;

    alu_sb(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x02);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3); /* result > 0 */
}

/* Zero result: 0x07 - 0x07 = 0x00, cc=2 */
UTEST(bin, sb_zero_result)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x07;
    g.mem[0x200] = 0x07;

    alu_sb(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2); /* result == 0 */
}

/* Negative result: 0x03 - 0x05 = -2 → stored as 0xFE (two's complement),
 * cc=1 (result < 0).
 */
UTEST(bin, sb_negative_result_complemented)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x03;
    g.mem[0x200] = 0x05;

    alu_sb(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0xFE); /* -2 in two's complement */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);  /* result < 0 */
}

/* Borrow across bytes: 0x0100 - 0x0001 = 0x00FF */
/* A = 0x0100: MSB at 0x100 = 0x01, LSB at 0x101 = 0x00 */
/* B = 0x0001: MSB at 0x200 = 0x00, LSB at 0x201 = 0x01 */
UTEST(bin, sb_borrow_across_bytes)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x01;
    g.mem[0x101] = 0x00;

    g.mem[0x200] = 0x00;
    g.mem[0x201] = 0x01;

    alu_sb(&g, 0x101, 2, 0x201, 2);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0xFF);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3); /* result > 0 */
}

/* Unequal lengths: A=2 bytes, B=1 byte (B zero-extended).
 * A = 0x0005 - B = 0x0003 = 0x0002, cc=3.
 */
UTEST(bin, sb_unequal_len)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x00; /* A MSB */
    g.mem[0x101] = 0x05; /* A LSB */

    g.mem[0x200] = 0x03; /* B: 1 byte */

    alu_sb(&g, 0x101, 2, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x02);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3); /* result > 0 */
}

/* Second operand is not altered */
UTEST(bin, sb_b_not_modified)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x05;
    g.mem[0x200] = 0x03;

    alu_sb(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x200], (uint8_t)0x03);
}

/* ================================================================== */
/* AD – Add Decimal (unpacked / zoned)                                 */
/* ================================================================== */

/* Simple 1-digit add: digit 3 + digit 4 = digit 7, cc=1 */
UTEST(bin, ad_simple)
{
    struct ge g;
    ge_init(&g);

    /*
     * Zoned format: high nibble = zone (0x00 here for simplicity),
     * low nibble = digit.
     */
    g.mem[0x100] = 0x03; /* digit 3 in A */
    g.mem[0x200] = 0x04; /* digit 4 in B */

    alu_ad(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x07); /* digit 7 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);  /* non-zero */
}

/* Zero result: 0 + 0 = 0, cc=0 */
UTEST(bin, ad_zero_result)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x00;
    g.mem[0x200] = 0x00;

    alu_ad(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)0); /* zero */
}

/* Decimal carry: 9 + 1 = 10 → low digit=0, carry to next position */
/* 2-digit field: A = 09 (MSB=0x00 at 0x100, LSB=0x09 at 0x101)    */
/*                B = 01 (MSB=0x00 at 0x200, LSB=0x01 at 0x201)    */
/* Expected: 10 → digits 1 0 → mem[0x100]=0x01, mem[0x101]=0x00    */
UTEST(bin, ad_decimal_carry)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x00; /* A: tens digit = 0 */
    g.mem[0x101] = 0x09; /* A: units digit = 9 */

    g.mem[0x200] = 0x00; /* B: tens digit = 0 */
    g.mem[0x201] = 0x01; /* B: units digit = 1 */

    alu_ad(&g, 0x101, 2, 0x201, 2);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x01); /* tens = 1 */
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x00); /* units = 0 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);  /* non-zero */
}

/* Zone nibble in operand is ignored (zone = 0x4 per EBCDIC convention) */
/* 3 + 4 = 7; both operands have zone 0x4 in high nibble               */
UTEST(bin, ad_zone_ignored)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x43; /* zone=4, digit=3 */
    g.mem[0x200] = 0x44; /* zone=4, digit=4 */

    alu_ad(&g, 0x100, 1, 0x200, 1);

    /* Result digit = 7, zone set to 0x00 (zones not processed) */
    ASSERT_EQ(g.mem[0x100], (uint8_t)0x07);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

/* Unequal lengths: A=2 digits, B=1 digit (B zero-extended on left)
 * A = 15 (0x01 at 0x100, 0x05 at 0x101)
 * B = 03 (0x03 at 0x200, 1-byte)
 * Expected: 15 + 03 = 18
 */
UTEST(bin, ad_unequal_len)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x01; /* tens digit of A */
    g.mem[0x101] = 0x05; /* units digit of A */

    g.mem[0x200] = 0x03; /* 1-byte B */

    alu_ad(&g, 0x101, 2, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x01); /* tens = 1 */
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x08); /* units = 8 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

/* Carry beyond field is silently dropped (no overflow CC) */
/* 1-digit: 9 + 9 = 18 → digit=8, carry dropped, cc=1 (non-zero) */
UTEST(bin, ad_carry_dropped)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x09;
    g.mem[0x200] = 0x09;

    alu_ad(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x08); /* 18 mod 10 = 8 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);  /* non-zero, no overflow CC */
}

/* Second operand is not altered */
UTEST(bin, ad_b_not_modified)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x03;
    g.mem[0x200] = 0x04;

    alu_ad(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x200], (uint8_t)0x04);
}

/* ================================================================== */
/* SD – Subtract Decimal                                               */
/* ================================================================== */

/* Simple: 7 - 3 = 4, cc=1 */
UTEST(bin, sd_simple)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x07;
    g.mem[0x200] = 0x03;

    alu_sd(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x04);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1); /* non-zero */
}

/* Zero result: 5 - 5 = 0, cc=0 */
UTEST(bin, sd_zero_result)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x05;
    g.mem[0x200] = 0x05;

    alu_sd(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)0); /* zero */
}

/* Decimal borrow: 2-digit 10 - 01 = 09
 * A = 10 (mem[0x100]=0x01 tens, mem[0x101]=0x00 units)
 * B = 01 (mem[0x200]=0x00 tens, mem[0x201]=0x01 units)
 * Expected: 09
 */
UTEST(bin, sd_decimal_borrow)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x01; /* A: tens = 1 */
    g.mem[0x101] = 0x00; /* A: units = 0 */

    g.mem[0x200] = 0x00; /* B: tens = 0 */
    g.mem[0x201] = 0x01; /* B: units = 1 */

    alu_sd(&g, 0x101, 2, 0x201, 2);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x00); /* tens = 0 */
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x09); /* units = 9 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);  /* non-zero */
}

/* Zone nibble ignored: digit 0x47 (zone=4,digit=7) - digit 0x43 (zone=4,digit=3) = 4 */
UTEST(bin, sd_zone_ignored)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x47; /* zone=4, digit=7 */
    g.mem[0x200] = 0x43; /* zone=4, digit=3 */

    alu_sd(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x04); /* digit 4, zone cleared */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

/* Unequal lengths: A=2 digits, B=1 digit.
 * A = 18, B = 05 → 13
 */
UTEST(bin, sd_unequal_len)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x01; /* A: tens = 1 */
    g.mem[0x101] = 0x08; /* A: units = 8 */

    g.mem[0x200] = 0x05; /* B: 1-byte */

    alu_sd(&g, 0x101, 2, 0x200, 1);

    ASSERT_EQ(g.mem[0x100], (uint8_t)0x01); /* tens = 1 */
    ASSERT_EQ(g.mem[0x101], (uint8_t)0x03); /* units = 3 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

/* Second operand is not altered */
UTEST(bin, sd_b_not_modified)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x100] = 0x07;
    g.mem[0x200] = 0x03;

    alu_sd(&g, 0x100, 1, 0x200, 1);

    ASSERT_EQ(g.mem[0x200], (uint8_t)0x03);
}
