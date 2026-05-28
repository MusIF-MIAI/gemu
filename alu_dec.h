/**
 * @file  alu_dec.h
 * @brief GE-130 packed/signed decimal ALU helpers.
 *
 * Operand model (manual §2.3.5 / §5.3, verified lines ~3148-3169):
 *   PACKED format: two BCD digits per byte; the rightmost (highest-address)
 *   byte holds one digit in the high nibble and the SIGN in the low nibble.
 *   Addresses supplied to every function are the address of the RIGHTMOST
 *   (least-significant, highest-address) byte of the operand — matching
 *   the manual's "address of the rightmost position" rule.
 *   Length parameter L → field is L+1 bytes long; digits = 2L+1 (always odd).
 *
 *   Sign codes (manual ~line 3155):
 *     0xB (1011) and 0xD (1101) → negative
 *     all others                → positive
 *     Generated sign: 0xC (positive), 0xD (negative)
 *
 *   ZONED format (for PK/UPK/PKS/UPKS, §5.5.3.4-5.5.3.5, §5.6.2):
 *     One digit per byte; low nibble = digit, high nibble = zone (normally 0xF
 *     for EBCDIC-style, but 0x4 in GE notation; PKS interprets zone 0xA in
 *     the LAST/rightmost character as negative sign).
 *     UPKS always generates zone 0x4 on all result characters.
 *
 * Condition code (§5.6.1.x, all packed arithmetic / compare):
 *   FA04  FA05  (= cc bit1  cc bit0)
 *     0     0   overflow (or, for CMP: L1 < L2 overflow)
 *     0     1   result < 0
 *     1     0   result = 0
 *     1     1   result > 0
 *
 *   For PKS / UPKS:
 *     0     0   not possible
 *     0     1   result < 0
 *     1     0   result = 0
 *     1     1   result > 0
 *
 *   EDT / PK / UPK: CC not used.
 */

#ifndef ALU_DEC_H
#define ALU_DEC_H

#include <stdint.h>
#include "ge.h"
#include "alu_cc.h"

/*
 * Packed arithmetic (§5.6.1)
 * addr = address of RIGHTMOST byte; len = L (field = L+1 bytes).
 */

/** AP  0xEA  Add Packed: op1 = op1 + op2; CC set. */
void alu_ap (struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen);

/** SP  0xEB  Subtract Packed: op1 = op1 - op2; CC set. */
void alu_sp (struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen);

/** MP  0xEC  Multiply Packed: op1 = op1 * op2; CC set.
 *  Constraints: blen <= 8, blen < alen; op1 must have ≥blen+1 leading zero
 *  digits; otherwise overflow (CC=0, operation not performed). */
void alu_mp (struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen);

/** DP  0xED  Divide Packed: op1[left L1-L2 chars] = quotient,
 *  op1[right L2+1 chars] = remainder; CC set.
 *  Overflow if: L1 <= L2, L2 > 7, divisor == 0, or result doesn't fit.
 *  On overflow, operation is NOT performed. */
void alu_dp (struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen);

/** CMP 0xE9  Compare Packed (algebraic, no operand change); CC set.
 *  Overflow if L1 < L2. */
void alu_cmp(struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen);

/** MVP 0xE8  Move Packed: op1 = op2 (sign preserved from op2); CC set. */
void alu_mvp(struct ge *ge, uint16_t a, uint8_t alen, uint16_t b, uint8_t blen);

/*
 * Format conversion (§5.5.3.4-5.5.3.5 and §5.6.2)
 * For PK/UPK: dst/src addresses are also rightmost byte; lens are L (L+1 bytes).
 * CC not affected by PK/UPK; affected by PKS/UPKS.
 */

/** PK  0xDA  Pack: zoned op2 → packed op1 (no sign processing). */
void alu_pk (struct ge *ge, uint16_t dst, uint8_t dlen, uint16_t src, uint8_t slen);

/** UPK 0xD8  Unpack: packed op2 → zoned op1 (no sign processing;
 *  zone of each result byte is taken from the pre-existing zone in ge->mem). */
void alu_upk(struct ge *ge, uint16_t dst, uint8_t dlen, uint16_t src, uint8_t slen);

/** PKS 0xEE  Pack with Sign: zoned op2 → packed op1; sign from zone of
 *  rightmost source byte (zone 0xA → negative, else positive). CC set. */
void alu_pks(struct ge *ge, uint16_t dst, uint8_t dlen, uint16_t src, uint8_t slen);

/** UPKS 0xEF  Unpack with Sign: packed op2 → zoned op1; zone always 0x4.
 *  CC set (reflects sign/value of packed source). */
void alu_upks(struct ge *ge, uint16_t dst, uint8_t dlen, uint16_t src, uint8_t slen);

/*
 * Edit (§5.5.3.6)
 */

/** EDT 0xDE  Edit packed source into pattern at op1.
 *  pattern: address of leftmost byte of pattern; plen = pattern length (bytes).
 *  src:     address of RIGHTMOST byte of packed source field.
 *  Pattern control chars: 0x20 = SST (digit substitute + zero suppress),
 *                         0x21 = TSZ (digit substitute, ends zero suppress),
 *                         0x22 = RSZ (reset zero suppress start),
 *                         other = insertion char (kept as-is).
 *  First byte of pattern is the fill character.
 *  CC: 1 0 = zero suppression still active (result = 0); 1 1 = active off.
 *  (manual §5.5.3.6 / §5.5.3.7 CC table; note "not possible" for 0/1.) */
void alu_edt(struct ge *ge, uint16_t pattern, uint8_t plen, uint16_t src);

#endif /* ALU_DEC_H */
