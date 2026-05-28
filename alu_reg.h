/**
 * @file  alu_reg.h
 * @brief ALU helpers for GE-120/GE-130 register and memory-field operations.
 *
 * Covers the "PMM" and "PM" group of register instructions:
 *   LR  (0xBC) - Load Register from memory (2-byte field)
 *   STR (0x84) - Store Register to memory  (2-byte field)
 *   CMR (0xBD) - Compare memory to register; sets CC only
 *   AMR (0xBE) - Add memory value to register; sets CC + carry
 *   SMR (0xBF) - Subtract memory value from register; sets CC + carry
 *   LA  (0x68) - Load effective address into register (no memory data fetch)
 *   SR  (0xD9) - Search Right: scan a memory field left→right for a model byte;
 *                store address-after-match (or address-after-field) in register 7
 *   SL  (0xDB) - Search Left:  scan a memory field right→left for a model byte;
 *                store address-after-match (or address-before-field) in register 7
 *   MVQ (0xF8) - Move Quartets: copy len bytes, ignoring zone nibbles (upper 4
 *                bits); sets CC (0=result zero, 1=result nonzero)
 *   CMQ (0xF9) - Compare Quartets: compare two byte fields ignoring zone nibbles;
 *                sets CC (1=first<second, 2=first==second, 3=first>second)
 *
 * Register-operand convention
 * ---------------------------
 * The GE-120/130 has eight 16-bit "change registers" (index registers) numbered
 * 0-7, stored in memory at addresses 0x00F0-0x00FF (two bytes each).  At the
 * struct-ge level the registers rV1..rV4 are the C-side mirrors for registers
 * 4-7 (codes 1100-1111); registers 0-3 (codes 1000-1011) live only in memory at
 * 0x00F0-0x00F7.
 *
 * To keep the helper layer simple and testable without full CPU decode, the
 * helpers take a (uint16_t *reg) pointer directly — the CPU decode layer must
 * map the 3-bit N field to the right pointer before calling.
 *
 * Register 7 (rV4, memory address 0x00FE/0x00FF) receives the result address
 * from SR/SL — the helpers therefore also take a (uint16_t *r7) pointer.
 *
 * Condition-code mapping (ffFI bits 4+5 via alu_cc.h)
 * -------------------------------------------------------
 * This follows the table in cpu_6.txt §5.10.2, confirmed against §5.6.4:
 *
 *   CMR (FA04,FA05):  00=impossible  01=Reg<Storage  10=Reg==Storage  11=Reg>Storage
 *         → ALU_CC values:            1               2                3
 *           (0 is never set; "Not possible" → not an expected outcome)
 *
 *   AMR (FA04,FA05):  00=zero  01=nonzero  10=overflow+zero  11=overflow+nonzero
 *         → 0=zero/equal, 1=nonzero, 2=overflow(zero partial), 3=overflow(nonzero)
 *
 *   SMR (FA04,FA05):  00=impossible  01=result<0(complement)  10=result==0  11=result>0
 *         → ALU_CC values:             1                        0             2
 *           Note: SMR result is always in 2's-complement form.
 *
 *   MVQ (FA04,FA05):  00=result==0  01=result!=0  10,11=impossible
 *         → ALU_CC_EQUAL(0) for all-zero, ALU_CC_LOW(1) for nonzero
 *
 *   CMQ (FA04,FA05):  00=impossible  01=OP1<OP2  10=OP1==OP2  11=OP1>OP2
 *         → ALU_CC values:            1           2             3  (same encoding as CMR)
 *
 *   SR/SL: qualitative result is "not interested" per §5.5.4.
 *
 * Carry convention
 * ----------------
 * URPE and URPU are the two carry flip-flops (ge.h lines 387-388).
 * For AMR: URPE=carry out of bit-15 (overflow carry), URPU=carry into bit-15.
 * For SMR: same convention applied to the 2's-complement subtraction.
 * This matches the convention used by the existing CPU code (ce.c/msl.c usage).
 */

#ifndef ALU_REG_H
#define ALU_REG_H

#include <stdint.h>
#include "ge.h"
#include "alu_cc.h"

/* -------------------------------------------------------------------------
 * Register-load / store
 * -------------------------------------------------------------------------
 * All register operations work on 2-byte (16-bit) fields.
 * Memory is addressed as individual bytes; the register holds the pair.
 * The GE stores the MSB at the LOWER address (big-endian within the word).
 */

/**
 * alu_lr - Load Register (LR, 0xBC)
 *
 * Loads the 16-bit value from memory at [addr, addr+1] into *reg.
 * The byte at addr is the high byte (MSB); addr+1 is the low byte (LSB).
 * Qualitative result: not affected.
 *
 * @param ge   machine state
 * @param reg  pointer to the target 16-bit register (e.g. &ge->rV1)
 * @param addr memory address of the high (rightmost significant) byte
 */
void alu_lr(struct ge *ge, uint16_t *reg, uint16_t addr);

/**
 * alu_str - Store Register (STR, 0x84)
 *
 * Stores the 16-bit reg_val into memory at [addr, addr+1].
 * High byte → mem[addr], low byte → mem[addr+1].
 * Qualitative result: not affected.
 *
 * @param ge      machine state
 * @param reg_val value to store (register content)
 * @param addr    memory address of the high byte
 */
void alu_str(struct ge *ge, uint16_t reg_val, uint16_t addr);

/* -------------------------------------------------------------------------
 * Arithmetic / compare on registers
 * -------------------------------------------------------------------------*/

/**
 * alu_cmr - Compare Memory to Register (CMR, 0xBD)
 *
 * Compares reg_val against value (both treated as unsigned 16-bit integers).
 * Sets CC only; neither operand is modified.
 *
 * CC result (§5.6.4.4 table, FA04=bit4, FA05=bit5):
 *   FA04 FA05
 *    0    0  → "Not possible" (never emitted)
 *    0    1  → Register < memory  → ALU_CC_LOW  (1)
 *    1    0  → Register == memory → ALU_CC_HIGH is wrong; the manual says
 *                                   10 = "Reg equal to memory" → ALU_CC_HIGH (2)
 *    1    1  → Register > memory  → ALU_CC_OVF  (3)
 *
 * NOTE: The CC encoding is: (FA04<<1)|FA05, so:
 *   Reg < mem  → cc=1   Reg == mem → cc=2   Reg > mem → cc=3
 *
 * @param ge       machine state
 * @param reg_val  the register value (first operand)
 * @param value    the memory value (second operand)
 */
void alu_cmr(struct ge *ge, uint16_t reg_val, uint16_t value);

/**
 * alu_amr - Add Memory to Register (AMR, 0xBE)
 *
 * register = register + value (16-bit binary, unsigned)
 * Overflow beyond 16 bits is discarded (carry lost).
 *
 * CC result (§5.6.4.2):
 *   FA04 FA05
 *    0    0  → result == 0, no overflow     → ALU_CC_EQUAL (0)
 *    0    1  → result != 0, no overflow     → ALU_CC_LOW   (1)
 *    1    0  → overflow, partial result ==0 → ALU_CC_HIGH  (2)
 *    1    1  → overflow, partial result !=0 → ALU_CC_OVF   (3)
 *
 * URPE = carry out of bit-15 (the "overflow" bit)
 * URPU = carry into bit-15 (bit-14 carry out; used in arithmetic logic)
 *
 * @param ge    machine state
 * @param reg   pointer to the 16-bit register (modified in place)
 * @param value value to add
 */
void alu_amr(struct ge *ge, uint16_t *reg, uint16_t value);

/**
 * alu_smr - Subtract Memory from Register (SMR, 0xBF)
 *
 * register = register - value  (2's-complement form, 16-bit)
 * Result is left in 2's-complement (negative result stays complemented).
 *
 * CC result (§5.6.4.3):
 *   FA04 FA05
 *    0    0  → "Not possible" (never emitted)
 *    0    1  → result < 0 (negative; complemented form) → ALU_CC_LOW  (1)
 *    1    0  → result == 0                              → ALU_CC_HIGH (2)
 *    1    1  → result > 0                               → ALU_CC_OVF  (3)
 *
 * URPE = borrow out (1 if borrow from bit-16, i.e. result wrapped)
 * URPU = borrow into bit-15
 *
 * @param ge    machine state
 * @param reg   pointer to the 16-bit register (modified in place)
 * @param value value to subtract from *reg
 */
void alu_smr(struct ge *ge, uint16_t *reg, uint16_t value);

/**
 * alu_la - Load Address (LA, 0x68)
 *
 * Stores the effective address (already computed by caller) into *reg.
 * No memory data fetch is performed — this is like IBM S/360 LA.
 * Qualitative result: not affected.
 *
 * @param ge   machine state
 * @param reg  pointer to the target register
 * @param addr the effective address to load
 */
void alu_la(struct ge *ge, uint16_t *reg, uint16_t addr);

/* -------------------------------------------------------------------------
 * Search instructions (SR/SL)
 * -------------------------------------------------------------------------
 * These are PMM-format (2-address) instructions.
 * They scan a memory field for a model byte.
 * The result address is deposited in register 7 (pointed to by r7).
 * SR scans left-to-right; SL scans right-to-left.
 * field = address of leftmost byte of search field.
 * len   = number of bytes to search (L1+1 in instruction encoding).
 * model = the byte value to search for.
 * Qualitative result: "not interested" per §5.5.4.
 * -------------------------------------------------------------------------*/

/**
 * alu_sr - Search Right (SR, 0xD9)
 *
 * Scan mem[field .. field+len-1] left-to-right for a byte equal to model.
 * If found at position i: *r7 = field + i + 1 (address following the match).
 * If not found:           *r7 = field + len    (address following the field).
 * No CC change.
 *
 * @param ge    machine state
 * @param r7    pointer to register 7 (receives result address)
 * @param field memory address of the leftmost byte of the search field
 * @param len   number of bytes in the search field
 * @param model byte value to search for
 */
void alu_sr(struct ge *ge, uint16_t *r7,
            uint16_t field, uint16_t len, uint8_t model);

/**
 * alu_sl - Search Left (SL, 0xDB)
 *
 * Scan mem[field .. field+len-1] right-to-left for a byte equal to model.
 * If found at position i (0=leftmost): *r7 = field + i - 1 (address before match,
 *   i.e. the address preceding the found character in the direction of search).
 *   For i==0 (leftmost byte matches): *r7 = field - 1 (one before field start).
 * If not found: *r7 = field - 1 (one before the field start).
 *
 * NOTE: The manual says for SL "the address stored is the one of the character
 * following the end of the research field in the direction of the research" —
 * direction is right-to-left, so "following end" means field-1 when not found.
 * This is the symmetric of SR.
 *
 * @param ge    machine state
 * @param r7    pointer to register 7 (receives result address)
 * @param field memory address of the leftmost byte of the search field
 * @param len   number of bytes in the search field
 * @param model byte value to search for
 */
void alu_sl(struct ge *ge, uint16_t *r7,
            uint16_t field, uint16_t len, uint8_t model);

/* -------------------------------------------------------------------------
 * Quartet instructions (MVQ / CMQ)
 * -------------------------------------------------------------------------
 * "Quartets" = decimal unpacked format.  Each byte holds a zone nibble (high)
 * and a digit nibble (low).  MVQ/CMQ ignore the zone nibbles and operate only
 * on the low 4 bits of each byte.
 *
 * Both instructions use the 2-address PMM format:
 *   dst  = address of the rightmost byte of the destination field
 *   src  = address of the rightmost byte of the source field
 *   len  = number of bytes (L1+1 in instruction encoding)
 * Operations proceed right-to-left (from rightmost to leftmost byte).
 * -------------------------------------------------------------------------*/

/**
 * alu_mvq - Move Quartets (MVQ, 0xF8)
 *
 * Copies the low 4 bits (digit nibble) of each byte from src field to dst
 * field, right-to-left, preserving the zone nibble of the destination.
 * Length is the number of bytes (len = L1+1).
 *
 * UNCERTAINTY (OCR): The manual states MVQ does not use or change the zones
 * (§5.5.1 context, §3.084/3.098); the digit nibbles are moved.  The phrasing
 * is that the zones are "not processed."  This implementation preserves the
 * destination zone nibble and writes only the digit nibble from source.
 * If the real hardware copies the full byte (ignoring-zone meaning something
 * different), this needs a hardware trace correction.
 *
 * CC result (§5.10.2 table):
 *   FA04 FA05
 *    0    0  → transferred result == 0  → ALU_CC_EQUAL (0)
 *    0    1  → transferred result != 0  → ALU_CC_LOW   (1)
 * "Result" refers to whether all transferred digit nibbles are zero.
 *
 * @param ge   machine state
 * @param dst  address of the rightmost byte of destination
 * @param src  address of the rightmost byte of source
 * @param len  number of bytes to process
 */
void alu_mvq(struct ge *ge, uint16_t dst, uint16_t src, uint8_t len);

/**
 * alu_cmq - Compare Quartets (CMQ, 0xF9)
 *
 * Compares the digit nibbles (low 4 bits) of two byte fields, right-to-left.
 * Neither operand is altered.
 *
 * CC result (§5.5.1.4):
 *   FA04 FA05
 *    0    0  → "Not possible" (never emitted)
 *    0    1  → first operand < second → ALU_CC_LOW  (1)
 *    1    0  → first operand == second → ALU_CC_HIGH (2)
 *    1    1  → first operand > second → ALU_CC_OVF  (3)
 *
 * The comparison is lexicographic on digit nibbles, left-to-right
 * (most significant digit first), consistent with treating the field
 * as a multi-digit decimal integer.
 *
 * @param ge   machine state
 * @param a    address of the rightmost byte of the first operand
 * @param b    address of the rightmost byte of the second operand
 * @param len  number of bytes to compare
 */
void alu_cmq(struct ge *ge, uint16_t a, uint16_t b, uint8_t len);

#endif /* ALU_REG_H */
