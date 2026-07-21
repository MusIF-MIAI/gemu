# GE-120 micro-sequencer flow charts → gemu state map & fidelity audit

The CPU micro-sequences are documented as the **flow-chart foldout set drawing
14023130** (and the parallel **timing charts 14024137**) in
`CPU/GE 120 CENTRAL PROCESSOR [7].pdf` — Volume 7, the schematics binder (its
index, render-page 2, lists item 2 = Flow charts 14023130, item 3 = Timing
charts 14024137). Each foldout is one "sequence" sheet; the boxes are CPU
states (top-left label = the SO/SA state code in hex, e.g. `64 65`, `C8`, `80`),
the right column is the command code(s) issued (`COxx`/`CIxx`/`CExx`/`CUxx`),
and the bracketed text is the firing condition. gemu's `msl-states.c` charts are
a direct transcription of these (`struct msl_timing_chart` = {clock, command,
condition, additional}).

This file maps each flow-chart sheet to the gemu state(s) that implement it and
records the verification status. It was built by rendering CPU[7] with
`pdftoppm` (the OCR text layer does not contain the foldouts) and reading the
title blocks + box labels. Render-page numbers are 1-based pages of CPU[7].

## Sheet → state map

| CPU[7] render‑pg | Drawing | Sheet title | gemu state(s) | Status |
|---|---|---|---|---|
| 23 | 14023130   | FASE ALFA / ALPHA PHASE | `e2/e3`, `e0`, `e4`, `e6`, `e5`, `e7` (+ `ec/ed`, `ee/ef` indexing) | ✅ faithful — all rows are documented CO/CI/CE/CU commands; `e5`/`e7` and the indexing pair verified row-by-row against the p63-p64 timing tables |
| 24 | 14023130A  | DISPLAY SEQUENCE | `00` | ✅ verified row-by-row |
| 25 | 14023130B  | FORCING SEQUENCE | `08` | ◑ states match; a few bracket conditions need a higher-DPI/physical recheck |
| 26 | 14023130C  | INTERRUPTION | `F0`,`D2`,`D3`,`D0`,`D1` | ✅ implemented (PSR save → 0x0300) + test |
| 27 | 14023130D  | LPSR SEQUENCE | `C2`,`C3`,`C0`,`C1` | ✅ interrupt restore implemented; `0x9d` now enters `C2` through real `CU` routing with no beta commit |
| 28 | 14023130…  | JS1/JS2/JIE/JC/NOP2/HLT/INS/ENS/LON/LOFF/LOLL SEQUENCES | `beta-control` variant of `64/65` | ◑ isolated manual sheet; datapath rows retained from prior implementation |
| 29 | 14023130E  | JU‑JC‑JRT‑JS‑JE SEQUENCE | `beta-jrt` / control variants (`CI00s`, `verified_condition`, `JRT_LINK`) | ◑ isolated and validated; JRT executive path still hybrid |
| 30 | 14023130…  | LR‑AMR‑CMR‑SMR‑STR SEQUENCES | `beta-register` → `60/62` → `[50/52]` → `40/42` | ◑ manual state path wired; terminal datapath commit still hybrid |
| 31 | 14023130O  | NI‑XI‑OI‑TM SEQUENCES | `beta-immediate` → `60/62` → `50/52` → `40/42` | ✅ memory, knot, UA (`CI45/46/47/68`) and result rows transcribed |
| 32 | 14023130F  | PER‑PERI (preliminary phase) | `64/65`→`c8`→`d8/d9/da/db`→`dc`→`cc` | ✅ cluster verified row-by-row; only residual is the `PCOV` status stub |
| 33 | 14023130G  | TPER‑CPER external sequence | `ca`, `a8`, `a9`, `aa`, `ab` | ◑ states present; per-row needs higher-DPI recheck |
| 34 | 14023130₁  | CHANNEL‑**1** DATA TRANSFER phase | `b8`, `b9`, `ea`, `eb` | ◑ states present; write-back condition reworked (`L207_output_writeback`) |
| 35 | 14023130O  | CHANNEL‑**3** DATA TRANSFER phase | (channel‑3 `rSI` sub-states) | ✗ not modelled |
| 36 | 14023130₁  | CHANNEL‑**2** DATA TRANSFER phase | `rSI` sub-states `0C/0E` (in), `04/06` (compare), `02/03` (printer out, `CE16`), `0A/0B` (end print) | ◑ recovered (docs/peripherals.md "CAN2 data-transfer phase"); wiring is Phase 3/5 |
| 38 | 14023130…  | CMI‑CHI sequence | immediate-family variant and executive state pairs | ✅ CMI complement-add and qualitative-result rows transcribed; CMC remains in SS hybrid |
| 44‑45 | 14023130…| EXECUTIVE PHASE OP (data ops) | `64/65` (`EXEC_SS` + `alu_*`) | ◑ hybrid (SS executes in beta at TO65 like every other `EXEC_*` one-shot; per-clock executive states p93-p120 not transcribed) |


## Executive-phase timing-chart map (cp07 p80-136, dwg 14024137) — audit round 3

Complete sheet index, read from source (headers + exit boxes; fo. = pdf-page*2
-113/-112 for L/R, chained with no gaps fo.47-159). Peripheral chains p80-92:
PER-PERI C8>D8>D9>DA>DB(>D8 loop)>DC>CC, TPER-CPER CA>A8>A9>AA>AB>B8>EA>EB,
TPER data B9/B1/01/0C-0E/04-06/02-03/0A-0B. Instruction families p93-136, each
entering from beta 64|65 (SS-conversion roadmap; * = converted to per-clock):

| family | pages | topology |
|---|---|---|
| MVI*-MVC* | 93-94 | 64>60>40, loop {L1(2,1)=1i} |
| CMC*-CMI* | 94-96 | 64>60>50>40, loop + {dRO=0} |
| AP-SP | 96-101 | 64>20>60>50>40 loop; recomplement tail 22>26>A6-A7>A4-A5>A0-A1 |
| MVP | 101-103 | 64>20>60>40 loop |
| CMP | 103-105 | 64>20>60>50>40 loop |
| MP-DP | 106-118 | 64>20-21>28-29 loop>23; MP/DP bodies on 4-state codes 60-63/50-53/40-43 + 5B-59, 7A-7B, F2-F3, FA-FB, F8-F9 |
| PKS | 118-120 | 64>60-63>40-43 loop |
| UPKS | 120-122 | 64>60-63>40-43 loop |
| PK | 122-123 | 64>60-63>40-43 loop {+SA00} |
| UPK | 124-126 | 64>60-63>{50-53}>40-43 loop {SA00} |
| AB-SB-AD-SD-MVQ-CMQ* | 126-128 | 64>60>50>40 loop (converted; fo.140-143, three condition cells corrected at the gates) |
| XC-OC-NC* | 128-130 | 64>60>50>40 loop |
| EDT | 130-132 | 64>50>60>40 loop (50 first) |
| SR-SL (transcribed: docs/transcriptions/) | 132-134 | 64>60>50>40, exit to EA {(dRO=0)+(L1=1i)} (the EA/EB register write) |
| TR | 134-136 | 64>50>60>40 loop (50 first; last sheet fo.159) |
| LR-AMR-SMR-CMR-STR* | 75-76 | 64>60>(50)>40, X-bit two-pass (converted) |
| JRT*/LA* | 73-74 | 64>EA>EB (converted) |
| LPSR*/interruption* | 67-72 | 64>C2>C3>C0>C1 / E2>F0>D2>D3>D0>D1>C2.. (converted) |

Exit-loop conditions on the 40-group sheets are the {(L1_2,1=1i)} terminal
count (with overbar ambiguities noted in the round-3 survey); the X-bit pass
encoding used for the register family generalizes only to 2-pass ops — the
byte-loop families (MVC etc.) need the L1 terminal count, i.e. the CI41/CI42
counter-init decode, before conversion.

(Render-pages 28/30 drawing-suffix letters were not legible at 300 DPI; the
render-page is authoritative. Beyond render-45 the set continues with the
remaining data-op sequences — PKS at render-46, etc.)

## Fidelity audit (per state, evidence-based)

**Legend:** ✅ verified match · ◑ functionally correct but not a per-clock
transcription, or partially transcribed · ⚠️ flagged for recheck · ✗ absent.

### Alpha family — ✅ faithful
- **`e2/e3`, `e0`, `e4`, `e6`, `e5`, `e7`** (sheet 23): direct transcription;
  corroborated by the operand-fetch analysis and `tests/alpha.c`. The E4/E5
  symmetry (`not_RO07` gating CI60) is the key fix that makes the modifier
  reach V1; verified.
- **`ec/ed` + `ee/ef`** (modified-address indexing): per-clock transcription of
  the timing tables CPU[7] **p64** (`ED-EC` fo.15, `EF-EE` fo.16): CO18 +
  CO97..CO90 force the change-register byte address `1111 nnn b` (= 240/241+2N)
  onto NO, MEM→RO reads it from the shadow RAM, and the UA byte-adder
  (CI69 low + CI68 high, carry through URPE, reset by CO49) accumulates
  `V2 + cr[N]` into V2 (CI02), with CI01 `{SA00}` copying to V1 for the first
  operand. The `<SA00>` diamond is SA **bit 0** itself: E6 routes operand 1 to
  `ED` (bit 0 set via its unconditional CU00), E7 routes operand 2 to `EC`
  (bit 0 cleared by CU10 = DI64A0). Every row is a documented CXXX command;
  4 unit tests in `tests/exec.c` + the deck + `cc` stay green. Residuals:
  the DI13/DI64/DI65/DI66/DI67 gate equations are derived from the state codes
  (names confirmed in the CPU[6] signal index, printed equations not located),
  and CI68 masks BO bits 15-12 from the high-byte add (fetch residue in the
  operand register's top quartet; hardware exclusion mechanism not yet found —
  recheck the UA pages).
- **`00` DISPLAY** (sheet 24): verified row-by-row — every CO/CI command and
  console-selector (`AFxx`) condition matches. One scan artifact: the chart's
  `V3 → BO` row reads `[AF36]`, which is `[AF30]` (= `RS_V3`); gemu uses `AF30`.

### Forcing — ◑/⚠️
- **`08` FORCING** (sheet 25): the command set (`CO11/CO41/CO01`, `CO30/CO31`,
  `CI20/CI33`, `CI04/02/05/01/00/08/07/03/06/09`, `CI70..CI76` set-FIxx,
  `CU00..CU17` from `RO`) matches the chart structure and `tests/forcing.c`
  passes. ⚠️ The exact firing brackets on three rows (the `Mem→RO`/`AM→RO`
  forcing reads: `CO30`, `CO31`, `CI20`/`CI33`) could not be aligned with
  confidence at 240–300 DPI — `CO30 [AF51]`/`CO31 [AF41]` in gemu vs. what looks
  like `CO30 [—]`/`CO31 [AF51]` on the foldout. **Recheck against the physical
  foldout or a higher-quality scan before changing** — `forcing.c` currently
  passes, so the live behaviour is constrained.

### Beta execution — ◑ manual-sheet dispatcher; datapath transcription ongoing
The old interleaved `state_64_65[]` has been replaced by a sparse timing-chart
matrix. Each instruction family selects a separate chart bearing its CPU[7]
sheet reference (`beta-control`, `beta-jrt`, `beta-la`, `beta-lpsr`,
`beta-register`, `beta-immediate`, `beta-per`, or `beta-ss`). Repeated rows are
deliberately kept in each array so the source can be reviewed directly beside
the printed sheet. `tests/msl_dispatch.c` locks the decode mapping and gives
undocumented codes a visible compatibility chart rather than an accidental
fall-through through unrelated rows.

Register and logical-immediate operations traverse the documented executive
state pairs. NI/XI/OI/TM use `64|65 → 60|62 → 50|52 → 40|42 → E2|E3`;
LR/STR and MVI take their documented `50|52` bypasses. State-path tests lock
these routes.

The logical UA combinations are now implemented from sheets 41–44:
`CI45+CI46` selects AND, `CI45+CI47` XOR, and all three select OR; `CI68`
admits the result through NI before the ordinary memory-write cycle. MVI uses
the same knots and memory cycle without a UA operation. The remaining
executive fidelity gap is register-address generation plus the word-arithmetic
gates for AMR/SMR/CMR. Their terminal commits remain explicit. SS/decimal
sheets still commit in beta pending their multi-cycle transcription.

### PER‑PERI preliminary phase — ✅ routing / ◑ status decode
- **`64/65`→`c8`→`d8`→`d9`→`da`→`db`→⟨!FA05·!FA04⟩→`dc`→`cc`** and
  **`80`→⟨AINI⟩→`c8` | `e2/e3`** (sheet 32): the full state graph and its
  diamonds are verified by tracing the `CUxx` future-state bit arithmetic
  (e.g. `0x80`+CU06+CU03→`0xC8`; `0x64`+CU07+CU15+CU03+CU12+CU10→`0xC8`;
  `0xC8`+CU04→`0xD8`; `0xDC`+CU14→`0xCC`). `tests/initial-load.c` locks the
  per-state register values along this path.
- ✅ The boxes `c8`, `d8`, `d9`, `da`, `db`, `dc`, `cc` were verified **row-by-row**
  against high-quality crops of sheet 32: every CO/CI/CE command row is present
  and the conditions match. Specifics confirmed:
  - `d8`: `PO-1→PO [FA05·‾DU93]` = `CO10/CO40/CO41/CO00` (CO00 gated `FA05·!DU93`);
    `CE02 [‾FA05·‾FA04]`; `L12,1→RE` = `CI15/CI33/CE01`. ✅
  - `cc`: `Set FI05 [PCOV·DU96·‾DU95 + FA00]` = `state_cc_TI06_CI75` **exactly**;
    `RO→RA [‾PUC3]` = `CE00 [!PUC3]`. The CC `Mem→RO [‾AINI]` overbar is legible
    here, confirming the `c8` `Mem→RO` bracket is also `‾AINI` (gemu `not_AINI`).
  - `dc`: the manual's explicit channel-2 SI build `Set S102,03 / Res S100,01
    [L200·‾FA05]` (CU02/03/10/11) is **omitted** in gemu, but is **functionally
    equivalent**: the base state `0xDC` already carries low-nibble `1100`, so
    `CU20` forces the correct `SI = 0xC` without them. Defensive-only; not a bug.
- ◑ The CC exit fork ⟨FA05⟩/⟨FA00⟩/⟨DU96⟩/⟨DU95⟩ → alpha / `CA` (TPER-CPER) /
  `EA` (SPER) is implemented via `CUxx` logic on the same signals, but the
  destinations depend on `DU95`/`DU96`/`PCOV`, and **`PCOV` is stubbed to 1**
  (`signals.h`) — so the EPER/SPER branch is only as faithful as the (unmodelled)
  channel peripheral status. **This is the one real residual gap in the cluster.**
- Two earlier flagged "discrepancies" were checked and are **not** bugs:
  `state_80` `CO96` uses `!FUL3` (matches the observed LOAD-2 → connector-3 name
  `0x00`, locked by `tests/initial-load.c load_2_button`); `c8` `Mem→RO` uses
  `not_AINI` (the PER path reads the descriptor; the chart bracket `[AINI]` is
  read as `[‾AINI]` with the overbar lost in the scan); and the `db` diamond
  proceeds to `DC` on `!FA05·!FA04` TRUE (the operationally-correct
  busy-wait-exit polarity).

### TPER‑CPER & data transfer — ◑ partial (peripheral)
- **`ca`, `a8`, `a9`, `aa`, `ab`** (sheet 33) and **`b8`, `b9`, `ea`, `eb`**
  (sheet 34): state coverage confirmed against the box labels. These carry
  in-code `TODO`/"missing manual page" notes and load-bearing stubs
  (`DI82A`/`DI83A` return constants; `RENIA`/`RILIA`/`PCOV` hardcoded), so they
  are the least cycle-faithful. They are constrained by `tests/initial-load.c`
  and the deck bootstrap, so changes are risky — recheck against the physical
  foldouts before editing.

### Interruption + LPSR — ✅ implemented this session
- Sheets 26/27. The `INTE = RINT·/MASC` branch in `e2/e3` already routed to
  `0xF0` (`CU04`), but `F0` and the chain were empty slots. Added states
  `F0→D2→D3→D0→D1→C2→C3→C0→C1→alpha` (hybrid `INT_*` commands): F0–D1 **save**
  the PSR (status byte `b5←FA04,b4←FA05,b0←FA06`; then `0`; then PO hi/lo) to the
  fixed store **0x0300**; C2–C1 are the **LPSR** load of the new PSR from
  **0x0304**, vectoring to its PO (the handler) and restoring `FI04/05/06`.
  `RINT` is acknowledged (cleared) in F0. Validated by `tests/interrupt.c`
  (`save_and_vector`, `masked_does_not_divert`). The path is dormant for the
  deck/other tests (`RINT` is never asserted there).

## Still not implemented
- **LPSR as an instruction** (`0x9d`): the interrupt *uses* the LPSR load states
  (C2–C1), but the `LPSR 0x9d` opcode is recognized and not decoded to enter
  them from beta.

## Authoritative per-clock timing tables — CPU[7] pages 61+

The flow-chart **foldouts** (sheets above, render-pg ~23-46) give the state graph
and register transfers but are low-contrast and hard to read precisely. CPU[7]
**pages 61 onward** carry the per-state **timing tables** — one (or two) per
page, **rotated 90°** but with **clean OCR** — and these are the authoritative
per-clock source that `struct msl_timing_chart` mirrors directly. Columns:

| Clock | Comando (command) | Equazione (firing equation) | Pin (board/loc) | Evento–Comment (register transfer) |

The *Equazione* column is the gold for fidelity: e.g. `CO1011 = …` / `(DI12A0)`
maps to gemu's `{clock, command, condition, additional}`. Read them upright with:
```sh
pdftoppm -png -r 320 -f <pg> -l <pg> "CPU/GE 120 CENTRAL PROCESSOR [7].pdf" t
convert t-0NN.png -rotate -90 +repage page.png      # -90 = upright
```
The state-code box at the table head is the SO/SA hex (e.g. `1110 0000` = `0xE0`).

**Verified against these tables this session** (full Clock↔Comando↔Equazione↔
Evento rows):
- **`state_E0`** (p61 top, code `1110 0000`): **1:1 match** — `V2→NO, COUNT FROM
  00, MEM→RO, NI→PO, NO→BO, RO1→NI1, RO2→NI2, RES AVER, NI→L1, Set S002, Reset
  S007` = `CO12/CO41/CO30/CO00/(TO20 BO)/CI67/CI62/CI39/CI05/CU02/CU17`, same
  order, `DI17A0`/`DI12A0` gates matching, routing cond `{FO06+FO07}` = `CU17`.
- **`state_E2_E3`** (p61 bottom): matched except the table lists **four** FI
  resets at TI06 (`CI80 CI81 CI82 CI83` = Reset FI00/01/02/03) and gemu was
  missing **`CI81` (Reset FI01)**. **Fixed** — added `{TI06, CI81, 0}` (FI01 is
  set only by forcing/b1, so this is a no-op for normal flow; deck + 244 tests
  stay green). Other gates confirmed: `CI82 = EC50A0 {dRO=PER}`, `CU04 = EC53A0
  {RINT·/FA06}`, `CU11 = (DI18A0)`, `CI89 = EC51A0 {dRO=HLT + ASIN(...)}` (gemu
  models the `dRO=HLT` term only).
- **`state_E4`** (p62): the high-byte top-quartet gate `CI60 = EC54A0` (an
  RO7-based decode) and `CI65 = (DI19A0)` confirm the bit-15 operand-fetch fix —
  gemu's `not_RO07` on CI60 captures EC54A0's discriminating term, symmetric with
  E5. ✅
- **`state_E5`** (p63 bottom, code `1110 0101`, DA-FROM E6): **1:1 match** with
  the gemu chart (CO10/CO41/CO30/CO00/CI67/CI62/CI65/CI60`{/R007}`/CI02/CI06/
  CU01), exit box `E7`. ✅
- **`state_E7`** (p63 top, code `1110 0111`, DA-FROM E5): verified row-by-row
  (transcribed in full in the msl-states.c comment). Two fixes fell out:
  `CI12 = DI20A0` / `CI62 (DI12A0)` gates added, and the **routing** was wrong —
  the table's `CU17 = EC57A0 {/L207}` exits to *beta* on an absolute second
  operand (the old condition had the polarity inverted) and `CU10 = DI64A0`
  clears SA bit 0 so a modified second operand enters `EC`. The exit box
  (`64+65 {/L207}` / `ED+EC {L207}`) requires a bit-1 reset the printed rows
  lack; a `CU11` is added — the same manual CU10/CU11 ambiguity already noted
  in `state_E6`. The `EXEC_SS`/`SS_TO_ALPHA`/`INDEX_*` future-state-forcing
  pseudo-commands this state used to carry are **gone**: SS ops now reach beta
  through the documented CU rows and execute there (`EXEC_SS` at TO65).
- **`DI201` polarity** (decode behind `EC56A0`, the E6/E7 modified-address
  CU03 gate): was transcribed as `DI201 = DI20A`, making EC56A0 constant-0 —
  the reason the indexing entry previously needed future-state forcing. Fixed
  to `DI201 = /DI20A` per the xxA/xx1 naming convention used by every sibling
  signal.
- **`CO18`** ("forcing in NO21"): assigned the enum constant to the forcings
  *value* instead of setting `force_mode`, so every CO18+CO9x NO-address build
  in the machine was inert. Fixed; the full suite (deck bootstrap, PER/PERI,
  channel transfers) stays green with the forcing active.
- **Beta phase structure** (audit round-2, cp07 p70-p79 read from source): the
  64|65 state is SEVEN per-instruction-family timing sheets with different
  exits — jump/control family (JS1-JS2-JIE-JC-NOP2-HLT-INS-ENS-LON-LOFF-LOLL)
  -> `E2` via `CU01/CU07 = CM01A0 (DE00A0)`; LPSR -> `C2`; JRT and LA -> `EA`;
  LR-AMR-SMR-CMR-STR and NI-XI-OI-TM -> `60+62` (the per-clock executive
  states, still untranscribed); PER-PERI -> `C8`. `CU10/CU12 = DI06A0` are
  unconditional in beta on every sheet. The CU01 combiner (cp06 ch.219-1) is
  an 8-input wired-OR (CM01A, CM02A, DE53A, ED35A, ED10A, ED84A, ED66A,
  ED50A). gemu's one-shot hybrid returns every non-PER op to `E2` directly
  (the `not_per_peri` CU01 gate) — the correct abstraction while 60/62/EA/C2
  executive recipes are not modeled. `DE00A = NAND(DO011, DI062)` verified on
  cp06 ch.248-9; `CI38`'s real home is E6/E7 TO80 with gate `DE51A =
  NAND(DO011, DI201)` (ch.261-8) — no CI38 row exists on any beta sheet.
**Caveat:** the OCR renders `CO1x` as `CO1O` (letter O), so plain token-diffs
miss CO-family commands — cross-check the rendered image when auditing E5/E6/E7
and the beta/peripheral tables. **Next:** walk pages 61+ row-by-row for the
remaining states (E5/E6/E7, beta jumps/data, forcing 08, the peripheral
cluster), image-confirming each like CI81.

## How to re-verify a sheet
```sh
P="CPU/GE 120 CENTRAL PROCESSOR [7].pdf"
pdftoppm -png -r 300 -f <render-pg> -l <render-pg> "$P" sheet
# then crop+enlarge a region, e.g. the command box:
convert sheet-0NN.png -crop WxH+X+Y +repage -resize 200% box.png
```
The scan in CPU[7] is a low-contrast foldout; bracket conditions and diamond
labels often need a higher-quality scan or the physical foldout to read with
confidence (consistent with the project's OCR page-image rule).
