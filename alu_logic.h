/**
 * @file  alu_logic.h
 * @brief GE-120/130 ALU logical and string-move primitives.
 *
 * Implements the "SS-format" (Storage-to-Storage) logical group and the
 * single-operand immediate logical/test instructions described in the
 * GE-130 Programmed Description Specification, chapter 5:
 *
 *   §5.5.3.1  MVC  – Move Characters          (opcode 0xD2)
 *   §5.5.3.2  CMC  – Compare Characters       (opcode 0xD5)
 *   §5.5.3.3  TL   – Translate                (opcode 0xDC, "TR" in manual)
 *   §5.5.3.7  XC   – Exclusive-OR Characters  (opcode 0xD7)
 *   §5.5.3.8  OC   – OR Characters            (opcode 0xD6)
 *   §5.5.3.9  NC   – AND Characters           (opcode 0xD4)
 *   §5.5.5.1  CMI  – Compare Immediate        (opcode 0x96, called "CI" here)
 *   §5.5.5.2  MVI  – Move Immediate           (opcode 0x92)
 *   §5.6.3.1  OI   – OR Immediate             (no CC – "it is not interested")
 *   §5.6.3.2  NI   – AND Immediate            (opcode 0x94, no CC)
 *   §5.6.3.3  XI   – Exclusive-OR Immediate   (opcode 0x97)
 *   §5.6.3.4  TM   – Test under Mask          (opcode 0x91)
 *
 * All SS helpers accept the starting address of each field (most-significant
 * / leftmost byte) and the number of bytes to process.  They operate on
 * ge->mem[] directly, left-to-right, one byte at a time, exactly as the
 * manual specifies.
 *
 * --------------------------------------------------------------------------
 * Condition-code conventions (from manual FA04/FA05 tables, §5.5.3.2 etc.)
 * --------------------------------------------------------------------------
 *
 * The machine encodes the 2-bit CC as:  cc = (FA04 << 1) | FA05
 * so the numeric cc values written by these helpers are:
 *
 *   CMC / CI (compare):
 *     cc = 1  first operand (or mem[addr]) is LESS than second (or K)
 *     cc = 2  operands are EQUAL
 *     cc = 3  first operand (or mem[addr]) is GREATER than second (or K)
 *     (cc = 0 is "not possible" per the manual)
 *
 *   XC / XI (exclusive-OR):
 *     cc = 2  result is all-zero
 *     cc = 3  result is non-zero
 *
 *   TM (test-under-mask):
 *     cc = 2  all selected bits are zero
 *     cc = 3  at least one selected bit is set
 *
 *   MVC, MVI, OC, NC, NI, TL: "qualitative result is not interested" (no CC
 *   written; caller should treat the CC as undefined after these).
 */

#ifndef ALU_LOGIC_H
#define ALU_LOGIC_H

#include "ge.h"
#include "alu_cc.h"

/**
 * alu_mvc – Move Characters (MVC, §5.5.3.1)
 *
 * Copies @len bytes from mem[src .. src+len-1] to mem[dst .. dst+len-1],
 * byte by byte, left to right.  Overlapping fields are supported because
 * the copy proceeds strictly left-to-right (same as the manual description).
 * Does not alter the condition code.
 */
void alu_mvc(struct ge *ge, uint16_t dst, uint16_t src, uint8_t len);

/**
 * alu_mvi – Move Immediate (MVI, §5.5.5.2)
 *
 * Stores the immediate byte @imm into mem[@addr].
 * Does not alter the condition code.
 */
void alu_mvi(struct ge *ge, uint16_t addr, uint8_t imm);

/**
 * alu_nc – AND Characters (NC, §5.5.3.9)
 *
 * Performs a bitwise AND of the @len-byte field at @a with the @len-byte
 * field at @b; result written back into @a.  Operates left-to-right.
 * "Qualitative result: it is not interested." — CC is not altered.
 */
void alu_nc(struct ge *ge, uint16_t a, uint16_t b, uint8_t len);

/**
 * alu_oc – OR Characters (OC, §5.5.3.8)
 *
 * Bitwise OR of @len bytes at @a with @len bytes at @b; result into @a.
 * "Qualitative result: it is not interested." — CC is not altered.
 */
void alu_oc(struct ge *ge, uint16_t a, uint16_t b, uint8_t len);

/**
 * alu_xc – Exclusive-OR Characters (XC, §5.5.3.7)
 *
 * Bitwise XOR of @len bytes at @a with @len bytes at @b; result into @a.
 * Sets CC:
 *   cc = 2  if all bytes of the result are 0x00
 *   cc = 3  if any byte of the result is non-zero
 */
void alu_xc(struct ge *ge, uint16_t a, uint16_t b, uint8_t len);

/**
 * alu_ni – AND Immediate (NI, §5.6.3.2)
 *
 * mem[@addr] &= @imm.
 * "Qualitative result: it is not interested." — CC is not altered.
 */
void alu_ni(struct ge *ge, uint16_t addr, uint8_t imm);
void alu_oi(struct ge *ge, uint16_t addr, uint8_t imm);

/**
 * alu_xi – Exclusive-OR Immediate (XI, §5.6.3.3)
 *
 * mem[@addr] ^= @imm.
 * Sets CC:
 *   cc = 2  if result is 0x00
 *   cc = 3  if result is non-zero
 */
void alu_xi(struct ge *ge, uint16_t addr, uint8_t imm);

/**
 * alu_cmc – Compare Characters (CMC, §5.5.3.2)
 *
 * Compares the @len-byte field at @a with the @len-byte field at @b,
 * byte by byte, left to right, treating each byte as an unsigned integer.
 * Stops as soon as a difference is found.
 * Neither field is modified.
 * Sets CC:
 *   cc = 1  mem[a..] < mem[b..]
 *   cc = 2  fields are equal
 *   cc = 3  mem[a..] > mem[b..]
 */
void alu_cmc(struct ge *ge, uint16_t a, uint16_t b, uint8_t len);

/**
 * alu_ci – Compare Immediate (CMI, §5.5.5.1)
 *
 * Compares the byte mem[@addr] (unsigned) with the immediate @imm (unsigned).
 * Sets CC:
 *   cc = 1  mem[@addr] < @imm
 *   cc = 2  mem[@addr] == @imm
 *   cc = 3  mem[@addr] > @imm
 */
void alu_ci(struct ge *ge, uint16_t addr, uint8_t imm);

/**
 * alu_tl – Translate (TL, §5.5.3.3, "TR" in manual)
 *
 * For each of the @len bytes b at mem[@a .. @a+len-1], replaces it with
 * mem[@table + b].  The @table base address must be a multiple of 256
 * (per manual).  Operates left to right.  The table is not altered.
 * "Qualitative result: it is not interested." — CC is not altered.
 */
void alu_tl(struct ge *ge, uint16_t a, uint8_t len, uint16_t table);

/**
 * alu_tm – Test under Mask (TM, §5.6.3.4)
 *
 * Performs bitwise AND of mem[@addr] and @mask but does NOT write the
 * result to memory.  Sets CC:
 *   cc = 2  result is 0x00  (all selected bits are zero)
 *   cc = 3  result is non-zero  (at least one selected bit is set)
 */
void alu_tm(struct ge *ge, uint16_t addr, uint8_t mask);

#endif /* ALU_LOGIC_H */
