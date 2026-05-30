/**
 * @file  alu_logic.c
 * @brief GE-120/130 ALU logical and string-move primitives.
 *
 * See alu_logic.h for the full API documentation and condition-code
 * conventions.  All semantics are derived from the GE-130 Programmed
 * Description Specification (cpu_6.txt), chapter 5.
 *
 * CC encoding reminder (from alu_cc.h / alu_cc.c):
 *   alu_set_cc(ge, cc) stores the 2-bit value as:
 *     bit1 (FA04) = cc >> 1
 *     bit0 (FA05) = cc & 1
 *
 * For compare operations the GE-120 uses:
 *   cc=1  first < second   (FA04=0, FA05=1)
 *   cc=2  equal            (FA04=1, FA05=0)
 *   cc=3  first > second   (FA04=1, FA05=1)
 *   cc=0  "not possible"   (never written by these helpers)
 *
 * For XC/XI/TM:
 *   cc=2  result all-zero
 *   cc=3  result non-zero
 */

#include "alu_logic.h"

/* ------------------------------------------------------------------ */
/* MVC – Move Characters                                               */
/* ------------------------------------------------------------------ */

void alu_mvc(struct ge *ge, uint16_t dst, uint16_t src, uint8_t len)
{
    uint8_t i;
    /*
     * The manual says "movement is from left to right through each field
     * a byte at a time" (§5.5.3.1).  A plain left-to-right byte loop is
     * correct even for overlapping fields when dst <= src (destructive
     * overlap dst > src works identically to IBM MVC — the destination
     * bytes already overwritten are used as source for later positions,
     * which replicates the documented GE behaviour).
     */
    for (i = 0; i < len; i++)
        ge_mem_store8(ge, (uint16_t)(dst + i), ge->mem[(uint16_t)(src + i)]);
    /* CC not altered */
}

/* ------------------------------------------------------------------ */
/* MVI – Move Immediate                                                */
/* ------------------------------------------------------------------ */

void alu_mvi(struct ge *ge, uint16_t addr, uint8_t imm)
{
    ge_mem_store8(ge, addr, imm);
    /* CC not altered */
}

/* ------------------------------------------------------------------ */
/* NC – AND Characters                                                 */
/* ------------------------------------------------------------------ */

void alu_nc(struct ge *ge, uint16_t a, uint16_t b, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
        ge->mem[(uint16_t)(a + i)] &= ge->mem[(uint16_t)(b + i)];
    /* "Qualitative result: it is not interested." — CC not altered */
}

/* ------------------------------------------------------------------ */
/* OC – OR Characters                                                  */
/* ------------------------------------------------------------------ */

void alu_oc(struct ge *ge, uint16_t a, uint16_t b, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
        ge->mem[(uint16_t)(a + i)] |= ge->mem[(uint16_t)(b + i)];
    /* "Qualitative result: it is not interested." — CC not altered */
}

/* ------------------------------------------------------------------ */
/* XC – Exclusive-OR Characters                                        */
/* ------------------------------------------------------------------ */

void alu_xc(struct ge *ge, uint16_t a, uint16_t b, uint8_t len)
{
    uint8_t i;
    uint8_t all_zero = 1;

    for (i = 0; i < len; i++) {
        ge->mem[(uint16_t)(a + i)] ^= ge->mem[(uint16_t)(b + i)];
        if (ge->mem[(uint16_t)(a + i)] != 0)
            all_zero = 0;
    }

    /*
     * §5.5.3.7 qualitative result table (FA04/FA05):
     *   FA04=1, FA05=0 -> cc=2: "Result equal to all zeroes"
     *   FA04=1, FA05=1 -> cc=3: "Result different from all zeroes"
     */
    alu_set_cc(ge, all_zero ? 2 : 3);
}

/* ------------------------------------------------------------------ */
/* NI – AND Immediate                                                  */
/* ------------------------------------------------------------------ */

void alu_ni(struct ge *ge, uint16_t addr, uint8_t imm)
{
    ge_mem_store8(ge, addr, (uint8_t)(ge->mem[addr] & imm));
    /* "Qualitative result: it is not interested." — CC not altered */
}

/* ------------------------------------------------------------------ */
/* OI / CI – OR Immediate (opcode 0x96; "CI" in this deck's mnemonics) */
/* ------------------------------------------------------------------ */

void alu_oi(struct ge *ge, uint16_t addr, uint8_t imm)
{
    uint8_t result = (uint8_t)(ge->mem[addr] | imm);
    ge_mem_store8(ge, addr, result);

    /*
     * Qualitative result (FA04/FA05), symmetric to XI:
     *   FA04=1, FA05=0 -> cc=2: result == 0
     *   FA04=1, FA05=1 -> cc=3: result != 0
     * Validated against funktionalcpu step 0x32 (CI 0xAA on 0x55 -> 0xFF).
     */
    alu_set_cc(ge, (result == 0) ? 2 : 3);
}

/* ------------------------------------------------------------------ */
/* XI – Exclusive-OR Immediate                                         */
/* ------------------------------------------------------------------ */

void alu_xi(struct ge *ge, uint16_t addr, uint8_t imm)
{
    uint8_t result = (uint8_t)(ge->mem[addr] ^ imm);
    ge_mem_store8(ge, addr, result);

    /*
     * §5.6.3.3 qualitative result table (FA04/FA05):
     *   FA04=1, FA05=0 -> cc=2: "Result equal to zero"
     *   FA04=1, FA05=1 -> cc=3: "Result different from zero"
     */
    alu_set_cc(ge, (result == 0) ? 2 : 3);
}

/* ------------------------------------------------------------------ */
/* CMC – Compare Characters                                            */
/* ------------------------------------------------------------------ */

void alu_cmc(struct ge *ge, uint16_t a, uint16_t b, uint8_t len)
{
    uint8_t i;
    uint8_t ba, bb;

    /*
     * §5.5.3.2: "comparison is purely binary", proceeds left to right,
     * stops as soon as an inequality is found.
     *
     * Qualitative result (FA04/FA05):
     *   FA04=0, FA05=1 -> cc=1: "First operand smaller than the second"
     *   FA04=1, FA05=0 -> cc=2: "First operand equal to the second"
     *   FA04=1, FA05=1 -> cc=3: "First operand greater than the second"
     */
    for (i = 0; i < len; i++) {
        ba = ge->mem[(uint16_t)(a + i)];
        bb = ge->mem[(uint16_t)(b + i)];
        if (ba < bb) { alu_set_cc(ge, 1); return; }
        if (ba > bb) { alu_set_cc(ge, 3); return; }
    }
    alu_set_cc(ge, 2); /* all bytes equal */
}

/* ------------------------------------------------------------------ */
/* CI – Compare Immediate (CMI in manual, §5.5.5.1)                   */
/* ------------------------------------------------------------------ */

void alu_ci(struct ge *ge, uint16_t addr, uint8_t imm)
{
    uint8_t mem_byte = ge->mem[addr];

    /*
     * §5.5.5.1 qualitative result table (FA04/FA05):
     *   FA04=0, FA05=1 -> cc=1: "The storage character is smaller than K"
     *   FA04=1, FA05=0 -> cc=2: "The storage character is equal to K"
     *   FA04=1, FA05=1 -> cc=3: "The storage character is greater than K"
     * Both operands treated as unsigned bytes.
     */
    if (mem_byte < imm)
        alu_set_cc(ge, 1);
    else if (mem_byte == imm)
        alu_set_cc(ge, 2);
    else
        alu_set_cc(ge, 3);
}

/* ------------------------------------------------------------------ */
/* TL – Translate (TR in manual, §5.5.3.3)                            */
/* ------------------------------------------------------------------ */

void alu_tl(struct ge *ge, uint16_t a, uint8_t len, uint16_t table)
{
    uint8_t i;

    /*
     * §5.5.3.3: "every byte of the first operand is substituted by the
     * content of a location of the table adding the binary value of the
     * byte of the first operand to the address of the second operand."
     * The manual states the table base must be a multiple of 256.
     * We honour that precondition but do not assert it here (the CPU
     * instruction decoder is responsible for that check).
     */
    for (i = 0; i < len; i++) {
        uint8_t b = ge->mem[(uint16_t)(a + i)];
        ge_mem_store8(ge, (uint16_t)(a + i), ge->mem[(uint16_t)(table + b)]);
    }
    /* "Qualitative result: it is not interested." — CC not altered */
}

/* ------------------------------------------------------------------ */
/* TM – Test under Mask                                                */
/* ------------------------------------------------------------------ */

void alu_tm(struct ge *ge, uint16_t addr, uint8_t mask)
{
    uint8_t result = ge->mem[addr] & mask;

    /*
     * §5.6.3.4: "the result is not written in memory".
     * Qualitative result table (FA04/FA05):
     *   FA04=1, FA05=0 -> cc=2: "Result equal to zero"
     *   FA04=1, FA05=1 -> cc=3: "Result different from zero"
     * (Same encoding as XI.)
     */
    alu_set_cc(ge, (result == 0) ? 2 : 3);
}
