/**
 * @file  alu_reg.c
 * @brief ALU helpers for GE-120/GE-130 register and memory-field operations.
 *
 * Source documentation:
 *   Document: cpu_6.txt (GE-130 P.D.S., S.P.S.-GEISI, cod. 3896128)
 *   Sections: §5.6.4 (register ops), §5.5.1.3-4 (MVQ/CMQ),
 *             §5.5.4 (SR/SL search), §5.10.2 (qualitative results table)
 *
 * See alu_reg.h for full API and CC-encoding documentation.
 */

#include <stdint.h>
#include <string.h>
#include "ge.h"
#include "alu_cc.h"
#include "alu_reg.h"

/* =========================================================================
 * Internal helpers
 * =========================================================================*/

/**
 * mem_read16 - read a big-endian 16-bit word from memory.
 * addr is the high-byte address; addr+1 holds the low byte.
 */
static inline uint16_t mem_read16(struct ge *ge, uint16_t addr)
{
    return (uint16_t)(((uint16_t)ge->mem[addr] << 8) |
                      (uint16_t)ge->mem[(uint16_t)(addr + 1)]);
}

/**
 * mem_write16 - write a big-endian 16-bit word to memory.
 */
static inline void mem_write16(struct ge *ge, uint16_t addr, uint16_t val)
{
    ge_mem_store8(ge, addr, (uint8_t)(val >> 8));
    ge_mem_store8(ge, (uint16_t)(addr + 1), (uint8_t)(val & 0xFF));
}

/* =========================================================================
 * LR — Load Register (0xBC)
 * §5.6.4.1: "The content of the memory field, addressed by 11, is
 *            transferred to the N index register."
 * Qualitative result: not interested.
 * =========================================================================*/
void alu_lr(struct ge *ge, uint16_t *reg, uint16_t addr)
{
    *reg = mem_read16(ge, addr);
}

/* =========================================================================
 * STR — Store Register (0x84)
 * §5.6.4.5: "The content of the N register is transferred to the
 *            memory field with address 11."
 * Qualitative result: not interested.
 * =========================================================================*/
void alu_str(struct ge *ge, uint16_t reg_val, uint16_t addr)
{
    mem_write16(ge, addr, reg_val);
}

/* =========================================================================
 * CMR — Compare Memory to Register (0xBD)
 * §5.6.4.4: purely qualitative result, no operand modification.
 *
 * CC encoding (FA04=bit4, FA05=bit5 of ffFI; cc=(FA04<<1)|FA05):
 *   FA04 FA05  cc  meaning
 *    0    0     0  "Not possible" — never set
 *    0    1     1  Register < Memory  → ALU_CC_LOW   (1)
 *    1    0     2  Register == Memory → ALU_CC_EQUAL (2)
 *    1    1     3  Register > Memory  → ALU_CC_HIGH  (3)
 *
 * Confidence: HIGH (direct table from §5.6.4.4 and confirmed in §5.10.2)
 * =========================================================================*/
void alu_cmr(struct ge *ge, uint16_t reg_val, uint16_t value)
{
    uint8_t cc;

    if (reg_val < value)
        cc = ALU_CC_LOW;          /* 1 */
    else if (reg_val == value)
        cc = ALU_CC_EQUAL;        /* 2 — "Reg equal to memory" maps to FA04=1,FA05=0 */
    else
        cc = ALU_CC_HIGH;         /* 3 — "Reg greater than memory" */

    alu_set_cc(ge, cc);
}

/* =========================================================================
 * AMR — Add Memory to Register (0xBE)
 * §5.6.4.2: register = register + memory_value; result left in register.
 * Performed as 16-bit binary arithmetic; carry beyond 16 bits is lost
 * ("overflow").
 *
 * CC encoding (§5.6.4.2) — ADD-magnitude table:
 *   FA04 FA05  cc  meaning
 *    0    0     0  result == 0, no overflow  (raw 0)
 *    0    1     1  result != 0, no overflow  (raw 1)
 *    1    0     2  overflow, partial==0      (raw 2)
 *    1    1     3  overflow, partial!=0      (raw 3)
 *
 * Carry flip-flops:
 *   URPE = carry out of bit 15 (the "overflow" carry, bit 16 of 32-bit sum)
 *   URPU = carry out of bit 14 (carry into bit 15)
 *
 * Confidence: HIGH (direct table §5.6.4.2)
 * =========================================================================*/
void alu_amr(struct ge *ge, uint16_t *reg, uint16_t value)
{
    uint32_t full  = (uint32_t)(*reg) + (uint32_t)value;
    uint16_t result = (uint16_t)(full & 0xFFFF);

    /* Carry out of bit 15 → overflow */
    uint8_t carry_out = (full >> 16) & 1;

    /*
     * Carry out of bit 14 = carry into bit 15.
     * This is the carry from the low 15 bits into bit 15:
     *   bit-15 of the sum is produced by (a[14] + b[14] + carry_into_14).
     * We can detect it as: if the top bit of result differs from
     * what a plain 15-bit addition would give.
     * Simpler: carry_into_15 = carry out of bit 14 = ((a & 0x7FFF) + (b & 0x7FFF)) >> 15
     */
    uint8_t carry_into_15 = (uint8_t)(
        (((uint32_t)(*reg & 0x7FFF)) + ((uint32_t)(value & 0x7FFF))) >> 15
    );

    ge->URPE = carry_out;
    ge->URPU = carry_into_15;

    *reg = result;

    /* CC: overflow flag = FA04 = carry_out; nonzero flag = FA05 = (result!=0) */
    uint8_t cc = (uint8_t)((carry_out << 1) | (result != 0 ? 1 : 0));
    alu_set_cc(ge, cc);
}

/* =========================================================================
 * SMR — Subtract Memory from Register (0xBF)
 * §5.6.4.3: register = register - memory_value.
 * Result is in 2's-complement form; negative stays complemented.
 *
 * CC encoding (§5.6.4.3) — SIGNED-result table:
 *   FA04 FA05  cc  meaning
 *    0    0     0  "Not possible" — never set
 *    0    1     1  result < 0 (complemented)  → ALU_CC_NEG  (1)
 *    1    0     2  result == 0                → ALU_CC_ZERO (2)
 *    1    1     3  result > 0                 → ALU_CC_POS  (3)
 *
 * For signed interpretation we compare as signed 16-bit integers.
 * The subtraction is implemented as addition of 2's complement of value,
 * consistent with the GE-130 hardware.
 *
 * Carry flip-flops:
 *   URPE = borrow out of bit 15 (set when result wrapped, i.e. reg < value unsigned)
 *   URPU = borrow into bit 15
 *
 * Confidence: HIGH (direct table §5.6.4.3)
 * =========================================================================*/
void alu_smr(struct ge *ge, uint16_t *reg, uint16_t value)
{
    uint16_t result = (uint16_t)(*reg - value);

    /*
     * Borrow: in subtract-as-add-complement, carry out = NOT borrow.
     * Unsigned borrow out: reg < value → borrow (URPE=1).
     */
    uint8_t borrow_out   = (*reg < value) ? 1 : 0;

    /*
     * Borrow into bit 15: borrow from lower 15 bits.
     * = 1 if (reg & 0x7FFF) < (value & 0x7FFF)
     */
    uint8_t borrow_into_15 = (uint8_t)((*reg & 0x7FFF) < (value & 0x7FFF) ? 1 : 0);

    ge->URPE = borrow_out;
    ge->URPU = borrow_into_15;

    *reg = result;

    /*
     * CC based on signed 16-bit interpretation of result:
     * NOTE: §5.6.4.3 says "result < 0 (complemented)" for FA05=1.
     * Signed comparison:
     */
    uint8_t cc;
    int16_t sresult = (int16_t)result;

    if (sresult < 0)
        cc = ALU_CC_NEG;    /* 1 */
    else if (sresult == 0)
        cc = ALU_CC_ZERO;   /* 2 */
    else
        cc = ALU_CC_POS;    /* 3 */

    alu_set_cc(ge, cc);
}

/* =========================================================================
 * LA — Load Address (0x68)
 * §5.6.4.6: "The address 11 specified in the instruction is stored in the
 *            N register, after it has been modified by possible indexing."
 * No memory data fetch.  Qualitative result: not interested.
 * =========================================================================*/
void alu_la(struct ge *ge, uint16_t *reg, uint16_t addr)
{
    (void)ge;  /* no CC change, no memory access */
    *reg = addr;
}

/* =========================================================================
 * SR — Search Right (0xD9)
 * §5.5.4.1: scan field left-to-right for model byte; deposit result in
 *           register 7 (r7 pointer supplied by caller).
 *
 * Result address:
 *   Found at offset i: r7 = field + i + 1
 *   Not found:         r7 = field + len
 *
 * Qualitative result: "not interested" per §5.5.4.
 *
 * UNCERTAINTY: The manual says "The address 11 is the one to the left"
 * (i.e. field parameter = leftmost address). Confirmed: field is leftmost.
 * Confidence: MEDIUM (OCR of §5.5.4.1 truncated; logic is symmetric to SL)
 * =========================================================================*/
void alu_sr(struct ge *ge, uint16_t *r7,
            uint16_t field, uint16_t len, uint8_t model)
{
    uint16_t i;

    for (i = 0; i < len; i++) {
        if (ge->mem[(uint16_t)(field + i)] == model) {
            *r7 = (uint16_t)(field + i + 1);
            alu_set_cc(ge, 1u);   /* found -> FA05=1 */
            return;
        }
    }

    /* Not found: address following the field */
    *r7 = (uint16_t)(field + len);
    alu_set_cc(ge, 0u);           /* not found -> FA05=0 */
}

/* =========================================================================
 * SL — Search Left (0xDB) — the mirror of SR.
 * §5.5.4.2: scan field right-to-left for the model byte; deposit result in r7.
 *
 * The field's given address (A1) is its RIGHTMOST byte; the field extends LEFT
 * (lower addresses) for `len` bytes, i.e. [field-len+1 .. field], and is scanned
 * right-to-left.  Result is the address "following" the match in the direction
 * of search (leftward), i.e. match-1 — the mirror of SR's match+1.
 *
 *   Found at byte `pos`: r7 = pos - 1
 *   Not found:          r7 = field - len   (past the left end of the field)
 *
 * CC: FA05 = found (1) / not found (0), as for SR.
 *
 * Validated against funktionalcpu step 0x22: SL 2,0x0557,0x0558 scans
 * [0x0556..0x0557] right-to-left, finds 0x44 at 0x0557, returns 0x0556.
 * =========================================================================*/
void alu_sl(struct ge *ge, uint16_t *r7,
            uint16_t field, uint16_t len, uint8_t model)
{
    uint16_t i;

    /* Scan right-to-left from the rightmost byte (field) toward lower addresses */
    for (i = 0; i < len; i++) {
        uint16_t pos = (uint16_t)(field - i);
        if (ge->mem[pos] == model) {
            *r7 = (uint16_t)(pos - 1); /* address following the match (leftward) */
            alu_set_cc(ge, 1u);   /* found -> FA05=1 */
            return;
        }
    }

    /* Not found: past the left end of the field */
    *r7 = (uint16_t)(field - len);
    alu_set_cc(ge, 0u);           /* not found -> FA05=0 */
}

/* =========================================================================
 * MVQ — Move Quartets (0xF8)
 * §5.5.1.3, §3.084/3.098: Copy digit nibbles (low 4 bits) from src to dst.
 * Zones (high 4 bits) of destination bytes are preserved.
 * Both fields addressed from rightmost byte; operation proceeds right-to-left.
 *
 * CC result (§5.10.2):
 *   FA04=0 FA05=0 (cc=0) → all digit nibbles of result are zero
 *   FA04=0 FA05=1 (cc=1) → at least one digit nibble nonzero
 *
 * NOTE: MVQ uses the ADD-magnitude raw values 0 and 1 directly — these do NOT
 * map to the COMPARE/SIGNED-result named constants; use raw literals here.
 *
 * UNCERTAINTY: "Zones are not processed" (§3.098) could mean:
 *   (a) zones of src are ignored (only digit nibbles copied) — implemented here
 *   (b) zones of dst are zeroed, entire byte overwritten
 *   (c) entire byte is copied (zones "not processed" meaning "not arithmetically
 *       significant")
 * Interpretation (a) is most consistent with "decimal unpacked" semantics.
 * Hardware trace recommended.
 * Confidence: MEDIUM (OCR partially clear; semantics from §3.098 context)
 * =========================================================================*/
void alu_mvq(struct ge *ge, uint16_t dst, uint16_t src, uint8_t len)
{
    uint8_t any_nonzero = 0;
    uint8_t i;

    /* Process right-to-left; addresses given are already rightmost */
    for (i = 0; i < len; i++) {
        uint16_t sa = (uint16_t)(src - i);
        uint16_t da = (uint16_t)(dst - i);
        uint8_t  digit = ge->mem[sa] & 0x0F;          /* take digit nibble from src */
        uint8_t  zone  = ge->mem[da] & 0xF0;          /* keep zone nibble from dst  */
        ge_mem_store8(ge, da, zone | digit);
        if (digit != 0)
            any_nonzero = 1;
    }

    /*
     * CC: 0 = all-zero result, 1 = nonzero result (ADD-magnitude raw values).
     * FA04=0 always for MVQ (only FA05 encodes the result).
     * Use raw 0/1 — these are not the COMPARE/SIGNED-result named constants.
     */
    alu_set_cc(ge, any_nonzero ? 1u : 0u);
}

/* =========================================================================
 * CMQ — Compare Quartets (0xF9)
 * §5.5.1.4: Compare digit nibbles of two fields. Neither altered.
 * "Not considering the zones."
 * Both fields addressed from rightmost byte.
 * Comparison is left-to-right (most significant digit first),
 * consistent with treating the field as a multi-digit number.
 *
 * CC result (§5.5.1.4) — COMPARE table:
 *   FA04 FA05  cc  meaning
 *    0    0     0  "Not possible" — never set
 *    0    1     1  OP1 < OP2  → ALU_CC_LOW   (1)
 *    1    0     2  OP1 == OP2 → ALU_CC_EQUAL (2)
 *    1    1     3  OP1 > OP2  → ALU_CC_HIGH  (3)
 *
 * UNCERTAINTY: Same as MVQ regarding zone handling.
 * Confidence: MEDIUM-HIGH (§5.5.1.4 text and §5.10.2 table consistent)
 * =========================================================================*/
void alu_cmq(struct ge *ge, uint16_t a, uint16_t b, uint8_t len)
{
    uint8_t i;

    /*
     * Compare left-to-right (most significant digit first).
     * Addresses a and b are the rightmost bytes, so the leftmost
     * byte of field 'a' is at (a - len + 1).
     */
    for (i = 0; i < len; i++) {
        uint8_t da = ge->mem[(uint16_t)(a - (len - 1) + i)] & 0x0F;
        uint8_t db = ge->mem[(uint16_t)(b - (len - 1) + i)] & 0x0F;

        if (da < db) {
            alu_set_cc(ge, ALU_CC_LOW);    /* 1 */
            return;
        }
        if (da > db) {
            alu_set_cc(ge, ALU_CC_HIGH);   /* 3 */
            return;
        }
    }

    /* All digit nibbles equal */
    alu_set_cc(ge, ALU_CC_EQUAL);          /* 2 */
}
