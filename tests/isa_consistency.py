#!/usr/bin/env python3
"""isa_consistency.py — guard the ISA opcode table against four-way drift.

The opcode/mnemonic table is encoded in four places:
  * opcodes.h            — `#define NAME_OPCODE 0xNN`     (gemu, source of truth)
  * assembler/gasm.c     — MNEMS[] `{ "NAME", 0xNN, ... }`
  * disassembler/gdis.c  — DTAB[]  `{ 0xNN, "NAME", ... }`
  * docs/ISA.md          — opcode map (human reference; not machine-checked here)

This checks the three *code* sources agree. The external authority for the
mnemonics/directives themselves is the GE "APS — Assembly Programming System"
manual (EDV-AFL vol. 03); its scans are not byte-extractable, so reconcile gasm
and ISA.md against it by reading the page images during the evidence pass.

Exit status: 0 = consistent, 1 = a real mismatch (same mnemonic, different
opcode across sources), 2 = could not read a source file.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def read(path):
    try:
        with open(path, "r") as f:
            return f.read()
    except OSError as e:
        print(f"isa-consistency: cannot read {path}: {e}", file=sys.stderr)
        sys.exit(2)


def parse_opcodes_h(text):
    """stem -> opcode byte, from `#define STEM_OPCODE 0xNN`."""
    out = {}
    for m in re.finditer(r"#define\s+(\w+)_OPCODE\s+(0[xX][0-9A-Fa-f]+)", text):
        out[m.group(1)] = int(m.group(2), 16)
    return out


def parse_gasm(text):
    """list of (name, op) from MNEMS entries `{ "NAME", 0xNN, ... }`."""
    out = []
    for m in re.finditer(r'\{\s*"([A-Z0-9]+)"\s*,\s*(0[xX][0-9A-Fa-f]+)\s*,', text):
        out.append((m.group(1), int(m.group(2), 16)))
    return out


def parse_gdis(text):
    """list of (name, op) from DTAB entries `{ 0xNN, "NAME", ... }`."""
    out = []
    for m in re.finditer(r'\{\s*(0[xX][0-9A-Fa-f]+)\s*,\s*"([A-Z0-9]+)"\s*,', text):
        out.append((m.group(2), int(m.group(1), 16)))
    return out


def main():
    opcodes = parse_opcodes_h(read(os.path.join(ROOT, "opcodes.h")))
    gasm = parse_gasm(read(os.path.join(ROOT, "assembler", "gasm.c")))
    gdis = parse_gdis(read(os.path.join(ROOT, "disassembler", "gdis.c")))

    if not opcodes or not gasm or not gdis:
        print("isa-consistency: a source table parsed empty — check the regexes",
              file=sys.stderr)
        return 2

    gasm_by_name = {}
    errors = []

    # gasm internal: a mnemonic must not be listed with two different opcodes.
    for name, op in gasm:
        if name in gasm_by_name and gasm_by_name[name] != op:
            errors.append(f"gasm: '{name}' listed as 0x{gasm_by_name[name]:02X} and 0x{op:02X}")
        gasm_by_name[name] = op

    # gasm vs opcodes.h: where a mnemonic shares opcodes.h's stem, bytes must match.
    for name, op in gasm_by_name.items():
        if name in opcodes and opcodes[name] != op:
            errors.append(f"opcodes.h/gasm: {name} is 0x{opcodes[name]:02X} in opcodes.h "
                          f"but 0x{op:02X} in gasm")

    # gdis vs gasm: a mnemonic in both must carry the same opcode.
    for name, op in gdis:
        if name in gasm_by_name and gasm_by_name[name] != op:
            errors.append(f"gasm/gdis: {name} is 0x{gasm_by_name[name]:02X} in gasm "
                          f"but 0x{op:02X} in gdis")
        if name in opcodes and opcodes[name] != op:
            errors.append(f"opcodes.h/gdis: {name} is 0x{opcodes[name]:02X} in opcodes.h "
                          f"but 0x{op:02X} in gdis")

    if errors:
        print("isa-consistency: FAIL")
        for e in errors:
            print("  " + e)
        return 1

    print(f"isa-consistency: ok "
          f"(opcodes.h {len(opcodes)} defs, gasm {len(gasm_by_name)} mnemonics, "
          f"gdis {len(gdis)} entries agree)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
