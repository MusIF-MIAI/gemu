/**
 * @file  alu_dec.c
 * @brief GE-130 packed/signed decimal ALU helpers.
 *
 * Manual source: GE 130 P.D.S. (cpu_6.txt), specifically:
 *   §2.3.5  (packed operand format, sign codes, ~lines 3148-3169)
 *   §5.5.3.4 PK, §5.5.3.5 UPK, §5.5.3.6 EDT
 *   §5.6.1.1 AP, §5.6.1.2 SP, §5.6.1.3 MP, §5.6.1.4 DP
 *   §5.6.1.5 MVP, §5.6.1.6 CMP
 *   §5.6.2.1 PKS, §5.6.2.2 UPKS
 *
 * OPERAND ADDRESSING CONVENTION
 *   Every function receives the address of the RIGHTMOST (highest-address,
 *   least-significant) byte.  Length L means L+1 bytes = 2L+1 digits + sign.
 *   Processing goes right-to-left (rightmost byte first) exactly as the
 *   manual prescribes.
 *
 * SIGN CODES (§2.3.5)
 *   0xB or 0xD in low nibble of rightmost byte → negative
 *   anything else                               → positive
 *   Results always written as 0xC (+) or 0xD (-)
 *
 * CONDITION CODE (§5.6.1, FA04/FA05)
 *   CC=0  overflow
 *   CC=1  result < 0
 *   CC=2  result = 0
 *   CC=3  result > 0
 *
 * UNCERTAINTY NOTES
 *   MP / DP:  The manual describes high-level constraints and the overflow
 *             rules.  The digit-level multiplication/division algorithm is
 *             implemented here using standard BCD multi-precision arithmetic
 *             as described in §7 of the manual (DETAILED DESCRIPTION OF
 *             MP AND DP SEQUENCES) — that section is only in outline form
 *             in the available OCR.  The implementation follows S/360-style
 *             packed-decimal semantics which are consistent with the GE-130
 *             constraints given.  Mark any MP/DP test results as
 *             "best-effort; verify against hardware traces."
 *   UPK:      Manual says the zone of the result bytes is taken from the
 *             "pre-existing zone" in the destination field.  This
 *             implementation preserves the high nibble of destination bytes.
 *             UPKS always uses zone 0x4 (confirmed in §5.6.2.2).
 *   EDT:      Control char values SST=0x20, TSZ=0x21, RSZ=0x22 are verified
 *             in §5.5.3.6 ("code 00100000", "00100001", "00100010").
 *             CC encoding follows §5.5.3.6 table (10=zero-suppress still
 *             active, 11=zero-suppress off).
 */

#include "alu_dec.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/** Returns 1 if the sign nibble represents a negative value. */
static int dec_sign_is_neg(uint8_t sign_nibble)
{
    return (sign_nibble == 0xB || sign_nibble == 0xD);
}

/**
 * Extract a single decimal digit from a packed field.
 *
 * packed_addr  = address of rightmost byte of packed field
 * packed_bytes = total byte count (len+1)
 * digit_idx    = 0 = rightmost (most-significant of last byte hi nibble),
 *                1 = next right-to-left, ...
 *                sign is NOT a digit
 *
 * Digit layout within bytes (right to left, byte = packed_addr - byte_offset):
 *   byte 0 (rightmost): high nibble = digit[0], low nibble = sign
 *   byte 1:             high nibble = digit[1],  low nibble = digit[2]
 *   byte k>0:           high nibble = digit[2k-1], low nibble = digit[2k]
 */
static uint8_t dec_get_digit(const uint8_t *mem, uint16_t packed_addr,
                             int packed_bytes, int digit_idx)
{
    /* digit 0 is in the high nibble of packed_addr (rightmost byte) */
    int total_digits = 2 * packed_bytes - 1;
    if (digit_idx < 0 || digit_idx >= total_digits)
        return 0;

    /* byte offset from rightmost byte (rightmost = offset 0) */
    /* digit 0: byte 0 high nibble
       digit 1: byte 1 high nibble
       digit 2: byte 1 low  nibble
       digit 3: byte 2 high nibble
       ...
       digit 2k-1: byte k high nibble   (k >= 1)
       digit 2k  : byte k low  nibble   (k >= 1)
    */
    int byte_off, hi;
    if (digit_idx == 0) {
        byte_off = 0;
        hi = 1;
    } else {
        /* digit_idx >= 1 */
        byte_off = (digit_idx + 1) / 2;
        hi = ((digit_idx + 1) % 2 == 0) ? 0 : 1;  /* odd idx → hi nibble */
    }

    uint8_t b = mem[(uint16_t)(packed_addr - byte_off)];
    return hi ? (b >> 4) & 0xF : b & 0xF;
}

/** Set a single digit in a packed field (same indexing as dec_get_digit). */
static void dec_set_digit(uint8_t *mem, uint16_t packed_addr,
                          int packed_bytes, int digit_idx, uint8_t digit)
{
    int total_digits = 2 * packed_bytes - 1;
    if (digit_idx < 0 || digit_idx >= total_digits)
        return;

    int byte_off, hi;
    if (digit_idx == 0) {
        byte_off = 0;
        hi = 1;
    } else {
        byte_off = (digit_idx + 1) / 2;
        hi = ((digit_idx + 1) % 2 == 0) ? 0 : 1;
    }

    uint16_t addr = (uint16_t)(packed_addr - byte_off);
    if (hi)
        mem[addr] = (uint8_t)((mem[addr] & 0x0F) | ((digit & 0xF) << 4));
    else
        mem[addr] = (uint8_t)((mem[addr] & 0xF0) | (digit & 0xF));
}

/** Get the sign nibble of a packed field. */
static uint8_t dec_get_sign(const uint8_t *mem, uint16_t packed_addr)
{
    return mem[packed_addr] & 0x0F;
}

/** Set the sign nibble of a packed field. */
static void dec_set_sign(uint8_t *mem, uint16_t packed_addr, uint8_t sign)
{
    mem[packed_addr] = (uint8_t)((mem[packed_addr] & 0xF0) | (sign & 0x0F));
}

/**
 * Clear all digits (not sign) in a packed field to zero.
 */
static void dec_zero_digits(uint8_t *mem, uint16_t packed_addr, int packed_bytes)
{
    /* rightmost byte: clear high nibble only (preserve sign in low nibble) */
    mem[packed_addr] &= 0x0F;
    for (int i = 1; i < packed_bytes; i++)
        mem[(uint16_t)(packed_addr - i)] = 0x00;
}

/**
 * Compute the CC value from a sign and whether the result is zero.
 * CC=1 neg, CC=2 zero, CC=3 pos — matching manual FA04/FA05 table.
 */
static uint8_t dec_result_cc(int is_zero, uint8_t result_sign)
{
    if (is_zero)
        return ALU_CC_ZERO;   /* 2 — also zero for negative zero */
    return dec_sign_is_neg(result_sign) ? ALU_CC_NEG : ALU_CC_POS;
}

/**
 * BCD add two digit arrays (right-to-left, big-endian [0]=rightmost).
 * a_digits / b_digits: arrays of decimal digits (0-9), length = n_digits.
 * Returns carry out (0 or 1).
 */
static int bcd_add_digits(uint8_t *result, const uint8_t *a, const uint8_t *b,
                          int n_digits)
{
    int carry = 0;
    for (int i = 0; i < n_digits; i++) {
        int sum = a[i] + b[i] + carry;
        carry = sum / 10;
        result[i] = (uint8_t)(sum % 10);
    }
    return carry;
}

/**
 * BCD subtract b from a (right-to-left).  Assumes a >= b (unsigned).
 * Returns 0 (no borrow should remain if a >= b).
 */
static int bcd_sub_digits(uint8_t *result, const uint8_t *a, const uint8_t *b,
                          int n_digits)
{
    int borrow = 0;
    for (int i = 0; i < n_digits; i++) {
        int diff = (int)a[i] - (int)b[i] - borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result[i] = (uint8_t)diff;
    }
    return borrow;
}

/**
 * Compare two unsigned BCD digit arrays (big-endian: [n-1]=most-significant).
 * Returns >0 if a>b, 0 if equal, <0 if a<b.
 */
static int bcd_cmp_digits(const uint8_t *a, const uint8_t *b, int n_digits)
{
    for (int i = n_digits - 1; i >= 0; i--) {
        if (a[i] > b[i])
            return 1;
        if (a[i] < b[i])
            return -1;
    }
    return 0;
}

/** Returns 1 if digit array is all zeros. */
static int bcd_is_zero(const uint8_t *d, int n)
{
    for (int i = 0; i < n; i++)
        if (d[i])
            return 0;
    return 1;
}

/**
 * Read a packed field into a digit array (right-to-left, [0]=rightmost digit).
 */
static void dec_read_digits(const uint8_t *mem, uint16_t packed_addr,
                            int packed_bytes, uint8_t *digits, int n_digits)
{
    for (int i = 0; i < n_digits; i++)
        digits[i] = dec_get_digit(mem, packed_addr, packed_bytes, i);
}

/**
 * Write a digit array back into a packed field.
 * Sign is NOT written by this function.
 */
static void dec_write_digits(uint8_t *mem, uint16_t packed_addr,
                             int packed_bytes, const uint8_t *digits, int n_digits)
{
    dec_zero_digits(mem, packed_addr, packed_bytes);
    for (int i = 0; i < n_digits && i < (2 * packed_bytes - 1); i++)
        dec_set_digit(mem, packed_addr, packed_bytes, i, digits[i]);
}

/* -------------------------------------------------------------------------
 * §5.6.1.1  AP — Add Packed
 * ---------------------------------------------------------------------- */

void alu_ap(struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen)
{
    int ab = alen + 1;   /* bytes in operand 1 */
    int bb = blen + 1;   /* bytes in operand 2 */

    /* Overflow: L1 < L2 (manual §5.6.1.1). The deck (step 0x45) shows the
     * result field is STILL written (truncated to L1) and cc=0 is reported,
     * so flag it and fall through rather than returning early. */
    int len_ovf = (alen < blen);

    int an = 2 * ab - 1;  /* digits in operand 1 */
    int bn = 2 * bb - 1;  /* digits in operand 2 */

    uint8_t a_sign = dec_get_sign(ge->mem, a);
    uint8_t b_sign = dec_get_sign(ge->mem, b);
    int a_neg = dec_sign_is_neg(a_sign);
    int b_neg = dec_sign_is_neg(b_sign);

    uint8_t a_d[33] = {0};  /* max 16 bytes = 31 digits */
    uint8_t b_d[33] = {0};
    uint8_t r_d[33] = {0};

    dec_read_digits(ge->mem, a, ab, a_d, an);
    dec_read_digits(ge->mem, b, bb, b_d, bn);

    /* Extend b with leading zeros to length an */
    /* b_d is already zero-padded since array is zero-initialized */

    uint8_t result_sign;
    int overflow = 0;

    if (a_neg == b_neg) {
        /* Same sign: add magnitudes */
        int carry = bcd_add_digits(r_d, a_d, b_d, an);
        if (carry) {
            overflow = 1;
            /* Result is incomplete per manual — keep truncated digits */
        }
        result_sign = a_neg ? 0xD : 0xC;
    } else {
        /* Different signs: subtract smaller from larger */
        int cmp = bcd_cmp_digits(a_d, b_d, an);
        if (cmp >= 0) {
            bcd_sub_digits(r_d, a_d, b_d, an);
            result_sign = a_neg ? 0xD : 0xC;
        } else {
            bcd_sub_digits(r_d, b_d, a_d, an);
            result_sign = b_neg ? 0xD : 0xC;
        }
    }

    if (overflow || len_ovf) {
        /* Overflow: write the truncated low-order digits and PRESERVE the
         * destination's existing sign nibble (deck step 0x45: 0x45+0x0025 ->
         * 0x65; step 0x46: 902+136 -> 1038 truncated to 038, sign 5 kept ->
         * 0x0385). Report cc=0 (the MP-DP/AP NOTE overflow slot). */
        (void)result_sign;
        dec_write_digits(ge->mem, a, ab, r_d, an);
        alu_set_cc(ge, ALU_CC_OVF);
        return;
    }

    dec_write_digits(ge->mem, a, ab, r_d, an);
    dec_set_sign(ge->mem, a, result_sign);
    alu_set_cc(ge, dec_result_cc(bcd_is_zero(r_d, an), result_sign));
}

/* -------------------------------------------------------------------------
 * §5.6.1.2  SP — Subtract Packed
 * ---------------------------------------------------------------------- */

void alu_sp(struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen)
{
    /*
     * Invert the sign of operand 2 in a temporary copy and call ap.
     * This is equivalent to the manual description ("proceeds similarly to
     * AP except for sign processing").
     * We avoid modifying ge->mem[b], so we toggle the sign in a scratch byte.
     */
    uint8_t orig_sign = dec_get_sign(ge->mem, b);
    uint8_t flipped;

    /* Flip sign for subtraction */
    if (dec_sign_is_neg(orig_sign))
        flipped = 0xC;  /* was negative → treat as positive */
    else
        flipped = 0xD;  /* was positive → treat as negative */

    dec_set_sign(ge->mem, b, flipped);
    alu_ap(ge, a, alen, b, blen);
    dec_set_sign(ge->mem, b, orig_sign);  /* restore operand 2 */
}

/* -------------------------------------------------------------------------
 * §5.6.1.3  MP — Multiply Packed
 * ---------------------------------------------------------------------- */

/*
 * Algorithm: standard BCD long-multiplication.
 * Manual constraints (§5.6.1.3):
 *   - blen <= 8 (second operand ≤ 8 bytes = 15 digits + sign)
 *   - blen < alen (multiplicand length < multiplier length)
 *   - Multiplier (op1) must have at least blen+1 leading zero digits
 *   - Overflow → operation NOT performed, CC=0
 */
void alu_mp(struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen)
{
    int ab = alen + 1;
    int bb = blen + 1;

    /* Overflow conditions: the multiplier field must be at most 8 bytes
     * (bb counts bytes; the deck's step-0x25 MP 10,9 has a 9-byte multiplier
     * and must overflow), and must be shorter than the result/multiplicand
     * field. (blen is the 0-indexed length code, so the byte test is on bb.) */
    if (bb > 8 || blen >= alen)
        goto overflow;

    int an = 2 * ab - 1;
    int bn = 2 * bb - 1;

    uint8_t a_d[33] = {0};
    uint8_t b_d[33] = {0};

    dec_read_digits(ge->mem, a, ab, a_d, an);
    dec_read_digits(ge->mem, b, bb, b_d, bn);

    uint8_t a_sign = dec_get_sign(ge->mem, a);
    uint8_t b_sign = dec_get_sign(ge->mem, b);

    /*
     * Check that the top (an - bn) digits of op1 are zero (the "multiplier
     * leading zeros" rule: the result replaces the entire op1 field).
     * Actually the manual says the multiplier must contain at least L2+1
     * characters equal to zero to the left of the significant part.
     * L2+1 = bb bytes = 2*bb-1 digits... but that would mean all digits
     * in the top half must be zero.  Interpret: top bn digits of op1 must
     * be zero.
     *
     * UNCERTAINTY: the exact leading-zero count required is not unambiguously
     * stated in the available OCR.  Using: the top bb*2 digits of op1 (i.e.
     * indices an-1 down to bn) must all be zero.
     */
    for (int i = bn; i < an; i++) {
        if (a_d[i] != 0)
            goto overflow;
    }

    /* Multiply: r[i+j] += a_d[i] * b_d[j] (with BCD correction) */
    uint8_t r_d[33] = {0};
    for (int i = 0; i < bn; i++) {
        int carry = 0;
        for (int j = 0; j < an; j++) {
            int prod = r_d[i + j] + a_d[j] * b_d[i] + carry;
            carry = prod / 10;
            r_d[i + j] = (uint8_t)(prod % 10);
        }
        /* carry beyond an+bn digits → overflow */
        if (i + an < 33)
            r_d[i + an] += (uint8_t)carry;
    }

    /* Check result fits in an digits */
    for (int i = an; i < 33; i++) {
        if (r_d[i])
            goto overflow;
    }

    /* Sign: algebraic product */
    int a_neg = dec_sign_is_neg(a_sign);
    int b_neg = dec_sign_is_neg(b_sign);
    uint8_t result_sign = (a_neg != b_neg) ? 0xD : 0xC;

    dec_write_digits(ge->mem, a, ab, r_d, an);
    dec_set_sign(ge->mem, a, result_sign);
    alu_set_cc(ge, dec_result_cc(bcd_is_zero(r_d, an), result_sign));
    return;

overflow:
    /* On overflow MP clears the second-operand (V2 = b) field and reports
     * cc=0 (the MP-DP NOTE table's overflow slot). funktionalcpu step 0x27
     * checks the cleared b field (CMC 5,0x00E8,0x05A5) after an overflowing
     * MP 6,5; steps 0x25/0x26 only check the cc. */
    for (int k = 0; k < bb; k++)
        ge_mem_store8(ge, (uint16_t)(b - k), 0x00);
    alu_set_cc(ge, ALU_CC_OVF);
}

/* -------------------------------------------------------------------------
 * §5.6.1.4  DP — Divide Packed
 * ---------------------------------------------------------------------- */

/*
 * Result layout: quotient in leftmost (alen-blen) bytes of op1,
 *                remainder in rightmost (blen+1) bytes.
 * Manual constraints:
 *   a) alen > blen  (L1 > L2)
 *   b) blen <= 7    (L2 <= 7)
 *   c) quotient fits in the (alen-blen) character slot
 *   d) divisor != 0
 * On overflow: operation NOT performed.
 *
 * UNCERTAINTY: the exact digit-slot arithmetic for quotient/remainder
 * placement is derived from the field-size rules in §5.6.1.4 and §7.2.
 * Verified: "quotient placed leftmost (L1-L2 positions), remainder
 * rightmost (L2+1 positions)".  Implemented with big-integer BCD division.
 */
void alu_dp(struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen)
{
    int ab = alen + 1;
    int bb = blen + 1;

    /* Overflow checks a, b */
    if (alen <= blen || blen > 7) {
        alu_set_cc(ge, ALU_CC_OVF);
        return;
    }

    int an = 2 * ab - 1;
    int bn = 2 * bb - 1;

    uint8_t a_d[33] = {0};
    uint8_t b_d[33] = {0};

    dec_read_digits(ge->mem, a, ab, a_d, an);
    dec_read_digits(ge->mem, b, bb, b_d, bn);

    uint8_t a_sign = dec_get_sign(ge->mem, a);
    uint8_t b_sign = dec_get_sign(ge->mem, b);

    /* Overflow check d: divisor != 0 */
    if (bcd_is_zero(b_d, bn)) {
        alu_set_cc(ge, ALU_CC_OVF);
        return;
    }

    /*
     * Perform BCD long division: dividend a_d[an-1..0] / divisor b_d[bn-1..0]
     * Both arrays have [0]=rightmost (least significant) digit.
     * We work most-significant-first, so reverse indices.
     *
     * Quotient slot: qn = 2*(alen-blen)-1 digits  (leftmost bytes count = alen-blen)
     * Remainder slot: rn = bn digits
     */
    int q_bytes = alen - blen;     /* bytes for quotient field */
    int qn = 2 * q_bytes - 1;     /* max quotient digits */

    /*
     * Division by value.  The operand fields are decimal; convert to integers,
     * divide, and convert the quotient/remainder back to BCD (least-significant
     * digit first) for placement.  unsigned __int128 covers the full field
     * width (up to ~31 digits) without overflow.
     */
    unsigned __int128 dividend = 0, divisor = 0;
    for (int i = an - 1; i >= 0; i--) dividend = dividend * 10 + a_d[i];
    for (int i = bn - 1; i >= 0; i--) divisor  = divisor  * 10 + b_d[i];
    /* divisor != 0 already verified above */

    unsigned __int128 quotient  = dividend / divisor;
    unsigned __int128 remainder = dividend % divisor;

    /* Overflow check c: quotient must fit in qn digits */
    unsigned __int128 qcap = 1;
    for (int i = 0; i < qn; i++) qcap *= 10;
    if (quotient >= qcap) {
        alu_set_cc(ge, ALU_CC_OVF);
        return;
    }

    /* Convert to least-significant-digit-first BCD arrays */
    uint8_t q_lsf[33] = {0};
    uint8_t r_lsf[33] = {0};
    for (int i = 0; i < qn; i++) { q_lsf[i] = (uint8_t)(quotient  % 10); quotient  /= 10; }
    for (int i = 0; i < bn; i++) { r_lsf[i] = (uint8_t)(remainder % 10); remainder /= 10; }

    /* Signs */
    int a_neg = dec_sign_is_neg(a_sign);
    int b_neg = dec_sign_is_neg(b_sign);
    uint8_t q_sign = (a_neg != b_neg) ? 0xD : 0xC;
    uint8_t r_sign = a_neg ? 0xD : 0xC;  /* remainder sign = dividend sign */

    /*
     * Write quotient into leftmost q_bytes of op1:
     *   Quotient field address = a - bb  (bb bytes from rightmost = offset to quotient's rightmost)
     *   Quotient rightmost byte address = (a - bb)
     *   (rightmost byte of full field is a; rightmost byte of quotient sub-field
     *    is a - bb since remainder occupies bb bytes at the right)
     */
    uint16_t q_addr = (uint16_t)(a - bb);
    dec_zero_digits(ge->mem, q_addr, q_bytes);
    dec_write_digits(ge->mem, q_addr, q_bytes, q_lsf, qn);
    dec_set_sign(ge->mem, q_addr, q_sign);

    /* Write remainder into rightmost bb bytes of op1 */
    uint16_t r_addr = a;
    dec_zero_digits(ge->mem, r_addr, bb);
    dec_write_digits(ge->mem, r_addr, bb, r_lsf, bn);
    dec_set_sign(ge->mem, r_addr, r_sign);

    /* CC reflects quotient (most significant result) */
    alu_set_cc(ge, dec_result_cc(bcd_is_zero(q_lsf, qn), q_sign));
}

/* -------------------------------------------------------------------------
 * §5.6.1.6  CMP — Compare Packed
 * ---------------------------------------------------------------------- */

void alu_cmp(struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen)
{
    /* Overflow if L1 < L2 */
    if (alen < blen) {
        alu_set_cc(ge, ALU_CC_OVF);
        return;
    }

    int ab = alen + 1;
    int bb = blen + 1;
    int an = 2 * ab - 1;
    int bn = 2 * bb - 1;

    uint8_t a_d[33] = {0};
    uint8_t b_d[33] = {0};

    dec_read_digits(ge->mem, a, ab, a_d, an);
    dec_read_digits(ge->mem, b, bb, b_d, bn);

    uint8_t a_sign = dec_get_sign(ge->mem, a);
    uint8_t b_sign = dec_get_sign(ge->mem, b);
    int a_neg = dec_sign_is_neg(a_sign);
    int b_neg = dec_sign_is_neg(b_sign);

    /* Compare algebraically; positive zero == negative zero */
    int a_zero = bcd_is_zero(a_d, an);
    int b_zero = bcd_is_zero(b_d, bn);

    if (a_zero && b_zero) {
        alu_set_cc(ge, ALU_CC_EQUAL);
        return;
    }

    if (a_neg != b_neg) {
        /* Different signs: negative < positive */
        alu_set_cc(ge, a_neg ? ALU_CC_LOW : ALU_CC_HIGH);
        return;
    }

    /* Same sign: compare magnitudes */
    int mag_cmp = bcd_cmp_digits(a_d, b_d, an);
    if (mag_cmp == 0) {
        alu_set_cc(ge, ALU_CC_EQUAL);
    } else if (a_neg) {
        /* Both negative: larger magnitude = smaller value */
        alu_set_cc(ge, mag_cmp > 0 ? ALU_CC_LOW : ALU_CC_HIGH);
    } else {
        alu_set_cc(ge, mag_cmp > 0 ? ALU_CC_HIGH : ALU_CC_LOW);
    }
}

/* -------------------------------------------------------------------------
 * §5.6.1.5  MVP — Move Packed
 * ---------------------------------------------------------------------- */

void alu_mvp(struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen)
{
    int ab = alen + 1;
    int bb = blen + 1;
    int an = 2 * ab - 1;
    int bn = 2 * bb - 1;

    uint8_t b_d[33] = {0};
    dec_read_digits(ge->mem, b, bb, b_d, bn);
    uint8_t b_sign = dec_get_sign(ge->mem, b);

    /* Overflow if L1 < L2 — but operation IS performed with incomplete result */
    int overflow = (alen < blen);

    dec_zero_digits(ge->mem, a, ab);

    /* Copy as many digits as fit in op1; if bn > an the excess (left) are dropped */
    int copy_n = (bn < an) ? bn : an;
    for (int i = 0; i < copy_n; i++)
        dec_set_digit(ge->mem, a, ab, i, b_d[i]);

    /* MVP moves the source sign nibble VERBATIM (it is a move, not an
     * arithmetic op) — deck step 0x4D moves source sign 0xA and expects 0xA,
     * not a normalized 0xC. */
    uint8_t result_sign = b_sign;
    dec_set_sign(ge->mem, a, result_sign);

    if (overflow) {
        alu_set_cc(ge, ALU_CC_OVF);
        return;
    }

    uint8_t r_d[33] = {0};
    dec_read_digits(ge->mem, a, ab, r_d, an);
    alu_set_cc(ge, dec_result_cc(bcd_is_zero(r_d, an), result_sign));
}

/* -------------------------------------------------------------------------
 * §5.5.3.4  PK — Pack (no sign processing)
 * ---------------------------------------------------------------------- */

/*
 * Zoned operand: one digit per byte; digit = low nibble.
 * Packed operand: two digits per byte; rightmost byte high nibble = last digit,
 *                 low nibble = sign (NOT processed by PK).
 * The manual says: "loads in packed form the 2L+2 less significant halves
 * (low nibbles) of the characters of the second operand."
 * "Operation proceeds from left to right."
 *
 * Source length: 2*dlen+2 bytes of zoned source (2L+2 digits; we use all of them).
 * Destination: dlen+1 packed bytes.
 * Addressing: dst = rightmost byte of destination packed field.
 *             src = rightmost byte of zoned source field (manual: "address of
 *             rightmost position").
 *
 * NOTE: the manual says L+1 for destination and 2L+2 source characters
 * where L = dlen.  So src occupies slen+1 zoned bytes; we pack the low nibbles
 * of those into the destination, taking 2*(dlen+1) digits = dlen+1 packed bytes.
 * If source is shorter (slen < 2*dlen+1) we left-pad with zeros.
 * If source is longer (slen > 2*dlen+1) we use only the rightmost 2*(dlen+1) chars.
 *
 * Sign nibble: left as-is in destination (not altered by PK per §5.5.3.4).
 *
 * UNCERTAINTY: PK/UPK addressing — manual is slightly ambiguous on whether
 * src address is leftmost or rightmost for zoned format.  §5.6.2 PKS/UPKS
 * confirms "address of rightmost position"; we use the same convention for
 * PK/UPK.
 */
void alu_pk(struct ge *ge, uint16_t dst, uint8_t dlen, uint16_t src, uint8_t slen)
{
    int db = dlen + 1;   /* destination packed bytes (= L+1) */
    (void)slen;          /* source is 2L+2 zoned chars, derived from dlen */

    /*
     * Re-derived from the PK microcode (flowchart "PK Dalla Fase Alfa", states
     * 64|65 -> 60-63 -> 40-43, dwg timing charts).  Both pointers INCREMENT
     * (V2+1->V2 source, V1+1->V1 dest), so PK runs from the given (leftmost,
     * most-significant) address UPWARD, not from a rightmost byte.
     *
     * The accumulate state (60-63) does MEM->RO, then routes the source digit
     * (RO low nibble) to NI4 (high nibble) on one SA00 phase and NI3 (low
     * nibble) on the alternate phase; the write state (40-43) does RO->MEM.
     * So two consecutive zoned digits fill one packed byte: 1st (more
     * significant) -> high nibble, 2nd -> low nibble.  PK does NOT process a
     * sign, so every byte holds two full digits (2L+2 digits total).
     */
    int total_digits = 2 * db;   /* 2 digits/byte, no sign nibble */

    for (int d = 0; d < total_digits; d++) {
        uint8_t digit = ge->mem[(uint16_t)(src + d)] & 0x0F;  /* read upward */
        uint16_t daddr = (uint16_t)(dst + d / 2);             /* write upward */
        uint8_t cur = ge->mem[daddr];
        if (d & 1)
            cur = (uint8_t)((cur & 0xF0) | digit);              /* 2nd digit -> low  */
        else
            cur = (uint8_t)((cur & 0x0F) | (uint8_t)(digit << 4)); /* 1st digit -> high */
        ge_mem_store8(ge, daddr, cur);
    }
}

/* -------------------------------------------------------------------------
 * §5.5.3.5  UPK — Unpack (no sign processing, preserve existing zones)
 * ---------------------------------------------------------------------- */

/*
 * UPK is the inverse of PK and uses the same "process upward from the given
 * (leftmost) address" convention (UPK microcode flowchart, states 64|65 ->
 * 60-63, pointers increment).  Each packed source byte holds two digits and is
 * expanded into two zoned destination bytes: high nibble -> first (more
 * significant) zoned byte, low nibble -> second.  No sign is processed, so a
 * source of slen+1 packed bytes yields 2*(slen+1) zoned digits.  The
 * destination's existing zone (high nibble) is preserved; only the low nibble
 * (digit) is written.  Validated against funktionalcpu steps 0x1D/0x1E.
 */
void alu_upk(struct ge *ge, uint16_t dst, uint8_t dlen, uint16_t src, uint8_t slen)
{
    (void)dlen;                 /* dest length is 2*(slen+1), derived from the source */
    int sb = slen + 1;          /* packed source bytes */
    int total = 2 * sb;         /* zoned destination bytes (2 digits per source byte) */

    for (int d = 0; d < total; d++) {
        uint8_t sbyte = ge->mem[(uint16_t)(src + d / 2)];        /* read upward */
        uint8_t digit = (d & 1) ? (uint8_t)(sbyte & 0x0F)        /* 2nd: low nibble  */
                                : (uint8_t)((sbyte >> 4) & 0x0F);/* 1st: high nibble */
        uint16_t daddr = (uint16_t)(dst + d);                    /* write upward */
        /* Preserve high nibble (zone) of destination, put digit in low nibble */
        ge_mem_store8(ge, daddr, (uint8_t)((ge->mem[daddr] & 0xF0) | digit));
    }
}

/* -------------------------------------------------------------------------
 * §5.6.2.1  PKS — Pack with Sign
 * ---------------------------------------------------------------------- */

/*
 * Like PK but:
 *   1. Source right nibble zone is examined: if 0xA → negative sign (1101 = D)
 *      any other → positive sign (1100 = C).
 *   2. Generated sign is written into packed result's sign nibble.
 *   3. CC is set.
 *
 * "Packing interprets the combination 1010 in the zone of the first character
 * to the right of the first operand as a minus sign and any other combination
 * as a plus sign." (§5.6.2.1)
 * "First character to the right of the first operand" means the rightmost
 * source zoned byte.
 */
void alu_pks(struct ge *ge, uint16_t dst, uint8_t dlen, uint16_t src, uint8_t slen)
{
    int db = dlen + 1;
    int sb = slen + 1;
    int total_digits = 2 * db - 1;

    /* Determine sign from zone of rightmost source byte */
    uint8_t src_zone = (ge->mem[src] >> 4) & 0x0F;
    uint8_t result_sign = (src_zone == 0xA) ? 0xD : 0xC;

    /* Pack digits (same as PK) */
    for (int d = 0; d < total_digits; d++) {
        uint8_t digit;
        if (d < sb) {
            digit = ge->mem[(uint16_t)(src - d)] & 0x0F;
        } else {
            digit = 0;
        }
        dec_set_digit(ge->mem, dst, db, d, digit);
    }

    dec_set_sign(ge->mem, dst, result_sign);

    /* CC: check if all digits are zero */
    uint8_t r_d[33] = {0};
    dec_read_digits(ge->mem, dst, db, r_d, total_digits);
    int is_zero = bcd_is_zero(r_d, total_digits);

    if (is_zero)
        alu_set_cc(ge, ALU_CC_ZERO);   /* = 2 */
    else
        alu_set_cc(ge, dec_sign_is_neg(result_sign) ? ALU_CC_NEG : ALU_CC_POS);
}

/* -------------------------------------------------------------------------
 * §5.6.2.2  UPKS — Unpack with Sign
 * ---------------------------------------------------------------------- */

/*
 * "The unpacking always generates the 0100 zone on all the digits."
 * Zone = 0x4 on every result byte.
 * CC set to reflect sign and value of packed source.
 */
void alu_upks(struct ge *ge, uint16_t dst, uint8_t dlen, uint16_t src, uint8_t slen)
{
    int sb = slen + 1;
    int sn = 2 * sb - 1;
    int db = dlen + 1;

    for (int i = 0; i < db; i++) {
        uint8_t digit;
        if (i < sn) {
            digit = dec_get_digit(ge->mem, src, sb, i) & 0x0F;
        } else {
            digit = 0;
        }
        uint16_t daddr = (uint16_t)(dst - i);
        /* Zone is always 0x4 */
        ge_mem_store8(ge, daddr, (uint8_t)(0x40 | digit));
    }

    /* CC reflects sign and value of packed source operand */
    uint8_t src_sign = dec_get_sign(ge->mem, src);
    uint8_t s_d[33] = {0};
    dec_read_digits(ge->mem, src, sb, s_d, sn);
    int is_zero = bcd_is_zero(s_d, sn);

    if (is_zero)
        alu_set_cc(ge, ALU_CC_ZERO);
    else
        alu_set_cc(ge, dec_sign_is_neg(src_sign) ? ALU_CC_NEG : ALU_CC_POS);
}

/* -------------------------------------------------------------------------
 * §5.5.3.6  EDT — Edit
 * ---------------------------------------------------------------------- */

/*
 * Pattern control codes (verified from §5.5.3.6 bit patterns):
 *   SST = 0x20 (00100000) — digit substitute, continues zero suppression
 *   TSZ = 0x21 (00100001) — digit substitute, terminates zero suppression
 *   RSZ = 0x22 (00100010) — reset (start) zero suppression condition,
 *                            replaces with fill char, pointer doesn't advance
 *
 * First byte of pattern = fill character.
 * Source: unpacked decimal (one digit per byte, low nibble), addressed via
 *         src = RIGHTMOST byte; we advance LEFT (decrement address) to walk
 *         through digits.
 *
 * "Operation proceeds from left to right" through the pattern.
 * We maintain a pointer into the source field, advancing it when a digit
 * is consumed.
 *
 * CC (§5.5.3.7):
 *   FA04=1 FA05=0 (CC=2): operation ended in zero suppression condition
 *   FA04=1 FA05=1 (CC=3): operation ended in no-zero-suppression condition
 *   (FA04=0 cases listed as "not possible")
 *
 * UNCERTAINTY: The source field is described as "normally unpacked decimal"
 * (§5.5.3.6).  The source addressing — whether src points to the rightmost
 * or leftmost byte — is inferred from context (digits are consumed left to
 * right as pattern is scanned left to right, and the source pointer advances
 * forward through memory).  We treat src as the address of the FIRST (leftmost)
 * digit of the source, incrementing as we consume digits.
 * ALSO: "plen" is the pattern length in bytes (auxiliary character); the
 * source field extent is implicitly defined by digit-select chars in pattern.
 */
void alu_edt(struct ge *ge, uint16_t pattern, uint8_t plen, uint16_t src)
{
    if (plen == 0)
        return;

    uint8_t fill = ge->mem[pattern];  /* first pattern byte = fill char */
    int zero_suppress = 1;            /* starts in zero suppression condition */
    uint16_t src_ptr = src;           /* current source byte pointer */

    for (int i = 0; i < plen; i++) {
        uint16_t pat_addr = (uint16_t)(pattern + i);
        uint8_t pc = ge->mem[pat_addr];

        if (pc == 0x20) {
            /* SST: substitute digit */
            uint8_t digit = ge->mem[src_ptr] & 0x0F;
            if (zero_suppress) {
                if (digit == 0) {
                    ge_mem_store8(ge, pat_addr, fill);
                } else {
                    ge_mem_store8(ge, pat_addr, ge->mem[src_ptr]); /* keep digit byte */
                    zero_suppress = 0;
                }
            } else {
                ge_mem_store8(ge, pat_addr, ge->mem[src_ptr]);
            }
            src_ptr++;
        } else if (pc == 0x21) {
            /* TSZ: digit substitute + terminate zero suppression */
            ge_mem_store8(ge, pat_addr, ge->mem[src_ptr]);
            zero_suppress = 0;
            src_ptr++;
        } else if (pc == 0x22) {
            /* RSZ: reset zero suppression (restore fill), pointer stays */
            ge_mem_store8(ge, pat_addr, fill);
            zero_suppress = 1;
            /* source pointer does NOT advance */
        } else {
            /* Insertion character */
            if (zero_suppress) {
                ge_mem_store8(ge, pat_addr, fill);
                /* pointer does NOT advance */
            } else {
                /* leave insertion char in place */
                /* pointer does NOT advance */
            }
        }
    }

    /* CC: 2 if still in zero suppression, 3 if zero suppression lifted */
    alu_set_cc(ge, zero_suppress ? ALU_CC_ZERO : ALU_CC_POS);
}
