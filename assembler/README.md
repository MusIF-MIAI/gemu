# gasm — GE-120 / GE-130 assembler

`gasm` is a small, self-contained two-pass assembler that turns GE-120 assembly
source into a **punched card deck** — the encode counterpart of the `gemu`
decoder, and of its card reader. A deck is the only thing a GE-120 can be handed
a program on, so it is the only thing gasm emits: the same `.cap` runs in the
emulator and goes in the real machine's hopper.

It is intentionally faithful to the emulator's encoding: every opcode, the
address-field split, the condition masks, and the SS length byte are transcribed
from `opcodes.h`, `msl-commands.c`, and `signals.h`. The complete instruction
dictionary lives in [`../docs/ISA.md` Appendix A](../docs/ISA.md).

## Build

```sh
make            # produces ./gasm
make examples   # punches examples/*.s into build/*.cap
make clean
```

## Usage

```sh
gasm [-o out.cap] [-l listing.txt] [--org 0xNNNN] [--card|--boot] input.s
```

- `-o out.cap`   output deck (default `a.cap`).
- `-l list.txt`  also write an address/byte listing.
- `--org N`      starting origin (default **0x0000**; see *Loading* below —
  a program at 0 must be a 40-byte boot card).
- `--card`       one IPL boot card instead of a loader deck.
- `--boot`       loader deck led by the `boot.s` template instead of the
  original IPL scatter loader.

The image is flat from the lowest `ORG` to the highest byte emitted; gaps are
zero-filled, and it is then punched onto cards. Any error prints
`file:line: error: …` and produces no output.

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
0x0500          ; absolute (<= 0x7FFF) -> field = 0x0500, bit 15 = 0
buf             ; label, resolved to its absolute address (bit 15 = 0)
0x100(2)        ; displacement 0x100 against change register 2 -> field = 0xA100, bit 15 = 1
```

Bit 15 of the address field is the absolute/modified flag (ISA §4.2). An
absolute value/label encodes with bit 15 = 0 and is used as the effective
address directly (no base added). `disp(N)` encodes with bit 15 = 1 and resolves
at run time to `change_register[N] + displacement`. To reach `0x8000`+ you must
reprogram a base register (`LA`/`LR`) and use `disp(N)`.

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

There is one load path, because the machine has one. `CLEAR → LOAD1 → LOAD →
START` reads **exactly one card**: 80 columns, nibble-packed by the channel into
40 bytes at `0x0000`, and executed there. Everything after that is the
program's own doing. So a deck comes in one of two shapes:

1. **A boot card** (`--card`, `ORG 0x0000`, ≤ 40 bytes). The whole program is
   the card the IPL reads. `assembler/examples/halt.s` and `bootcard.s` are
   both this shape.

2. **A loader deck** (the default). Card 1 is the original IPL scatter loader,
   embedded verbatim from the funktionalcpu SAT deck and proven on the real
   machine; the program follows as 66-byte `LL`/`II` relocation cards, and a
   final card jumps to the origin. The origin must be **0x0086 or above** — the
   loader and its `0x0036` card buffer occupy `0x0000-0x0085`. `0x0100` is the
   `DUMP1`/`funktionalcpu` convention; assemble with `--org 0x0100`.

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

$ gasm --org 0x0100 -o hello.cap hello.s
gasm: bootge deck hello.cap: GE loader + 2 scatter cards + termination, load+entry 0x0100 (arm hello.cap)

$ ../ge hello.cap
```

(The `ORG` lines in the source move with `--org`; `hello.s` above is written at
0x0000, which only a ≤40-byte boot card may be.)

## Status notes

`PER`, `PERI`, `RDC` take a generic `aux, addr` pair; `LPSR` and `JRT` have
assigned opcodes but **no decode path** in the current emulator (they assemble
but will not execute end-to-end). These are flagged in ISA.md Appendix A.

## The three deck shapes

Every one of these is a `.cap`. Run it with `ge prog.cap`, or feed the identical
file to the real machine with `arm prog.cap` on the rpi-pico-card-reader.

- `gasm -o prog.cap prog.s` — **the default.** Card 1 is the ORIGINAL IPL
  scatter loader, embedded verbatim from the funktionalcpu SAT deck and proven
  on the real machine. The program follows as 66-byte `LL`/`II` relocation
  cards, then a termination card that lays `NOP2 NOP2 JC-always <origin>` over
  the loader head, where its closing `JU 0x0004` lands. Origin ≥ 0x0086.
  (`--bootge` is an explicit spelling of this default.)

- `gasm --boot -o prog.cap prog.s` — the same idea with the local `boot.s`
  template in card 1 instead: gasm assembles the program, then assembles
  `boot.s` with `DEST`/`DONE`/`NCARDS` patched to the program's origin and
  size, and the program follows as raw 80-byte COLBIN body cards read
  back-to-back. Origin should be 0x0100 or above (the boot card runs below
  0x0026).

- `gasm --card -o prog.cap prog.s` — ONE IPL boot card. The program must
  `ORG 0x0000` and fit 40 bytes (80 hex columns). This is the shape the machine
  reads unaided, and the shape everything else is bootstrapped from.
