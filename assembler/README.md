# gasm — GE-120 / GE-130 assembler

`gasm` is a small, self-contained two-pass assembler that turns GE-120 assembly
source into a **raw machine-code binary** (pure machine code, no header) — the
encode counterpart of the `gemu` decoder.

It is intentionally faithful to the emulator's encoding: every opcode, the
address-field split, the condition masks, and the SS length byte are transcribed
from `opcodes.h`, `msl-commands.c`, and `signals.h`. The complete instruction
dictionary lives in [`../docs/ISA.md` Appendix A](../docs/ISA.md).

## Build

```sh
make            # produces ./gasm
make examples   # assembles examples/*.s into build/*.bin
make clean
```

## Usage

```sh
gasm [-o out.bin] [-l listing.txt] [--org 0xNNNN] input.s
```

- `-o out.bin`   output file (default `a.bin`).
- `-l list.txt`  also write an address/byte listing.
- `--org N`      starting origin (default **0x0000**, the directly-runnable
  convention — see *Loading* below).

Output is a flat image from the lowest `ORG` to the highest byte emitted; gaps
are zero-filled. Any error prints `file:line: error: …` and produces no output.

## Source syntax

```
; comment              (# also starts a comment)
label:                 a label, defines a symbol = current address
NAME    EQU  expr      a constant symbol
        ORG  0x0100    set the location counter
        MNEMONIC operands
label:  MNEMONIC operands   ; label + instruction on one line
```

### Directives

| Directive | Meaning |
|---|---|
| `ORG expr` | set the location counter (default 0x0000) |
| `NAME EQU expr` | define a constant symbol |
| `DB b[, …]` | emit bytes; `"strings"` emit one byte per character (raw ASCII) |
| `DW w[, …]` | emit 16-bit **big-endian** words (handy for change-register tables) |
| `DS n` | reserve `n` zero bytes |

### Expressions

Terms are hex (`0x1F` or `$1F`), decimal (`42`), character (`'A'`), or a symbol;
joined with `+`/`-`. Example: `buf + 4`, `0x100 - 1`.

### Address operands

A memory address is written either as an absolute value/label, or as an explicit
base-relative `disp(N)`:

```
0x0500          ; absolute (must be <= 0x7FFF with identity bases) -> field = 0x0500
buf             ; label, resolved to its absolute address
0x100(2)        ; displacement 0x100 against change register 2 -> field = 0x2100
```

The instruction address field is `field = (modifier << 12) | displacement`, and
the effective address is `displacement + change_register[modifier]`. With the
reset (identity) change registers, an absolute address `A <= 0x7FFF` encodes as
`field == A`. To reach `0x8000`+ you must reprogram a base register (`LA`/`LR`)
and use `disp(N)`.

### Operand forms by instruction

| Group | Example | Encoding |
|---|---|---|
| control (P) | `HLT` | `[op][2nd]` |
| branch | `JC 0xF0, target` | `[op][mask&0xF0][field]` |
| `JU` | `JU target` | `47 F0 [field]` |
| jump aliases | `JE target` | `JC` with the matching mask |
| sense jumps | `JS1 target` | `53 [aux] [field]` |
| register | `LR 2, addr` | `[op][N&7][field]` |
| immediate | `MVI 0x41, addr` | `[op][K][field]` |
| SS single-length | `MVC 5, A1, A2` | `[op][len-1][A1][A2]` |
| SS two-length | `AP 3, 2, A1, A2` | `[op][((l1-1)<<4)|(l2-1)][A1][A2]` |

The jump aliases (`JE/JL/JH/JZ/JNZ/JNE/JLE/JGE/JOV/JMP/JANY`) are an assembler
convenience that emit a `JC` with the corresponding condition mask; the machine
itself only has `JC`, `JCC`, and `JU`. See ISA.md Appendix A for the mask table.

## Loading & running

`gasm` emits raw machine code. The emulator has two load paths:

1. **Direct injection (default ORG 0x0000).** `ge_load_program()` copies the
   image to `mem[0]` and reset leaves `PO = 0`, so an image assembled at the
   default origin runs immediately. This is what the unit tests use, e.g.:

   ```c
   uint8_t prog[/* … bytes from the .bin … */];
   struct ge g;
   ge_init(&g);
   ge_load_program(&g, prog, sizeof(prog));
   ge_clear(&g);
   ge_start(&g);
   while (!g.halted) ge_run_cycle(&g);
   ```

   (`ge_load_program` caps the image at 129 bytes — the storage size of the
   real bootstrap loader.)

2. **Card-deck bootstrap (ORG 0x0100).** Programs loaded through the integrated
   card reader load from `0x0100` (entry `JU 0x0100`), the convention used by
   the `DUMP1`/`funktionalcpu` decks. Assemble with `--org 0x0100` for that path.

## Worked example

```
$ cat hello.s
        ORG 0x0000
start:  MVI 'A', dst
        MVC 5, dst, src
        HLT
        JU  start
        ORG 0x0040
src:    DB  "HELLO"
dst:    DS  5

$ gasm -o hello.bin hello.s
gasm: wrote 74 bytes to hello.bin (origin 0x0000)
```

## Status notes

`PER`, `PERI`, `RDC` take a generic `aux, addr` pair; `LPSR` and `JRT` have
assigned opcodes but **no decode path** in the current emulator (they assemble
but will not execute end-to-end). These are flagged in ISA.md Appendix A.
