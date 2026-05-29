/**
 * @file  tests/alu_dec_edge.c
 * @brief Edge-case unit tests for GE-130 packed decimal ALU helpers.
 *
 * These tests complement tests/alu_dec.c.  Every case here is either
 * a combination or boundary that is NOT already exercised there.
 *
 * Packed field encoding reminder (alu_dec.h §2.3.5):
 *   addr = rightmost byte; byte[addr] = (digit[0] << 4) | sign
 *   byte[addr-1] = (digit[1] << 4) | digit[2] ...
 *   alen=0 → 1 byte, 1 digit; alen=1 → 2 bytes, 3 digits.
 *
 *   Generated sign: 0xC = positive, 0xD = negative.
 *   Input sign:     0xB or 0xD → negative; all others → positive.
 *
 * CC for signed-result ops (AP/SP/MP/DP/CMP, from alu_cc.h):
 *   0 = overflow, 1 = result < 0, 2 = result == 0, 3 = result > 0.
 */

#include "utest.h"
#include "../ge.h"
#include "../alu_dec.h"
#include "../alu_cc.h"

/* =========================================================================
 * AP — Add Packed
 * ========================================================================= */

UTEST(alu_dec_edge, ap_neg_plus_neg)
{
    /*
     * Add two negative values: -3 + (-4) = -7.
     * alu_dec.c ap_neg_result adds +3 and -7 via AP; this adds two negatives.
     *
     * op1: -3 (alen=0, 1 byte): digit[0]=3, sign=D → mem[0x0100]=0x3D
     * op2: -4 (blen=0, 1 byte): digit[0]=4, sign=D → mem[0x0200]=0x4D
     *
     * Result: -(3+4) = -7 → digit[0]=7, sign=D → mem[0x0100]=0x7D
     * CC = ALU_CC_NEG (1)
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0100] = 0x3D;
    g.mem[0x0200] = 0x4D;

    alu_ap(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x7D);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_NEG);   /* 1 */
}

UTEST(alu_dec_edge, ap_b_sign_treated_as_negative)
{
    /*
     * Input sign 0xB is negative (alu_dec.h: "0xB (1011) and 0xD → negative").
     * +5 + (-3 with 0xB sign) = +2.
     *
     * op1: +5, sign=C → mem[0x0100]=0x5C
     * op2: -3, sign=B → mem[0x0200]=0x3B
     *
     * Result: 5 + (-3) = +2 → digit[0]=2, generated sign=C → mem[0x0100]=0x2C
     * CC = ALU_CC_POS (3)
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0100] = 0x5C;
    g.mem[0x0200] = 0x3B;

    alu_ap(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x2C);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_POS);   /* 3 */
}

/* =========================================================================
 * SP — Subtract Packed
 * ========================================================================= */

UTEST(alu_dec_edge, sp_to_zero)
{
    /*
     * Subtract equal values → zero.
     * alu_dec.c ap_zero_result uses AP (+100 + -100); here we use SP directly.
     *
     * op1: +6 (alen=0) → mem[0x0100]=0x6C
     * op2: +6 (blen=0) → mem[0x0200]=0x6C
     *
     * 6 - 6 = 0 → digit[0]=0, generated sign=C → mem[0x0100]=0x0C
     * CC = ALU_CC_ZERO (2)
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0100] = 0x6C;
    g.mem[0x0200] = 0x6C;

    alu_sp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100] & 0x0F, 0x0C);         /* generated positive sign */
    ASSERT_EQ((g.mem[0x0100] >> 4) & 0x0F, 0x0);   /* digit[0] = 0 */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_ZERO);          /* 2 */
}

UTEST(alu_dec_edge, sp_neg_minus_pos_more_negative)
{
    /*
     * Subtracting a positive from a negative deepens the negative: -3 - 4 = -7.
     * alu_dec.c sp tests only cover pos-pos cases.
     *
     * op1: -3 (alen=0) → mem[0x0100]=0x3D
     * op2: +4 (blen=0) → mem[0x0200]=0x4C
     *
     * -3 - 4 = -7 → digit[0]=7, sign=D → mem[0x0100]=0x7D
     * CC = ALU_CC_NEG (1)
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0100] = 0x3D;
    g.mem[0x0200] = 0x4C;

    alu_sp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x7D);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_NEG);   /* 1 */
}

/* =========================================================================
 * CMP — Compare Packed
 * ========================================================================= */

UTEST(alu_dec_edge, cmp_neg_vs_neg_greater)
{
    /*
     * Compare two negatives: -3 vs -9.  Algebraically -3 > -9 → CC=3 (high).
     * alu_dec.c cmp tests compare neg vs pos or single-digit positives only.
     *
     * op1: -3 (alen=0) → mem[0x0100]=0x3D
     * op2: -9 (blen=0) → mem[0x0200]=0x9D
     *
     * CC = ALU_CC_HIGH (3)
     * Both operands must be unchanged.
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0100] = 0x3D;
    g.mem[0x0200] = 0x9D;

    alu_cmp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_HIGH);   /* 3: -3 > -9 */
    /* operands must not be modified */
    ASSERT_EQ(g.mem[0x0100], 0x3D);
    ASSERT_EQ(g.mem[0x0200], 0x9D);
}

UTEST(alu_dec_edge, cmp_neg_vs_neg_less)
{
    /*
     * -9 vs -3: algebraically -9 < -3 → CC=1 (low).
     *
     * op1: -9 (alen=0) → mem[0x0100]=0x9D
     * op2: -3 (blen=0) → mem[0x0200]=0x3D
     *
     * CC = ALU_CC_LOW (1)
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0100] = 0x9D;
    g.mem[0x0200] = 0x3D;

    alu_cmp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_LOW);    /* 1: -9 < -3 */
    /* operands unchanged */
    ASSERT_EQ(g.mem[0x0100], 0x9D);
    ASSERT_EQ(g.mem[0x0200], 0x3D);
}

/* =========================================================================
 * MVP — Move Packed
 * ========================================================================= */

UTEST(alu_dec_edge, mvp_zero_source)
{
    /*
     * Move a zero value: source = +0.
     * alu_dec.c mvp tests cover positive non-zero and negative non-zero sources.
     *
     * op2: +0 (blen=0) → mem[0x0200]=0x0C
     * op1: garbage    → mem[0x0100]=0xFF
     *
     * After MVP: mem[0x0100]=0x0C (digit[0]=0, sign=C), CC=ALU_CC_ZERO (2)
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0200] = 0x0C;
    g.mem[0x0100] = 0xFF;

    alu_mvp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x0C);
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_ZERO);   /* 2 */
}

UTEST(alu_dec_edge, mvp_b_sign_normalized)
{
    /*
     * MVP preserves the source sign's VALUE but emits it in generated form:
     * alu_mvp sets result_sign = neg ? 0xD : 0xC. So a 0xB (negative) source
     * sign lands as 0xD on the result (not copied verbatim).
     *
     * op2: 0x7B (digit[0]=7, sign=0xB = negative) → mem[0x0200]=0x7B
     * After MVP: mem[0x0100]=0x7D (digit 7, generated negative sign), CC=NEG.
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0200] = 0x7B;
    g.mem[0x0100] = 0x00;

    alu_mvp(&g, 0x0100, 0, 0x0200, 0);

    ASSERT_EQ(g.mem[0x0100], 0x7D);          /* sign normalized 0xB -> 0xD */
    ASSERT_EQ(alu_get_cc(&g), ALU_CC_NEG);   /* 1 */
}

/* =========================================================================
 * DP — Divide Packed (operand-unchanged check on overflow)
 * ========================================================================= */

UTEST(alu_dec_edge, dp_div_zero_op1_unchanged)
{
    /*
     * alu_dec.h DP: "On overflow, operation is NOT performed."
     * alu_dec.c dp_overflow_divide_by_zero checks CC only; here we also
     * verify that the dividend bytes are not clobbered.
     *
     * op1 (dividend): +15 (alen=1, 2 bytes):
     *   digit[0]=5, digit[1]=1, digit[2]=0
     *   mem[0x0100]=0x5C, mem[0x00FF]=0x01
     * op2 (divisor): 0 (blen=0):
     *   mem[0x0200]=0x0C
     *
     * Overflow (divide by zero): CC=0, op1 bytes unchanged.
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0100] = 0x5C;
    g.mem[0x00FF] = 0x01;
    g.mem[0x0200] = 0x0C;

    alu_dp(&g, 0x0100, 1, 0x0200, 0);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);
    ASSERT_EQ(g.mem[0x0100], 0x5C);   /* op1 rightmost byte untouched */
    ASSERT_EQ(g.mem[0x00FF], 0x01);   /* op1 left byte untouched */
}

/* =========================================================================
 * MP — Multiply Packed (overflow: no leading zero room)
 * ========================================================================= */

UTEST(alu_dec_edge, mp_overflow_no_leading_zero_room)
{
    /*
     * alu_dec.h MP: "op1 must have ≥ blen+1 leading zero digits; otherwise
     * overflow (CC=0, operation not performed)."
     *
     * alen=1 (3 digits), blen=0 (1 digit) → need ≥ 1 leading zero digit in op1.
     * If op1 digit[2] (the leftmost digit) is non-zero, the constraint fails.
     *
     * op1: +523 (alen=1):
     *   digit[0]=3, digit[1]=2, digit[2]=5   ← non-zero leading digit
     *   mem[0x0100]=0x3C, mem[0x00FF]=0x52
     * op2: +4 (blen=0) → mem[0x0200]=0x4C
     *
     * blen=0 < alen=1 (field-length constraint satisfied), but digit[2]=5 ≠ 0
     * → overflow, CC=0, op1 not modified.
     *
     * NOTE: this case is distinct from mp_overflow_blen_ge_alen in alu_dec.c,
     * which tests the blen >= alen constraint.
     */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0100] = 0x3C;
    g.mem[0x00FF] = 0x52;
    g.mem[0x0200] = 0x4C;

    alu_mp(&g, 0x0100, 1, 0x0200, 0);

    ASSERT_EQ(alu_get_cc(&g), ALU_CC_OVF);   /* 0 */
    ASSERT_EQ(g.mem[0x0100], 0x3C);           /* op1 rightmost byte untouched */
    ASSERT_EQ(g.mem[0x00FF], 0x52);           /* op1 left byte untouched */
}
