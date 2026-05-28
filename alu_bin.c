/**
 * @file  alu_bin.c
 * @brief Binary and decimal (unpacked) arithmetic helpers for the GE-120/130.
 *
 * Manual references:
 *   cpu_6.txt §5.5.1  – Decimal arithmetics (AD, SD)
 *   cpu_6.txt §5.5.2  – Binary arithmetics (AB, SB)
 *   cpu_6.txt §5.10.2 – Qualitative results summary table
 *
 * Operand model (§5.5.1 / §5.5.2):
 *   addr points to the RIGHTMOST (least-significant) byte.
 *   The field occupies memory[addr - len + 1 .. addr].
 *   All operations are performed right-to-left (LSD first).
 *
 * Length rules (verified from manual text):
 *   If L2 > L1: truncate operand 2 to L1 (treat as if L2 = L1).
 *   If L2 < L1: zero-extend operand 2 on the left.
 *
 * CC encoding (§5.10.2):
 *   cc = (FA04 << 1) | FA05
 *   Stored by alu_set_cc() into ffFI bits 4 (hi) and 5 (lo).
 */

#include "alu_bin.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/**
 * Read one byte from a multi-byte big-endian field at position @p pos
 * (0 = least-significant byte = rightmost, len-1 = most-significant).
 *
 * The field is stored big-endian in memory:
 *   MSB at address (base_addr - len + 1), LSB at address base_addr.
 * So byte at position pos from LSB is at address (base_addr - pos).
 */
static inline uint8_t field_byte(const struct ge *ge,
                                 uint16_t base_addr, uint8_t len,
                                 uint8_t pos)
{
    if (pos >= len)
        return 0; /* zero-extension */
    return ge->mem[(uint16_t)(base_addr - pos)];
}

/**
 * Write one byte into a multi-byte big-endian field.
 */
static inline void field_byte_set(struct ge *ge,
                                  uint16_t base_addr, uint8_t len,
                                  uint8_t pos, uint8_t val)
{
    if (pos < len)
        ge->mem[(uint16_t)(base_addr - pos)] = val;
}

/* ------------------------------------------------------------------ */
/* AB – Add Binary (opcode 0xFE)                                       */
/*                                                                     */
/* CC (§5.5.2.1, §5.10.2):                                            */
/*   0 (FA04=0,FA05=0): sum is zero                                    */
/*   1 (FA04=0,FA05=1): sum is non-zero (no overflow)                  */
/*   2 (FA04=1,FA05=0): overflow, partial result is zero               */
/*   3 (FA04=1,FA05=1): overflow, partial result is non-zero           */
/* ------------------------------------------------------------------ */
void alu_ab(struct ge *ge,
            uint16_t a_addr, uint8_t a_len,
            uint16_t b_addr, uint8_t b_len)
{
    /* If L2 > L1 truncate to L1 (manual: "executed as if L2 = L1") */
    if (b_len > a_len)
        b_len = a_len;

    uint8_t carry = 0;
    uint8_t result_nonzero = 0;
    uint8_t overflow = 0;

    for (uint8_t pos = 0; pos < a_len; pos++) {
        uint16_t sum = (uint16_t)field_byte(ge, a_addr, a_len, pos)
                     + (uint16_t)field_byte(ge, b_addr, b_len, pos)
                     + carry;
        carry = (uint8_t)(sum >> 8);
        uint8_t byte = (uint8_t)(sum & 0xFF);
        field_byte_set(ge, a_addr, a_len, pos, byte);
        if (byte != 0)
            result_nonzero = 1;
    }

    /*
     * A carry out of the most-significant byte means a carry was lost
     * beyond the receiving field — this is the overflow condition.
     * Manual (§5.5.2.1): "the loss of a possible carry-over beyond the
     * length of the receiving field (overflow)".
     */
    if (carry)
        overflow = 1;

    uint8_t cc;
    if (overflow)
        cc = result_nonzero ? 3u : 2u;  /* OV+nonzero | OV+zero */
    else
        cc = result_nonzero ? 1u : 0u;  /* nonzero | zero       */

    alu_set_cc(ge, cc);
}

/* ------------------------------------------------------------------ */
/* SB – Subtract Binary (opcode 0xFF)                                  */
/*                                                                     */
/* CC (§5.5.2.2):                                                      */
/*   0: not possible (never set by this implementation)                */
/*   1 (FA04=0,FA05=1): result < 0 (stored complemented to 2^8*L1)   */
/*   2 (FA04=1,FA05=0): result == 0                                    */
/*   3 (FA04=1,FA05=1): result > 0                                     */
/* ------------------------------------------------------------------ */
void alu_sb(struct ge *ge,
            uint16_t a_addr, uint8_t a_len,
            uint16_t b_addr, uint8_t b_len)
{
    /* If L2 > L1 truncate to L1 */
    if (b_len > a_len)
        b_len = a_len;

    /*
     * Perform A = A - B using borrow propagation right-to-left.
     * borrow = 1 means we borrowed from the next higher position.
     */
    uint8_t borrow = 0;
    uint8_t result_nonzero = 0;

    for (uint8_t pos = 0; pos < a_len; pos++) {
        int16_t diff = (int16_t)field_byte(ge, a_addr, a_len, pos)
                     - (int16_t)field_byte(ge, b_addr, b_len, pos)
                     - borrow;
        if (diff < 0) {
            diff += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }
        uint8_t byte = (uint8_t)(diff & 0xFF);
        field_byte_set(ge, a_addr, a_len, pos, byte);
        if (byte != 0)
            result_nonzero = 1;
    }

    /*
     * If borrow != 0 after the MSB the mathematical result is negative.
     * The manual (§5.5.2.2) states: "If the result is negative it is stored
     * in complemented form (complemented to 2^8*(L1+1))".
     * The two's complement is already naturally produced by the borrow
     * propagation (borrowing from an implicit position above yields the
     * correct two's complement representation).
     *
     * Note: the manual writes "2^8(L1+1)" where L1 in the encoding is
     * len-1 (since "L1+1 positions"), so this is 2^(8*a_len) — exactly
     * what our byte-level subtraction with wrap produces.
     */
    uint8_t negative = borrow; /* borrow out = mathematical underflow */

    uint8_t cc;
    if (negative) {
        /* result < 0, stored in complemented form; FA04=0, FA05=1 -> cc=1 */
        cc = 1u;
    } else if (!result_nonzero) {
        /* result == 0; FA04=1, FA05=0 -> cc=2 */
        cc = 2u;
    } else {
        /* result > 0; FA04=1, FA05=1 -> cc=3 */
        cc = 3u;
    }
    alu_set_cc(ge, cc);
}

/* ------------------------------------------------------------------ */
/* Internal: decimal digit extraction                                  */
/*                                                                     */
/* In unpacked (zoned) format each byte holds one decimal digit in its */
/* low nibble (0x0..0x9); the high nibble is the zone and is ignored   */
/* during arithmetic (§5.5.1: "The zones of the characters are not     */
/* processed").                                                         */
/* ------------------------------------------------------------------ */
static inline uint8_t dec_digit(const struct ge *ge,
                                uint16_t base_addr, uint8_t len,
                                uint8_t pos)
{
    return field_byte(ge, base_addr, len, pos) & 0x0F;
}

/* ------------------------------------------------------------------ */
/* AD – Add Decimal (opcode 0xFA)                                      */
/*                                                                     */
/* CC (§5.10.2, inferred – confidence medium, see header):             */
/*   0: result is zero                                                  */
/*   1: result is non-zero                                             */
/*   2/3: not possible (carry beyond field is silently dropped)        */
/* ------------------------------------------------------------------ */
void alu_ad(struct ge *ge,
            uint16_t a_addr, uint8_t a_len,
            uint16_t b_addr, uint8_t b_len)
{
    /* If L2 > L1 truncate to L1 */
    if (b_len > a_len)
        b_len = a_len;

    uint8_t carry = 0;
    uint8_t result_nonzero = 0;

    for (uint8_t pos = 0; pos < a_len; pos++) {
        uint8_t sum = dec_digit(ge, a_addr, a_len, pos)
                    + dec_digit(ge, b_addr, b_len, pos)
                    + carry;
        carry = sum / 10;
        uint8_t digit = sum % 10;
        /*
         * Write back just the digit in the low nibble; zone (high nibble) is
         * set to 0x00 because the manual states zones are not processed.
         * The CPU microsequencer is responsible for zone restoration if needed.
         */
        field_byte_set(ge, a_addr, a_len, pos, digit);
        if (digit != 0)
            result_nonzero = 1;
    }
    /* carry beyond field boundary is silently dropped (§5.5.1) */

    alu_set_cc(ge, result_nonzero ? 1u : 0u);
}

/* ------------------------------------------------------------------ */
/* SD – Subtract Decimal (opcode 0xFB)                                 */
/*                                                                     */
/* CC (§5.10.2, inferred – confidence medium, see header):             */
/*   0: result is zero                                                  */
/*   1: result is non-zero (includes underflow case)                   */
/*   2/3: not possible                                                  */
/*                                                                     */
/* Underflow (borrow beyond the MSP): the manual treats AD/SD operands */
/* as positive integers and does not explicitly describe the underflow  */
/* case for SD in the available OCR.  We wrap modulo 10^a_len and set  */
/* cc=1.  Human re-inspection of the page images is recommended.       */
/* ------------------------------------------------------------------ */
void alu_sd(struct ge *ge,
            uint16_t a_addr, uint8_t a_len,
            uint16_t b_addr, uint8_t b_len)
{
    /* If L2 > L1 truncate to L1 */
    if (b_len > a_len)
        b_len = a_len;

    uint8_t borrow = 0;
    uint8_t result_nonzero = 0;

    for (uint8_t pos = 0; pos < a_len; pos++) {
        int8_t diff = (int8_t)(dec_digit(ge, a_addr, a_len, pos))
                    - (int8_t)(dec_digit(ge, b_addr, b_len, pos))
                    - (int8_t)borrow;
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        uint8_t digit = (uint8_t)diff;
        field_byte_set(ge, a_addr, a_len, pos, digit);
        if (digit != 0)
            result_nonzero = 1;
    }
    /* borrow beyond field: result wrapped modulo 10^a_len */

    alu_set_cc(ge, result_nonzero ? 1u : 0u);
}
