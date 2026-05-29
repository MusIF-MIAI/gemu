# GE-120 / GE-130 Instruction Set Architecture

A modern-style reference for a 1960s–70s byte-oriented mainframe.

This document describes the instruction set of the **GE-120 / GE-115 / GE-130**
family (descended from the Olivetti/GE **ELEA 9003** line). It is written in the
shape of a contemporary ISA manual — encoding diagrams, operand conventions,
condition-code tables, a per-instruction reference, and an opcode map — but the
machine itself is a variable-length, character/decimal-oriented design of the
IBM System/360 era.

---

## 0. Provenance and confidence

Per the project's evidence rules, this is reconstructed from primary material and
should be read with explicit confidence labels.

**Primary evidence used here:**

| Source | What it establishes | Confidence |
|---|---|---|
| `software/gemu/opcodes.h` | Opcode bytes, mnemonics, format grouping (P/PM/PMM) | high |
| `software/gemu/alu_bin.h`, `alu_logic.h`, `alu_dec.h`, `alu_reg.h` | Per-instruction semantics + CC tables (each cites `cpu_6.txt` §5.x of the *GE-130 Programmed Description Specification*) | high for wired ops; medium where OCR-flagged |
| `software/gemu/msl-commands.c`, `msl-states.c` | Decode routing, dispatch, addressing helpers | high |
| `software/gemu/signals.h` (`verified_condition`) | Branch condition-mask logic | high |
| `software/gemu/pulse.c` | FI→FA condition latch (`cpu fo. 129`) | high |
| `software/gemu/ge.c` (`ge_clear`) | Segment-base register defaults | high |
| CPU[1] *GE 120 CENTRAL PROCESSOR [1]* (DIAGNOSTIC ORGANIZATION; page images) | Indicative HLT codes, SMAC language, card formats (§6.1, `docs/punchcards.md`) | medium (OCR garbled; read via page images per the project's OCR rule) |
| GE *APS — Assembly Programming System* manual (EDV-AFL vol. 03) | External authority for mnemonics/directives (cross-check for gasm/§9) | not yet reconciled (page-image read pending) |

**What "Status" means in the tables below** — whether the *emulator* (`gemu`)
currently executes the instruction, independent of whether the *architecture*
defines it:

- ✅ **wired** — decoded and executed in the current build.
- ◑ **ALU-only** — a pure, unit-tested helper exists (`alu_*.c`) but it is **not
  yet connected to instruction decode**, so the opcode does not run end-to-end.
- ○ **recognized** — the opcode is decoded but its effect is partial/unverified.
- ✗ **reserved** — opcode byte assigned in `opcodes.h`, no execution path.

> ⚠️ Several manual section numbers below are quoted from the `alu_*.h` headers,
> which transcribe poorly-OCR'd pages. Where a value is OCR-derived rather than
> cleanly read, it is flagged. Treat those as *likely* and re-check page images.

---

## 1. Machine model at a glance

- **Memory**: flat, **byte-addressable**, **16-bit addresses** → up to **64 KiB**
  (`mem[65536]`). Big-endian within multi-byte fields (most-significant byte at
  the lowest address).
- **Orientation**: variable-length **character / decimal** machine. Most data
  instructions name *fields* (a start address + a length), not fixed-width
  registers. This is the System/360 "SS" (storage-to-storage) idiom.
- **Programmer registers** (C mirrors in `struct ge`):
  - `PO` — program counter (instruction address).
  - `V1..V4` — working address registers (operand-address staging during a
    cycle; also the C-side mirror of change registers 4–7).
  - `L1..L3` — length / auxiliary-character registers.
  - `FO` — current opcode register.
  - `RO` — memory data register.
  - `SO`/`SA`/`SI` — micro-sequencer state registers (not program-visible).
  - `FI`/`FA` — condition registers (hold the 2-bit condition code; see §5).
- **Change registers**: **eight 16-bit** index / segment-base registers,
  **memory-mapped at `0x00F0–0x00FF`** (two bytes each, big-endian). These are
  the basis of all addressing (§4).
- **Condition code (CC)**: a **2-bit** result indicator, set by most data and
  compare instructions, tested by conditional branches (§5).

**Loading software.** gemu has two input paths: a positional binary
(`./ge prog.bin`) in the **unified format** (a small `origin`+`entry` header then
the flat image — see `docs/binformat.md`), produced by `gasm` and by
`gdis --image`; and `./ge --deck deck.cap`, the Hollerith card-reader path
(`docs/punchcards.md`). The binary path is the primary one and is what the real
machine's card-reader interface will inject.

---

## 2. Data formats

| Format | Layout | Used by |
|---|---|---|
| **Character / byte** | one 8-bit byte | MVC, CMC, NC/OC/XC, TL, immediate ops |
| **Binary integer** | 1–N bytes, big-endian, two's-complement on negative | AB, SB |
| **Zoned (unpacked) decimal** | one digit per byte: high nibble = *zone*, low nibble = BCD digit | AD, SD, MVQ, CMQ, PK/UPK source/dest |
| **Packed decimal** | two BCD digits per byte; **rightmost byte** = one digit (high nibble) + **sign** (low nibble) | AP, SP, MP, DP, CMP, MVP, PKS/UPKS |

**Decimal sign codes** (`alu_dec.h`, manual ~line 3155): low nibble `0xB`/`0xD` =
negative; everything else = positive. Generated signs: `0xC` positive, `0xD`
negative. Zoned "GE notation" zone nibble is `0x4` (vs `0xF` EBCDIC-style); `UPKS`
always emits zone `0x4`, and `PKS` reads zone `0xA` in the rightmost source byte
as a negative sign. *(Confidence: high for the model, medium for the exact zone
constants — OCR.)*

### 2.1 Character set — GE 100 Series Graphic Character Set

This is **not ASCII** (which did not yet exist). The machine's 64-character
graphic set is documented in the GE *APS Reference Manual* (EDV-AFL vol. 03)
Figure 3, p.16 — internal byte codes occupy **`0x40`–`0x5F`** and **`0xA0`–`0xBF`**:

| Range | Glyphs |
|---|---|
| `0x40`–`0x49` | `0 1 2 3 4 5 6 7 8 9` |
| `0x4A`–`0x4F` | `[  #  @  :  >  ?` |
| `0x50` | space |
| `0x51`–`0x59` | `A B C D E F G H I` |
| `0x5A`–`0x5F` | `&  .  ]  (  <  \` |
| `0xA0`–`0xA9` | `↑  J K L M N O P Q R` |
| `0xAA`–`0xAF` | `—  $  •  )  ;  '` |
| `0xB0`–`0xB9` | `+  /  S T U V W X Y Z` |
| `0xBA`–`0xBF` | `←  ,  %  =  "  !` |

`gdis` uses this table to annotate `DB` data bytes with their machine glyph.

> ✅ **Reconciled with the card transcoder (2026-05-29).** The CRZ subsystem
> transcoder (GIS 481 for IBM cards, GIS 482 for BULL) in **normal mode**
> *"transcodes the card code to suit the internal CPU code"* — CRZ[2] §5.3,
> "Table 3 — IBM card code and Internal GECB code equivalent" (CRZ vol. [2]
> p.113). Table 3 reads the **same** code as APS Figure 3: digit `0` = `0x40`
> (column `hgfe=0100`, rows `dcba=0000..1001` → `0x40`–`0x49`), and `J` = `0xA1`
> (from IBM punch `11/1`). So **the transcoder output IS the GE-100 internal
> graphic code** — CRZ and APS agree, and `gdis`'s `GE_GLYPH` table is correct.
>
> The outlier is **`transcode.c`'s `TC_NORMAL` table**, whose "GE char" output is
> actually **EBCDIC** (digits `0xF0`–`0xF9`, `X`=`0xE7`). It was fit to reproduce
> the externally-supplied `funktionalcpu.bin` (which is an EBCDIC rendering of the
> deck), **not** the machine's documented internal code. This affects only
> hypothetical NORMAL-mode *text* reads; program loading is unaffected because
> program decks are read in **by-pass/COLBIN** mode, whose B2R map is itself
> document-confirmed (`docs/punchcards.md` §4.3). See `transcode.c` for the
> flagged caveat.

---

## 3. Instruction formats and encoding

The **top two bits of the opcode** select the format (confirmed both by the
opcode ranges in `opcodes.h` and by the alpha-phase decode in `msl-states.c:124`,
which routes on `FO` bits 6–7):

| Opcode range | Top bits | Format | Length | Shape |
|---|---|---|---|---|
| `0x00–0x3F` | `00` | **P** (control) | 2 bytes | `[op][aux]` — no memory address |
| `0x40–0xBF` | `01`/`10` | **PM** (one address) | 4 bytes | `[op][aux][A-hi][A-lo]` |
| `0xC0–0xFF` | `11` | **PMM / SS** (two address) | 6 bytes | `[op][LL][A1-hi][A1-lo][A2-hi][A2-lo]` |

### 3.1 The "aux char" (second byte) is overloaded

In **PM** instructions the second byte means different things per opcode group —
this is the machine's way of packing a sub-function into one byte:

```
PM  [ opcode ][   aux char   ][  16-bit address  ]
                    |
       branches  -> condition MASK (which CC values jump)   §6.2
       immediate -> the IMMEDIATE operand byte K            §6.4
       register  -> the 4-bit change-register code (N = code & 7)  §6.3
```

### 3.2 The `LL` length byte (SS instructions)

```
SS  [ opcode ][ LL ][ A1 (2 bytes) ][ A2 (2 bytes) ]
                |
  single-length ops:  field length = (LL & 0xFF) + 1            (1..256 bytes)
  two-length decimals: len(A1) = (LL>>4)+1 ,  len(A2) = (LL&0x0F)+1   (1..16)
```

Source: `EXEC_SS` in `msl-commands.c:210` (`len = (rL1 & 0xff)+1`,
`alen = ((rL1>>4)&0xf)+1`, `blen = (rL1&0x0f)+1`).

### 3.3 Worked encoding examples

```
0A 00                  HLT          (P,  2 bytes) — halt
43 F0 17 5A            JC  ,0x175A  (PM, 4 bytes) — mask 0xF0 = jump on any CC
47 .. 01 00            JU  0x0100   (PM, 4 bytes) — unconditional jump
D2 04 0E00 0F00        MVC          (SS, 6 bytes) — move (4+1)=5 chars A2->A1
```

(The `43 F0 17 5A` / `0A 00` pair is the actual idle-halt tail observed when
running `DUMP1/funktionalcpu` to completion.)

---

## 4. Addressing

This is the most important section for understanding the machine, and the part
that most resembles — yet subtly differs from — a modern base+displacement ISA.

### 4.1 Address space and ranges

- Addresses are **16-bit**, so the architectural space is **0x0000–0xFFFF (64 KiB)**.
- **Low memory is reserved**: the eight change registers occupy **`0x00F0–0x00FF`**.
  Programs in `DUMP1` load from **`0x0100`** upward (entry is `JU 0x0100`).
- A "**segment**" is a **4 KiB (0x1000)** window — the granularity of the 3-bit
  address modifier (§4.2).

### 4.2 The 16-bit address field: displacement + modifier

Every PM/SS memory address in an instruction is **not** a flat 16-bit pointer.
It is split (`eff_addr` / `seg_base_of` in `msl-commands.c:166`):

```
 bit  15 | 14 13 12 | 11 10 9 8 7 6 5 4 3 2 1 0
        ?  \  mod  /  \------ displacement (12 bits) ------/
           (3-bit)            (0x000 .. 0xFFF)
```

- **displacement** = bits 0–11 → a value `0x000–0xFFF` (4 KiB reach).
- **modifier** = bits 12–14 → selects one of the **8 change registers** (the
  *segment base*).
- bit 15 — **unused / unknown in the current model** (masked off). *Flagged for
  verification against the manual; could be a sign/indirect bit.* Confidence: low.

**Effective address:**

```
EA = displacement  +  change_register[ modifier ]
```

This is exactly an IBM S/360-style **base + displacement** computation, but with
the base register *selected by a 3-bit field inside the address* rather than a
separate operand field.

### 4.3 Change registers and segment bases

- Eight 16-bit registers at **`0x00F0–0x00FF`** (register `N` at `0xF0 + 2N`,
  big-endian).
- At power-on / CLEAR they are initialised to **identity segment bases**:
  `change_register[N] = N << 12` (see `ge_clear`, `ge.c:55`). So out of reset:

  | modifier N | base | addresses |
  |---|---|---|
  | 0 | `0x0000` | segment 0 (`0x0000–0x0FFF`) |
  | 1 | `0x1000` | segment 1 (`0x1000–0x1FFF`) |
  | 2 | `0x2000` | segment 2 |
  | … | … | … |
  | 7 | `0x7000` | segment 7 (`0x7000–0x7FFF`) |

- Programs **reprogram** a base with `LR` / `LA` / `AMR` (§6.3) to point a 4 KiB
  window anywhere in the 64 KiB space — this is how the CPU diagnostic sweeps
  memory from 8 K to 32 K: it advances a base register and re-issues the same
  displacement-relative accesses.

### 4.4 Relative vs. global addressing

The same encoding supports both styles; the difference is entirely in **what the
selected base register holds**:

- **Global / absolute addressing** — name a fixed cell directly.
  - Trivial form: **modifier 0 with base[0] = 0** → `EA = displacement`, a plain
    `0x000–0xFFF` pointer into segment 0.
  - General form: any base left at its identity value means
    `EA = N*0x1000 + displacement`, i.e. the address byte *literally encodes* the
    absolute address. Example: `JU 0x172A` decodes as displacement `0x72A`,
    modifier `1` → `0x72A + base[1] (0x1000) = 0x172A`. With identity bases the
    written address and the effective address coincide — the program reads as if
    it used flat 16-bit pointers.
- **Relative (relocatable) addressing** — name a cell *relative to a base you
  control*. Load a base register with the start of a buffer/table/segment, then
  use small displacements (`0x000–0xFFF`) against it. Move the base and the whole
  access window relocates without changing any displacement. This is the
  mechanism behind position-independent table walks and the memory-test sweep.

> **Practical reading rule:** if a program never reloads its change registers,
> treat its addresses as **global/absolute** (identity bases make the encoding
> transparent). The moment you see `LR`/`LA`/`AMR` targeting a change register,
> subsequent accesses through that modifier are **relative** to the new base.

### 4.5 Operand-pointer conventions (a deliberate asymmetry)

Two different "where does the address point" rules coexist — match them to the
instruction group or you will read fields backwards:

- **Arithmetic & packed/decimal fields** (`alu_bin.h`, `alu_dec.h`): the address
  is the **rightmost (least-significant, highest) byte**; the field occupies
  `[addr-len+1 .. addr]`; processing is **right-to-left** (least-significant
  digit first). Length truncation: if op2 is longer than op1 it is treated as op1's
  length; if shorter, it is zero-extended on the left.
- **Logical / character SS fields** (`alu_logic.h`): the address is the
  **leftmost byte**; processing is **left-to-right**, byte by byte (so MVC of
  overlapping fields propagates, by design).
- **Search fields** (`SR`/`SL`, `alu_reg.h`): address = **leftmost** byte;
  `SR` scans left→right, `SL` scans right→left.

---

## 5. Condition code and flags

### 5.1 Encoding

The CC is **2 bits**, held in bits **4 (high)** and **5 (low)** of a condition
register: `cc = (bit4 << 1) | bit5` (`alu_cc.c`). The four values, with the
conventional meanings used across compare/arithmetic ops:

| cc | bit4 bit5 | typical meaning |
|----|-----------|-----------------|
| 0 | 0 0 | result zero / overflow / "not possible" (op-specific) |
| 1 | 0 1 | result < 0, or first operand **<** second |
| 2 | 1 0 | result = 0, or operands **equal** |
| 3 | 1 1 | result > 0, or first operand **>** second |

### 5.2 Two condition registers: `FI` (working) and `FA` (latched)

A subtle but load-bearing detail:

- ALU helpers write the CC into **`ffFI`** (`alu_set_cc`).
- Conditional branches and the console lamps read **`ffFA`** (`verified_condition`,
  `signals.h:83`; lamps OF/NZ/IM = `ffFA` bits 4/5/6, `console.c:29`).
- The transfer **`ffFA ← ffFI`** happens once per cycle at pulse **`TO10`**
  (`pulse.c:56`, *cpu fo. 129*).

**Consequence:** an instruction's condition becomes testable by the *next*
instruction (it is latched at the following cycle boundary), not within the same
micro-cycle. This is the classic "CC latched at cycle start" behaviour.

### 5.3 Console-visible flags (`ffFA`)

`OF` lamp = bit4 (overflow), `NZ` lamp = bit5 (non-zero), `IM` lamp = bit6
(`console.c:29`). The other `ffFI` bits 0–6 are individually set/reset by the
microcode (`CI70–CI86`) for interrupt/error conditioning.

### 5.4 Interrupts (architectural model — documented, not yet implemented)

From the descriptive manual CPU[4] *GE 120 CENTRAL PROCESSOR [4]*, §5.2
"Interruption" (dwg 30004122, folio ~41–42). An interrupt is taken **between
instructions** (never mid-instruction):

1. store the **PSR** (program status register) into the **OPSR** zone in memory;
2. load the **PSR** from the **IPSR** zone;
3. resume execution.

- **Enable mask**: interrupts are felt only when **PSR bit 24 == 0**. In the
  emulator that bit is **`FA06`** — exactly the `!FA06` term already gating the
  alpha→`0xF0` branch (`state_E2_E3_TI06_CU04 = RINT && !BIT(ffFA,6)`,
  `msl-states.c`). After CLEAR interrupts are **disabled**; a program enables
  them with **`LPSR`** (opcode `0x9D`) loading a PSR whose bit 24 = 0.
- **Re-entrancy guard**: the IPSR always has bit 24 = 1, so the handler runs with
  interrupts disabled. Returning is done with **`LPSR 768`** (load PSR from OPSR),
  which both resumes the interrupted program and re-enables interrupts.
- **Interrupt zone**: memory **`0x0300`–`0x0307`** (decimal 768–775; "zone used by
  the interruption mechanism", CPU[4] §2.3.1). OPSR is at **`0x0300`** (768,
  named by the `LPSR 768` resume); IPSR is the other half of the 8-byte zone
  (likely `0x0304`, **inferred** — confirm against the page image).
- **Sources**: only specially-enabled peripheral subsystems raise the interrupt
  signal (`RINT` / `INTE` = "interruption present", CPU[4] line 4412).

Status in gemu: **not implemented.** The decode branch (`RINT && !FA06` →
state `0xF0`) and the FFs (`RINT`, `INTE`, `ge.h`) exist, but state `0xF0` is an
empty timing chart, `LPSR` is reserved (§6.11), there is no PSR save/restore, and
no peripheral raises `RINT`. Implementing it needs: a PSR ⇄ OPSR/IPSR model, the
`LPSR` execution, the state-`0xF0` interrupt-sequence timing chart (still to be
located as a page image), and an interrupt-capable peripheral. Confidence: high
for the model (clean prose OCR), medium for the exact IPSR address.

---

## 6. Instruction reference

Grouped by function. Opcodes are hex from `opcodes.h`. "CC" notes whether the
condition code is set. "St" is the implementation status legend from §0
(✅ wired · ◑ ALU-only · ○ recognized · ✗ reserved).

### 6.1 Control / status — P format (2 bytes)

| Mn | Op | 2nd | Summary | CC | St |
|----|----|-----|---------|----|----|
| **HLT** | `0A` | — | Halt the CPU (`CI89` sets `halted=1`). The deck's idle tail is `HLT` + `JU self`. | — | ✅ |
| **NOP2** | `07` | — | No operation (2-byte). | — | ✅ |
| **ENS** | `02` | `10` | *Likely* "enable system / interrupts." | — | ○ |
| **INS** | `02` | `20` | *Likely* "inhibit/interrupt system." | — | ○ |
| **LOFF** | `02` | `40` | *Likely* indicator/interrupt **off**. | — | ○ |
| **LON** | `02` | `80` | *Likely* indicator/interrupt **on**. | — | ○ |
| **LOLL** | `02` | `91` | Purpose unclear from available evidence. | — | ○ |

> ⚠️ The five `0x02` sub-functions (ENS/INS/LON/LOFF/LOLL) are **decoded** in
> `msl-states.c` (`ens`/`ins`/`lon_loll`/`loff`) but their exact architectural
> effect is **not verified** here — names are suggestive only. Confidence: low.
> Recommended next step: locate the interrupt/console-indicator pages of the CPU
> manual and confirm each sub-function.

#### Indicative HLT codes (diagnostic convention)

The diagnostic decks use **HLT as a status/error stop** and identify the reason by
an *indicative code* (read off the console address/data display, or carried in a
SMAC "C/S WORD"). Documented in CPU[1] (DIAGNOSTIC ORGANIZATION):

| Code | Meaning | Source |
|---|---|---|
| `0050` | error HALT during program loading | CPU[1] "PROGRAM LISTING FORMAT" |
| `004A` | program loading end (normal) | CPU[1]; matches `software/loader.txt` `0x004A HLT` |
| `0EXX` | SMAC test error halt (`XX` = `00`..`FF`, the failing C/S code) | CPU[1] SMAC LANGUAGE DESCRIPTION, folio 21 |

These match the emulator: the bootstrap `loader.txt` places `HLT` at `0x004A`
(loading end) and `0x0050` (read error).

**funktionalcpu console-option dispatch & halts (from the deck, via `gdis`).**
CPU[5] turned out to be the Board-Tester / maintenance volume — it only *mentions*
the CPU Functional Test (run-after-memory-swap), it does **not** contain the
test's halt table. The mapping was instead recovered by disassembling the deck
itself (ground truth). The entry jump lands at `0x172A`, a dispatch tree on the
console option byte **`mem[0x0E00]`**:

| `mem[0x0E00]` | action (sets a selector to `0xF0`, runs the test driver at `0x1760`) |
|---|---|
| `0x10` | `MVI 0xF0 -> mem[0x19C3]`, run driver |
| `0x20` | `MVI 0xF0 -> mem[0x18F5]`, run driver |
| `0x40` / `0x80` | via `0x178C`: set up, run driver (`0x1752 -> 0x1760`) |
| `0x00` / unmatched | fall through to the **idle HLT at `0x175A`** (`HLT; JU 0x175A`) |

Confirmed in-image halts: **`0x175A`** (idle / no option), a block of `0A`
(HLT) bytes at **`0x1460`–`0x14A4`** = the memory-test **error-halt table** (the
documented `1466`–`146B` codes are halts inside it), and **`0x19DE`** = `HLT; JU
0x0104` = the "End" halt + restart. The earlier-cited `3465` is **not** an
in-image address (image ends `0x1C53`) — likely a displayed/runtime value, left
unverified. The selected memory tests cannot run to their halts in gemu yet:
they exercise RAM above the loaded image and peripheral output the emulator does
not realistically model, so options `0x10`–`0x80` wander near `0x2003` instead of
halting (only `0x00` reaches `0x175A`). Confidence: high for the dispatch + halt
addresses (disassembled), low for the per-error code values.

### 6.2 Conditional & unconditional branches — PM format (4 bytes)

The branch **target** is the 4th/5th bytes resolved through §4.2
(`CI00s`, `msl-commands.c:180`). Whether the branch is taken is decided by
`verified_condition` (`signals.h:75`): a **4-bit mask `M` = aux char bits 4–7** is
ANDed against the current CC:

```
take branch if:  (M7 & cc==0) | (M6 & cc==1) | (M5 & cc==2) | (M4 & cc==3)
```

So the aux char selects *which CC values* cause the jump. `JC ,0xF0` → mask
`1111` → jump on any CC (used as an unconditional jump in the deck).

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **JC** | `43` | Jump on condition; mask `M` in aux char selects CC values that jump. | reads | ✅ |
| **JU** | `47` | Unconditional jump (JC-family; effectively mask = all). | reads | ✅ |
| **JCC** | `40` | Jump on condition, decimal-deck variant (mask in aux char). | reads | ✅ |
| **JS1** | `53` / `80` | Jump if console **sense switch 1** set (`ge->JS1`). | — | ✅ |
| **JS2** | `53` / `40` | Jump if console **sense switch 2** set (`ge->JS2`). | — | ✅ |
| **JIE** | `53` / `20` | Jump "if end"(?) — decoded; condition source not yet confirmed. | — | ○ |
| **JRT** | `41` | Jump-and-return / return (subroutine linkage). | — | ✗ |

> ⚠️ `JU`/`JCC` share the mask path with `JC` in the current code (mask read from
> the aux char `L1`); the comment in `opcodes.h:40` notes JU as "mask 0xF0" —
> i.e. the *family* low-nibble distinguishes the variants. Exact mask source per
> variant: medium confidence. `JRT` (`0x41`) has an assigned opcode but **no
> decode path** (`jc_js1_js2_jie` does not list it) — reserved/unimplemented.

### 6.3 Register & address — PM format (4 bytes)

Operate on the memory-mapped **change registers** (§4.3). The aux char carries the
register code; index `N = aux & 7` → register at `0xF0 + 2N`
(`reg_addr_of`, `msl-commands.c:141`). The memory operand address is resolved with
the segment base from `L2`'s modifier (`eff_v1_l2`).

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **LR** | `BC` | Load Register: `reg[N] ← mem16[EA]` (big-endian 2-byte). | — | ✅ |
| **STR** | `84` | Store Register: `mem16[EA] ← reg[N]`. | — | ✅ |
| **LA** | `68` | Load Address: `reg[N] ← EA` (no memory fetch; like S/360 LA). | — | ✅ |
| **CMR** | `BD` | Compare reg[N] to memory word; set CC (1<,2=,3>). | set | ✅ |
| **AMR** | `BE` | Add memory word to reg[N]; CC + carry (`URPE`/`URPU`). | set | ✅ |
| **SMR** | `BF` | Subtract memory word from reg[N] (two's-complement result); CC. | set | ✅ |
| **TM** | `91` | Test under Mask: `mem[EA] & K`; CC=2 if all selected bits 0, else 3 (no write). | set | ✅ |

### 6.4 Immediate (single byte `K`) — PM format (4 bytes)

The aux char **is** the immediate operand `K`; the address selects the target byte
(`eff_v1_l2`). Dispatched from beta via `pm_imm_exec` (`msl-states.c:325`).

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **MVI** | `92` | Move Immediate: `mem[EA] ← K`. | — | ✅ |
| **NI** | `94` | AND Immediate: `mem[EA] &= K`. | — | ✅ |
| **XI** | `97` | XOR Immediate: `mem[EA] ^= K`; CC=2 if 0 else 3. | set | ✅ |
| **CI** | `96` | Compare Immediate: `mem[EA]` vs `K`; CC=1/2/3 (`<`/`=`/`>`). | set | ✅ |
| **CMI** | `95` | Compare-logical Immediate (same compare as CI in this build). | set | ✅ |
| **OI** | — | OR Immediate (manual §5.6.3.1) — **no opcode assigned** in `opcodes.h`. | — | ✗ |

> Note: `CI` and `CMI` are documented as distinct mnemonics but both route to
> `alu_ci` here (`msl-commands.c:131`). If the real machine distinguishes signed
> vs logical compare, that distinction is not yet modelled. Confidence: medium.

### 6.5 Character / logical strings — SS format (6 bytes)

Fields point to the **leftmost** byte, processed **left-to-right**, `LL+1` bytes.
Dispatched by `EXEC_SS` (`msl-commands.c:210`).

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **MVC** | `D2` | Move Characters A2→A1 (propagating on overlap). | — | ✅ |
| **CMC** | `D5` | Compare Characters (unsigned, stops at first diff); CC 1/2/3. | set | ✅ |
| **NC** | `D4` | AND Characters: A1 ← A1 & A2. | — | ✅ |
| **OC** | `D6` | OR Characters: A1 ← A1 \| A2. | — | ✅ |
| **XC** | `D7` | XOR Characters: A1 ← A1 ^ A2; CC=2 if all-zero else 3. | set | ✅ |
| **TL** | `DC` | Translate: each byte `b` of A1 ← `mem[A2 + b]` (A2 = 256-aligned table). Manual "TR". | — | ✅ |

### 6.6 Binary arithmetic — SS format (6 bytes)

Fields point to the **rightmost** byte; big-endian; right-to-left.

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **AB** | `FE` | Add Binary: A1 ← A1 + A2 (op2 truncated/zero-extended to op1 length). | set | ✅ |
| **SB** | `FF` | Subtract Binary: A1 ← A1 − A2; negative stored in two's-complement. | set | ✅ |

> Wired via `EXEC_SS` (two-length encoding: `L1=(LL>>4)+1`,
> `L2=(LL&0xF)+1`, full byte counts). Covered by `tests/exec.c`
> (`ab_adds_binary`, `sb_subtracts_binary`).

### 6.7 Unpacked (zoned) decimal arithmetic — SS format (6 bytes)

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **AD** | `FA` | Add Decimal (zoned, unsigned; zones ignored; carry-out dropped). | set | ✅ |
| **SD** | `FB` | Subtract Decimal (zoned; wraps mod 10^L1 on underflow). | set | ✅ |

> Wired (two-length encoding, full byte counts; result zone cleared
> to 0). Tests: `ad_adds_unpacked_decimal`, `sd_subtracts_unpacked_decimal`.
> **CC tables for AD/SD remain OCR-inferred** (`alu_bin.h:34`) — medium
> confidence; re-check page images.

### 6.8 Packed decimal arithmetic — SS format (6 bytes)

Two-length encoding: `len(A1)=(LL>>4)+1`, `len(A2)=(LL&0xF)+1`. Rightmost byte
holds a digit + the sign nibble. CC: 0 overflow, 1 `<0`, 2 `=0`, 3 `>0`
(`alu_dec.h:24`).

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **AP** | `EA` | Add Packed: A1 ← A1 + A2. | set | ✅ |
| **SP** | `EB` | Subtract Packed: A1 ← A1 − A2. | set | ✅ |
| **MP** | `EC` | Multiply Packed: A1 ← A1 × A2 (needs leading-zero room; else overflow, no-op). | set | ✅ |
| **DP** | `ED` | Divide Packed: A1 = quotient‖remainder; overflow if divisor 0 / won't fit. | set | ✅ |
| **CMP** | `E9` | Compare Packed (algebraic, no operand change); overflow if L1<L2. | set | ✅ |
| **MVP** | `E8` | Move Packed: A1 ← A2 (sign preserved from A2). | set | ✅ |

### 6.9 Format conversion / edit — SS format (6 bytes)

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **PK** | `DA` | Pack: zoned A2 → packed A1 (no sign processing). | — | ✅ |
| **UPK** | `D8` | Unpack: packed A2 → zoned A1 (result zones taken from existing memory). | — | ✅ |
| **PKS** | `EE` | Pack with Sign: zoned→packed; sign from rightmost source zone (`0xA`=neg). | set | ✅ |
| **UPKS** | `EF` | Unpack with Sign: packed→zoned; result zone always `0x4`. | set | ✅ |
| **EDT** | `DE` | Edit packed source into a pattern at A1 (fill char, digit-substitute / zero-suppress controls `0x20`/`0x21`/`0x22`). | set | ✅ |

### 6.10 Quartet & search — SS format (6 bytes)

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **MVQ** | `F8` | Move Quartets: copy digit nibbles (low 4 bits) A2→A1, preserve dest zones; CC 0=zero,1=nonzero. | set | ✅ |
| **CMQ** | `F9` | Compare Quartets: compare digit nibbles only; CC 1/2/3. | set | ✅ |
| **SR** | `D9` | Search Right: scan A-field L→R for model byte; result address → register 7. | — | ◑ |
| **SL** | `DB` | Search Left: scan A-field R→L for model byte; result address → register 7. | — | ◑ |

> **MVQ/CMQ** wired (single-length: `len = (LL&0xFF)+1`); tests
> `mvq_moves_digit_preserving_zone`, `cmq_compares_quartets_high`. `MVQ` zone
> handling carries an OCR uncertainty (`alu_reg.h:266`).
> ◑ **SR/SL** remain unwired: their model-byte source and result-register
> encoding in the SS format are unconfirmed — held for the manual-evidence pass.

### 6.11 Peripheral / I-O & status — PM format

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **PER** | `9E` | Peripheral / external operation (issue command to a channel). | via status | ✅ |
| **PERI** | `9C` | Peripheral operation, interrupt variant. | via status | ✅ |
| **RDC** | `90` | Read Card (peripheral read, decimal-deck variant; PER-family). | via status | ✅ |
| **LPSR** | `9D` | Load Program Status Register (PSR ← mem). The interrupt-enable/return mechanism: `LPSR` with bit 24 = 0 enables interrupts, `LPSR 768` resumes from OPSR (§5.4). | — | ✗ |

> The PER-family is decoded by `per_peri` (`msl-states.c:348`) and drives the
> connector / card-reader handshake. The "examine abnormal conditions" (EPER)
> sub-case maps channel status into the CC (`CE_chan1_status`,
> `msl-commands.c:291`). `LPSR` (`0x9D`) has an opcode but no decode path yet.

---

## 7. Execution model (context)

Instructions are not executed by a hard-wired decoder but by a **micro-sequenced**
engine driven by data tables (the *Sequence Logic Matrix*, MSL). Each instruction
passes through CPU **phases**, advanced by timing pulses `TO00…TI10`:

1. **Alpha** (states `0xE2`/`0xE3`) — fetch opcode into `FO`, recognise format,
   detect `HLT`/`PER`.
2. **Operand fetch** (`0xE0`, `0xE4–0xE7`) — pull addresses into `V1`/`V2`,
   length into `L1`; the micro-loop count depends on format (P → none, PM → one
   address, SS → two).
3. **Beta** (states `0x64`/`0x65`/`0x66`) — execute: branches resolve the target
   (`CI00s`), immediate/register/SS ops call their `alu_*` helper, then the
   future-state network points back to alpha for the next instruction.

The CC computed in beta (`ffFI`) becomes visible to the next instruction's branch
test when latched into `ffFA` at the following `TO10` (§5.2).

---

## 8. Open questions / low-confidence items

Read these against page images, not the OCR text layer (the manual OCR is too
garbled for byte-accurate use — render the specific page with `pdftoppm` and read
it). External mnemonic/directive authority: the GE **APS** manual (EDV-AFL 03).

| Item | Issue | Suggested check |
|---|---|---|
| Address bit 15 | masked off; role unknown (sign? indirect?) | CPU addressing pages / schematics |
| ENS/INS/LON/LOFF/LOLL | sub-function meanings unverified | interrupt + console-indicator manual pages; APS manual |
| JRT (`0x41`) | opcode assigned, no decode | branch/linkage section of manual; APS manual |
| LPSR (`0x9D`) | opcode assigned, no decode | PSW / status-register section |
| CI vs CMI | both map to `alu_ci`; signed-vs-logical distinction? | §5.5.5.1 page image |
| JU/JCC mask source | shared mask path with JC | §fo.56/57 page image |
| AD/SD CC tables | OCR-inferred | §5.5.1.1 / §5.5.1.2 page images |
| MVQ zones | "zones not processed" interpretation | §3.084/3.098 + hardware trace |
| SR/SL | ALU done, not wired — SS model-byte/result-register encoding unconfirmed | search-instruction page image |
| funktionalcpu memory-test execution | option-byte dispatch + halt addresses now recovered from the deck (§6.1); running the tests to their halts needs realistic RAM-above-image + peripheral-output modelling (they wander near `0x2003`). `0x3465` code unverified. | run under a fuller memory/peripheral model |
| isolation decks (`isolationcpu0x`) | distinct card framing (binary cols 1–76, Hollerith 77–80) + SMAC/INTE interpretation | CPU[1] folio 52/53a + CPU[3]; see `docs/punchcards.md` §5 |

---

## 9. Opcode map (quick reference, sorted by byte)

| Op | Mn | Fmt | Op | Mn | Fmt | Op | Mn | Fmt |
|----|----|-----|----|----|-----|----|----|-----|
| `02` | ENS/INS/LOFF/LON/LOLL | P | `91` | TM | PM | `D8` | UPK | SS |
| `07` | NOP2 | P | `92` | MVI | PM | `D9` | SR | SS |
| `0A` | HLT | P | `94` | NI | PM | `DA` | PK | SS |
| `40` | JCC | PM | `95` | CMI | PM | `DB` | SL | SS |
| `41` | JRT | PM | `96` | CI | PM | `DC` | TL | SS |
| `43` | JC | PM | `97` | XI | PM | `DE` | EDT | SS |
| `47` | JU | PM | `9C` | PERI | PM | `E8` | MVP | SS |
| `53` | JS1/JS2/JIE | PM | `9D` | LPSR | PM | `E9` | CMP | SS |
| `68` | LA | PM | `9E` | PER | PM | `EA` | AP | SS |
| `84` | STR | PM | `90` | RDC | PM | `EB` | SP | SS |
| `BC` | LR | PM | `D2` | MVC | SS | `EC` | MP | SS |
| `BD` | CMR | PM | `D4` | NC | SS | `ED` | DP | SS |
| `BE` | AMR | PM | `D5` | CMC | SS | `EE` | PKS | SS |
| `BF` | SMR | PM | `D6` | OC | SS | `EF` | UPKS | SS |
| | | | `D7` | XC | SS | `F8` | MVQ | SS |
| | | | | | | `F9` | CMQ | SS |
| | | | | | | `FA` | AD | SS |
| | | | | | | `FB` | SD | SS |
| | | | | | | `FE` | AB | SS |
| | | | | | | `FF` | SB | SS |

---

*Generated from the `gemu` emulator sources (the project's executable transcription
of the GE-130 Programmed Description Specification). Treat OCR-flagged values as
provisional pending page-image review.*

---

# Appendix A — Assembler instruction dictionary

This appendix is the language reference for **`gasm`**, the standalone assembler
in `software/gemu/assembler/`. It restates the encoding of §§3–6 in the concrete
form the assembler accepts, and is the authoritative dictionary of every
mnemonic and operand variant. The assembler's opcode/aux/format tables are
transcribed from `opcodes.h`, `msl-commands.c`, and `signals.h`; this appendix
and `gasm.c` are meant to be kept in sync.

## A.1 Source format

```
; comment                 (# also begins a comment)
label:                    label definition  (symbol = current address)
NAME    EQU  expr         constant symbol
        ORG  0x0100       set the location counter (default 0x0000)
        MNEMONIC operands
label:  MNEMONIC operands ; label + instruction on one line
```

**Directives**

| Directive | Bytes | Meaning |
|---|---|---|
| `ORG expr` | 0 | set the location counter |
| `NAME EQU expr` | 0 | define a constant symbol |
| `DB b[, …]` | 1/byte | emit bytes; `"text"` emits one (raw ASCII) byte per character |
| `DW w[, …]` | 2/word | emit 16-bit **big-endian** words |
| `DS n` | n | reserve `n` zero bytes |

**Expressions** — terms are hex (`0x1F`/`$1F`), decimal, char (`'A'`), or a
symbol, combined with `+`/`-` (e.g. `buf+4`, `0x100-1`).

## A.2 Operand kinds

- **K** — an 8-bit immediate (`0x00–0xFF`).
- **N** — a change-register number (`0–7`).
- **mask** — a byte whose high nibble (bits 4–7) is the condition mask (§6.2).
- **len** — an SS single field length, `1–256` (encoded `LL = len-1`).
- **l1, l2** — SS two-field lengths, `1–16` each
  (encoded `LL = ((l1-1)<<4) | (l2-1)`).
- **addr** — a memory address, written one of two ways:

  | Written | Field encoded | Effective address |
  |---|---|---|
  | `expr` (absolute, ≤ `0x7FFF`) or a label | `field = value` | `value` (identity bases) |
  | `disp(N)` | `field = (N<<12)\|(disp&0xFFF)` | `disp + change_register[N]` |

  The 16-bit address field is `(modifier<<12) \| displacement` (§4.2); bit 15 is
  unused. Absolute addresses above `0x7FFF` cannot be encoded directly — load a
  base register (`LA`/`LR`) and use `disp(N)`.

## A.3 Master dictionary

`Op`/`2nd` are hex. `Len` is the instruction length in bytes. **St** is the
emulator status from §0 (✅ wired · ◑ ALU-only · ○ recognized · ✗ reserved/undecoded).

### A.3.1 Control — P format (2 bytes, no operands)

| Mnemonic | Syntax | Op | 2nd | Len | Meaning | St |
|---|---|----|----|----|---|----|
| `HLT` | `HLT` | `0A` | `00` | 2 | Halt the CPU. | ✅ |
| `NOP2` / `NOP` | `NOP2` | `07` | `00` | 2 | No operation. | ✅ |
| `ENS` | `ENS` | `02` | `10` | 2 | Enable system / interrupts *(name unverified)*. | ○ |
| `INS` | `INS` | `02` | `20` | 2 | Inhibit/interrupt system *(unverified)*. | ○ |
| `LOFF` | `LOFF` | `02` | `40` | 2 | Indicator/interrupt off *(unverified)*. | ○ |
| `LON` | `LON` | `02` | `80` | 2 | Indicator/interrupt on *(unverified)*. | ○ |
| `LOLL` | `LOLL` | `02` | `91` | 2 | Purpose unclear *(unverified)*. | ○ |

### A.3.2 Branches — PM format (4 bytes)

| Mnemonic | Syntax | Op | aux | Len | Meaning | St |
|---|---|----|----|----|---|----|
| `JC` | `JC mask, addr` | `43` | `mask & 0xF0` | 4 | Jump if CC ∈ mask (§6.2). | ✅ |
| `JCC` | `JCC mask, addr` | `40` | `mask & 0xF0` | 4 | Jump on condition, decimal-deck variant. | ✅ |
| `JU` | `JU addr` | `47` | `F0` | 4 | Unconditional jump. | ✅ |
| `JS1` | `JS1 addr` | `53` | `80` | 4 | Jump if console sense switch 1 set. | ✅ |
| `JS2` | `JS2 addr` | `53` | `40` | 4 | Jump if console sense switch 2 set. | ✅ |
| `JIE` | `JIE addr` | `53` | `20` | 4 | Jump "if end" *(condition source unconfirmed)*. | ○ |

**Jump-alias sugar** — `gasm` convenience mnemonics that emit `JC` (`0x43`) with
a computed mask. The machine has no separate opcodes for these. Condition codes
follow §5.1 (cc1 `<`, cc2 `=`, cc3 `>`, cc0 overflow/special):

| Alias | Mask | CC values that jump | Reading |
|---|----|---|---|
| `JMP` / `JANY` | `F0` | 0,1,2,3 | always |
| `JL` / `JLT` | `40` | 1 | first `<` second / result `< 0` |
| `JE` / `JEQ` / `JZ` | `20` | 2 | equal / result `= 0` |
| `JH` / `JGT` | `10` | 3 | first `>` second / result `> 0` |
| `JNE` / `JNZ` | `50` | 1,3 | not equal / result `≠ 0` |
| `JLE` | `60` | 1,2 | `≤` |
| `JGE` | `30` | 2,3 | `≥` |
| `JOV` | `80` | 0 | overflow / special (op-specific, see §5.1) |

### A.3.3 Register & address — PM format (4 bytes)

`aux = N & 7` selects change register `N` (memory-mapped at `0xF0 + 2N`).

| Mnemonic | Syntax | Op | Len | Meaning | St |
|---|---|----|----|---|----|
| `LR`  | `LR N, addr`  | `BC` | 4 | `reg[N] ← mem16[addr]`. | ✅ |
| `STR` | `STR N, addr` | `84` | 4 | `mem16[addr] ← reg[N]`. | ✅ |
| `LA`  | `LA N, addr`  | `68` | 4 | `reg[N] ← addr` (effective address, no fetch). | ✅ |
| `CMR` | `CMR N, addr` | `BD` | 4 | Compare `reg[N]` to `mem16[addr]`; set CC. | ✅ |
| `AMR` | `AMR N, addr` | `BE` | 4 | `reg[N] += mem16[addr]`; set CC. | ✅ |
| `SMR` | `SMR N, addr` | `BF` | 4 | `reg[N] -= mem16[addr]`; set CC. | ✅ |

### A.3.4 Immediate — PM format (4 bytes)

`aux = K` (the immediate byte); `addr` selects the target byte.

| Mnemonic | Syntax | Op | Len | Meaning | St |
|---|---|----|----|---|----|
| `MVI` | `MVI K, addr` | `92` | 4 | `mem[addr] ← K`. | ✅ |
| `NI`  | `NI K, addr`  | `94` | 4 | `mem[addr] &= K`. | ✅ |
| `XI`  | `XI K, addr`  | `97` | 4 | `mem[addr] ^= K`; set CC. | ✅ |
| `CI`  | `CI K, addr`  | `96` | 4 | compare `mem[addr]` with `K`; set CC. | ✅ |
| `CMI` | `CMI K, addr` | `95` | 4 | compare-logical immediate (routes to `CI`). | ✅ |
| `TM`  | `TM K, addr`  | `91` | 4 | test `mem[addr] & K`; set CC (no write). | ✅ |

### A.3.5 Peripheral / misc — PM format (4 bytes, generic `aux, addr`)

The Z/aux byte for the PER family encodes channel and sub-operation; `gasm`
takes it as a raw `aux` byte (see §6.11 for the bit meanings).

| Mnemonic | Syntax | Op | Len | Meaning | St |
|---|---|----|----|---|----|
| `PER`  | `PER aux, addr`  | `9E` | 4 | Peripheral / external operation. | ✅ |
| `PERI` | `PERI aux, addr` | `9C` | 4 | Peripheral operation, interrupt variant. | ✅ |
| `RDC`  | `RDC aux, addr`  | `90` | 4 | Read card (PER-family, decimal-deck variant). | ✅ |
| `LPSR` | `LPSR aux, addr` | `9D` | 4 | Load program status register. | ✗ |
| `JRT`  | `JRT aux, addr`  | `41` | 4 | Jump-and-return / linkage. | ✗ |

> `LPSR` and `JRT` have assigned opcodes but **no decode path** in the current
> emulator: they assemble but will not execute end-to-end.

### A.3.6 SS single-length — SS format (6 bytes): `len, A1, A2`

`LL = len-1`, `len` ∈ `1..256`. Fields point to the **leftmost** byte (§4.5).

| Mnemonic | Syntax | Op | Len | Meaning | St |
|---|---|----|----|---|----|
| `MVC` | `MVC len, A1, A2` | `D2` | 6 | Move characters A2→A1. | ✅ |
| `NC`  | `NC len, A1, A2`  | `D4` | 6 | `A1 ← A1 & A2`. | ✅ |
| `CMC` | `CMC len, A1, A2` | `D5` | 6 | Compare characters; set CC. | ✅ |
| `OC`  | `OC len, A1, A2`  | `D6` | 6 | `A1 ← A1 \| A2`. | ✅ |
| `XC`  | `XC len, A1, A2`  | `D7` | 6 | `A1 ← A1 ^ A2`; set CC. | ✅ |
| `TL`  | `TL len, A1, A2`  | `DC` | 6 | Translate A1 through table at A2. | ✅ |
| `MVQ` | `MVQ len, A1, A2` | `F8` | 6 | Move digit quartets; set CC. | ✅ |
| `CMQ` | `CMQ len, A1, A2` | `F9` | 6 | Compare digit quartets; set CC. | ✅ |
| `EDT` | `EDT len, A1, A2` | `DE` | 6 | Edit packed A2 into pattern at A1; set CC. | ✅ |
| `SR`  | `SR len, A1, A2`  | `D9` | 6 | Search right; result address → reg 7. *(encoding unconfirmed)* | ◑ |
| `SL`  | `SL len, A1, A2`  | `DB` | 6 | Search left; result address → reg 7. *(encoding unconfirmed)* | ◑ |

### A.3.7 SS two-length — SS format (6 bytes): `l1, l2, A1, A2`

`LL = ((l1-1)<<4) | (l2-1)`, with `l1, l2` ∈ `1..16`. Decimal/binary fields point
to the **rightmost** byte and are processed right-to-left (§4.5).

| Mnemonic | Syntax | Op | Len | Meaning | St |
|---|---|----|----|---|----|
| `PK`   | `PK l1, l2, A1, A2`   | `DA` | 6 | Pack zoned A2 → packed A1. | ✅ |
| `UPK`  | `UPK l1, l2, A1, A2`  | `D8` | 6 | Unpack packed A2 → zoned A1. | ✅ |
| `PKS`  | `PKS l1, l2, A1, A2`  | `EE` | 6 | Pack with sign; set CC. | ✅ |
| `UPKS` | `UPKS l1, l2, A1, A2` | `EF` | 6 | Unpack with sign; set CC. | ✅ |
| `MVP`  | `MVP l1, l2, A1, A2`  | `E8` | 6 | Move packed A2→A1; set CC. | ✅ |
| `CMP`  | `CMP l1, l2, A1, A2`  | `E9` | 6 | Compare packed; set CC. | ✅ |
| `AP`   | `AP l1, l2, A1, A2`   | `EA` | 6 | Add packed `A1 += A2`; set CC. | ✅ |
| `SP`   | `SP l1, l2, A1, A2`   | `EB` | 6 | Subtract packed `A1 -= A2`; set CC. | ✅ |
| `MP`   | `MP l1, l2, A1, A2`   | `EC` | 6 | Multiply packed; set CC. | ✅ |
| `DP`   | `DP l1, l2, A1, A2`   | `ED` | 6 | Divide packed; set CC. | ✅ |
| `AD`   | `AD l1, l2, A1, A2`   | `FA` | 6 | Add zoned decimal; set CC. | ✅ |
| `SD`   | `SD l1, l2, A1, A2`   | `FB` | 6 | Subtract zoned decimal; set CC. | ✅ |
| `AB`   | `AB l1, l2, A1, A2`   | `FE` | 6 | Add binary; set CC. | ✅ |
| `SB`   | `SB l1, l2, A1, A2`   | `FF` | 6 | Subtract binary; set CC. | ✅ |

> `SR` (`D9`) and `SL` (`DB`) — the search instructions — assemble as plain
> single-length SS ops (so they round-trip with the `gdis` disassembler), but
> their true model-byte/result-register encoding is **unconfirmed** (§6.10,
> ◑ ALU-only in gemu). Treat the operand layout as provisional until verified.

## A.4 Round-trip encoding examples

These are the assembler's regression vectors; each matches the emulator decode
(the first four are the §3.3 worked examples; `MVI 0xAB,0x0050` is the
`tests/exec.c` `mvi_stores_immediate` vector).

| Source | Bytes |
|---|---|
| `HLT` | `0A 00` |
| `JC 0xF0, 0x175A` | `43 F0 17 5A` |
| `JU 0x0100` | `47 F0 01 00` |
| `MVC 5, 0x0E00, 0x0F00` | `D2 04 0E 00 0F 00` |
| `MVI 0xAB, 0x0050` | `92 AB 00 50` |
| `JE 0x0100` | `43 20 01 00` |
| `LR 2, 0x0050` | `BC 02 00 50` |
| `AP 3, 2, 0x0E00, 0x0F00` | `EA 21 0E 00 0F 00` |
| `MVC 4, 0x100(2), 0xFFF(7)` | `D2 03 21 00 7F FF` |

## A.5 Loading the output

`gasm` writes pure machine code with no header. Default origin is `0x0000`,
which matches `ge_load_program()` (copies the image to `mem[0]`; reset leaves
`PO = 0`) — the path the unit tests use. For card-deck-style programs that the
integrated reader bootstraps at `0x0100` (the `DUMP1`/`funktionalcpu`
convention), assemble with `--org 0x0100`. See
`software/gemu/assembler/README.md` for the full CLI and a `ge_load_program`
harness snippet.
