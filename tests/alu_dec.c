/**
 * @file  tests/alu_dec.c
 * @brief Unit tests for GE-130 packed decimal ALU helpers (alu_dec.c).
 *
 * Test strategy:
 *   - Write packed/zoned bytes directly into g.mem at known addresses.
 *   - Call helper.
 *   - Assert result bytes and alu_get_cc(&g).
 *
 * Packed field reminder:
 *   addr = rightmost byte; byte[addr] = (digit0 << 4) | sign
 *   byte[addr-1] = (digit1 << 4) | digit2  ...
 *
 * All expected values are hand-computed and annotated.
 *
 * MP/DP tests are marked UNCERTAINTY where exact overflow/placement rules
 * could not be fully verified from the available OCR.
 */

#include "utest.h"
#include "../ge.h"
#include "../alu_dec.h"

/* =========================================================================
 * AP — Add Packed
 * ========================================================================= */

UTEST(dec, ap_pos_plus_pos)
{
    /*
     * op1 at 0x0100 (alen=1, 2 bytes): +123  = bytes [0x12, 0x3C]
     *   byte 0x0100: 0x3C  (digit0=3, sign=C=positive)
     *   byte 0x00FF: 0x12  (digit1=1, digit2=2)
     * op2 at 0x0200 (blen=1, 2 bytes): +456  = bytes [0x45, 0x6C]
     *   byte 0x0200: 0x6C  (digit0=6, sign=C)
     *   byte 0x01FF: 0x45  (digit1=4, digit2=5)
     *
     * Expected: 123 + 456 = 579
     *   byte 0x0100: 0x9C  (digit0=9, sign=C)
     *   byte 0x00FF: 0x57  (digit1=5, digit2=7)
     * CC = 3 (positive)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x3C;
    g.mem[0x00FF] = 0x12;
    g.mem[0x0200] = 0x6C;
    g.mem[0x01FF] = 0x45;

    alu_ap(&g, 0x0100, 1, 0x0200, 1);

    ASSERT_EQ(g.mem[0x0100], 0x9C);
    ASSERT_EQ(g.mem[0x00FF], 0x57);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);  /* 3 */
}

UTEST(dec, ap_zero_result)
{
    /*
     * op1: +100 = [0x00, 0x1C]  (bytes at 0x0100, 0x00FF)
     *   byte 0x0100: 0x0C  (digit0=0, sign=C)
     *   byte 0x00FF: 0x10  (digit1=1, digit2=0)
     *   Wait: +100 has digits 1,0,0 (rightmost first: 0,0,1)
     *   digit0 (rightmost digit) = 0 → byte[0x0100] high nibble = 0
     *   digit1 = 0 → byte[0x00FF] high nibble = 0
     *   digit2 = 1 → byte[0x00FF] low  nibble = 1
     *   byte[0x0100] = 0x0C (digit0=0, sign=C)
     *   byte[0x00FF] = 0x01 (digit1=0, digit2=1... wait)
     *
     * Let me re-derive carefully.
     * alen=1: 2 bytes, 3 digits (positions 0=rightmost..2=leftmost).
     * +100:  digit[0]=0, digit[1]=0, digit[2]=1
     *   byte[addr]   = (digit[0]<<4) | sign = 0x0C
     *   byte[addr-1] = (digit[1]<<4) | digit[2] = 0x01
     * op1: +100 → mem[0x0100]=0x0C, mem[0x00FF]=0x01
     *
     * op2: -100 → same digits, sign=D
     *   mem[0x0200]=0x0D, mem[0x01FF]=0x01
     *
     * 100 + (-100) = 0 → CC=2 (zero), sign=+C
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x0C;
    g.mem[0x00FF] = 0x01;
    g.mem[0x0200] = 0x0D;
    g.mem[0x01FF] = 0x01;

    alu_ap(&g, 0x0100, 1, 0x0200, 1);

    ASSERT_EQ(g.mem[0x0100] & 0x0F, 0x0C);  /* sign = positive */
    ASSERT_EQ((g.mem[0x0100] >> 4) & 0x0F, 0);  /* digit0 = 0 */
    ASSERT_EQ(g.mem[0x00FF], 0x00);             /* digits 1,2 = 0,0 */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_ZERO);      /* 2 */
}

UTEST(dec, ap_neg_result)
{
    /*
     * op1: +3 (1 byte, alen=0)  → mem[0x0100] = 0x3C
     * op2: -7 (1 byte, blen=0)  → mem[0x0200] = 0x7D
     * alen=0: 1 byte, 1 digit.
     * 3 + (-7) = -4  → digit[0]=4, sign=D
     * Expected: mem[0x0100] = 0x4D,  CC=1 (negative)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x3C;
    g.mem[0x0200] = 0x7D;

    alu_ap(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x4D);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_NEG);   /* 1 */
}

UTEST(dec, ap_carry_positive)
{
    /*
     * op1: +999 (alen=1, 2 bytes)
     *   digit[0]=9, digit[1]=9, digit[2]=9
     *   mem[0x0100]=0x9C, mem[0x00FF]=0x99
     * op2: +1 (blen=0, 1 byte, but we need blen<=alen so blen=1 zero-padded)
     *   Use blen=1: digit[0]=1, others=0
     *   mem[0x0200]=0x1C, mem[0x01FF]=0x00
     * 999+1 = 1000 → overflow, result truncated (3 digits can only hold 000)
     * CC=0 (overflow)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x9C;
    g.mem[0x00FF] = 0x99;
    g.mem[0x0200] = 0x1C;
    g.mem[0x01FF] = 0x00;

    alu_ap(&g, 0x0100, 1, 0x0200, 1);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);   /* 0 */
}

UTEST(dec, ap_overflow_l1_less_than_l2)
{
    /* alen=0, blen=1 → L1 < L2 → overflow immediately */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x3C;
    g.mem[0x0200] = 0x1C;
    g.mem[0x01FF] = 0x02;

    alu_ap(&g, 0x0100, 0, 0x0200, 1);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);
}

/* =========================================================================
 * SP — Subtract Packed
 * ========================================================================= */

UTEST(dec, sp_basic)
{
    /*
     * op1: +8 (alen=0) → mem[0x0100]=0x8C
     * op2: +3 (blen=0) → mem[0x0200]=0x3C
     * 8 - 3 = 5 → mem[0x0100]=0x5C, CC=3 (positive)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x8C;
    g.mem[0x0200] = 0x3C;

    alu_sp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x5C);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);
}

UTEST(dec, sp_gives_negative)
{
    /*
     * op1: +2 (alen=0) → mem[0x0100]=0x2C
     * op2: +5 (blen=0) → mem[0x0200]=0x5C
     * 2 - 5 = -3 → mem[0x0100]=0x3D, CC=1 (negative)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x2C;
    g.mem[0x0200] = 0x5C;

    alu_sp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x3D);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_NEG);
}

UTEST(dec, sp_op2_not_modified)
{
    /*
     * Verify operand 2 sign is restored after SP (internal sign flip should
     * not be visible to caller).
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x5C;  /* +5 */
    g.mem[0x0200] = 0x3C;  /* +3 */

    alu_sp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0200], 0x3C);  /* op2 sign unchanged */
    ASSERT_EQ(g.mem[0x0100], 0x2C);  /* 5-3=2 positive */
}

/* =========================================================================
 * CMP — Compare Packed
 * ========================================================================= */

UTEST(dec, cmp_equal)
{
    /*
     * op1 = op2 = +42
     * alen=blen=1
     * mem[0x0100]=0x2C, mem[0x00FF]=0x04
     * mem[0x0200]=0x2C, mem[0x01FF]=0x04
     * CC = 2 (equal)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x2C; g.mem[0x00FF] = 0x04;
    g.mem[0x0200] = 0x2C; g.mem[0x01FF] = 0x04;

    alu_cmp(&g, 0x0100, 1, 0x0200, 1);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_EQUAL);   /* 2 */
}

UTEST(dec, cmp_low)
{
    /*
     * op1 = +5 (alen=0): mem[0x0100]=0x5C
     * op2 = +9 (blen=0): mem[0x0200]=0x9C
     * 5 < 9 → CC=1 (low)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x5C;
    g.mem[0x0200] = 0x9C;

    alu_cmp(&g, 0x0100, 0, 0x0200, 0);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_LOW);   /* 1 */
}

UTEST(dec, cmp_high)
{
    /*
     * op1 = +9, op2 = +5 → CC=3 (high)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x9C;
    g.mem[0x0200] = 0x5C;

    alu_cmp(&g, 0x0100, 0, 0x0200, 0);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_HIGH);   /* 3 */
}

UTEST(dec, cmp_neg_less_than_pos)
{
    /*
     * op1 = -1 (alen=0): mem[0x0100]=0x1D
     * op2 = +1 (blen=0): mem[0x0200]=0x1C
     * -1 < +1 → CC=1 (low)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x1D;
    g.mem[0x0200] = 0x1C;

    alu_cmp(&g, 0x0100, 0, 0x0200, 0);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_LOW);
}

UTEST(dec, cmp_negative_zero_eq_positive_zero)
{
    /*
     * Manual §5.6.1.6: "positive zero and a negative zero are considered equal"
     * op1 = -0 (0xD sign): mem[0x0100]=0x0D
     * op2 = +0 (0xC sign): mem[0x0200]=0x0C
     * CC = 2 (equal)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x0D;
    g.mem[0x0200] = 0x0C;

    alu_cmp(&g, 0x0100, 0, 0x0200, 0);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_EQUAL);
}

UTEST(dec, cmp_overflow_l1_less_l2)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x5C;
    g.mem[0x0200] = 0x5C;
    g.mem[0x01FF] = 0x01;

    alu_cmp(&g, 0x0100, 0, 0x0200, 1);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);
}

/* =========================================================================
 * MVP — Move Packed
 * ========================================================================= */

UTEST(dec, mvp_basic)
{
    /*
     * op2: +73 (alen=1, 2 bytes)
     *   digit[0]=3, digit[1]=7, digit[2]=0
     *   mem[0x0200]=0x3C, mem[0x01FF]=0x70
     * op1: any 2-byte destination at 0x0100
     * After MVP: op1 = +73, CC=3 (positive)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0x3C;
    g.mem[0x01FF] = 0x07;  /* digit1=0, digit2=7 → 0x07 */
    g.mem[0x0100] = 0xFF;
    g.mem[0x00FF] = 0xFF;

    alu_mvp(&g, 0x0100, 1, 0x0200, 1);

    ASSERT_EQ(g.mem[0x0100] & 0x0F, 0x0C);  /* sign = + */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);
}

UTEST(dec, mvp_negative_source)
{
    /*
     * op2: -5 (alen=0) → mem[0x0200]=0x5D
     * After MVP: op1 = -5 → mem[0x0100]=0x5D, CC=1 (negative)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0x5D;
    g.mem[0x0100] = 0x00;

    alu_mvp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x5D);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_NEG);
}

UTEST(dec, mvp_overflow)
{
    /*
     * alen=0 < blen=1 → overflow, partial move
     * CC=0
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0x3C;
    g.mem[0x01FF] = 0x12;  /* +123 in 2-byte packed */

    alu_mvp(&g, 0x0100, 0, 0x0200, 1);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);
}

/* =========================================================================
 * PK — Pack (no sign handling)
 * ========================================================================= */

UTEST(dec, pk_basic)
{
    /*
     * PK semantics from the microcode flowchart ("PK Dalla Fase Alfa",
     * 64|65 -> 60-63 -> 40-43) and validated against the funktionalcpu deck
     * (step 0x12/0x13): both pointers INCREMENT, so PK reads the zoned source
     * UPWARD from the given (leftmost, most-significant) address and writes the
     * packed result UPWARD from the given dest address; two consecutive digits
     * fill one packed byte (1st -> high nibble, 2nd -> low), with NO sign
     * nibble (PK does not process a sign). dlen=L -> L+1 packed bytes from
     * 2L+2 zoned digits (low nibbles).
     */
    struct ge g;
    ge_init(&g);

    /* Single-byte case (the deck's step 0x12): zoned 5,6 -> packed 0x56. */
    g.mem[0x0200] = 0x05;  /* digit 5 (more significant) */
    g.mem[0x0201] = 0x06;  /* digit 6 */
    g.mem[0x0100] = 0x00;
    alu_pk(&g, 0x0100, 0, 0x0200, 0);
    ASSERT_EQ(g.mem[0x0100], 0x56);

    /* Multi-byte case: zoned 1,2,3,4 -> packed 0x12 0x34 (dest upward). */
    g.mem[0x0210] = 0xF1;  /* zone ignored; digit 1 (most significant) */
    g.mem[0x0211] = 0xF2;
    g.mem[0x0212] = 0xF3;
    g.mem[0x0213] = 0xF4;  /* digit 4 (least significant) */
    g.mem[0x0110] = 0x00;
    g.mem[0x0111] = 0x00;
    alu_pk(&g, 0x0110, 1, 0x0210, 1);
    ASSERT_EQ(g.mem[0x0110], 0x12);  /* first packed byte (high-order) */
    ASSERT_EQ(g.mem[0x0111], 0x34);  /* second packed byte (low-order) */
}

/* =========================================================================
 * UPK — Unpack (no sign, preserve existing zones)
 * ========================================================================= */

UTEST(dec, upk_basic)
{
    /*
     * Source (packed) at 0x0200 (slen=0, 1 byte):
     *   +7 → mem[0x0200]=0x7C  (digit[0]=7, sign=C)
     * Destination (zoned) at 0x0100 (dlen=0, 1 byte):
     *   Pre-set zone to 0xF: mem[0x0100]=0xF0
     * Expected after UPK: mem[0x0100]=(0xF<<4)|7 = 0xF7
     *   (zone from existing mem preserved, digit=7)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0x7C;
    g.mem[0x0100] = 0xF0;

    alu_upk(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0xF7);
}

/* =========================================================================
 * PK/UPK round-trip
 * ========================================================================= */

UTEST(dec, pk_upk_roundtrip)
{
    /*
     * Start with packed +42 (alen=1):
     *   digit[0]=2, digit[1]=4, digit[2]=0
     *   mem[0x0300]=0x2C, mem[0x02FF]=0x04
     *
     * UPKS → zoned at 0x0200 (dlen=1, 2 bytes):
     *   Each byte: zone=0x4, digit=source digit
     *   mem[0x0200]=(0x4<<4)|2=0x42, mem[0x01FF]=(0x4<<4)|4=0x44,
     *   (but 2*sb-1=3 digits, db=2 bytes, so: i=0 → digit[0]=2 → dst[0]=0x42
     *    i=1 → digit[1]=4 → dst[1]=0x44)
     *
     * PKS from 0x0200 (slen=1) → packed at 0x0100 (dlen=1):
     *   zone of rightmost source (0x0200)=0x4 (not 0xA) → positive sign C
     *   digit[0] from mem[0x0200] low nibble = 2
     *   digit[1] from mem[0x01FF] low nibble = 4
     *   digit[2] = 0 (left-padded)
     *   → packed: digit[0]=2,digit[1]=4,digit[2]=0, sign=C
     *   Same as original! CC=3 (positive)
     */
    struct ge g;
    ge_init(&g);

    /* +42 packed */
    g.mem[0x0300] = 0x2C;
    g.mem[0x02FF] = 0x04;

    /* unpack with sign to 0x0200 */
    alu_upks(&g, 0x0200, 1, 0x0300, 1);

    ASSERT_EQ(g.mem[0x0200] & 0x0F, 0x2);   /* rightmost digit = 2 */
    ASSERT_EQ((g.mem[0x0200] >> 4) & 0x0F, 0x4); /* zone = 4 */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);   /* +42 → positive */

    /* pack with sign back to 0x0100 */
    alu_pks(&g, 0x0100, 1, 0x0200, 1);

    ASSERT_EQ(g.mem[0x0100] & 0x0F, 0xC);   /* sign = positive C */
    ASSERT_EQ((g.mem[0x0100] >> 4) & 0x0F, 0x2); /* digit[0] = 2 */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);
}

/* =========================================================================
 * PKS — sign codes
 * ========================================================================= */

UTEST(dec, pks_negative_sign)
{
    /*
     * Source (zoned): 2 bytes at 0x0200 (slen=1):
     *   Rightmost byte zone = 0xA → negative
     *   0xA5 at 0x0200 (zone=0xA, digit=5)
     *   0xF1 at 0x01FF (zone=F, digit=1)
     * Destination (packed) at 0x0100, dlen=1 (2 bytes):
     *   digit[0]=5 (from mem[0x0200] low nibble)
     *   digit[1]=1 (from mem[0x01FF] low nibble)
     *   digit[2]=0 (left-pad)
     *   sign = D (negative, because zone=0xA)
     *   mem[0x0100]=0x5D, mem[0x00FF]=0x01
     * CC = 1 (negative)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0xA5;
    g.mem[0x01FF] = 0xF1;

    alu_pks(&g, 0x0100, 1, 0x0200, 1);

    ASSERT_EQ(g.mem[0x0100] & 0x0F, 0xD);         /* sign = negative */
    ASSERT_EQ((g.mem[0x0100] >> 4) & 0x0F, 0x5);  /* digit[0] = 5 */
    ASSERT_EQ(g.mem[0x00FF] & 0x0F, 0x1);         /* digit[2] = 1 */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_NEG);
}

UTEST(dec, pks_positive_sign)
{
    /*
     * Rightmost source zone = 0xF → positive sign C
     * digit=3 → mem[0x0100]=0x3C, CC=3 (positive)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0xF3;  /* zone=F (not 0xA) → positive */

    alu_pks(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100] & 0x0F, 0xC);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);
}

UTEST(dec, pks_zero_result)
{
    /*
     * Source: 0x00 zone=0 digit=0 → positive sign, zero value
     * CC = 2 (zero)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0x00;

    alu_pks(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100] & 0x0F, 0xC);   /* positive sign */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_ZERO);
}

/* =========================================================================
 * UPKS — Unpack with Sign
 * ========================================================================= */

UTEST(dec, upks_zone_always_0x4)
{
    /*
     * Source (packed): -9 at 0x0200 (slen=0): mem[0x0200]=0x9D
     * Destination: 0x0100 (dlen=0)
     * Expected: zone=0x4, digit=9 → mem[0x0100]=0x49
     * CC=1 (negative)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0x9D;
    g.mem[0x0100] = 0xFF;  /* any garbage to verify overwrite */

    alu_upks(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x49);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_NEG);
}

UTEST(dec, upks_zero)
{
    /*
     * Source: +0 → mem[0x0200]=0x0C
     * Expected: 0x40, CC=2
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0200] = 0x0C;

    alu_upks(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x40);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_ZERO);
}

/* =========================================================================
 * MP — Multiply Packed
 * =========================================================================
 * UNCERTAINTY: MP internal algorithm is best-effort (see alu_dec.c notes).
 * Testing a simple case: 3 * 4 = 12.
 */

UTEST(dec, mp_basic_3x4)
{
    /*
     * op1 (multiplier): +3, alen=1 (2 bytes, 3 digits)
     *   We need at least blen+1=2 leading zero digits in op1.
     *   alen=1: 3 digits total.  Leading zeros needed ≥ 1 (blen=0+1=1 byte → 1 digit).
     *   +003: digit[0]=3, digit[1]=0, digit[2]=0
     *   mem[0x0100]=0x3C, mem[0x00FF]=0x00
     * op2 (multiplicand): +4, blen=0 (1 byte, 1 digit)
     *   mem[0x0200]=0x4C
     * Expected: 3*4=12, alen=1 result
     *   digit[0]=2, digit[1]=1, digit[2]=0
     *   mem[0x0100]=0x2C, mem[0x00FF]=0x01
     * CC=3 (positive)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x3C;
    g.mem[0x00FF] = 0x00;
    g.mem[0x0200] = 0x4C;

    alu_mp(&g, 0x0100, 1, 0x0200, 0);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);
    ASSERT_EQ((g.mem[0x0100] >> 4) & 0xF, 0x2);  /* units digit = 2 */
    ASSERT_EQ(g.mem[0x0100] & 0x0F, 0xC);        /* sign=positive */
    /* 12 packs as 0x01 0x2C: tens=1 in lo nibble of mem[0x00FF] */
    ASSERT_EQ(g.mem[0x00FF] & 0x0F, 0x1);        /* tens digit = 1 */
}

UTEST(dec, mp_overflow_blen_ge_alen)
{
    /* blen=1 >= alen=1 → not valid (blen must be < alen) → overflow */
    struct ge g;
    ge_init(&g);
    g.mem[0x0100] = 0x3C; g.mem[0x00FF] = 0x00;
    g.mem[0x0200] = 0x4C; g.mem[0x01FF] = 0x00;

    alu_mp(&g, 0x0100, 1, 0x0200, 1);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);
}

/* =========================================================================
 * DP — Divide Packed
 * =========================================================================
 * UNCERTAINTY: DP field placement is best-effort.
 * Testing: 12 / 4 = quotient 3, remainder 0.
 */

UTEST(dec, dp_basic_12_div_4)
{
    /*
     * op1 (dividend): +12, alen=2 (3 bytes, 5 digits)
     *   digit[0]=2,digit[1]=1,digit[2..4]=0
     *   mem[0x0102]=0x2C, mem[0x0101]=0x01, mem[0x0100]=0x00
     * op2 (divisor): +4, blen=0 (1 byte, 1 digit)
     *   mem[0x0200]=0x4C
     * alen=2 > blen=0 ✓, blen≤7 ✓, divisor≠0 ✓
     * q_bytes = alen-blen = 2 (leftmost 2 bytes), rem at rightmost bb=1 byte.
     *   q_addr = op1_addr - bb = 0x0102 - 1 = 0x0101
     *   r_addr = op1_addr = 0x0102
     * Quotient: 12/4 = 3, remainder = 0.
     * Quotient field: q_bytes=2, qn=3 digits: digit[0]=3,others=0
     *   mem[0x0101]=0x3C (digit[0]=3, sign=positive), mem[0x0100]=0x00
     * Remainder field: bb=1, bn=1 digit: digit[0]=0, sign=C (from dividend)
     *   mem[0x0102]=0x0C
     * CC=3 (quotient positive, non-zero)
     *
     * UNCERTAINTY: quotient/remainder placement and sign generation
     *   verified per manual §5.6.1.4 text.
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0102] = 0x2C;  /* rightmost byte of dividend, digit0=2 */
    g.mem[0x0101] = 0x01;  /* digit1=0, digit2=1 */
    g.mem[0x0100] = 0x00;  /* digit3=0, digit4=0 */
    g.mem[0x0200] = 0x4C;  /* divisor: +4 */

    alu_dp(&g, 0x0102, 2, 0x0200, 0);

    /* Quotient +3 is positive, non-zero. Decimal-arith CC = ALU_CC_POS.
     * (CC convention is provisional pending Wave-5 oracle validation.) */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);
    /* Remainder at 0x0102 should be 0 */
    ASSERT_EQ((g.mem[0x0102] >> 4) & 0xF, 0x0);  /* remainder digit=0 */
    /* Quotient rightmost byte at 0x0101: digit[0]=3 */
    ASSERT_EQ((g.mem[0x0101] >> 4) & 0xF, 0x3);
}

UTEST(dec, dp_overflow_l1_le_l2)
{
    struct ge g;
    ge_init(&g);
    g.mem[0x0100] = 0x5C;
    g.mem[0x0200] = 0x2C;

    alu_dp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);
}

UTEST(dec, dp_overflow_divide_by_zero)
{
    struct ge g;
    ge_init(&g);
    g.mem[0x0100] = 0x5C; g.mem[0x00FF] = 0x00;
    g.mem[0x0200] = 0x0C;  /* divisor = 0 */

    alu_dp(&g, 0x0100, 1, 0x0200, 0);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);
}

/* =========================================================================
 * EDT — Edit
 * ========================================================================= */

UTEST(dec, edt_simple_zero_suppress)
{
    /*
     * Pattern (plen=5) at 0x0100:
     *   byte[0x0100] = 0x20  fill char (0x20 is also SST — first byte is fill)
     *   Hmm: first byte is fill char.  Use fill = '*' = 0x2A instead.
     *
     * Let's use a clear example:
     *   Pattern: [fill='*', SST, SST, SST]  (plen=4, 4 bytes at 0x0100..0x0103)
     *   Source (zoned digits): [0, 0, 5] at 0x0200..0x0202
     *     (rightmost digit = mem[0x0202] low nibble)
     *     mem[0x0200]=0xF0 (digit=0), mem[0x0201]=0xF0 (digit=0),
     *     mem[0x0202]=0xF5 (digit=5)
     *   src = 0x0200 (leftmost)
     *
     * Processing:
     *   i=0: pc=0x2A (fill char, INS), zero_suppress=1 → fill byte with fill='*'
     *        Wait — pattern[0] is the fill char, so we should NOT process it
     *        as a pattern byte.  The manual says "first character of pattern is
     *        used as fill character" — it participates in the scan but acts as
     *        fill char storage.
     *        Actually re-reading: "The operation starts always in zero suppression.
     *        The instruction proceeds from left to right, examining the characters
     *        of the pattern."  The first byte IS examined as a pattern char too.
     *        Since 0x2A is not SST/TSZ/RSZ, it's an insertion char.
     *        In zero-suppress mode, insertion chars → replaced with fill char
     *        (per §5.5.3.6 "If RSZ or INS is detected, it commands substitution
     *        with fill char").
     *        So fill char = 0x2A, and mem[0x0100] stays 0x2A.
     *
     * Simpler approach: use pattern where byte[0] is fill='*' (0x2A) and
     * bytes[1-3] are SST (0x20).
     * Source digits (right to left as consumed): we advance src_ptr each SST.
     * src starts at 0x0200.
     *   i=1: SST, digit=mem[0x0200]&0xF=0, zero_suppress=1 → fill → mem[0x0101]=0x2A
     *   i=2: SST, digit=mem[0x0201]&0xF=0, zero_suppress=1 → fill → mem[0x0102]=0x2A
     *   i=3: SST, digit=mem[0x0202]&0xF=5, zero_suppress=1, digit≠0 → store digit,
     *         zero_suppress=0, mem[0x0103]=mem[0x0202]=0xF5
     * Final: zero_suppress=0 → CC=3 (no-zero-suppress ended)
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x2A;  /* fill = '*' */
    g.mem[0x0101] = 0x20;  /* SST */
    g.mem[0x0102] = 0x20;  /* SST */
    g.mem[0x0103] = 0x20;  /* SST */

    g.mem[0x0200] = 0xF0;  /* digit 0 */
    g.mem[0x0201] = 0xF0;  /* digit 0 */
    g.mem[0x0202] = 0xF5;  /* digit 5 */

    alu_edt(&g, 0x0100, 4, 0x0200);

    ASSERT_EQ(g.mem[0x0100], 0x2A);  /* fill char itself */
    ASSERT_EQ(g.mem[0x0101], 0x2A);  /* first two 0-digits replaced by fill */
    ASSERT_EQ(g.mem[0x0102], 0x2A);
    ASSERT_EQ(g.mem[0x0103], 0xF5);  /* digit 5 written, zero suppress ended */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);   /* ended with no zero suppression */
}

UTEST(dec, edt_all_zero_stays_suppressed)
{
    /*
     * Pattern: [fill=0x2A, SST, SST] (plen=3)
     * Source: [0, 0] — all zero digits
     * All SST replacements use fill char.
     * zero_suppress remains 1 at end → CC=2
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x2A;
    g.mem[0x0101] = 0x20;
    g.mem[0x0102] = 0x20;

    g.mem[0x0200] = 0xF0;
    g.mem[0x0201] = 0xF0;

    alu_edt(&g, 0x0100, 3, 0x0200);

    ASSERT_EQ(g.mem[0x0101], 0x2A);
    ASSERT_EQ(g.mem[0x0102], 0x2A);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_ZERO);   /* still in zero suppression */
}

UTEST(dec, edt_tsz_ends_suppress)
{
    /*
     * Pattern: [fill=0x2A, TSZ] (plen=2)
     * TSZ always substitutes the digit AND ends zero suppression.
     * Source digit = 0 (mem[0x0200]=0xF0).
     * Even though digit is 0, TSZ copies it and clears zero_suppress.
     * mem[0x0101] = 0xF0 (digit byte), zero_suppress=0, CC=3.
     */
    struct ge g;
    ge_init(&g);

    g.mem[0x0100] = 0x2A;
    g.mem[0x0101] = 0x21;  /* TSZ */

    g.mem[0x0200] = 0xF0;

    alu_edt(&g, 0x0100, 2, 0x0200);

    ASSERT_EQ(g.mem[0x0101], 0xF0);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);   /* zero suppression ended */
}
