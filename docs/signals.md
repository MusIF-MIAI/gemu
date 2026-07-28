# GE-120 signal reference

Global table of the hardware signals the emulator models (or is being driven to
model), what each represents, what it is used for, and the side-effects /
conditions it activates.

This document is the authoritative reference for the **pin-by-pin, signal-level**
reader↔CPU effort: the goal is to connect the card reader to the CPU channel
*signal by signal* (the COCA connector), drive each line, and have the emulator
react to it exactly as the hardware would. New signals are added here as they are
implemented; each entry states its **direction**, **type**, **meaning**, and the
**effect/condition** it drives in the model.

Status legend: ✅ implemented · ◑ partial / abstracted · ☐ not yet modelled.

Polarity convention (GE): a numeric mnemonic suffix denotes a positive-going
signal; an alphabetic suffix (e.g. trailing `N`) denotes negative-going /
active-low. gemu works in logical-true terms (the `N` is the wire polarity).

**Backplane (card·pin) column.** Each signal table now carries the signal's
location(s) on the CPU **card-layout Atlas** (`/Atlas/row_*_pinout_verified.csv`,
backplane rows A–T = drawing *14026 136*, cabinet **section 2A**). Format:
`<row><slot>·<pin>` — e.g. `G7·07` = row G, slot 7, pin 07. Entries are matched on
the 4-character signal **base** (mnemonic+number), so every suffix/polarity variant
is listed with its actual backplane code (e.g. `LU08B` the conn-2 line and `LU082`
its companion). All 20 Atlas rows (A–T) are now alphabet-cleaned to `[A-Z0-9]`, so
the index covers every legible pin on the 2A layout. `—` means the signal is
**not on the 2A card-layout pins**: it lives on another cabinet section not in this
Atlas, or the only pin that would carry it was illegible on the scan and left blank.
A handful of located cells still carry residual OCR glyph slips (O↔0 / B↔8 / I↔1 /
S↔B, e.g. `LU01S` for `LU01B`) — the **position** (card·pin) is correct; the suffix
may need a glance. The microcode-command tables (§6 CO/CI/CU/CA) are mostly `—`:
those are timed micro-commands, not pin-labelled backplane lines.

---

## 1. Card reader ↔ CPU/channel — the COCA connector

The card reader (LS 600 / GIS 450 controller) attaches on **connector 2**. COCA is
the controller↔channel connector (CRZ schematics, dwg 14112 0781). These are the
target pins for the signal-level model. Where gemu already carries the line, the
"gemu" column names the signal/field; otherwise it is the next thing to wire.

### 1.1 Reader → CPU/channel

| Pin | Type | Meaning | gemu | Effect / condition in the model | Backplane (card·pin) |
|-----|------|---------|------|----------------------------------|---|
| `LU00N`–`LU07N` | data | transcoded character bits (8) | ◑ `integrated_reader.data` (a byte) | The presented character. Today carried as one byte, not 8 separate lines. The channel read latches it into `RO` and stores to `mem[V]`. | LU00B: O8·05, O11·09; LU01S: M1·02; LU01B: O8·06, O11·13; LU023: M1·03; LU02B: O8·09, O11·12; LU03B: M1·04, O8·10, P12·16; LU04B: O8·11, O12·03; LU05B: M1·07, O8·12, O12·07; LU06B: M1·09, O8·13, O13·01; LU07B: M1·10, O8·14, O13·02 |
| `LU08N` | handshake/clock | **character-ready strobe** | ✅ `integrated_reader.lu08` (`LU081`) | "a byte is on the data lines." While set, the channel input cycle (`b9`/`b1`, cmd `CI34` `NE→RO→Mem`) reads it; the peripheral clears it after consumption. The single most important read handshake. | LU08B: G7·07; LU082: G7·14, H8·10 |
| `FININ` | handshake | end-of-read, raised with the **last** character | ✅ `FINI1` (`reader_get_FINI1`) | The controller "end" (→ `RIG1`). Bounds the transfer at the physical card boundary: drives the load-end sequence (`b9→ea→eb→e3`) instead of continuing. **It rides the last presentation and STAYS standing after the strobe**, along with the last data nibble, until the next read command, the next `TU03N`, or a ~1 ms timeout takes it down (`cr_finin_release`) — the presenter has stalled with the word still on the pins. Same as the Pico reader. (Test `cardreader.finin_stands_after_the_last_strobe`.) | FINIB: L1·06 |
| `FIDEN` | status | end-of-sequence (all data transferred) | ◑ `integrated_reader.fiden` (`FIDE1`) | Set by `cardreader_on_clock` when the deck is exhausted (`CR_DONE`). Observable end-of-sequence; the full in-transfer-clear/`LUPOR`-reassert handshake is still minimal. | — |
| `LUPOR` | status | reader free / ready | ✅ `LUPO1` (`reader_get_LUPO1`) | **A per-card ready/busy line, not a per-character one.** One predicate, shared verbatim with the Pico reader (`feeder.c lupob_update`): ready iff a deck is loaded, nothing is presenting, no command is waiting for its feed, no `FININ` is standing, and the reader is parked before or after a card. Everything else — presenting, error, deck exhausted, **no deck at all** — is busy. On the wire LOW is ready and HIGH is busy (bench-settled July 2026), so `LUPO1` here is the logical sense, the inverse of the wire. `PELEA = !(LU08·LUPO1)` then holds by construction. Note there is **no ready front between cards** in the steady loader loop: the next command releases `FININ` and latches first. (Tests `cardreader.lupor_ready_invariant`, `cardreader.lupob_is_busy_for_the_whole_card`.) | LUPOB: L1·07, L7·04, N8·05, N14·03 |
| `LUREN` | status | error (transcoder / jam) | ◑ `integrated_reader.luren` (`LURE1`) | `cardreader_on_clock`: when set, the reader cannot deliver data — presents nothing and the read stalls in error. The faithful `RG011`-decode → peripheral-error/interrupt condition is deferred to the CAN2 interrupt integration. (Test `cardreader.luren_error_stalls`.) | LUREB: L1·09, M13·03, N8·09 |
| `LUSEN` | status | out-of-service | ✅ `integrated_reader.lusen` (`LUSE1`) | `cardreader_on_clock`: when set, the reader is offline — presents nothing and reports not-ready, so a read parks/completes as unit-not-ready instead of getting data. (Test `cardreader.lusen_out_of_service_stalls`.) | LUSEB: M15·01, N8·06 |
| `LENON` | status | **"not operable"** condition (official `LENOB`/`LENO`, decoded by `RG131`; earlier mislabelled "manual mode") | ✅ `integrated_reader.lenon` (`LENO1`) | `reader_send_tu10` (`CE09`): when set, the `TU03N` card-feed strobe is suppressed — a not-operable reader does not advance under the CPU feed. (Test `reader_signals.lenon_inhibits_feed`.) | LENOB: L1·13, M16·01 |
| `BI20` | clock | binary-read aux clock — 2nd nibble, ~15 µs after `LU08` | ✅ `integrated_reader.bi20` (`BI201`) | `cardreader_on_clock` raises it while the **low** nibble of a packed binary column is on the lines (the second sub-read). Observable framing of the packed nibble-pair feed (§3). (Test `cardreader.feed_state_lines_pom_pico_bi20`.) | — |
| `POM01` | status | binary-mode indicator | ✅ `integrated_reader.pom01` (`POM01`) | `cardreader_on_clock`: high while presenting in a binary / by-pass read (`TC_BINARY`/`TC_COLBIN`). Observable. (Test `cardreader.feed_state_lines_pom_pico_bi20`.) | POMOD: L7·01; POMO2: L7·16 |
| `PICON` | handshake | first-column check | ✅ `integrated_reader.picon` (`PICO1`) | `cardreader_on_clock`: high while presenting column 0 of a card — the leading-column marker. Observable. (Test `cardreader.feed_state_lines_pom_pico_bi20`.) | — |

### 1.2 CPU/channel → reader

| Pin | Type | Meaning | gemu | Effect / condition in the model | Backplane (card·pin) |
|-----|------|---------|------|----------------------------------|---|
| `RE00N`–`RE08N` | data (8+parity) | character data toward the reader register | ◑ `rRE` (connector-name byte) | The byte the CPU presents (output / command code). 8 data + odd parity. | RE00B: I1·01, L6·03; RE002: I6·09, M9·10; RE00A: N9·03; RE01B: H6·09; RE018: I1·02; RE012: M9·07; RE01A: M9·13, N20·06; RE02B: I1·03; RE022: M9·06; RE024: N20·03; RE03B: I1·04; RE032: I6·12, M10·10; RE039: L6·04; RE03A: N10·03; RE034: N20·05; RE04B: I1·06; RE04Z: I6·13; RE048: L6·05; RE042: M10·07; RE04A: M10·13, M20·05; RE05B: I1·07, L6·11; RE052: I6·05, M10·06; RE05A: M10·09, M20·03, M20·07; RE06B: I1·09, L6·12; RE062: I6·04, M11·10; RE06A: M20·06, N11·03; RE07B: I1·10, L6·13; RE072: I6·03, M11·07; RE07A: M11·13, M20·02; RE08B: I1·12, L6·14; RE082: I6·02, M11·06; RE08A: M11·09 |
| `TU00N` | timing | read-strobe clock for `RE` data | ☐ | Clocks `RE` data into the reader. | TU002: G6·04; TU00B: I1·15 |
| `TU03N` | command | card feed — **the trigger** | ✅ `integrated_reader.tu03` (`CE09`, at `b8`/`b9` `TI10`) | The bench order is command → feed → strobes. A read command on `RE` with its `TU00N` strobe only *latches* at the reader (`cmd_pending`); **`TU03N` is what starts the card moving**. Presenting on the command instead hands the card to a channel that has not armed its transfer yet, and the card is lost (rpi-pico-card-reader commit `65232b4`). `cardreader_on_clock` reads it as the previous cycle's pulse; a feed arriving at `CARD_DONE` with nothing pending releases `FININ` and brings the next card under the station. A fallback presents anyway after 12500 cycles (the Pico's D) and counts an autofeed. Suppressed by `LENON`. (Tests `cardreader.tu03_triggers_the_card`, `cardreader.no_feed_means_no_card`, `cardreader.sequential_two_cards`.) | TU032: H9·16, I6·01; TU03B: L6·15; TU038: M1·13 |
| `N001`,`N002` | command | normal-mode read decode (GE char code) | ☐ | Selects the normal transcoding/read mode; still only represented abstractly through the latched `active_mode`. | — |
| `DEBI` | command | binary-read mode decode (by-pass) | ◑ (by-pass implied by `TC_COLBIN`) | Selects raw column-binary read; the loader's "set by-pass" PER asserts this on the real machine. | — |
| `MI01`,`MI02` | command | mixed-mode read decode | ☐ | Selects the mixed-mode reader decode. | — |
| `RIFAN` | command | card reject / eject | ☐ | Reject/eject command toward the reader. | — |
| `REGEN` | control | general clear / reset | ◑ `ge_clear` resets reader latches | Clears reader/controller latches on power-up / error recovery. | — |
| `SESEN` | control | put-in-manual | ☐ | Forces the reader/controller to manual mode. | — |
| `COCON` | clock | mode-select clock to the transcoder | ☐ | Latches the selected read mode (`N001/N002/DEBI/MI01/MI02`) into the transcoder/controller. | — |

---

## 2. CPU channel & sequencer signals (the transfer machinery)

These are the internal signals that drive a peripheral transfer; the reader pins
above feed into them. Implemented in `signals.h` (`SIG(...)`) and `struct ge`.

| Signal | Meaning | Effect / condition | Backplane (card·pin) |
|--------|---------|--------------------|---|
| `RC00` | CPU-active cycle request | When set, `NA_knot` routes the program sequencer (`rSO`); dropping it at a transfer-wait *freezes* the CPU until a channel request arrives. | — |
| `RC01` | channel-1 cycle request (async) | Reader/connector-1 wants a memory cycle. | RC012: H13·06, H16·11, I14·14 |
| `RC02` | channel-2 cycle request (async) | Integrated reader (`LU08`-derived) **and** the printer (OR'd via `RIMZA`) want a channel-2 cycle. **Key for the reader-on-channel-2 read.** | — |
| `RC03` | channel-3 cycle request (async) | Connector-3 peripheral. | RC032: H13·10, H16·07, I14·15 |
| `RIA0`/`RESI`/`RIA2`/`RIA3` | synchronous request, latched from `RC0x` at `TO00` | Stage the async request into the cycle-assignment logic. `RIA2 = RC02` latched. | — |
| `RES0` | program/CPU cycle assigned | `= !RESI & ...` — this cycle belongs to the CPU sequencer. | RES06: C5·10, D4·13, G16·13 |
| `RES2` | **channel-2 cycle assigned** | `= !RIA3 & !RESI & RIA2`. When true, `NA_knot` routes `rSI & 0x0f` — i.e. the cycle runs a **channel-2 sequencer (`rSI`) state**. This is the gate the channel-2 reader transfer needs. | RES26: C5·06, G13·13, G16·14, M17·16 |
| `RES3` | channel-3 cycle assigned | Routes channel-3 (`rSV`/connector-3). | RES36: C5·02, C16·09, G13·09, G16·09, H15·03, O10·11, P12·03, P13·03; RES3B: C16·10, G15·12 |
| `RIUC` | micro-cycle / executing-state assignment | Routes the executing state (`rSA`). | — |
| `rSO` | program + channel-1 sequencer state | The main CPU state register. | — |
| `rSI` | channel-2 sequencer state | The channel-2 transfer micro-state; routed into `rSA` on a `RES2` cycle. Input states `0C/0E`; output `02/03`; end `0A/0B`. | — |
| `rSA` | the state actually executing this cycle (`NA_knot`) | `RES2 ⇒ rSA = rSI&0x0f`; `RES0/RIUC ⇒ rSA = rSO`; no request ⇒ `0` (idle/frozen). | — |
| `RASI` | channel-1 in-transfer flag | Set at org-phase `ab`; gates the integrated reader to present bytes during a channel-1 read (`b8/b9/b1`). | — |
| `RACI`/`RICI` | console / register-selector inhibit & step controls | Gate sequencer advance under console forcing. | — |
| `RIG1` | controller "end" (from `FINI`) | Terminates a record; steers `b9` to the load-end states. | — |
| `RIVE` | length terminal-count | End-of-transfer when the instruction length `L1` is exhausted (the count-based end, complementary to `RIG1`). | — |
| `RIMZA` | printer→`RC02` OR | Lets the integrated printer raise the channel-2 request without its own `RC0x`. | — |
| `DU97` | `= PUC2 ^ L2.3` | Gates state `b8`'s exit to alpha; with `PUC2` asserted the channel-2 external-request wait completes (used by the printer model). | DU971: A24·14, B26·02, D32·10, E11·11; DU97A: E11·10 |
| `PUC2` | channel-2 unit-ready | Asserting it (with `RC00`) lets `state_b8`'s own microcode complete a parked channel-2 PER. | PUC2L: E25·12, F12·09; PUC26: E26·04, E36·03, E40·11, F12·07, I10·13, L12·04, L13·14, L35·07 |
| `CI34` | command: `NE → RO → mem` | The input-read store command in the channel input transfer (`b9` `TO50`). | — |
| `CE16` | command: "Load Printer Buffer" (`RO → channel-2 sink`) | The output emit command (`state_02`); hands `RO` to the printer. | CE161: E12·14 |
| `L204`/`L206`/`L207` | order-block control bits (`rL2`) | Direction/qualifier of the transfer: `L207` selects OUTPUT vs INPUT; `L204/L206` qualify the `b9` branch. | L204F: D34·04, D34·05, D35·03, M28·09; L206F: A25·02, A26·04, A26·06, E14·11, F13·07, N28·13; L207F: A26·09, A27·07, B31·05, F8·06, F22·05, F38·05, N28·10 |

---

## 3. Notes on the current model vs the pin-by-pin target

- **Channel-1 read** (`b8/b9/b1`, driven by `LU08`/`FINI`/`RASI`) is fully
  implemented and is the working integrated-reader read path. The bootstrap IPL
  uses it.
- **Channel-2 read** is the open gate: a loader PER that reads on channel 2 parks
  at `b8` *frozen* (`rSA=0`, no request); it needs `RC02 → RIA2 → RES2 →` an
  `rSI` **input** transfer state to un-freeze and read. Implementing the `rSI`
  input states (`0C/0E`) + having the reader assert `RC02` is the next step.
- **Data lines / modes**: the reader still presents a *byte* (`LU00N–LU07N`
  collapsed) and a `lu08` strobe; the CPU-selected read mode is decoded into the
  `N001/N002/DEBI/MI01/MI02` flags (Phase 2) and the framing is now exposed on
  `POM01` (binary indicator), `BI20` (2nd-nibble clock) and `PICON` (first
  column) during the feed (Phase 6). Splitting the 8 data bits into separate
  lines remains the only representation choice still collapsed.
- **Status lines**: `LUSEN` (out-of-service), `LENON` (manual), `LUPOR` (ready),
  `FIDEN` (end-of-sequence) (Phase 3) and `LUREN` (error/jam, Phase 6) are
  modelled and reactive. The only remaining gap is the faithful `LUREN`→`RG011`
  → interrupt condition, deferred to the CAN2 interrupt integration.

Extend this table whenever a new signal is wired; keep the **Effect / condition**
column concrete (what it sets/clears/gates), not just a gloss.

---

## 4. GE-120 official signal index (signal dictionary)

Transcribed from the official GE-120 signal index (NOME/NAME · CAPITOLO/CHAPTER ·
SCATOLA/BOX · descrizione IT/EN). `Ch` = schematic chapter, `Bx` = box on that
sheet — the authoritative place to find the signal's logic. `gemu` marks what we
model: ✅ implemented · ◑ partial/abstracted · ☐ not yet.

This is the reference for the channel-transfer and reader work: the channel-2
read needs `RC021`→`RIA21`→`RES26` and terminates on `RUF26`/`RIG?`/`RIL?`.

### 4.1 Registers (bit groups)

| Name | Ch | Bx | Meaning (EN) | gemu | Backplane (card·pin) |
|------|----|----|--------------|------|---|
| `RO006`,`RO016`,`RO026`,`RO036`,`RO046`,`RO056`,`RO066`,`RO076`,`RO081` | 081–086 | — | Bits of **RO** memory register | ✅ `rRO` | — |
| `RI002`,`RI012`,`RI022`,`RI032`,`RI042`,`RI052`,`RI062`,`RI072` | 047,050 | — | Bits of **RI** register | ✅ `rRI` | — |
| `RA001`…`RA081` | 121 | — | Bits of **RA** register | ✅ `rRA` | RA00A: N9·01, N20·15; RA01A: M9·15, N20·16; RA02A: M9·03, N20·13; RA03A: N10·01, N20·09; RA04A: M10·15, N20·14; RA05A: M10·03, N20·11; RA06A: M20·01, N11·01, N20·12; RA061: N15·12; RA07A: M11·15, N20·10; RA08A: M11·03 |
| `RE002`…`RE082` | 153–155 | — | Bits of **RE** register | ✅ `rRE` | RE00B: I1·01, L6·03; RE002: I6·09, M9·10; RE00A: N9·03; RE01B: H6·09; RE018: I1·02; RE012: M9·07; RE01A: M9·13, N20·06; RE02B: I1·03; RE022: M9·06; RE024: N20·03; RE03B: I1·04; RE032: I6·12, M10·10; RE039: L6·04; RE03A: N10·03; RE034: N20·05; RE04B: I1·06; RE04Z: I6·13; RE048: L6·05; RE042: M10·07; RE04A: M10·13, M20·05; RE05B: I1·07, L6·11; RE052: I6·05, M10·06; RE05A: M10·09, M20·03, M20·07; RE06B: I1·09, L6·12; RE062: I6·04, M11·10; RE06A: M20·06, N11·03; RE07B: I1·10, L6·13; RE072: I6·03, M11·07; RE07A: M11·13, M20·02; RE08B: I1·12, L6·14; RE082: I6·02, M11·06; RE08A: M11·09 |
| `SO002`…`SO072` | 106,107 | — | Bits of **SO** future-status register | ✅ `rSO` | — |
| `SIO01`,`SIO11`,`SIO21`,`SIO31` | 108 | — | Bits of **SI** future-status register | ✅ `rSI` | — |
| `SAO06`…`SAO76` | 110,111 | — | Bits of **SA** present-status register | ✅ `rSA` | — |
| `FO006`…`FO076` | 104,105 | — | Bits of **FO** function register | ✅ `rFO`/`ffFO` | FO06F: B27·01 |
| `FIO01`…`FIO61` | 112–114 | — | Bits of **FI** conditions register | ✅ `ffFI` | — |
| `FAO06`,`FAO16`,`FAO36`,`FAO46`,`FAO56`,`FAO66` | 112–114 | — | Bits of **FA** conditions register | ✅ `ffFA` | — |
| `SOCO1` | 116 | 15 | Permission to load SO | ◑ | — |
| `FOS1A` | 278 | 2 | Forces S1 loading | ☐ | — |
| `L1006`…`L1156` | 057–066 | — | Bits of **L1** length register | ✅ `rL1` | L104F: B23·13; L104L: E13·10, E28·02, E32·03, F11·06; L105F: B23·16; L105L: E13·12, F13·16; L106F: B23·06, B26·14; L106L: E13·09, G18·03; L107L: D22·09, D22·10, E11·13, E28·04, E33·01; L111F: N22·01 |
| `L2006`…`L2076` | 041–044 | — | Bits of **L2** auxiliary register (the channel order/`z`-byte) | ✅ `rL2` | L200F: E10·15, E15·13, F13·05, M19·02, M27·09; L201F: E10·13, E15·12, M27·06; L202F: D8·09, D8·11, D17·06, E10·12, N27·13; L203F: E10·01, E15·02, E15·06, M19·03, N27·10; L204F: D34·04, D34·05, D35·03, M28·09; L205F: M28·06; L206F: A25·02, A26·04, A26·06, E14·11, F13·07, N28·13; L207F: A26·09, A27·07, B31·05, F8·06, F22·05, F38·05, N28·10 |
| `L3001`… | 041 | 6 | Bits of **L3** length register, **channel 3** | ✅ `rL3` | — |

### 4.2 Channel / cycle-request machinery (the transfer core)

| Name | Ch | Bx | Meaning (EN) | gemu | Backplane (card·pin) |
|------|----|----|--------------|------|---|
| `RC001` | 130 | 2 | **Async** storage for **C.P.U.** cycle request | ✅ `RC00` | — |
| `RC011` | 129 | 4 | **Async** storage for **channel-1** cycle request | ✅ `RC01` | RC012: H13·06, H16·11, I14·14 |
| `RC021` | 129 | 11 | **Async** storage for **channel-2** cycle request | ✅ `RC02` | — |
| `RC031` | 129 | 18 | **Async** storage for **channel-3** cycle request | ✅ `RC03` | RC032: H13·10, H16·07, I14·15 |
| `RIA01` | 131 | 3 | **Sync** storage of **C.P.U.** cycle request | ✅ `RIA0` | — |
| `RIA21` | 131 | 19 | **Sync** storage of **channel-2** cycle request | ✅ `RIA2` | — |
| `RIA31` | 131 | 22 | **Sync** storage of **channel-3** cycle request | ✅ `RIA3` | — |
| `RESO6` | 131 | 8 | Cycle assignment to **C.P.U. or channel 1** | ✅ `RES0` | — |
| `RESI6` | 131 | 7 | Cycle assignment to **channel 1** | ✅ `RESI` | — |
| `RES26` | 131 | 14 | Cycle assignment to **channel 2** | ✅ `RES2` | RES26: C5·06, G13·13, G16·14, M17·16 |
| `RES36` | 131 | 17 | Cycle assignment to **channel 3** | ✅ `RES3` | RES36: C5·02, C16·09, G13·09, G16·09, H15·03, O10·11, P12·03, P13·03; RES3B: C16·10, G15·12 |
| `RIUC1` | 131 | 5 | Cycle assigned to **C.P.U.** (executing micro-cycle) | ✅ `RIUC` | — |
| `RETO6` | 132 | 3 | Assign-to-CPU/channel-1 cycle **stored** (staticized) | ◑ | RETO6: H16·13; RETOG: L14·16, L30·10 |
| `RET26` | 132 | 8 | Assign-to-CPU/channel-2 cycle **stored** (staticized) | ◑ | RET26: D5·06, D8·06, H16·14, L14·15, O10·10, P11·06, P12·09 |
| `RA101` | 140 | 8 | OR of **char-exchange request, channel 1** | ◑ `RA101`→`RC01` | RA101: H12·14, H13·13 |
| `RA301` | 148 | 8 | OR of **char-exchange request, channel 3** | ☐ | RA301: H13·09, H14·14 |
| `RAS12` | 136 | 12 | **Data transfer by channel 1** | ◑ (`b8/b9/b1`) | RAS12: E37·06, E40·09, G9·01, H13·04, L13·15, L14·14, L16·11, N18·11 |
| `RB101` | 140 | 1 | OR of trigger TE30 channel 1 | ☐ | — |
| `RB301` | 148 | 1 | OR of trigger TE30 channel 3 | ☐ | — |
| `RIMZA` | 143 | 13 | **MZ printer cycle request** | ◑ (printer raises `RC02`; the scan confirms this is the printer's own channel-request source, which gemu currently ORs into `RC02`) | — |
| `REAB2` | 143 | 15 | **General logic reset of channel 2** | ☐ (the index wording matches the ch-2 reset role already inferred from the flowchart) | REAB2: L14·06, M17·01 |
| `RAC16` | 140 | 18 | Storage of **rejected command** | ☐ (command-reject latch; likely part of the external/peripheral-command reject path) | RAC1L: A25·01, B26·12, E35·07, E37·05, F12·11, H36·03; RAC16: F12·10, F38·07, F40·04, G12·02, I14·02 |
| `RAMO2` | 133 | 16 | Conditioning signal — **C.P.U. internal speed** | ☐ (timing/speed qualifier, not yet functionally modeled) | RAMOR: C7·05; RAMO2: C7·06; RAMOB: C23·03 |
| `RATE1` | 141 | 4 | **Emission of selection trigger P.U. AEBE** | ☐ | RATE1: G11·01, I9·01; RATEI: L10·01 |
| `RAV12` | 140 | 14 | **Emission of signal VICU** | ◑ `CE08` sets `RAVI` under `TO19 && RETO`; broader VICU consequences are still partial. | RAV12: G12·01, G12·05, I14·11, M9·12, M10·12, M11·12 |

### 4.3 Transfer end / length / status

| Name | Ch | Bx | Meaning (EN) | gemu | Backplane (card·pin) |
|------|----|----|--------------|------|---|
| `RUFI2` | 139 | 20 | **End of data exchange on channel 1** | ◑ | — |
| `RUF26` | 143 | 7 | **End of data exchange on channel 2** | ☐ ← needed for ch-2 read termination | RUF26: E12·12, E15·05, I14·09, N17·05; RUF2L: E12·13, G35·04; RUF2S: N18·07 |
| `RUF32` | 147 | 20 | End of data exchange on channel 3 | ☐ | RUF32: H15·14, I14·10, L13·06, M9·02, M10·02, M11·02, N15·03, N15·04; RUF3B: N15·05, N16·04 |
| `RUSC6` | 148 | 14 | Data exchange in output on channel 3 | ☐ | RUSCL: D33·04, E30·15, F12·06, F35·09; RUSC6: F12·05, F14·01, G14·01, H15·13, I14·04 |
| `RIG16` | 138 | 4 | **End from controller on channel 1** (`RIG1`) | ✅ `RIG1` | — |
| `RIG36` | 146 | 4 | End from controller on channel 3 | ☐ | — |
| `RIL11` | 138 | 13 | **End from length on channel 1** | ◑ `RENIA = !(RL1U1·L204)` wired into `RIVE`; inert until the per-char L1 decrement is added to the read datapath (reads still end on FININ). | — |
| `RIL31` | 146 | 13 | End from length on channel 3 | ☐ | — |
| `RIVEF` | 138 | 11 | **Condition of end of transfer on channel 1** (`RIVE`) | ✅ `RIVE` | — |
| `RIVAF` | 146 | 1 | Condition of end of transfer on channel 3 | ☐ | — |
| `RF101` | 140 | 4 | OR of **"END condition" from P.U. for channel 1** | ◑ `RF101()` is wired from `FINI1/FINE3/FINE4` + channel-select decode; `reader.c` still short-circuits the downstream `RIG1` latch on end-of-card/end-of-transfer. | RF101: H12·15, L11·01 |
| `RF301` | 148 | 4 | OR of "END condition" from P.U. for channel 3 | ☐ | RF301: H14·15, H15·01 |
| `RM101` | 140 | 12 | **"Out-of-service" condition for channel 1** | ◑ `RM101()` is wired from the selected-unit out-of-service inputs (`FUSE1/MARE3/MARE4`); the full terminate/reset consequences remain only partially modeled. | RM101: H12·11, L16·10 |
| `RM301` | 148 | 12 | "Out-of-service" condition for channel 3 | ☐ | RM301: H14·11 |
| `RER12` | 139 | 5 | Odd-parity error in input, channel 1 | ☐ | RER12: L11·15, L14·10, O14·01 |
| `RER32` | 147 | 5 | Odd-parity error in input, channel 3 | ☐ | RER32: H15·15, L10·09, L14·01 |
| `RESC1` | 123 | 4 | Odd-parity error input data on **channel 1 or 2** | ☐ | RESC1: N16·16, P14·01 |
| `RINT6` | 141 | 9 | **Interruption present** | ✅ `RINT` | — |
| `RIND6` | 148 | 18 | Counts for decreasing addresses, connector 3 | ☐ | — |
| `RICO2`/`RICI2` | 142 | 7,18 | **Differential counter for MZ printer** | ☐ (printer-side occupancy/buffer counter: `CE16` increments it on each scanned character; each issued `TUO4` decrements it) | — |
| `RICS1` | 145 | 13 | **Counter permission for emission of TUO4** | ☐ (CPU[4] printer prose: set when `RICI=1` to enable the RUCO/RUCI timing counter that spaces `TUO4` at the fixed ~6 us rate) | — |
| `RUCO2`/`RUC12` | 145,133 | 22,1 | **Counter for TUO4 emission** | ☐ (the timing counter that paces printer `TUO4` issue; OCR prose says it "counts by threes" to realize the fixed issue cadence) | RUC1B: G16·06, L14·12, M18·04 |
| `RINO1`/`RIN11` | 144 | 21,24 | **Information buffer for emission of TUO2** | ☐ (a 2-stage shift/buffer storing equality results from the memory-vs-photodisc compare so any `TUO2` hammer-enable is synchronized with its partner `TUO4`) | — |

### 4.4 RO-decode conditions (`RG0x1`/`RG1x1`, ch.122–123) and length decodes

| Name | Bx | Decodes RO for condition(s) | Backplane (card·pin) |
|------|----|------------------------------|---|
| `RG001` | 3 | PEOO, FUPO, LUPO | RG001: N13·12, P14·15 |
| `RG011` | 7 | SEGE, LURE | RG011: N13·13, P14·04 |
| `RG021` | 11 | FISE, SAFE, FIDE | RG021: P14·14 |
| `RG031` | 13 | EGOL, SAFI | RG031: N14·09, N14·13, P14·12 |
| `RG041` | 16 | MAPE | RG041: N14·10, P14·11 |
| `RG051` | 18 | TESE, FIDA | RG051: N15·13, P14·02 |
| `RG061` | 21 | MARE, LUSE, FUSE | — |
| `RG071` | 5 | MATE | RG071: N13·10 |
| `RG081` | 9 | CAPE | RG081: P14·05 |
| `RG091` | 2 | IGOL | RG091: N15·09, P14·16 |
| `RG101` | 6 | NU10 | RG101: N15·10, P14·07 |
| `RG111` | 9 | NU20 | RG111: N16·09, P14·13 |
| `RG121` | 12 | NU30 | RG121: N16·10, N16·13, P14·10 |
| `RG131` | 14 | SECO, FU22, LENO | — |
| `RG141` | 16 | ERCA + input-parity error | — |
| `RL1U1` | 128/4 | Decode **L1 all "ones"** | ✅ `RL1U1` (`signals.h`: `(rL1 & 0xff) == 0xff`) — the channel-1 length terminal-count decode; consumed by `RENIA`. Same all-ones decode as `LIUM6` below (byte-wide). | RL1U1: I10·10 |
| `RL301` | 128/6 | Decode **L3 all "zeroes"** | RL301: G15·11, N19·03 |

**L1/L3 length-decode family** (from the L-family index pages, ch.057–068). These
are the raw register decodes feeding the end-of-transfer logic:

| Name | Ch/Bx | Decode (EN) | gemu | Backplane (card·pin) |
|------|-------|-------------|------|---|
| `LIU16` | 068/5  | L1 (00÷03) = **ALL ONES** (low nibble) | — | — |
| `LIUM6` | 068/10 | L1 (00÷07) = **ALL ONES** (low byte) | ≈ `RL1U1` (the terminal-count my `RENIA` uses) | — |
| `LIZIF` | 058/7  | L1 (00÷03) = **ALL ZEROES** | — | — |
| `LIZ2A` | 061/7  | L1 (04÷07) = **ALL ZEROES** | — | — |
| `LIZ3A` | 064/7  | L1 (08÷11) = **ALL ZEROES** | — | — |
| `LIZ4A` | 067/7  | L1 (12÷15) = **ALL ZEROES** | — | — |
| `LIZE6` | 068/23 | L1 (00÷15) = **ALL ZEROES** (full 16-bit `L1==0`) | — | — |

> **Phase-5 note / open question.** A length-counted read "ends at L1+1 chars"
> (CPU[4] §5.8.4.3a). Counting L1 *down*, that terminal lands on L1 underflowing
> to all-ones (`LIUM6`/`RL1U1`) — which is what `RENIA = !(RL1U1·L204)` uses.
> The alternative the hardware might use is the **all-zeroes** decode `LIZE6`
> (`L1==0`). This only matters once the per-character L1 decrement is wired into
> the read datapath (currently L1 stays constant, so `RENIA` is inert). Resolve
> against the B9 count timing (CPU[7]) before enabling true length termination.

### 4.5 Peripheral / connector lines (FU/FI/FA/SE/SA "bocchettone" = connector)

| Name | Ch | Bx | Meaning (EN) | gemu | Backplane (card·pin) |
|------|----|----|--------------|------|---|
| `FU00A`–`FU08A` | 005,006 | 7,5 | **Input bits from photodisc, connector 1** | ☐ (printer photodisc) | FU00A: M8·09, O11·10; FU01A: M8·10, O11·11; FU02A: I3·12, M8·11, O11·04; FU03A: I3·13, M8·12, O12·01; FU04A: I3·15, M8·13, O12·02; FU05A: I3·16, M8·14, O12·05; FU06A: M3·13, P8·12, P13·16; FU07A: M3·15, O13·03, P8·13; FU08A: M3·16, O13·07, P8·14 |
| `FU09A` | 005 | 4 | **Photodisc code strobe, connector 1** | ☐ (integrated printer photodisc code-valid strobe) | FU09A: G7·04, M7·06; FU091: G7·12, G8·13, G16·02 |
| `FU22A` | 005 | 5 | Condition "not operable", connector 1 | ☐ | FU22A: L3·12, M3·12, M16·02, P8·15 |
| `FUPOA` | 006 | 5 | Condition "availability", connector 1 | ☐ | FUPOA: L3·07 |
| `FUSEA` | 006 | 7 | Condition "out-of-service", connector 1 | ☐ | FUSE5: G7·10, G10·13; FUSEA: I3·04, N8·04 |
| `FIDAA` | 006 | 7 | **"Almost end of paper"**, connector 1 | ☐ | — |
| `FIDEB` | 006 | 3 | "End of file", connector 2 | ☐ (cf. reader end-of-deck) | — |
| `FIFEC`/`FIFED` | 007,008 | 2 | Information bit in **input**, connector 3/4 | ☐ | — |
| `FIFUA`/`FIFUC`/`FIFUD` | 154 | 21,12,17 | Information bit in **output**, connector 1/3/4 | ☐ | — |
| `FINO1` | 006 | 5 | "Out-of-service", connector 1 | ☐ | — |
| `FINAA`/`FINIB`/`FINEC`/`FINED` | 006–008 | 3,4 | END condition, connector 1/2/3/4 | ◑ (`FINI` family) | FINED: I7·04 |
| `FINUA`/`FINUC`/`FINUD` | 162,163 | 6,2,13 | Command FINU, connector 1/3/4 | ☐ | FINU4: L9·11 |
| `FIRUA` | 162 | 8 | **Trigger, paper-brake release**, connector 1 | ☐ (integrated printer/mech control) | — |
| `FISEC`/`FISED` | 007,008 | 3 | Condition of P.U., connector 3/4 | ☐ | — |
| `SAFIA`/`SAFEA` | 006 | 7 | **"End of sheet" 2nd / 1st trailer**, connector 1 | ☐ | SAFEA: I3·06, M8·07, M14·02; SAFIA: I3·07 |
| `SECOC`/`SECOD` | 007,008 | 3 | Manual condition, connector 3/4 | ☐ (cf. `LENON`) | SECOC: L2·12, M16·06; SECOD: M2·12, M16·07, P8·09 |
| `SEGEC`/`SEGED` | 007,008 | 3 | Condition of connector 3/4 | ☐ | SEGEC: L2·09, M13·04, N8·14; SEGED: M2·09, M13·05, P8·06 |
| `SEPE1` | 135 | 21 | Selection of connector 1 | ☐ | SEPEA: I3·01, L6·10; SEPE1: I6·06, I12·16, L28·13, L36·09 |
| `LIFEC`/`LIFED` | 007,008 | 2 | Information bit in **input**, connector 3/4 | ☐ | — |
| `LIFUA`/`LIFUC`/`LIFUD` | 155 | 23,19,6 | Information bit in **output**, connector 1/3/4 | ☐ | — |

#### Connector-2 integrated-reader lines (the COCA pins, official names)

These are the `B`-suffix (connector-2) reader lines from the L-family index;
they are the official names behind the COCA model in §1. The data byte +
strobe are collapsed in gemu (see §3); the four condition pins are wired in
§1 (Phases 3/6). Each condition is decoded by an `RG0x1` (§4.4) — the path for
the deferred error/interrupt reaction.

| Name | Ch/Bx | Meaning (EN) | RG-decode | gemu | Backplane (card·pin) |
|------|-------|--------------|-----------|------|---|
| `LU00B`–`LU07B` | 006/2 | **Information bits in input**, connector 2 (8 data lines) | — | ◑ `integrated_reader.data` (collapsed byte; `LU00N` family) | LU00B: O8·05, O11·09; LU01S: M1·02; LU01B: O8·06, O11·13; LU023: M1·03; LU02B: O8·09, O11·12; LU03B: M1·04, O8·10, P12·16; LU04B: O8·11, O12·03; LU05B: M1·07, O8·12, O12·07; LU06B: M1·09, O8·13, O13·01; LU07B: M1·10, O8·14, O13·02 |
| `LU08B` | 006/2 | **Information strobe in input**, connector 2 | — | ✅ `lu08` (`LU081`) | LU08B: G7·07; LU082: G7·14, H8·10 |
| `LUPOB` | 006/3 | **"Controller available"** condition, connector 2 | `RG001` (`LUPO`) | ✅ `lupor` (`LUPO1`) — **confirms LUPOR = ready, not parity** | LUPOB: L7·04, N8·05, N14·03 |
| `LUREB` | 006/3 | **"Error"** condition, connector 2 | `RG011` (`LURE`) | ◑ `luren` (`LURE1`) — stalls; interrupt path deferred | LUREB: L1·09, M13·03, N8·09 |
| `LUSEB` | 006/3 | **"Out-of-service"** condition, connector 2 | `RG061` (`LUSE`) | ✅ `lusen` (`LUSE1`) | LUSEB: M15·01, N8·06 |
| `LENOB` | 006/3 | **"Not operable"** condition, connector 2 | `RG131` (`LENO`) | ✅ `lenon` (`LENO1`) — **was mislabelled "manual mode"**; inhibits feed | LENOB: L1·13, M16·01 |
| `LESAB` | 006/3 | **Card-reader present** signal, connector 2 | — | ✅ `integrated_reader.lesab` — a strap, asserted for as long as a reader peripheral is registered. On the Pico it is a hardware strap too, not a GPIO. | LESAB: H7·09, L1·15, N8·11; LESA2: H7·10, H9·10, H9·12; LESAA: H9·13, H10·14 |

### 4.6 Configuration jumpers ("fiscelle") and misc

| Name | Ch | Bx | Meaning (EN) | gemu | Backplane (card·pin) |
|------|----|----|--------------|------|---|
| `FEL06`/`FELI6` | 002 | 4 | Connections for **C.P.U. cycle-period** choice | ◑ (timing fixed) | FEL06: D8·04, D19·09, E3·01 |
| `FUL26`/`FUL36` | 002 | 3 | Connections for **program-loading connector** choice (`FUL2`/`FUL3`) | ✅ `FUL2`/`FUL3`=1 | FUL26: D8·14, E4·02, G32·14; FUL25: E36·07; FUL36: D8·15, E4·03, G32·16 |
| `FUL46` | 217 | 1 | Connections to enable additional performances | ☐ | FUL4G: B8·16, D19·03, D19·04, F4·05; FUL4F: C19·14, E12·01; FUL46: E12·02, E30·09 |

**Note on suffix digits:** the index names carry a trailing form/rev digit (e.g.
`RES26`, `RIA21`, `RC021`); gemu uses the base mnemonic (`RES2`, `RIA2`, `RC02`).
The `Ch/Bx` columns point at the GE schematic sheet to consult when wiring the
exact logic equation for a signal we promote from ☐/◑ to ✅.

### 4.7 Connector/channel selection & peripheral status (PC/PE/PU)

| Name | Ch | Bx | Meaning (EN) | gemu | Backplane (card·pin) |
|------|----|----|--------------|------|---|
| `PCOV6` | 128 | 11 | **Network output, external-condition examination** | ☐ | PCOV6: C34·06, C34·07, C35·06; PCOVL: D17·04 |
| `PC111`/`PC121`/`PC131`/`PC141` | 159,156,157 | — | Connector 1/2/3/4 selection, **channel 1** | ◑ | PC11B: A26·03, A31·06; PC118: A31·09, E12·15, E25·15; PC11F: E12·05, E15·04, G8·10, L12·07, N14·06, N15·06, N16·06; PC111: G8·14, G9·11, G10·01, G10·11; PC113: M10·14, N15·07; PC119: M11·14, N13·15, N16·07; PC115: N9·04, N14·07; PC128: E25·11, E25·14, E35·05, F12·16; PC12F: F7·16, F12·15, G23·06, H8·07, I12·01, M14·15, N13·06; PC121: G9·05, H8·09, H9·15, L8·15; PC124: N13·07, N14·15, N15·15, N16·15, P11·01; PC131: G8·09, G9·09, G11·13, I8·06, N9·05, N10·04, N11·05, P12·07; PC13A: G11·02, L12·05, N13·01, N14·01, N15·01, N16·01; PC142: I8·04, I8·11, L8·09, N11·04; PC14A: I9·02, L8·07, L12·11, N13·02, N14·02, N15·02, N16·02; PC141: I9·13, M9·14, N10·05, P13·07 |
| `PC211`/`PC221` | 160 | 5,1 | Connector 1/2 selection, **channel 2** | ◑ | PC218: A31·07, E25·16, F12·14; PC21B: B26·10; PC21F: F2·05, F12·02, G9·10, L12·15; PC211: G9·14, H16·10, P10·07, P11·07; PC22F: D23·05, D24·05, H9·07, L12·10; PC221: H9·09, H10·12, H10·15, H13·02, P10·04, P12·01; PC22A: N9·02 |
| `PC311`/`PC321`/`PC331`/`PC341` | 161,156,157 | — | Connector 1/2/3/4 selection, **channel 3** | ☐ | PC31A: F15·09, G10·10, L12·13; PC311: G8·11, G9·03, G10·06, G10·14, M10·11, M11·11; PC32F: E30·16, F28·01, H10·07, I12·12; PC321: H8·12, H10·09, P10·05, P12·04; PC331: G10·04, G11·16, M9·16, M11·16, N10·02, P13·04; PC33A: H11·03, L12·01; PC33I: I8·09; PC34A: I8·10, L9·03, L12·12; PC342: I8·14, L8·12; PC341: I9·16, M9·11, M10·16, N11·02, P13·01 |
| `PUC16`/`PUC26`/`PUC36` | 136 | 8,18,29 | **Channel 1/2/3 selection** | ◑ (`PUC2`) | PUC16: H13·01, L13·16, L35·09; PUC2L: E25·12, F12·09; PUC26: E26·04, E36·03, E40·11, F12·07, I10·13, L12·04, L13·14, L35·07; PUC36: E15·09, E17·12, I28·16, L12·06, L13·13; PUC3G: F6·03, I35·11 |
| `PUOO2` | 135 | 22 | Connector 2 selection | ◑ | PUOOC: I2·01 |
| `PUOOC`/`PUOOD` | 163,162 | 3,7 | Connector 3/4 selection | ☐ | PUOOC: I2·01 |
| `PELS1` | 156 | 15 | **Card reader connected to connector 2** | ◑ (the integrated reader) | PELSA: H9·11, H11·07; PELS1: H11·09 |
| `PELM6` | 161 | 4 | **Magnetic reader** connected to connector 2 (`PELM` in flowchart: selects `V4−1` vs `V4+1`) | ☐ (gemu = card reader, `+1`) | PELM6: D23·01, D24·01, H10·16 |
| `PELSA` | 125 | 15 | "Out-of-service" condition of the **integrated reader** | ◑ `LUSEN` is modelled behaviorally (reader presents nothing when offline); CPU[4] OCR prose also shows the reader-side terminate term `PELEA=0 (LUPO1·LU081)` for out-of-service on ch-2, so the `PELSA/PELEA→RM101` style equation path is the still-unwired faithful logic. | PELSA: H9·11, H11·07; PELS1: H11·09 |
| `PEST1` | 134 | 24 | Odd-parity error on input character | ☐ | PEST1: H15·07, L12·03, N17·01; PESTI: L11·07 |
| `PEOOC`/`PEOOD` | 007,008 | 3 | **Condition of availability** from connector 3 / 4 | ☐ | PEOOC: L2·07 |
| `PEBIA`/`PEBAA`/`PEBEA`/`PEBUA` | 134 | 19,10,13,22 | **Selected connector 3/1/2/4 busy condition** | ☐ | — |
| `PUBO6` | 134 | 14 | **Selected-connector busy condition** | ☐ | — |
| `PEC11`/`PEC21`/`PEC31`/`PEC41` | 137,136 | — | **It stores reset channel 1/2/3/(1) conditions** | ◑ `PEC1` now latches on the CPU pulse side (`TO50`) from a pending peripheral-end condition, rather than being set immediately by the reader/connector helper; the full manual `PIM11` chain is still approximated. | PEC4B: I13·02, M12·04 |

> The page-36 flowchart's **`PC22`** ("reader, integrated controller, on channel
> 2") is the reader-present decode that branches the channel-2 transfer: after the
> input state `0C`, `PC22`-YES (reader) returns to `B8` for the next request;
> `PC22`-NO (printer) goes to `04|06`. It corresponds to the `PELS1`/`PELM6`
> reader-on-connector-2 selection (vs the printer). `PELM` selects the addresser
> direction (`V4−1` magnetic vs `V4+1` card/photo).

### 4.7b Program addresser + channel-select / NE-input-enable (PO/PIB/PIC/PIM/PB)

| Name | Ch | Bx | Meaning (EN) | gemu | Backplane (card·pin) |
|------|----|----|--------------|------|---|
| `PO001`…`PO151` | 056,059,062,065 | — | Bits of the **PO** program-addresser register | ✅ `rPO` | — |
| `POD11` | 024 | 13 | Increases the delay-line cycle time | ☐ | — |
| `POMOB` | 006 | 2 | **"Reader in binary condition from connector 2"** (= `POM01` binary-mode pin) | ◑ (by-pass implied) | POMOD: L7·01; POMO2: L7·16 |
| `PIB11` | 150 | 23 | Enables input of **photodisc code** into NE | ☐ | — |
| **`PIB21`** | 150 | 24 | **Enables input into NE of information from connector 2** (the reader-input enable; `= !(PB12A·PB22A·PB32A)`) | ◑ (channel-1 only) | — |
| `PIB31`/`PIB41` | 151,152 | 23 | Enables NE input from connector 3 / 4 | ☐ | — |
| `PIC11`/`PIC32` | 136 | 2,33 | **Storage of channel 1 / 3 selection** | ◑ | — |
| `PIM1A`/`PIM2A`/`PIM3A` | 137 | 3,8,13 | **Enables selection reset** channel 1 / 2 / 3 | ◑ `PIM1A/PIM11` are implemented for channel 1; channels 2 and 3 still need faithful reset-chain wiring. | — |
| `PIPO2` | 136 | 5 | **Trigger of register RE and RA** (latches the connector name) | ◑ | — |
| `PB061`/`PB071` | 134 | 9,3 | Storage of connector name **with channel 1** | ◑ | — |
| `PB261` | 134 | 18 | Storage of **connector-1 selection with channel 2** (`PB26`; `PC221=PUC21·¬PB26`, `PC211=PUC21·PB26`) | ☐ | — |
| `PB361`/`PB371` | 134 | 11,6 | Storage of connector name **with channel 3** | ☐ | — |
| `PC016`/`PC036` | 128 | 8,10 | **Channel 1 / 3 selection condition** | ◑ | PC01L: B26·11, B31·04, B31·13; PC016: G29·02, H18·07, H21·07, I13·11, L11·09, N19·04, O14·02; PC01A: N16·11, N19·06; PC037: C33·06, D33·10, E12·16, E35·02, F38·10; PC03L: E26·01, E35·03, F12·01; PC036: H15·09, I12·03, I13·06, I13·09, N19·05; PC03A: N13·11 |
| `PAR21` | 137 | 6 | **Reset channel 1 condition** | ◑ Referenced by the channel-1 selection/reset comments and documented from CPU[4], but not yet exposed as a first-class signal helper. | — |
| `PAZ1A` | 141 | 6 | Permission to disconnect DATANET from connector 4 | ☐ | PAZ1A: I10·02, I11·04 |

> **Channel-2 reader-input chain (the Phase-4 target), decoded from the index +
> signals.h:** `PIB21` (NE input from conn 2) `= !(PB12A·PB22A·PB32A)`, with
> `PB12A=!(RESI1·PC121)` (ch-1), `PB22A=!(RET21·PC221)` (ch-2),
> `PB32A=!(RES31·PC321)` (ch-3). For channel 2: `PC221=!PC22A=PUC21·¬PB26` (conn-2
> on ch-2: channel-2 unit `PUC2` selected and the connector-1/2 selector `PB26=0`)
> and `RET21` (ch-2 cycle-assignment memory). Both are implemented; audit
> round-2 verified them on the cp06 sheets: `PC22F = NAND(PUC26, PB26A)`
> (ch.135-24) with `PC221 = /PC22F` (ch.160-1) — gemu's equation exact — and
> `RET21` is a T010-clocked latch of RES2 (ch.132-6: `RET21 =
> /((RES2A·T0107)+(T010C·RET2A))`), now modeled as the `RET2` latch in
> pulse.c (was: raw `RIA2`, which missed the RES2 priority masking).

### 4.8 Arithmetic unit (UA)

| Name | Ch | Bx | Meaning (EN) | gemu | Backplane (card·pin) |
|------|----|----|--------------|------|---|
| `UA001`…`UA071` | 091,094 | — | Output bits of the arithmetic unit | ✅ (ALU result) | UA001: O26·16, O33·02; UA011: O26·07, O33·01; UA021: O33·03, P26·07; UA031: O33·04, P26·01; UA041: O27·16, O33·06, O33·11; UA051: O27·07, O33·05; UA061: P27·07; UA071: O33·07, P27·01 |
| `UAZO6` | 116 | 4 | **Decode UA 00+07 = "all zeroes"** (the `UAZO` in the ch-2 flowchart) | ◑ (ALU zero flag) | UAZOF: E28·12, E28·14, E30·11 |
| `URO31`/`URO71` | 090,093 | 21 | Carry from A.U. bits 00+03 / 04+07 | ◑ | URO71: P27·12 |
| `URPE6` | 091 | 3 | Carry going **into** the A.U. | ◑ | URPE6: A15·09, A24·04, A26·02, C17·06, E18·04, E33·16, F11·04, N35·09, P26·06, P26·15; URPE1: A31·04; URPEL: C10·07, D10·01, D23·10, E29·15, E33·05, F11·12, F22·04, F28·03; URPE7: C29·11, F22·12 |
| `URPU2` | 094 | 3 | Carry coming **out** of the A.U. | ◑ | URPU2: P26·12, P27·15 |

---

## 5. Channel-2 data-transfer microcode (CPU[7] sheet 36, dwg 14023130)

"Sequenza Esterna TPER — Fase trasferimento dati canale 2 (per lettore e stampante
integrate)." States are obtained by unloading `S1 → SA1` (i.e. `rSA = rSI&0x0f` on
a `RES2` cycle — see `NA_knot`). **State `0C` runs on every request from the
reader, and on the first request from the integrated printer.**

### State `0C|0E` — channel-2 INPUT (reader → memory)
```
V4 → VO                        (CO14: NO knot = V4 → memory address)
V4−1 → V4   [PELM]             (magnetic reader: decrement addresser)
V4+1 → V4   [¬PELM]            (card/photo reader: increment — CO41 + CO04)
NE  → RO                       (CI34: channel-2 input data → RO)
RO  → Mem                      (CO31: memory WRITE RO → mem[VO=V4])
RO  → RI
Ab reset RIAP
Ab set external error  [¬PC22] (only when NOT the integrated reader)
```
Diamond after `0C|0E`:
- `PC22` (reader) & channel-2 not overlapping → **`B8`** (org-phase wait for the next request)
- `¬PC22` (printer) → `04|06`
- `PC22` & overlapping with a CAN1/CAN3 request → run that channel's status; with itself → back to `0C|0E`; CAN1 operating → `B8`; else continue the internal program (`SO→SA`), a CAN2 request preempts for an external cycle.

### State `04|06` — printer photodisc compare
```
V4→VO (CO14); V4+1→V4 (CO41/CO04); RO→Mem [SA01] (CO31); Mem→RO [SA01] (CO30);
RI→RO [¬SA01] (CI21/CI32); RI→BO4,3 (CI21); RO⊕BO4,3→NI4,3 (C145/47/68);
Set S101 (CI71); Set S100 [(UAZO+ERAR+FINO+AITE)·SA01] (CI70)
```

### State `02|03` — channel-2 OUTPUT (memory → printer) — implemented as `state_02`
```
V4→VO (CO14); V4+1→V4 (CO41/CO04); Mem→RO (CO30); RI→BO4,3 (CI21);
RO⊕BO4,3→NI4,3 (C145/47/68); Load Printer Buffer (CE16)
```
Diamond after `02|03`: `SA00` or `RUF2` → `0A|0B` (end); else → back to `02|03`.

### State `0A|0B` — end of channel-2 transfer.

Signals referenced: `PELM` (magnetic reader on ch-2), `PC22` (reader+integrated
controller on ch-2), `UAZO` (ALU=0, `UAZO6`), `ERAR` (printer photodisc parity
error), `FINO` (printer out-of-service), `RUF2` (end of photodisc-code compare
run-through ≈ `RUF26`).

**Implementation status:** `02|03` (output) is wired (`state_02`). `0C|0E`
(input) is implemented as `state_0c` (commit 76d67fe) and **verified to route**
(a `RES2` cycle with `rSI=0x0c` → `rSA=0c`) and **advance `V4`**. The `NE→RO`
read (`CI34`) is gated on the reader-input select **`PIB21`**, which on a
channel-2 cycle needs `PB22A=0` ⇒ **`RET21 && PC221`** (ch-2 cycle-assignment
stored AND connector-2/channel-2 selected).

(Resolved: `RET21` and `PC221` are implemented and schematic-verified — see
§ above. The historical blocker note is kept for context: finishing the
channel-2 read needed **Phase 4**: implement
`RET21` (ch.132) + `PC221` (ch.160) for the channel-2 reader selection and/or
reconcile the integrated reader's channel (the `RA101→RC01` channel-1 wiring vs
the documented `LU08→RC02` channel-2), without breaking the channel-1 bootstrap
IPL (which the locked `bootstrap`/`initial-load` tests guard). Then the reader
asserts `RC02`→`RES2`→`state_0c` per byte, looping via `B8`, terminating on the
card-end (`FINI`→`RUF26`).

---

## 6. Microcode command dictionary (CO / CI / CE / CU / CA), from the index

The per-clock commands the MSL states issue (`msl-commands.c`), transcribed from
the official command index (name · chapter · box · EN). gemu uses the base
mnemonic (`CO30`, `CI34`, `CE16`, …) for the indexed `COxx1`/`CIxx1`/`CExx1`.

### 6.1 CO — output-clock (TOxx) commands
| Cmd | Ch | Meaning (EN) | Backplane (card·pin) |
|-----|----|--------------|---|
| CO00 | 205 | NI → PO | — |
| CO01 | 206 | NI → V1 | — |
| CO02 | 207 | NI → V2 | — |
| CO03 | 217 | NI → V3 | — |
| CO04 | 190 | **NI → V4** (advance the ch-2/V4 addresser) | — |
| CO06 | 206 | NI(00÷07) → L2 | — |
| CO10 | 208 | PO → NO | — |
| CO11 | 206 | **V1 → NO** (ch-1 mem address) | — |
| CO12 | 205 | V2 → NO | — |
| CO13 | 205 | V3 → NO | — |
| CO14 | 208 | **V4 → NO** (ch-2 mem address) | — |
| CO16 | 207 | L2 → NO(00÷07) | — |
| CO18 | 209 | enable forcing in NO(00÷07) | — |
| CO30 | 180 | **memory READ → RO** (commits TO50) | — |
| CO31 | 207 | **memory WRITE** (RO → mem; commits TO65) | — |
| CO35 | 210 | internal-error reset | — |
| CO40 | 193 | counts minus | — |
| CO41 | 200 | **counts from bit 00** (NI = +1) | — |
| CO48/CO49 | 211/212 | set / reset URPE & URPU | — |
| CO90–CO97 | 205–213 | force "1"/value into NO halves | — |

### 6.2 CI — input-clock (TIxx) commands
| Cmd | Ch | Meaning (EN) | Backplane (card·pin) |
|-----|----|--------------|---|
| CI00–CI07 | 180–207 | NI → PO/V1/V2/V3/V4/L1/L2(00÷07)/L3, NI(00÷07)→FO | — |
| CI09 | 205 | NI(08÷15) → RI | — |
| CI10–CI17 | 185–223 | PO/V1/V2/L1/L2/L3 → NO | — |
| CI19/CI20 | 185/190 | enable NO(08÷15) forcing / console forcing | — |
| CI21 | 191 | **RI → NO(08÷15)** | — |
| CI32 | 192 | NO(08÷15) → RO | — |
| CI33 | 185 | NO(00÷07) → RO | — |
| **CI34** | 193 | **NE → RO** (the channel input read — reader byte) | — |
| CI38/CI39 | 211/183 | enable set AVER&ALTO / reset AVER | — |
| CI40–CI44 | 193–215 | counts minus / from bit 00 / from bit 04 / block carry 03 / block carry 07 | — |
| CI45–CI47 | 196/190 | logic ops / decimal-or-AND / subtract-or-XOR | — |
| CI50/CI51 | 215/181 | operate only ALU bits 00÷03 / 04÷07 | — |
| CI60–CI67 | 181–199 | RO(04÷07 or 00÷03) → NI quartets | — |
| CI68/CI69 | 200/181 | U.A. → NI(08÷15)/(00÷07) | — |
| CI70–CI76 | 201–202 | **set FI00..FI06** | — |
| CI77/CI78 | 222/204 | set / reset ADIR | — |
| CI80–CI86 | 201–204 | reset FI00..FI06 | — |
| CI87/CI88/CI89 | 224/203/182 | set ALAM / reset ALAM / set ALTO | — |

### 6.3 CE — external / channel commands (the reader-relevant ones)
| Cmd | Ch | Meaning (EN) | gemu | Backplane (card·pin) |
|-----|----|--------------|------|---|
| CE00 | 214 | RO → RA | ✅ | CE001: H17·05, I13·07, N20·07 |
| CE01 | 214 | **RO → RE** (load the connector-name / command register) | ✅ | CE011: H17·03, M20·14 |
| CE02 | 215 | enable external-channel selection | ◑ | CE021: I11·07, I12·04, I13·05, L10·16 |
| CE03 | 216 | I/O logic reset | ◑ | CE031: E13·02, G12·09, H15·06, L10·11, L11·06 |
| CE05 | 214 | enable set of external error | ◑ | CE051: H17·01, I12·07 |
| CE06 | 215 | enable set of **channel-1 error** | ◑ | CE061: C29·05, G18·07, L10·10 |
| CE07 | 215 | **I/O logic set** (gemu also sets RASI here) | ✅ | CE07I: G14·07; CE071: G18·11, I13·13 |
| CE08 | 214 | **VICU issue** | ◑ Implemented as `CE08()` setting `RAVI` on `TO19 && RETO` and then `RACI` on `RB111`; wider VICU handling is still incomplete. | CE081: H17·06 |
| **CE09** | 214 | **Sends TU10 of channel 1** (gemu `reader_send_tu10`; the feed/advance hook) | ◑ | CE091: H17·04, I16·07 |
| **CE10** | 214 | **Sends TU20 of channel 1** (gemu `reader_send_tu00`; the read-strobe hook) | ◑ | CE101: H17·02, I11·09 |
| CE11 | 215 | Sends TU30 of channel 1 | ☐ | CE111: G18·14, I16·04, L11·05 |
| CE12/CE13/CE14 | 215/210 | Sends TU10/TU20/TU30 of channel 3 | ☐ | CE121: G18·13, I16·05; CE131: G15·09, H18·06; CE141: F22·16, H15·05, I16·03 |
| CE15 | 215 | issue FIRU | ◑ | CE151: H9·05, H18·12, L10·07 |
| CE16 | 217 | **Load printer buffer** (channel-2 OUTPUT emit) | ✅ | CE161: E12·14 |
| CE17 | 217 | stop printing | ☐ | CE171: E12·07, M17·06 |
| CE18 | 214 | enable cycle-request reset | ✅ | CE181: G13·05, H17·15 |
| CE19 | 215 | reset channel-3 selection | ☐ | CE191: H10·05, H18·11 |

> **Plan correction (CAN1):** the COCA `TU00N`/`TU03N` map onto these channel-1
> timing strobes — gemu emits **TU10 at `CE09`** and **TU20 at `CE10`** (the gemu
> function names `reader_send_tu10`/`reader_send_tu00` are off-by-name vs the
> official TU10/TU20). The card-feed/advance is the `CE09` (TU10) hook; the
> read-strobe is the `CE10` (TU20) hook. Exact TU00N↔TU10 / TU03N↔TU30 pin
> correspondence needs the controller schematic.

### 6.4 CU — status-register (SO/S1) commands
| Cmd | Ch | Meaning (EN) | Backplane (card·pin) |
|-----|----|--------------|---|
| CU00–CU07 | 218–222 | **set SO register** (build the future status) | CU00A: B5·14, C5·09 |
| CU10–CU17 | 220–224 | **reset SO register** | CU10A: B3·14, C5·12 |
| CU20 | 192 | **load future status into SO and S1** | CU20A: B5·02, H38·03 |

### 6.5 CA — timed commands; CAGU/CAPE
| Cmd | Ch | Meaning (EN) | Backplane (card·pin) |
|-----|----|--------------|---|
| CA10–CA21 | 036–039 | timed command pairs (CO/CI 10..21) | CA101: L33·16, M31·11, M32·11, M33·11, M34·11 |
| CA40–CA44 | 038 | timed command pairs (CO/CI 40..44) | CA401: L34·15, P20·04, P21·04 |
| CAGU7 | 141 | **External-operation general reset** (the REGEN-class clear) | CAGUF: C29·01, G16·15, L10·06, M17·07, N18·09; CAGU7: H15·02, L10·02, M6·11; CAGUC: L2·01, L6·06; CAGUA: L3·01, M3·01, N6·02; CAGUD: L6·07, M2·01; CAGUB: L6·09 |
| CAPEC/CAPED | 007/008 | command-rejection condition from UP connector 3/4 | CAPEC: G7·06, I2·09, M14·10, N7·05; CAPE7: G7·13, G8·07; CAPEB: I8·03; CAPED: L7·06, M14·09, N2·09, P8·05; CAPE8: L7·11 |

### 6.6 Registers added by this index page
- `BO002`…`BO152` (ch.073–076): **BO** register bits; `BOCO1` (116) = permission
  to load BO. `BU001`…`BU151` (ch.096–099): **counting-network output** bits.
  `BASI1` (218) = enables loading the **SI** register. `BIFEC/BIFED/BIFUA/BIFUC/
  BIFUD` (007/008/153) = info bit in/out, connectors 3/4/1.
