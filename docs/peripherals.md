# Integrated peripherals (channels 1 and 2)

This documents how gemu models the two integrated peripherals that hang off the
CPU's organisation phase: the **card reader** (channel 1, input) and the
**printer / console-typewriter** (channel 2, output + keyboard). Both attach
through the `ge_peri` callback list (`ge_register_peri`); their `on_clock`
callback runs at `TO00`, the first clock of every machine cycle, before the MSL
chart for that cycle executes.

> Confidence: the *completion mechanism* below is **medium-high** — it is derived
> from the channel-2 / TPER flow charts (state b8 → alpha via `DU97`) and
> verified empirically against the funktionalcpu deck. The *per-character
> channel-2 transfer* (the `rSI` sequencer states) is **not yet wired** (the
> pragmatic one-shot stands in); capture is therefore best-effort. See
> "Limitations". The faithful `rSI` transfer states are now recovered from the
> manuals — see "CAN2 data-transfer phase (recovered evidence)" below.

## CAN2 data-transfer phase (recovered evidence — dwg 14023130₁ sheet 18)

Source: `CPU/GE 120 CENTRAL PROCESSOR [7].pdf`, flow-chart foldout **dwg
14023130₁** titled *FASE TRASFERIMENTO DATI CANALE 2 / CHANNEL 2 DATA TRANSFER
PHASE* (render-pg **36**; UCE 460; "for integrated reader and printer"). Read via
page-image render at 500 DPI (no OCR text layer). Confidence: **high** for state
codes/commands/branches as drawn; **medium** for a few faint command suffixes.

> Sheet-map correction to `flowchart-sheets.md`: the channel-transfer foldouts are
> **34 = CHANNEL 1**, **35 = CHANNEL 3**, **36 = CHANNEL 2** (33 = TPER-CPER
> external sequence). The earlier "35–37 = channel 2" note was wrong.

**Cycle attribution.** Channel-2 transfer states are obtained "as unloading of
`SI→SA`" — i.e. they run as genuine `RES2` cycles where `NA_knot` routes
`rSA = rSI & 0x0f` (signals.h). State `0C` "is performed on the arrival of each
request from the integrated reader and printer." On completion the chart returns
to the external request-wait (`SO→46`, the `b8` family) or, if a higher-priority
`CAN3` request arrives, "interrupts the operation under way and performs an
external sequence cycle" (the inter-channel priority/interrupt interplay).

## Input default: the `.cap` card flow (and the IPL scatter-loader status)

`.cap` is the **default** emulator input: a positional `.cap` argument is fed
through the authentic card-reader bootstrap (`CLEAR/LOAD/START`, the
`00→80→c8…b8/b9/b1` load sequence). `--bin FILE` (or any non-`.cap` positional)
forces a direct unified-format binary load — the *debugging* path that skips the
bootstrap. `--deck` remains an explicit alias for the card flow.

The funktionalcpu deck is a **scatter-loader**: card 0 is the IPL loader (read by
the bootstrap into `mem[0]`); cards 1–114 each carry an 8-byte header, a load
address (`41 hi lo`, e.g. card 5 → `0x0100`), and a payload, and the loader
scatters each payload to its address. `gdis --image` already honours this format
to build the `.bin` (which is therefore a *scatter image*, not a raw card dump —
hence the now-skipped `.bin`-oracle bootstrap tests).

> **Known blocker (IPL chain-load):** under the authentic flow the card-0 loader
> executes and PO reaches `0x1424` (into the test region), but the in-machine
> scatter does **not** populate program regions (`mem[0x100]` stays empty) and
> execution loops at `PO=0x0002`. Fixing this needs an instruction-level trace of
> the loader against the card feed. Until then the authentic flow loads card 0
> and partially chains; the `--bin` path loads the full scatter image directly.
> This is what gates funktionalcpu executing/printing its real banner.

**The `rSI` micro-states (low nibble), commands as drawn:**

| rSI state | role | datapath | commands | branch/exit |
|---|---|---|---|---|
| `0C \| 0E` | INPUT transfer (reader) entry | `V4→VO`; `V4±1→V4` [PELM]; **`NE→RO`** (read peripheral byte); `RO→Mem`; `RO→RI`; `Ab reset RIAP`; set external error [PC22] | CO14, CO14-40-41-04, **CI34**, CO31, CO09-60-65, CE18, CE05 | → `PC22?` |
| `04 \| 06` | INPUT with photodisc compare | `V4→VO`; `V4+1→V4`; `RO→Mem`/`Mem→RO`/`RI→RO` [SA01]; `RI→BO`; `RO⊕BO→NI`; `Set S101`; `Set S100` [(UAZO+ERAR+FINO+AITE)·SA01] | CO14, CO31, CO30, CI21-31, CI21, CI45-47-68, CI71, CI70 | |
| `02 \| 03` | **OUTPUT (printer)** "Load Printer Buffer" | `V4→VO`; `V4+1→V4`; **`Mem→RO`** (read char to print); `RI→BO`; `RO⊕BO→NI`; *Carica Buffer Stampante* | CO14, CO14-41-04, CO30, CI21, CI45-47-68, **CE16** | → `SA00?` (YES→loop next char) → `RUF2?` (photodisc compare done) |
| `0A \| 0B` | END PRINT | *Fine stampa*: **emit `FIRU`** [SA00]; `0C→V4` | **CE15, CE17** | |

So the **output emit point is command `CE16`** in state `02|03` ("Load Printer
Buffer") — the CPU reads `mem[V4]` into `RO` and hands the character to the
printer buffer; the per-character loop is gated by `SA00`/`RUF2`, length-bounded.
The **input read** is `CI34` (`RO ← NE_knot`) in state `0C|0E`, exactly as the
existing `b9` reader path uses it. Channel addressing walks **`V4`** (`V4→VO`,
`V4+1→V4`), not `V1` — note `perperi.c` currently exercises the `V1` path via the
shared `b8/b9/b1` states; reconcile `V4` vs `V1` during Phase 3/5.

**Printer = photodisc print-wheel.** The integrated printer is a spinning
photodisc carrying the 64-char graphic set; printing a character loads the buffer
(`CE16`) and the hammer fires when the disc code matches (the `RO⊕BO→NI` compare,
end-of-run `RUF2`). Signals from the sheet's *Note* legend:

| Signal | Meaning (translated) |
|---|---|
| `PELM` | magnetic reader operating with channel 2 |
| `PC22` | magnetic reader / tape-reader with integrated controller, on channel 2 |
| `UAZO` | arithmetic unit result equal to `0C` |
| `ERAR` | **parity error on printer photodisc** |
| `FINO` | **out-of-service for integrated printer** |
| `RUF2` | end of comparison run-through on the 8-bit photodisc code |
| `FIRU` | emitted at end-of-print (`0A|0B`) |
| `SA00`, `SA01` | sequencer/status bits gating the per-char loop |

This is the Phase-3/Phase-5 wiring target: add states `0x02/0x03`, `0x04/0x06`,
`0x0C/0x0E`, `0x0A/0x0B` to `msl-states.c`, reached via `RES2 → rSI&0x0f`, with
`CE16` driving `channel_accept_output` (printer) and `CI34` the input read.

## The external-operation organisation phase and state b8

An external instruction (`PER`, the channel-2 print/typewriter operation) runs
the CPU through the organisation-phase sequence

```
65  e2 e0 e4 e6  65  c8  d8 d9 da db dc  cc ca  a8 a9 aa ab  b8
```

and parks at **state b8**, the org-phase *external request-wait*. There the CPU
suspends: the CPU-active request `RC00` drops, so the next-address knot
(`NA_knot`, signals.h) yields 0 and the executing state `rSA` stays **0** (idle)
while `rSO` holds **b8** pending. The wait is held in `state_00`, not `state_b8`.

`state_b8` is **shared** with the card reader's channel-1 input wait — the
reader's "waiting for the next column" gap looks identical at the register level
(`rSO=b8, rSA=0, RASI=1, lu08=0, RC00=0`).

### The faithful exit (DU97)

`state_b8` leaves to alpha when **`DU97`** is true. From signals.h:

```
DU97 = PUC2 ⊕ L2.3          (DU97A = !(PUC2 ^ L2.0 ^ L2.0 ^ L2.3), DU97 = !DU97A)
```

With `PUC2` asserted (the channel-2 unit signalling ready/done) and `L2.3 = 0`,
`DU97 = 1`, and `state_b8`'s own commands `CU01 / CU13 / CU14 / CU06` build the
PER-completion future_state. The sequencer returns to **alpha at the post-PER
instruction with the CPU context intact** — verified: the funktionalcpu deck
then executes its real final-verify `CMC` at `0x19d4` cleanly (correct PO, no
register corruption). Forcing `future_state` from outside instead lands alpha at
a garbage PO (`0x6324`), because it skips the org-phase cleanup that restores the
fetch context — so the completion must go through the machine's own microcode.

## Card reader — channel 1 (`cardreader.c`)

Feeds a `.cap` deck through the integrated-reader handshake
(`reader_setup_to_send` / `reader_clear_sending`), gated on `RASI`. See the file
header for the per-byte / per-card state machine and end-of-card cadence. This is
the bootstrap LOAD path (`./ge --deck X.cap`, or `mount_deck` + CLEAR/LOAD/START
in the wasm console).

## Printer / console-typewriter — channel 2 (`printer.c`)

Pragmatic model. gemu drives no channel-2 timing at signal level, so a print
`PER` would hang at the b8 wait forever. The printer peripheral:

1. **Detects** the channel-2 print-wait: `rSO==0xb8 && rSA==0 && !RC00 &&
   reader.lu08==0`, **debounced** by a stall counter (`STALL_THRESHOLD = 256`
   cycles). The card reader presents a byte within a couple of cycles, breaking
   any shared-b8 wait long before the threshold, so a concurrently-registered
   reader is never disturbed. A genuine channel-2 print-wait persists and trips
   the counter.
2. **Captures** (best-effort): reads up to 160 bytes of the print order block at
   the channel-2 operand addresser `rV2`, rendering each byte through the **GE
   100-series internal graphic set** (`gecode.c` — the machine's own character
   code, *not* ASCII; non-graphic codes → `.`), stopping at NUL, into
   `integrated_printer.out[]`. The glyph table is single-sourced with the
   disassembler's DB-comment glyphs (`gdis` links the same `gecode.c`).
3. **Completes**: asserts `PUC2` (→ `DU97 = 1`) and `RC00` (→ `NA_knot` routes
   `rSO=b8` into `rSA`) so the machine's own `state_b8` microcode returns to
   alpha. One-shot: `rSO` leaves b8 on the next cycle.

`present` is set only by `printer_register`, so bootstrap/reader tests (which do
not register a printer) leave it 0 and are completely unaffected.

### Two-way chat (wasm + interactive CLI)

* **Output** — `printer_output()` / `printer_output_len()` expose the captured
  paper feed. The wasm front-end (`console/wasm/console.html`) drains it each
  frame into a paper-teletype panel (`#teleprinter`), drawn deliberately unlike
  the metal console since it represents a peripheral, not a console control. The
  `--interactive` CLI echoes it as `PRN> …`.
* **Input** — `printer_feed_key()` enqueues operator-keyboard bytes into
  `integrated_printer.kbd[]` (a ring). The wasm chat input and the CLI's
  non-blocking stdin feed it.

## Limitations (honest scope)

* The **per-character channel-2 transfer** (the `rSI` sequencer states b9/b1 and
  the `RIVE`/`RIG1` length termination) is **not modelled**. The print `PER`
  completes in one shot rather than transferring byte-by-byte, so:
  * Capture reads the order block at `rV2` heuristically — for the funktionalcpu
    `report_and_end` PER (whose order block is `68 80 01 00 …`, all non-graphic)
    this renders as `...`, i.e. *not* a decoded line. Glyph rendering itself is
    correct (the GE graphic set, `gecode.c`); what's missing is the channel-2
    transfer that would point capture at the actual character buffer.
  * The **keyboard input queue is filled but not yet consumed** by the CPU —
    channel-2 *input* transfer is also unmodelled. Typed text is echoed locally
    in the chat for now; wiring it into a channel-2 read is future work, and will
    also need the inverse ASCII → GE-graphic encoding at the feed boundary.
* The funktionalcpu deck, after the completed print PER, takes its
  `report_restart` branch (the final 40-char signature `CMC` at `0x19d4`
  compares `mem[0x1100]` vs `mem[0x0036]` and mismatches) — i.e. it loops as a
  continuous self-test. That is **deck logic, not a printer issue**: the End HLT
  `0x19DE` is only reached when that signature matches.

## Tests

`tests/printer.c`:
* `printer.present_completes_channel2_per` — with the printer, the print PER
  completes (output captured) instead of parking at b8.
* `printer.absent_leaves_machine_waiting` — without it, the machine ends parked
  in the b8 wait (`rSO==0xb8 && rSA==0`), proving the model is inert unless
  registered.
* `printer.keyboard_queue` — `printer_feed_key` enqueues correctly.
