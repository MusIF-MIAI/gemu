/**
 * @file  tests/alu_reg.c
 * @brief Unit tests for alu_reg.c — GE-120/GE-130 register ALU helpers.
 *
 * No UTEST_MAIN here — tests/main.c provides it.
 *
 * Source evidence: cpu_6.txt §5.6.4 (LR/STR/CMR/AMR/SMR/LA),
 *                  §5.5.1.3-4 (MVQ/CMQ), §5.5.4 (SR/SL).
 *
 * UNCERTAINTY NOTES embedded in each test where OCR or manual ambiguity applies.
 */

#include "utest.h"
#include "../ge.h"
#include "../alu_cc.h"
#include "../alu_reg.h"

/* =========================================================================
 * LR / STR — round-trip
 * Expected: write 0xABCD to mem[0x100..0x101], load back into rV1.
 * =========================================================================*/
UTEST(reg, lr_str_round_trip)
{
    struct ge g;
    ge_init(&g);

    /* STR: store 0xABCD at address 0x100 (big-endian: 0xAB at 0x100, 0xCD at 0x101) */
    alu_str(&g, 0xABCD, 0x100);
    ASSERT_EQ(g.mem[0x100], (uint8_t)0xAB);
    ASSERT_EQ(g.mem[0x101], (uint8_t)0xCD);

    /* LR: load back into rV1 */
    g.rV1 = 0x0000;
    alu_lr(&g, &g.rV1, 0x100);
    ASSERT_EQ(g.rV1, (uint16_t)0xABCD);
}

UTEST(reg, lr_str_zero)
{
    struct ge g;
    ge_init(&g);

    g.mem[0x200] = 0x00;
    g.mem[0x201] = 0x00;
    alu_lr(&g, &g.rV2, 0x200);
    ASSERT_EQ(g.rV2, (uint16_t)0x0000);
}

UTEST(reg, str_does_not_set_cc)
{
    struct ge g;
    ge_init(&g);

    /* Pre-set a CC value */
    alu_set_cc(&g, ALU_CC_OVF);
    alu_str(&g, 0x1234, 0x300);
    /* CC must be unchanged */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)ALU_CC_OVF);
}

/* =========================================================================
 * CMR — Compare Memory to Register
 * §5.6.4.4 table: cc=1 (Reg<mem), cc=2 (Reg==mem), cc=3 (Reg>mem)
 * =========================================================================*/
UTEST(reg, cmr_equal)
{
    struct ge g;
    ge_init(&g);
    /* reg_val == value → cc=2 (FA04=1,FA05=0) */
    alu_cmr(&g, 0x1000, 0x1000);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
}

UTEST(reg, cmr_low)
{
    struct ge g;
    ge_init(&g);
    /* reg_val < value → cc=1 (FA04=0,FA05=1) */
    alu_cmr(&g, 0x0001, 0xFFFF);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

UTEST(reg, cmr_high)
{
    struct ge g;
    ge_init(&g);
    /* reg_val > value → cc=3 (FA04=1,FA05=1) */
    alu_cmr(&g, 0xFFFF, 0x0001);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}

UTEST(reg, cmr_zero_equal)
{
    struct ge g;
    ge_init(&g);
    /* Both zero → equal → cc=2 */
    alu_cmr(&g, 0x0000, 0x0000);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
}

/* =========================================================================
 * AMR — Add Memory to Register
 * §5.6.4.2: result=reg+value; cc encodes (overflow<<1)|(result!=0)
 * =========================================================================*/
UTEST(reg, amr_no_overflow_nonzero)
{
    struct ge g;
    ge_init(&g);
    /* 0x0001 + 0x0002 = 0x0003; no overflow, result!=0 → cc=1 */
    g.rV1 = 0x0001;
    alu_amr(&g, &g.rV1, 0x0002);
    ASSERT_EQ(g.rV1, (uint16_t)0x0003);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
    ASSERT_EQ(g.URPE, (uint8_t)0); /* no carry out */
}

UTEST(reg, amr_no_overflow_zero)
{
    struct ge g;
    ge_init(&g);
    /* 0x0000 + 0x0000 = 0x0000; no overflow, result==0 → cc=0 */
    g.rV1 = 0x0000;
    alu_amr(&g, &g.rV1, 0x0000);
    ASSERT_EQ(g.rV1, (uint16_t)0x0000);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)0);
    ASSERT_EQ(g.URPE, (uint8_t)0);
}

UTEST(reg, amr_overflow_nonzero)
{
    struct ge g;
    ge_init(&g);
    /*
     * 0xFFFF + 0x0001 = 0x10000; truncated to 0x0000 with carry.
     * FA04=1 (overflow), FA05=0 (partial result==0) → cc=2
     * UNCERTAINTY: manual says "overflow and partial result equal to zero"
     * maps to FA04=1,FA05=0 → cc=2.
     */
    g.rV1 = 0xFFFF;
    alu_amr(&g, &g.rV1, 0x0001);
    ASSERT_EQ(g.rV1, (uint16_t)0x0000);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
    ASSERT_EQ(g.URPE, (uint8_t)1); /* carry out set */
}

UTEST(reg, amr_overflow_partial_nonzero)
{
    struct ge g;
    ge_init(&g);
    /*
     * 0xFFFE + 0x0003 = 0x10001; truncated to 0x0001 with carry.
     * FA04=1 (overflow), FA05=1 (partial result!=0) → cc=3
     */
    g.rV1 = 0xFFFE;
    alu_amr(&g, &g.rV1, 0x0003);
    ASSERT_EQ(g.rV1, (uint16_t)0x0001);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
    ASSERT_EQ(g.URPE, (uint8_t)1);
}

UTEST(reg, amr_carry_into_bit15)
{
    struct ge g;
    ge_init(&g);
    /*
     * 0x7FFF + 0x0001 = 0x8000; no bit-16 carry out (URPE=0),
     * but there is a carry from bit-14 into bit-15 (URPU=1).
     * result != 0 → cc=1.
     */
    g.rV1 = 0x7FFF;
    alu_amr(&g, &g.rV1, 0x0001);
    ASSERT_EQ(g.rV1, (uint16_t)0x8000);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
    ASSERT_EQ(g.URPE, (uint8_t)0);
    ASSERT_EQ(g.URPU, (uint8_t)1);
}

/* =========================================================================
 * SMR — Subtract Memory from Register
 * §5.6.4.3: result=reg-value; cc=1 (<0), cc=2 (==0), cc=3 (>0)
 * =========================================================================*/
UTEST(reg, smr_result_zero)
{
    struct ge g;
    ge_init(&g);
    /* 0x0005 - 0x0005 = 0; result==0 → cc=2 (FA04=1,FA05=0) */
    g.rV1 = 0x0005;
    alu_smr(&g, &g.rV1, 0x0005);
    ASSERT_EQ(g.rV1, (uint16_t)0x0000);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
    ASSERT_EQ(g.URPE, (uint8_t)0); /* no borrow */
}

UTEST(reg, smr_result_positive)
{
    struct ge g;
    ge_init(&g);
    /* 0x000A - 0x0003 = 0x0007; result>0 → cc=3 (FA04=1,FA05=1) */
    g.rV1 = 0x000A;
    alu_smr(&g, &g.rV1, 0x0003);
    ASSERT_EQ(g.rV1, (uint16_t)0x0007);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
    ASSERT_EQ(g.URPE, (uint8_t)0);
}

UTEST(reg, smr_result_negative)
{
    struct ge g;
    ge_init(&g);
    /*
     * 0x0003 - 0x000A = -7 = 0xFFF9 in 2's complement.
     * result<0 (signed) → cc=1 (FA04=0,FA05=1).
     * borrow → URPE=1.
     */
    g.rV1 = 0x0003;
    alu_smr(&g, &g.rV1, 0x000A);
    ASSERT_EQ(g.rV1, (uint16_t)0xFFF9);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
    ASSERT_EQ(g.URPE, (uint8_t)1); /* borrow */
}

UTEST(reg, smr_wrap_around)
{
    struct ge g;
    ge_init(&g);
    /*
     * 0x0000 - 0x0001 = 0xFFFF; result<0 signed → cc=1; URPE=1 (borrow).
     */
    g.rV2 = 0x0000;
    alu_smr(&g, &g.rV2, 0x0001);
    ASSERT_EQ(g.rV2, (uint16_t)0xFFFF);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
    ASSERT_EQ(g.URPE, (uint8_t)1);
}

/* =========================================================================
 * LA — Load Address
 * §5.6.4.6: load effective address into register, no CC change.
 * =========================================================================*/
UTEST(reg, la_basic)
{
    struct ge g;
    ge_init(&g);
    /* LA: store address 0x1234 into rV3 */
    alu_la(&g, &g.rV3, 0x1234);
    ASSERT_EQ(g.rV3, (uint16_t)0x1234);
}

UTEST(reg, la_does_not_set_cc)
{
    struct ge g;
    ge_init(&g);
    alu_set_cc(&g, ALU_CC_OVF);
    alu_la(&g, &g.rV1, 0x5678);
    ASSERT_EQ(g.rV1,    (uint16_t)0x5678);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)ALU_CC_OVF); /* unchanged */
}

UTEST(reg, la_zero_address)
{
    struct ge g;
    ge_init(&g);
    alu_la(&g, &g.rV4, 0x0000);
    ASSERT_EQ(g.rV4, (uint16_t)0x0000);
}

/* =========================================================================
 * SR — Search Right
 * §5.5.4.1: scan left-to-right; r7 = address-after-match or address-after-field.
 *
 * UNCERTAINTY: SL/SR behavior at boundaries confirmed from §5.5.4 general text.
 * Specific result address formula is symmetric with §5.5.4.1 description.
 * =========================================================================*/
UTEST(reg, sr_found_first)
{
    struct ge g;
    ge_init(&g);
    /* field at 0x100, len=5: A B C D E; search for 'A' (0x41) */
    g.mem[0x100] = 0x41;  /* A — match at offset 0 */
    g.mem[0x101] = 0x42;
    g.mem[0x102] = 0x43;
    g.mem[0x103] = 0x44;
    g.mem[0x104] = 0x45;

    uint16_t r7 = 0;
    alu_sr(&g, &r7, 0x100, 5, 0x41);
    /* found at offset 0: r7 = 0x100 + 0 + 1 = 0x101 */
    ASSERT_EQ(r7, (uint16_t)0x101);
}

UTEST(reg, sr_found_middle)
{
    struct ge g;
    ge_init(&g);
    g.mem[0x100] = 0x01;
    g.mem[0x101] = 0x02;
    g.mem[0x102] = 0x42;  /* match at offset 2 */
    g.mem[0x103] = 0x04;
    g.mem[0x104] = 0x05;

    uint16_t r7 = 0;
    alu_sr(&g, &r7, 0x100, 5, 0x42);
    /* found at offset 2: r7 = 0x100 + 2 + 1 = 0x103 */
    ASSERT_EQ(r7, (uint16_t)0x103);
}

UTEST(reg, sr_not_found)
{
    struct ge g;
    ge_init(&g);
    g.mem[0x100] = 0x01;
    g.mem[0x101] = 0x02;
    g.mem[0x102] = 0x03;

    uint16_t r7 = 0;
    alu_sr(&g, &r7, 0x100, 3, 0xFF);
    /* not found: r7 = 0x100 + 3 = 0x103 */
    ASSERT_EQ(r7, (uint16_t)0x103);
}

/* =========================================================================
 * SL — Search Left
 * §5.5.4.2: scan right-to-left; r7 = address-before-match or field-1.
 *
 * UNCERTAINTY: Result address for SL is the symmetric complement of SR.
 * "Direction of research" = right-to-left → "following" = one byte before.
 * Confidence: MEDIUM — human review of §5.5.4.2 page image recommended.
 * =========================================================================*/
/* SL addresses the field by its RIGHTMOST byte (A1) and scans LEFT; result is
 * the address following the match in the search direction = match-1 (mirror of
 * SR's match+1). Validated against funktionalcpu step 0x22. */
UTEST(reg, sl_found_rightmost)
{
    struct ge g;
    ge_init(&g);
    /* field [0x100..0x103], A1=0x103 (rightmost): 0x01 0x02 0x03 0x42 */
    g.mem[0x100] = 0x01;
    g.mem[0x101] = 0x02;
    g.mem[0x102] = 0x03;
    g.mem[0x103] = 0x42;  /* match at the rightmost byte */

    uint16_t r7 = 0;
    alu_sl(&g, &r7, 0x103, 4, 0x42);
    /* found at 0x103 -> r7 = 0x103 - 1 = 0x102 */
    ASSERT_EQ(r7, (uint16_t)0x102);
}

UTEST(reg, sl_found_leftmost)
{
    struct ge g;
    ge_init(&g);
    g.mem[0x100] = 0x42;  /* match at the leftmost byte */
    g.mem[0x101] = 0x01;
    g.mem[0x102] = 0x02;

    uint16_t r7 = 0xFFFF;
    alu_sl(&g, &r7, 0x102, 3, 0x42); /* A1=0x102, field [0x100..0x102] */
    /* scan 0x102,0x101,0x100 -> found at 0x100 -> r7 = 0x100 - 1 = 0x00FF */
    ASSERT_EQ(r7, (uint16_t)0x00FF);
}

UTEST(reg, sl_not_found)
{
    struct ge g;
    ge_init(&g);
    g.mem[0x200] = 0x01;
    g.mem[0x201] = 0x02;
    g.mem[0x202] = 0x03;

    uint16_t r7 = 0;
    alu_sl(&g, &r7, 0x202, 3, 0xFF); /* A1=0x202, field [0x200..0x202] */
    /* not found: r7 = field - len = 0x202 - 3 = 0x1FF */
    ASSERT_EQ(r7, (uint16_t)0x1FF);
}

/* =========================================================================
 * MVQ — Move Quartets
 * §5.5.1.3: copy digit nibbles (low 4 bits) right-to-left; zones preserved.
 * CC: 0=all-zero result, 1=nonzero.
 *
 * UNCERTAINTY: "zones not processed" = preserve destination zone nibble +
 *              write source digit nibble. See alu_reg.h for alternatives.
 * =========================================================================*/
UTEST(reg, mvq_basic_move)
{
    struct ge g;
    ge_init(&g);

    /*
     * src field: rightmost byte at 0x110, len=3.
     *   0x10E: 0x31 (zone=3, digit=1)
     *   0x10F: 0x42 (zone=4, digit=2)
     *   0x110: 0x53 (zone=5, digit=3)
     * dst field: rightmost at 0x120, len=3.
     *   0x11E: 0xF9 (zone=F, digit=9)
     *   0x11F: 0xF8 (zone=F, digit=8)
     *   0x120: 0xF7 (zone=F, digit=7)
     * After MVQ: dst zones preserved (0xF), digit nibbles from src.
     *   0x11E → (0xF0 | 0x01) = 0xF1
     *   0x11F → (0xF0 | 0x02) = 0xF2
     *   0x120 → (0xF0 | 0x03) = 0xF3
     */
    g.mem[0x10E] = 0x31;
    g.mem[0x10F] = 0x42;
    g.mem[0x110] = 0x53;

    g.mem[0x11E] = 0xF9;
    g.mem[0x11F] = 0xF8;
    g.mem[0x120] = 0xF7;

    alu_mvq(&g, 0x120, 0x110, 3);

    ASSERT_EQ(g.mem[0x11E], (uint8_t)0xF1);
    ASSERT_EQ(g.mem[0x11F], (uint8_t)0xF2);
    ASSERT_EQ(g.mem[0x120], (uint8_t)0xF3);
    /* result nonzero (digits 1,2,3) → cc=1 */
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

UTEST(reg, mvq_all_zero_digits)
{
    struct ge g;
    ge_init(&g);
    /*
     * src: all digits == 0 (e.g. 0x40, 0x40).
     * dst: zone bytes preserved; digits all zero → cc=0.
     */
    g.mem[0x200] = 0x40;
    g.mem[0x201] = 0x40;

    g.mem[0x210] = 0xF1;
    g.mem[0x211] = 0xF2;

    alu_mvq(&g, 0x211, 0x201, 2);

    ASSERT_EQ(g.mem[0x210], (uint8_t)0xF0);
    ASSERT_EQ(g.mem[0x211], (uint8_t)0xF0);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)0); /* all-zero → cc=0 */
}

/* =========================================================================
 * CMQ — Compare Quartets
 * §5.5.1.4: compare digit nibbles; cc=1(<), cc=2(==), cc=3(>).
 *
 * UNCERTAINTY: same zone handling as MVQ.
 * =========================================================================*/
UTEST(reg, cmq_equal)
{
    struct ge g;
    ge_init(&g);
    /*
     * Field a: 0x300..0x301 = 0x41, 0x52 → digits: 1, 2
     * Field b: 0x310..0x311 = 0xF1, 0xC2 → digits: 1, 2
     * Equal (digit nibbles match) → cc=2 (FA04=1,FA05=0)
     */
    g.mem[0x300] = 0x41;
    g.mem[0x301] = 0x52;
    g.mem[0x310] = 0xF1;
    g.mem[0x311] = 0xC2;

    alu_cmq(&g, 0x301, 0x311, 2);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2);
}

UTEST(reg, cmq_first_less)
{
    struct ge g;
    ge_init(&g);
    /*
     * a: digits 1,2  b: digits 1,3
     * a < b → cc=1
     */
    g.mem[0x300] = 0x41;  /* digit 1 */
    g.mem[0x301] = 0x42;  /* digit 2 */
    g.mem[0x310] = 0x41;  /* digit 1 */
    g.mem[0x311] = 0x43;  /* digit 3 */

    alu_cmq(&g, 0x301, 0x311, 2);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)1);
}

UTEST(reg, cmq_first_greater)
{
    struct ge g;
    ge_init(&g);
    /*
     * a: digits 9,0  b: digits 8,9
     * First digit: a[0]=9 > b[0]=8 → a > b → cc=3
     */
    g.mem[0x300] = 0x49;  /* digit 9 */
    g.mem[0x301] = 0x40;  /* digit 0 */
    g.mem[0x310] = 0x48;  /* digit 8 */
    g.mem[0x311] = 0x49;  /* digit 9 */

    alu_cmq(&g, 0x301, 0x311, 2);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)3);
}

UTEST(reg, cmq_single_byte_equal)
{
    struct ge g;
    ge_init(&g);
    g.mem[0x400] = 0x35;  /* digit 5 */
    g.mem[0x410] = 0xA5;  /* digit 5 (different zone, same digit) */

    alu_cmq(&g, 0x400, 0x410, 1);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2); /* equal */
}

UTEST(reg, cmq_zones_ignored)
{
    struct ge g;
    ge_init(&g);
    /*
     * Zones differ but digits are identical → should compare equal.
     * UNCERTAINTY: validates "zones not processed" means only digit
     * nibbles are compared.
     */
    g.mem[0x500] = 0x07;  /* zone=0, digit=7 */
    g.mem[0x510] = 0xF7;  /* zone=F, digit=7 */

    alu_cmq(&g, 0x500, 0x510, 1);
    ASSERT_EQ(alu_get_cc(&g), (uint8_t)2); /* equal — zones ignored */
}
