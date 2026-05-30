#ifndef DISASM_H
#define DISASM_H

#include <stdint.h>
#include <stddef.h>

/*
 * Lightweight live disassembler for the console / debug views.
 *
 * Decodes straight out of a flat 64K memory image (no symbol table). All opcode
 * bytes come from opcodes.h, so this stays in lock-step with gasm/gdis without a
 * second opcode table to keep in sync. Address operands are printed as raw hex
 * (or disp(N) for a modified field, bit-15 set) since there are no labels.
 */

/* Disassemble one instruction at mem[addr]. Writes the assembly text
 * ("MNEM operands", or "DB 0xNN" for unknown/garbled) into out. Returns the
 * instruction length in bytes (>= 1). */
int ge_disasm_one(const uint8_t *mem, uint16_t addr, char *out, size_t outn);

/* Build a gdb-style window into `out`: `before` instructions, the instruction
 * at `pc`, then `after` instructions, one line each:
 *     "<marker><AAAA>: <hex bytes>  <asm>\n"
 * marker is '>' on the pc line and ' ' otherwise. A forward-resync heuristic
 * aligns the preceding lines. Returns the 0-based line index of the pc line. */
int ge_disasm_window(const uint8_t *mem, uint16_t pc,
                     int before, int after, char *out, size_t outn);

#endif /* DISASM_H */
