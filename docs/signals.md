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

---

## 1. Card reader ↔ CPU/channel — the COCA connector

The card reader (LS 600 / GIS 450 controller) attaches on **connector 2**. COCA is
the controller↔channel connector (CRZ schematics, dwg 14112 0781). These are the
target pins for the signal-level model. Where gemu already carries the line, the
"gemu" column names the signal/field; otherwise it is the next thing to wire.

### 1.1 Reader → CPU/channel

| Pin | Type | Meaning | gemu | Effect / condition in the model |
|-----|------|---------|------|----------------------------------|
| `LU00N`–`LU07N` | data | transcoded character bits (8) | ◑ `integrated_reader.data` (a byte) | The presented character. Today carried as one byte, not 8 separate lines. The channel read latches it into `RO` and stores to `mem[V]`. |
| `LU08N` | handshake/clock | **character-ready strobe** | ✅ `integrated_reader.lu08` (`LU081`) | "a byte is on the data lines." While set, the channel input cycle (`b9`/`b1`, cmd `CI34` `NE→RO→Mem`) reads it; the peripheral clears it after consumption. The single most important read handshake. |
| `FININ` | handshake | end-of-read, raised with the **last** character | ✅ `FINI1` (`reader_get_FINI1`) | The controller "end" (→ `RIG1`). Bounds the transfer at the physical card boundary: drives the load-end sequence (`b9→ea→eb→e3`) instead of continuing. |
| `FIDEN` | status | end-of-sequence (all data transferred) | ☐ | Reader has returned to idle after the record; would clear the in-transfer state and let `LUPOR` reassert. |
| `LUPOR` | status | reader free / ready | ✅ `LUPO1` (`reader_get_LUPO1`) | Unit idle and able to accept a command; gates the start of a new read. |
| `LUREN` | status | error (transcoder / jam) | ☐ | Fault → would raise a peripheral-error / interrupt condition and abort the transfer. |
| `LUSEN` | status | out-of-service | ☐ | Unit offline; a PER to it should complete as unit-not-ready rather than wait. |
| `LENON` | status | manual-mode active | ☐ | Operator has the reader in manual; inhibits automatic feed. |
| `BI20` | clock | binary-read aux clock — 2nd nibble, ~15 µs after `LU08` | ☐ | In binary/by-pass mode the column is read as two sub-reads; `BI20` strobes the second. (gemu currently reconstructs a full byte per column via the packed nibble-pair feed; see §3.) |
| `POM01` | status | binary-mode indicator | ☐ | High ⇒ transcoder is in binary (by-pass) read; selects raw 12-row column image vs GE char code. |
| `PICON` | handshake | first-column check | ☐ | Marks the leading column of a card; used to align/validate the start of a record. |

### 1.2 CPU/channel → reader

| Pin | Type | Meaning | gemu | Effect / condition in the model |
|-----|------|---------|------|----------------------------------|
| `RE00N`–`RE08N` | data (8+parity) | character data toward the reader register | ◑ `rRE` (connector-name byte) | The byte the CPU presents (output / command code). 8 data + odd parity. |
| `TU00N` | timing | read-strobe clock for `RE` data | ☐ | Clocks `RE` data into the reader. |
| `TU03N` | command | card-feed / advance clock | ☐ | Steps the card past the read station (next card / next column). |
| `N001`,`N002` | command | normal-mode read decode (GE char code) | ☐ | Selects Hollerith→GE transcoding. |
| `DEBI` | command | binary-read mode decode (by-pass) | ◑ (by-pass implied by `TC_COLBIN`) | Selects raw column-binary read; the loader's "set by-pass" PER asserts this on the real machine. |
| `MI01`,`MI02` | command | mixed-mode read decode | ☐ | Selects mixed transcoding. |
| `RIFAN` | command | card reject / eject | ☐ | Ejects the current card to the reject stacker. |
| `REGEN` | control | general clear / reset | ◑ `ge_clear` resets reader latches | Clears reader/controller latches on power-up / error recovery. |
| `SESEN` | control | put-in-manual | ☐ | Forces the reader to manual mode. |
| `COCON` | clock | mode-select clock to the transcoder | ☐ | Latches the selected read mode (`N001/N002/DEBI/MI01/MI02`) into the transcoder. |

---

## 2. CPU channel & sequencer signals (the transfer machinery)

These are the internal signals that drive a peripheral transfer; the reader pins
above feed into them. Implemented in `signals.h` (`SIG(...)`) and `struct ge`.

| Signal | Meaning | Effect / condition |
|--------|---------|--------------------|
| `RC00` | CPU-active cycle request | When set, `NA_knot` routes the program sequencer (`rSO`); dropping it at a transfer-wait *freezes* the CPU until a channel request arrives. |
| `RC01` | channel-1 cycle request (async) | Reader/connector-1 wants a memory cycle. |
| `RC02` | channel-2 cycle request (async) | Integrated reader (`LU08`-derived) **and** the printer (OR'd via `RIMZA`) want a channel-2 cycle. **Key for the reader-on-channel-2 read.** |
| `RC03` | channel-3 cycle request (async) | Connector-3 peripheral. |
| `RIA0`/`RESI`/`RIA2`/`RIA3` | synchronous request, latched from `RC0x` at `TO00` | Stage the async request into the cycle-assignment logic. `RIA2 = RC02` latched. |
| `RES0` | program/CPU cycle assigned | `= !RESI & ...` — this cycle belongs to the CPU sequencer. |
| `RES2` | **channel-2 cycle assigned** | `= !RIA3 & !RESI & RIA2`. When true, `NA_knot` routes `rSI & 0x0f` — i.e. the cycle runs a **channel-2 sequencer (`rSI`) state**. This is the gate the channel-2 reader transfer needs. |
| `RES3` | channel-3 cycle assigned | Routes channel-3 (`rSV`/connector-3). |
| `RIUC` | micro-cycle / executing-state assignment | Routes the executing state (`rSA`). |
| `rSO` | program + channel-1 sequencer state | The main CPU state register. |
| `rSI` | channel-2 sequencer state | The channel-2 transfer micro-state; routed into `rSA` on a `RES2` cycle. Input states `0C/0E`; output `02/03`; end `0A/0B`. |
| `rSA` | the state actually executing this cycle (`NA_knot`) | `RES2 ⇒ rSA = rSI&0x0f`; `RES0/RIUC ⇒ rSA = rSO`; no request ⇒ `0` (idle/frozen). |
| `RASI` | channel-1 in-transfer flag | Set at org-phase `ab`; gates the integrated reader to present bytes during a channel-1 read (`b8/b9/b1`). |
| `RACI`/`RICI` | console / register-selector inhibit & step controls | Gate sequencer advance under console forcing. |
| `RIG1` | controller "end" (from `FINI`) | Terminates a record; steers `b9` to the load-end states. |
| `RIVE` | length terminal-count | End-of-transfer when the instruction length `L1` is exhausted (the count-based end, complementary to `RIG1`). |
| `RIMZA` | printer→`RC02` OR | Lets the integrated printer raise the channel-2 request without its own `RC0x`. |
| `DU97` | `= PUC2 ^ L2.3` | Gates state `b8`'s exit to alpha; with `PUC2` asserted the channel-2 external-request wait completes (used by the printer model). |
| `PUC2` | channel-2 unit-ready | Asserting it (with `RC00`) lets `state_b8`'s own microcode complete a parked channel-2 PER. |
| `CI34` | command: `NE → RO → mem` | The input-read store command in the channel input transfer (`b9` `TO50`). |
| `CE16` | command: "Load Printer Buffer" (`RO → channel-2 sink`) | The output emit command (`state_02`); hands `RO` to the printer. |
| `L204`/`L206`/`L207` | order-block control bits (`rL2`) | Direction/qualifier of the transfer: `L207` selects OUTPUT vs INPUT; `L204/L206` qualify the `b9` branch. |

---

## 3. Notes on the current model vs the pin-by-pin target

- **Channel-1 read** (`b8/b9/b1`, driven by `LU08`/`FINI`/`RASI`) is fully
  implemented and is the working integrated-reader read path. The bootstrap IPL
  uses it.
- **Channel-2 read** is the open gate: a loader PER that reads on channel 2 parks
  at `b8` *frozen* (`rSA=0`, no request); it needs `RC02 → RIA2 → RES2 →` an
  `rSI` **input** transfer state to un-freeze and read. Implementing the `rSI`
  input states (`0C/0E`) + having the reader assert `RC02` is the next step.
- **Data lines / modes**: today the reader presents a *byte* (`LU00N–LU07N`
  collapsed) and a `lu08` strobe; the read mode (`N001/DEBI/MI01`, `BI20`,
  `POM01`) is implied by the `transcode_mode` and the packed nibble-pair feed
  rather than driven as separate pins. The pin-by-pin goal is to split these out
  so the controller and CPU exchange the real lines.
- **Status lines** (`FIDEN/LUREN/LUSEN/LENON/PICON`) are not yet modelled; they
  become relevant for error/interrupt behaviour and faithful end-of-sequence.

Extend this table whenever a new signal is wired; keep the **Effect / condition**
column concrete (what it sets/clears/gates), not just a gloss.
