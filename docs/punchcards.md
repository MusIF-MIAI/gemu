# GE-120 Punched-Card Formats — Decoding Notes

How the `.cap` card-image captures are structured, and the four hole-pattern →
byte conversions used to read GE-120 decks. The **column-binary ("by-pass")**
conversion is the one this project *cracked* empirically; the others are
documented here alongside it because they share the same on-card geometry.

> Scope: this is a reverse-engineering findings document. Verified claims are
> backed by the Burroughs reader firmware (ground truth), the `gemu` transcoder,
> and direct decode of `software/DUMP1/funktionalcpu.cap`. OCR/uncertain items
> are flagged. Confidence labels: **high / medium / low**.

---

## 0. Source of truth

The capture hardware is the project's own Raspberry-Pi-Pico reader; its firmware
**defines** the `.cap` format, so it is the authority for every bit position:

- `software/burroughs-card-reader/main.c` — sampling + dump format.
- `software/gemu/cap.c`, `cap.h` — the `.cap` parser used by the emulator.
- `software/gemu/transcode.c`, `transcode.h` — the four conversions.
- `software/DUMP1/funktionalcpu.cap` / `.bin` — the oracle pair.

Everything below that says "verified" was checked against one of these.

---

## 1. The physical card and its 12 rows

A standard 80-column Hollerith card has **12 punch rows** per column. Top to
bottom they are:

```
   row 12   (Y zone / "12-punch")
   row 11   (X zone / "11-punch")
   row  0   (zone-0, a.k.a. the "10-punch" / "ten" row)   <-- see §3
   row  1
   row  2
   row  3
   row  4
   row  5
   row  6
   row  7
   row  8
   row  9
```

A column is the set of rows punched in that vertical slice. The reader samples
all 12 rows of a column simultaneously into one value.

---

## 2. The `.cap` file format (verified, high)

A `.cap` file is **plain ASCII text** produced by the reader's USB console. It
contains **two independent dumps of the same deck**, selected by which key the
operator pressed during capture (`main.c:191`/`:213`):

1. **Hex dump** (key `a`/`A`): for each card, a header line `Card n. N`
   followed by **80 four-hex-digit tokens**, one per column —
   the raw 13-bit sample value (`main.c:217`, `printf("%04X ", …)`).
2. **ASCII hole-art** (key space): for each card, `Card n. N` followed by
   **12 rows of 80 characters**, `*` = hole, `_` = no hole, printed in row
   order **12, 11, 0, 1, 2 … 9** (`main.c:194-207`).

So a deck of *C* cards yields **2·C** `Card n.` headers. For `funktionalcpu.cap`:
228 headers = **114 hex cards + 114 ASCII cards** (verified by count). The
emulator's parser (`cap.c`) keys on the literal `Card n. ` prefix, collects only
strict 4-hex-digit tokens, ignores everything else (counters, `FEED ON/OFF`,
the hole-art), and masks each value to 13 bits.

### 2.1 The column value: which bit is which row (verified, high)

Each sample is `gpio_get_all() & BITMASK`, where **`BITMASK = 0x1BFF`**
(`main.c:14`). In the 13-bit value:

| cap value bit | card row | notes |
|---|---|---|
| 0 | row 0 | the "0 / ten" zone row |
| 1 … 9 | rows 1 … 9 | digit rows |
| **10** | — | **always 0 — GPIO 10 is not wired** (`main.c:141`, `BITMASK` clears `0x400`) |
| 11 | row 11 | X zone |
| 12 | row 12 | Y zone |

**The bit-10 hole is the crux.** The reader's data pins are GPIO 0–12 *except 10*
(deliberately skipped in init and masked out of every sample). So a cap value's
bit *b* equals card row *b* for `b ∈ {0–9, 11, 12}`, and bit 10 is structurally
absent. Miss this and every high-nibble decode is shifted.

---

## 3. The "0 is actually 10" insight

The single fact that unblocked the column-binary decode:

> **What the firmware labels "row 0 / bit 0" is the punch-card "ten" row** — the
> zero-zone row that sits *third from the top* (12, 11, **0**, 1…9), not a digit
> position below row 9.

Two consequences followed:

1. The byte's **most-significant bit comes from row 0** (the "ten" row at the top
   of the digit stack), and bits descend through rows 1, 2, 3 … — i.e. the row
   order that *reads* as MSB→LSB is `0,1,2,3, …`, the opposite of treating row 9
   as the high bit.
2. The **skipped GPIO 10** (§2.1) sits exactly between the digit rows (0–9) and
   the zone rows (11, 12), so the byte's two nibbles are sourced from two
   *separated* groups of rows with a gap — which is why a naive contiguous
   bit-grab produced garbage.

Combined, these gave the **B2R row map** in §4.3. Confidence: high — the resulting
decode produces valid, contiguously-loadable GE-120 code (§5).

---

## 4. The four conversions (`transcode_column`)

The same column geometry feeds four different read modes. Mode is chosen by the
reader command / deck type, not by the card.

| Mode | enum | Use | Output rule |
|---|---|---|---|
| Normal (Hollerith→GE) | `TC_NORMAL` | text / loader cards | 13-bit lookup table |
| Binary passthrough | `TC_BINARY` | raw low-8-bit reads | `column & 0xFF` |
| Loader hex-nibble | `TC_HEX` | the bootstrap loader listing | nibble = Σ set digit-row indices |
| **Column-binary / by-pass** | `TC_COLBIN` | **program (data) cards** | byte bit *i* ← row `B2R[i]` |

### 4.1 `TC_NORMAL` — Hollerith→GE table (verified, high)

An empirically derived 13-bit→byte table (`transcode.c`, 8192 entries). Built by
aligning all 9120 `(card,column)` pairs of the 114 hex cards against the 9120
bytes of `funktionalcpu.bin`: **267 distinct inputs, 0 collisions, 18 non-space
outputs**; everything else defaults to `0x20` (GE space / blank column). The
observed digit mappings (single-row punches) are:

```
row0→0xF0 '0'  row1→0xF1 '1'  row2→0xF2 '2'  row3→0xF3 '3'  row4→0xF4 '4'
row5→0xF5 '5'  row6→0xF6 '6'  row7→0xF7 '7'  row8→0xF8 '8'  row9→0xF9 '9'
```

plus a handful of zone+digit combos (`<`, `@`, `#`, `X`, `Y`, `,`, `+`, …). This
is the GE machine character set; `funktionalcpu.cap` → `funktionalcpu.bin` is an
exact byte match, which is the table's acceptance test (`tests/transcode.c`).

### 4.2 `TC_HEX` — loader hex-nibble (verified, medium)

The bootstrap loader is punched as hex digits where **a nibble equals the sum of
the set digit-row indices** (rows 0–9; zone rows ignored), and **two columns are
nibble-packed into one byte** (`transcode.c:562`). E.g. row6+row8 → 6+8 = 14 =
`0xE`. Verified against the loader listing (38 of 40 bytes; the 2 differences are
OCR noise in a trailing descriptor) — confidence medium on those 2 bytes.

### 4.3 `TC_COLBIN` — column-binary "by-pass" (the cracked one) (verified, high)

**One column → one 8-bit byte, NOT nibble-packed.** Each output bit is taken from
a specific row:

```
byte bit i  ←  card row B2R[i],   B2R = { 9, 8, 7, 6,  3, 2, 1, 0 }
                                          \--low nib--/ \--high nib--/
   bit0←row9  bit1←row8  bit2←row7  bit3←row6     (low nibble, bits 0–3)
   bit4←row3  bit5←row2  bit6←row1  bit7←row0     (high nibble, bits 4–7)

   rows 4, 5, 11, 12  are UNUSED.   row 0 is the MSB (bit 7).
```

So the digit rows split into two nibbles around the unused middle rows 4/5, and
the zone rows 11/12 carry no data in this mode.

**Worked example** — the same column value `0x0202` (rows 9 and 1 punched) under
each mode:

| Mode | result | reasoning |
|---|---|---|
| `TC_COLBIN` | `0x41` | bit0←row9=1, bit6←row1=1 → `0100 0001` |
| `TC_NORMAL` | `0x20` | `0x0202` not in the observed table → space |
| `TC_BINARY` | `0x02` | low 8 bits of `0x0202` |

The mode you pick changes the byte completely — which is why the program payload
read as nonsense under `TC_NORMAL`.

---

## 5. Program-card on-card layout (verified empirically, high)

Decoding the 114 hex cards of `funktionalcpu.cap` with `TC_COLBIN` reveals a
clean, **self-describing data-card format**. Cards **6–111** are the program
payload; each is:

```
 col:  0 1 2 3 4 5 6 7 | 8  | 9  10 | 11 ............... 76 | 77 78 | 79
       └── ID prefix ──┘  LL  └addr─┘  └──── 66 data bytes ──┘         seq
```

| Field | Cols | Value (verified) | Meaning |
|---|---|---|---|
| Identifier prefix | 0–7 | constant `00 04 40 00 20 40 40 42` | card-type tag (same on every data card) |
| `LL` | 8 | `0x41` (= 65) | payload length − 1 → **66 payload bytes** (`LL+1`) |
| Load address | 9–10 | big-endian | absolute target address of byte 0 of payload |
| Payload | 11–76 | 66 bytes | the program image fragment |
| Sequence/check | 79 | rotating (`40,20,10,08,04,02,01,00…`) | per-card sequence or check byte (meaning unconfirmed) |

**Contiguous, self-describing load (verified):** the load addresses step by
exactly **66 (`0x42`)** per card — `0x0100, 0x0142, 0x0184, 0x01C6, …` — covering
**`0x0100` … `0x1C54`** with no gaps. Combined with an entry jump `JU 0x0100`, the
deck boots into the program at `0x0100`.

**Order-independence (verified, notable):** cards 107–108 are physically *out of
sequence* in the capture (their addresses `0x18FA`, `0x193C` belong between cards
98 and 99), yet because each card carries its own load address the image is still
assembled correctly. The deck can be shuffled without corrupting the load — a
deliberate robustness property of the format.

### 5.1 Control / non-payload cards (medium)

A few cards are **not** 66-byte payload cards (different `LL`); they appear to be
loader/patch/terminator records. Roles are inferred, not confirmed:

| Card | `LL` | Addr | Note |
|---|---|---|---|
| 1–5 | mixed | `0404`, `4010`, `0A00`, `4001`, `0A02` | loader / patch / control (purpose TBD) |
| 112 | `00` | **`0E00`** | all-zero payload at **`0x0E00`** — the console test-option byte (explains the idle-HLT: no test selected) |
| 113 | `07` | `0000` | short record near address 0 (entry/jump area) |
| 114 | `20` | `1040` | trailing record |

The card-112 → `0x0E00` correspondence is consistent with the observed run
halting in the "no test selected" branch when that byte is zero.

---

## 6. Verification summary

| Claim | How verified | Confidence |
|---|---|---|
| `.cap` = 2 dumps (hex + hole-art), 228 = 114+114 | header count + `main.c` dump code | high |
| cap bit *b* = row *b*; bit 10 absent | `BITMASK 0x1BFF`, GPIO init skips 10 | high |
| `TC_NORMAL` table | exact match `funktionalcpu.cap`→`.bin` (9120 B) | high |
| `TC_COLBIN` B2R map | decodes to valid GE code; program runs to HLT | high |
| `TC_HEX` nibble-sum | loader listing match (38/40 bytes) | medium |
| Data-card layout (LL/addr/66 B/prefix) | direct decode of 106 cards, addresses step 66 | high |
| col-79 meaning | pattern observed, semantics not pinned | low |
| Control-card roles (1–5, 112–114) | addresses decoded, purpose inferred | medium |

---

## 7. Open questions

- **col 79** — rotating value `40,20,10,08,04,02,01,00…`; is it a sequence number,
  a checksum, or a column-sync mark? (low)
- **cap bit 15 / zone rows in COLBIN** — rows 11/12 are unused by `TC_COLBIN`; do
  any GE program decks use them (e.g. for a sign/flag)? (open)
- **Control cards 1–5** — exact loader/patch semantics, and why card 1 decodes a
  66-byte block at `0x0404` overlapping the main image. (medium)
- **`TC_HEX` 2-byte discrepancy** — confirm whether OCR noise or a real
  end-of-record descriptor. (medium)

---

## 8. Practical recipe (reading a GE deck from `.cap`)

1. Parse with `cap.c` → per-card arrays of 80 × 13-bit columns.
2. Choose the mode for the deck/section: **loader** → `TC_HEX`; **text** →
   `TC_NORMAL`; **program payload** → `TC_COLBIN`.
3. For payload cards: byte 8 = `LL`, bytes 9–10 = load address, bytes 11..(11+LL)
   = `LL+1` payload bytes; write them to `mem[address …]`.
4. Ignore physical card order — trust each card's self-described address.
5. After loading, enter at `0x0100` (or the deck's entry jump).

---

*Findings derived from the project's own reader firmware and the `gemu`
transcoder, cross-checked against `funktionalcpu.cap`/`.bin`. The column-binary
(B2R) decode was solved 2026-05-28; the program-card layout was re-verified by
direct decode for this document.*
