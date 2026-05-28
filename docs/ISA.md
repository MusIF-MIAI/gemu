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
| **AB** | `FE` | Add Binary: A1 ← A1 + A2 (op2 truncated/zero-extended to op1 length). | set | ◑ |
| **SB** | `FF` | Subtract Binary: A1 ← A1 − A2; negative stored in two's-complement. | set | ◑ |

> ◑ `alu_ab`/`alu_sb` are implemented and unit-tested (`alu_bin.c`) but **not in
> the `EXEC_SS` dispatch** yet — they will not execute end-to-end until wired.

### 6.7 Unpacked (zoned) decimal arithmetic — SS format (6 bytes)

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **AD** | `FA` | Add Decimal (zoned, unsigned; zones ignored; carry-out dropped). | set | ◑ |
| **SD** | `FB` | Subtract Decimal (zoned; wraps mod 10^L1 on underflow). | set | ◑ |

> ◑ Implemented in `alu_bin.c`; not yet wired. **CC tables for AD/SD are
> OCR-inferred** (`alu_bin.h:34`) — medium confidence; re-check page images.

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
| **MVQ** | `F8` | Move Quartets: copy digit nibbles (low 4 bits) A2→A1, preserve dest zones; CC 0=zero,1=nonzero. | set | ◑ |
| **CMQ** | `F9` | Compare Quartets: compare digit nibbles only; CC 1/2/3. | set | ◑ |
| **SR** | `D9` | Search Right: scan A-field L→R for model byte; result address → register 7. | — | ◑ |
| **SL** | `DB` | Search Left: scan A-field R→L for model byte; result address → register 7. | — | ◑ |

> ◑ All four exist in `alu_reg.c` with unit tests but are **absent from
> `EXEC_SS`/`is_ss_data_op`** — not yet dispatched. `MVQ` zone handling carries an
> OCR uncertainty (`alu_reg.h:266`).

### 6.11 Peripheral / I-O & status — PM format

| Mn | Op | Summary | CC | St |
|----|----|---------|----|----|
| **PER** | `9E` | Peripheral / external operation (issue command to a channel). | via status | ✅ |
| **PERI** | `9C` | Peripheral operation, interrupt variant. | via status | ✅ |
| **RDC** | `90` | Read Card (peripheral read, decimal-deck variant; PER-family). | via status | ✅ |
| **LPSR** | `9D` | Load Program Status Register. | — | ✗ |

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

| Item | Issue | Suggested check |
|---|---|---|
| Address bit 15 | masked off; role unknown (sign? indirect?) | CPU addressing pages / schematics |
| ENS/INS/LON/LOFF/LOLL | sub-function meanings unverified | interrupt + console-indicator manual pages |
| JRT (`0x41`) | opcode assigned, no decode | branch/linkage section of manual |
| LPSR (`0x9D`) | opcode assigned, no decode | PSW / status-register section |
| CI vs CMI | both map to `alu_ci`; signed-vs-logical distinction? | §5.5.5.1 page image |
| JU/JCC mask source | shared mask path with JC | §fo.56/57 page image |
| AD/SD CC tables | OCR-inferred | §5.5.1.1 / §5.5.1.2 page images |
| MVQ zones | "zones not processed" interpretation | §3.084/3.098 + hardware trace |
| AB/SB/AD/SD/MVQ/CMQ/SR/SL | ALU done, not wired into decode | finish `EXEC_SS`/`is_ss_data_op` wiring |

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
