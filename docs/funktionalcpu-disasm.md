# funktionalcpu deck — annotated disassembly & routine names

Reverse-engineered from `DUMP1/funktionalcpu.cap` (the "130 CPU FUNCTIONAL TEST",
E. Maccario, Feb 1970). Method: `gdis --image` depunch → load into gemu → dump
`ge.mem` → recursive flow-following disassembler. The program lives at **0x0100+**;
data/scratch in low memory; the per-instruction test vectors are in **0x04B0–0x06xx**.

The deck has two phases: a **CPU instruction self-test** (every ISA op, in order)
and a set of **core-memory tests** (write/verify patterns over address ranges).
A one-byte **progress code is written to `mem[0x0010]`** (memory tests use
`mem[0x2010]`) before each sub-test, so a halt tells the operator exactly which
sub-test failed.

## Proposed routine names (symbol table)

| Addr | Name | What it does |
|---|---|---|
| 0x0000 | `cold_start` | `NOP NOP` → `main` |
| 0x0100 | `main` | → `select_test` |
| 0x0104 | `cpu_selftest` | seed reg7 from 0x04B0, → `selftest_begin` |
| 0x010E | `error_halt` | **fault handler**: light operator-call lamp (`LON`), copy the failing step code (`mem[0x0010]`) to console display `mem[0x0E71]`, → `show_code_then_halt` |
| 0x0120 | `selftest_begin` | start of the ISA self-test (step 0x02) |
| 0x012C | `selftest_continue` | resume point after an operator "continue" |
| 0x013E | `chk_jrt_retaddr` | subroutine: verify `JRT` deposited the right return address |
| 0x0496 | `selftest_switch_restart` | `JS2` (SWITCH 2) → restart at `main` |
| 0x0790 | `selftest_decimal_arith` | decimal multiply/divide (MP/DP) block |
| 0x0E64 | `show_code_then_halt` | show code; `JS2`→`op_resume`, else `final_halt` |
| 0x0E72 | `op_resume` | `LOFF` (lamp off) → `selftest_continue` |
| 0x1402 | `final_halt` | `HLT` |
| 0x172A | `select_test` | read option image `mem[0x0E00]`; dispatch by bit |
| 0x1760 | `run_test_engine` | reset counters; print banner (`PER`); → `cpu_selftest` |
| 0x175A | `idle_halt` | `HLT` — no test option selected (the default gemu run lands here) |
| 0x105E | `memtest_error_halt` | fault handler for the memory tests (segment-relative) |
| 0x17FE | `memtest_4000` | core test of the **0x4000–0x5FFF** range (option 0x40; steps 0x66/0x67/0x68) |
| 0x18F0 | `memtest_6000` | core test of the **0x6000–0x7FFF** range (option 0x20; steps 0x69/0x6A/0x6B) |
| 0x1A22 | `memtest_0100` | core test of the **0x0100–0x0FFF** range |
| 0x1B66 | `memtest_1000` | core test of the **0x1000–0x1FFF** range (steps 0x61/0x64/0x65) |
| 0x19C6 | `report_and_end` | print results (`PER`), compare result table, **End HLT 0x19DE**; else restart |
| 0x19F4 | `memtest_move_lowhigh` | address-uniqueness move test (copy 0x0100↔0x2100) |

Within each `memtest_*`: `*_fill_loop` writes a 256-byte pattern walking a change
register to the range limit, `*_verify_loop` reads it back (`CMC 255`), a
`*_marker_pass` writes/reads the address-uniqueness markers `0x5C`/`0xC6`, and a
`*_countdown` repeats descending. A mismatch branches to `error_halt`/`memtest_error_halt`.

## CPU self-test: step code → instruction under test

Halt with `mem[0x0010] = NN` means that instruction failed. (The deck walks the
whole ISA, each op against a hand-built vector in 0x04B0–0x06xx.)

| step | instr | step | instr | step | instr |
|---|---|---|---|---|---|
| 02 | `JRT` (link) | 13 | `PK` | 24 | `EDT` |
| 03 | `JRT` (return addr) | 14–16 | `AD` (add decimal) | 25–2A | `MP` (mult. packed) |
| 05–06 | `CMI` | 17–18 | `AB` (add binary) | 2B–30 | `DP` (div. packed) |
| 07 | `MVI` | 19 | `SD` (sub decimal) | 31 | `NI` |
| 08–0A | `CMC` | 1A | `SB` (sub binary) | 32 | `CI` |
| 0B–0C | `MVC` | 1B | `MVQ` | 33 | `XI` |
| 0D | `OC` (or) | 1C | `CMQ` | 34–36 | `TM` |
| 0E | `XC` (xor) | 1D–1E | `UPK` (unpack) | 37 | `LR` |
| 0F | `NC` (and) | 1F–21 | `SR` (shift right) | 38 | `CMR`/`AMR`/`SMR` |
| 10–11 | `TL` (translate) | 22 | `SL` (shift left) | | |
| 12 | `PK` (pack) | 23 | `EDT` (edit) | | |

Memory-test step codes: **61/64/65** = 0x1000 range, **66/67/68** = 0x4000 range,
**69/6A/6B** = 0x6000 range (per `mem[0x0010]`/`mem[0x2010]`).

## Console interaction
- **Option image `mem[0x0E00]`** (set by the operator from the console before
  START): bit `0x10`/`0x20`/`0x40`/`0x80` selects a test path via `select_test`;
  with none set, every `CMI` falls through → `idle_halt` (0x175A). This is exactly
  why gemu's plain run stops at 0x175A.
- **SWITCH 2** (`JS2`, opcode 0x53/0x40): poll-to-skip/continue, sprinkled through
  every test (e.g. `selftest_switch_restart`, the `*_marker_pass` heads).
- **SWITCH 1** (`JS1`, 0x53/0x80): restart-to-`cpu_selftest` / `main`.
- **Operator-call lamp**: `LON` on fault, `LOFF` on resume.

## HLT codes (oracle, from the [1].pdf listing)
`3465` = error 0–8K · `1466` = 8–24K · `1467/1468` = 16–24K count fwd/rev ·
`1469` = 24–32K · `146A/146B` = count · **`19DE` = End** (normal completion,
emitted by `report_and_end`).

## Named disassembly (`gdis --symbols`)

The names above are checked in as **`disassembler/funktionalcpu.sym`** (format:
`ADDR NAME ; comment`). `gdis --symbols` applies them: it renames `L_xxxx`
labels and operand references to the symbol names, comments each variable, and
emits an `EQU` block for data cells. The annotated listing is committed as
**`disassembler/funktionalcpu.sym.s`** and re-assembles cleanly with `gasm`.

```sh
cd software/gemu/disassembler
make named            # -> funktionalcpu.sym.s
# or directly:
./gdis --symbols funktionalcpu.sym -o funktionalcpu.sym.s ../../DUMP1/funktionalcpu.cap
```
Excerpt:
```
select_test:          ; read opt_image, dispatch by bit 0x10/0x20/0x40/0x80
        CMI    0x10, opt_image
        JC     0xD0, sel_chk_20
        MVI    0xF0, 0x19C3
        JC     0xF0, run_test_engine
```
To name more routines/cells, add lines to `funktionalcpu.sym` and re-run
`make named`. (The raw flow-following decoder used to discover these is the
disposable `/tmp/d4.py`; `gdis --symbols` is the supported path.)
