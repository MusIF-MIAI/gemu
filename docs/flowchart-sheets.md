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
| 23 | 14023130   | FASE ALFA / ALPHA PHASE | `e2/e3`, `e0`, `e4`, `e6`, `e5`, `e7` (+ `ec/ed`, `ee/ef` indexing) | ✅ faithful (alpha + modified-index micro-cycle) |
| 24 | 14023130A  | DISPLAY SEQUENCE | `00` | ✅ verified row-by-row |
| 25 | 14023130B  | FORCING SEQUENCE | `08` | ◑ states match; a few bracket conditions need a higher-DPI/physical recheck |
| 26 | 14023130C  | INTERRUPTION | `F0` | ✗ **not implemented** (slot empty in `msl_timings`) |
| 27 | 14023130D  | LPSR SEQUENCE | (LPSR `0x9d`) | ✗ not implemented (opcode recognized, no decode) |
| 28 | 14023130…  | JS1/JS2/JIE/JC/NOP2/HLT/INS/ENS/LON/LOFF/LOLL SEQUENCES | `64/65` (`jc_js1_js2_jie`, `nop`, `lon_loll`, `ins`, `ens`, `loff`, HLT) | ◑ hybrid (functionally wired, not per-clock) |
| 29 | 14023130E  | JU‑JC‑JRT‑JS‑JE SEQUENCE | `64/65` jump path (`CI00s`, `verified_condition`, `JRT_LINK`) | ◑ hybrid (validated by `tests/exec.c` jumps + JRT) |
| 30 | 14023130…  | LR‑AMR‑CMR‑SMR‑STR SEQUENCES | `64/65` (`EXEC_LR/STR/CMR/AMR/SMR`) | ◑ hybrid (validated by `tests/exec.c` reg ops) |
| 31 | 14023130O  | NI‑XI‑OI‑TM SEQUENCES | `64/65` (`EXEC_NI/XI/TM`, + `MVI/CI/CMI`) | ◑ hybrid |
| 32 | 14023130F  | PER‑PERI (preliminary phase) | `64/65`→`c8`→`d8/d9/da/db`→`dc`→`cc` | ✅ cluster verified row-by-row; only residual is the `PCOV` status stub |
| 33 | 14023130G  | TPER‑CPER external sequence | `ca`, `a8`, `a9`, `aa`, `ab` | ◑ states present; per-row needs higher-DPI recheck |
| 34 | 14023130H  | CHANNEL‑1 DATA TRANSFER phase | `b8`, `b9`, `ea`, `eb` | ◑ states present; write-back condition reworked (`L207_output_writeback`) |
| 35‑37 | 14023130…| CHANNEL‑2 DATA TRANSFER phase | `b1` (+ channel‑2 sub-states) | ◑ partial |
| 38 | 14023130…  | CMI‑CHI sequence | `64/65` (`EXEC_CMI`/`EXEC_CI`) | ◑ hybrid |
| 44‑45 | 14023130…| EXECUTIVE PHASE OP (data ops) | `64/65` (`EXEC_SS` + `alu_*`) | ◑ hybrid (SS executed by Mechanism B in `e7`/`ef`) |

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
- **`ec/ed` + `ee/ef`** (modified-address indexing): a hybrid fusion of the
  sheet-23 `ED|EC`/`EF|EE` boxes (one C-add instead of bit-serial low+high
  passes). Faithful in result and control flow; 4 unit tests in `tests/exec.c`.
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

### Beta execution — ◑ hybrid by design (the main fidelity gap)
Sheets **28, 29, 30, 31, 38, 44, 45** are the per-clock recipes for the beta
phase (jumps, JS/JC/NOP/HLT/INS/ENS/LON/LOFF/LOLL, register ops, immediate
logic ops, compares, and the SS data ops). gemu implements all of these in
`state_64_65` (and `EXEC_SS` for SS) via the **hybrid mechanism**: the MSL drives
fetch/decode/sequence and routing, but the actual operation is performed once by
a pure `alu_*` helper rather than transcribing the chart's per-clock datapath
micro-steps. This is functionally correct and validated end-to-end
(`tests/exec.c`, the `funktionalcpu` deck to 0x175a, `cc/test.sh`), **but it is
not cycle-accurate** to these sheets.
> **Roadmap to close it:** transcribe sheets 28/29/30/31/38/44/45 box-by-box
> into proper `state_64_65`-family charts (or per-opcode beta states), replacing
> the `EXEC_*` one-shots with the charted CO/CI/CU steps. Large, must stay green
> against `exec.c` + the deck + `cc`. Lower priority than correctness, since the
> observable result already matches.

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

## Not implemented (flow-chart sheets with no gemu state)
- **Interruption** (sheet 26, state `F0`): the `INTE = RINT·/MASC` branch out of
  alpha exists in `e2/e3`, but state `F0` itself is an empty slot — interrupts
  are not serviced.
- **LPSR** (sheet 27, `0x9d`): opcode recognized, not decoded.

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
