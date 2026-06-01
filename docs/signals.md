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

---

## 4. GE-120 official signal index (signal dictionary)

Transcribed from the official GE-120 signal index (NOME/NAME · CAPITOLO/CHAPTER ·
SCATOLA/BOX · descrizione IT/EN). `Ch` = schematic chapter, `Bx` = box on that
sheet — the authoritative place to find the signal's logic. `gemu` marks what we
model: ✅ implemented · ◑ partial/abstracted · ☐ not yet.

This is the reference for the channel-transfer and reader work: the channel-2
read needs `RC021`→`RIA21`→`RES26` and terminates on `RUF26`/`RIG?`/`RIL?`.

### 4.1 Registers (bit groups)

| Name | Ch | Bx | Meaning (EN) | gemu |
|------|----|----|--------------|------|
| `RO006`,`RO016`,`RO026`,`RO036`,`RO046`,`RO056`,`RO066`,`RO076`,`RO081` | 081–086 | — | Bits of **RO** memory register | ✅ `rRO` |
| `RI002`,`RI012`,`RI022`,`RI032`,`RI042`,`RI052`,`RI062`,`RI072` | 047,050 | — | Bits of **RI** register | ✅ `rRI` |
| `RA001`…`RA081` | 121 | — | Bits of **RA** register | ✅ `rRA` |
| `RE002`…`RE082` | 153–155 | — | Bits of **RE** register | ✅ `rRE` |
| `SO002`…`SO072` | 106,107 | — | Bits of **SO** future-status register | ✅ `rSO` |
| `SIO01`,`SIO11`,`SIO21`,`SIO31` | 108 | — | Bits of **SI** future-status register | ✅ `rSI` |
| `SAO06`…`SAO76` | 110,111 | — | Bits of **SA** present-status register | ✅ `rSA` |
| `FO006`…`FO076` | 104,105 | — | Bits of **FO** function register | ✅ `rFO`/`ffFO` |
| `FIO01`…`FIO61` | 112–114 | — | Bits of **FI** conditions register | ✅ `ffFI` |
| `FAO06`,`FAO16`,`FAO36`,`FAO46`,`FAO56`,`FAO66` | 112–114 | — | Bits of **FA** conditions register | ✅ `ffFA` |
| `SOCO1` | 116 | 15 | Permission to load SO | ◑ |
| `FOS1A` | 278 | 2 | Forces S1 loading | ☐ |

### 4.2 Channel / cycle-request machinery (the transfer core)

| Name | Ch | Bx | Meaning (EN) | gemu |
|------|----|----|--------------|------|
| `RC001` | 130 | 2 | **Async** storage for **C.P.U.** cycle request | ✅ `RC00` |
| `RC011` | 129 | 4 | **Async** storage for **channel-1** cycle request | ✅ `RC01` |
| `RC021` | 129 | 11 | **Async** storage for **channel-2** cycle request | ✅ `RC02` |
| `RC031` | 129 | 18 | **Async** storage for **channel-3** cycle request | ✅ `RC03` |
| `RIA01` | 131 | 3 | **Sync** storage of **C.P.U.** cycle request | ✅ `RIA0` |
| `RIA21` | 131 | 19 | **Sync** storage of **channel-2** cycle request | ✅ `RIA2` |
| `RIA31` | 131 | 22 | **Sync** storage of **channel-3** cycle request | ✅ `RIA3` |
| `RESO6` | 131 | 8 | Cycle assignment to **C.P.U. or channel 1** | ✅ `RES0` |
| `RESI6` | 131 | 7 | Cycle assignment to **channel 1** | ✅ `RESI` |
| `RES26` | 131 | 14 | Cycle assignment to **channel 2** | ✅ `RES2` |
| `RES36` | 131 | 17 | Cycle assignment to **channel 3** | ✅ `RES3` |
| `RIUC1` | 131 | 5 | Cycle assigned to **C.P.U.** (executing micro-cycle) | ✅ `RIUC` |
| `RETO6` | 132 | 3 | Assign-to-CPU/channel-1 cycle **stored** (staticized) | ◑ |
| `RET26` | 132 | 8 | Assign-to-CPU/channel-2 cycle **stored** (staticized) | ◑ |
| `RA101` | 140 | 8 | OR of **char-exchange request, channel 1** | ◑ `RA101`→`RC01` |
| `RA301` | 148 | 8 | OR of **char-exchange request, channel 3** | ☐ |
| `RAS12` | 136 | 12 | **Data transfer by channel 1** | ◑ (`b8/b9/b1`) |
| `RB101` | 140 | 1 | OR of trigger TE30 channel 1 | ☐ |
| `RB301` | 148 | 1 | OR of trigger TE30 channel 3 | ☐ |
| `RIMZA` | 143 | 13 | **MZ printer cycle request** (printer→`RC02` OR) | ◑ (printer raises `RC02`) |
| `REAB2` | 143 | 15 | **General logic reset of channel 2** | ☐ |
| `RAC16` | 140 | 18 | Storage of **rejected command** | ☐ |
| `RAMO2` | 133 | 16 | Conditioning signal — C.P.U. internal speed | ☐ |
| `RATE1` | 141 | 4 | Emission of selection trigger P.U. AEBE | ☐ |
| `RAV12` | 140 | 14 | Emission of signal VICU | ☐ |

### 4.3 Transfer end / length / status

| Name | Ch | Bx | Meaning (EN) | gemu |
|------|----|----|--------------|------|
| `RUFI2` | 139 | 20 | **End of data exchange on channel 1** | ◑ |
| `RUF26` | 143 | 7 | **End of data exchange on channel 2** | ☐ ← needed for ch-2 read termination |
| `RUF32` | 147 | 20 | End of data exchange on channel 3 | ☐ |
| `RUSC6` | 148 | 14 | Data exchange in output on channel 3 | ☐ |
| `RIG16` | 138 | 4 | **End from controller on channel 1** (`RIG1`) | ✅ `RIG1` |
| `RIG36` | 146 | 4 | End from controller on channel 3 | ☐ |
| `RIL11` | 138 | 13 | **End from length on channel 1** | ◑ (`RENIA`/`RIVE`, TODO) |
| `RIL31` | 146 | 13 | End from length on channel 3 | ☐ |
| `RIVEF` | 138 | 11 | **Condition of end of transfer on channel 1** (`RIVE`) | ✅ `RIVE` |
| `RIVAF` | 146 | 1 | Condition of end of transfer on channel 3 | ☐ |
| `RF101` | 140 | 4 | OR of "END condition" from P.U. for channel 1 | ☐ |
| `RF301` | 148 | 4 | OR of "END condition" from P.U. for channel 3 | ☐ |
| `RM101` | 140 | 12 | "Out-of-service" condition for channel 1 | ☐ (cf. `LUSEN`) |
| `RM301` | 148 | 12 | "Out-of-service" condition for channel 3 | ☐ |
| `RER12` | 139 | 5 | Odd-parity error in input, channel 1 | ☐ |
| `RER32` | 147 | 5 | Odd-parity error in input, channel 3 | ☐ |
| `RESC1` | 123 | 4 | Odd-parity error input data on **channel 1 or 2** | ☐ |
| `RINT6` | 141 | 9 | **Interruption present** | ✅ `RINT` |
| `RIND6` | 148 | 18 | Counts for decreasing addresses, connector 3 | ☐ |
| `RICO2`/`RICI2` | 142 | 7,18 | Differential counter for MZ printer | ☐ |
| `RICS1` | 145 | 13 | Counter permission for emission of TUO4 | ☐ |
| `RUCO2`/`RUC12` | 145,133 | 22,1 | Counter for TUO4 emission | ☐ |
| `RINO1`/`RIN11` | 144 | 21,24 | Information buffer for emission TUO2 | ☐ |

### 4.4 RO-decode conditions (`RG0x1`/`RG1x1`, ch.122–123) and length decodes

| Name | Bx | Decodes RO for condition(s) |
|------|----|------------------------------|
| `RG001` | 3 | PEOO, FUPO, LUPO |
| `RG011` | 7 | SEGE, LURE |
| `RG021` | 11 | FISE, SAFE, FIDE |
| `RG031` | 13 | EGOL, SAFI |
| `RG041` | 16 | MAPE |
| `RG051` | 18 | TESE, FIDA |
| `RG061` | 21 | MARE, LUSE, FUSE |
| `RG071` | 5 | MATE |
| `RG081` | 9 | CAPE |
| `RG091` | 2 | IGOL |
| `RG101` | 6 | NU10 |
| `RG111` | 9 | NU20 |
| `RG121` | 12 | NU30 |
| `RG131` | 14 | SECO, FU22, LENO |
| `RG141` | 16 | ERCA + input-parity error |
| `RL1U1` | 128/4 | Decode **L1 all "ones"** |
| `RL301` | 128/6 | Decode **L3 all "zeroes"** |

### 4.5 Peripheral / connector lines (FU/FI/FA/SE/SA "bocchettone" = connector)

| Name | Ch | Bx | Meaning (EN) | gemu |
|------|----|----|--------------|------|
| `FU00A`–`FU08A` | 005,006 | 7,5 | **Input bits from photodisc, connector 1** | ☐ (printer photodisc) |
| `FU09A` | 005 | 4 | Photodisc code strobe, connector 1 | ☐ |
| `FU22A` | 005 | 5 | Condition "not operable", connector 1 | ☐ |
| `FUPOA` | 006 | 5 | Condition "availability", connector 1 | ☐ |
| `FUSEA` | 006 | 7 | Condition "out-of-service", connector 1 | ☐ |
| `FIDAA` | 006 | 7 | "Almost end of paper", connector 1 | ☐ |
| `FIDEB` | 006 | 3 | "End of file", connector 2 | ☐ (cf. reader end-of-deck) |
| `FIFEC`/`FIFED` | 007,008 | 2 | Information bit in **input**, connector 3/4 | ☐ |
| `FIFUA`/`FIFUC`/`FIFUD` | 154 | 21,12,17 | Information bit in **output**, connector 1/3/4 | ☐ |
| `FINO1` | 006 | 5 | "Out-of-service", connector 1 | ☐ |
| `FINAA`/`FINIB`/`FINEC`/`FINED` | 006–008 | 3,4 | END condition, connector 1/2/3/4 | ◑ (`FINI` family) |
| `FINUA`/`FINUC`/`FINUD` | 162,163 | 6,2,13 | Command FINU, connector 1/3/4 | ☐ |
| `FIRUA` | 162 | 8 | Trigger, paper-brake release, connector 1 | ☐ |
| `FISEC`/`FISED` | 007,008 | 3 | Condition of P.U., connector 3/4 | ☐ |
| `SAFIA`/`SAFEA` | 006 | 7 | "End of sheet" 2nd/1st trailer, connector 1 | ☐ |
| `SECOC`/`SECOD` | 007,008 | 3 | Manual condition, connector 3/4 | ☐ (cf. `LENON`) |
| `SEGEC`/`SEGED` | 007,008 | 3 | Condition of connector 3/4 | ☐ |
| `SEPE1` | 135 | 21 | Selection of connector 1 | ☐ |

### 4.6 Configuration jumpers ("fiscelle") and misc

| Name | Ch | Bx | Meaning (EN) | gemu |
|------|----|----|--------------|------|
| `FEL06`/`FELI6` | 002 | 4 | Connections for **C.P.U. cycle-period** choice | ◑ (timing fixed) |
| `FUL26`/`FUL36` | 002 | 3 | Connections for **program-loading connector** choice (`FUL2`/`FUL3`) | ✅ `FUL2`/`FUL3`=1 |
| `FUL46` | 217 | 1 | Connections to enable additional performances | ☐ |

**Note on suffix digits:** the index names carry a trailing form/rev digit (e.g.
`RES26`, `RIA21`, `RC021`); gemu uses the base mnemonic (`RES2`, `RIA2`, `RC02`).
The `Ch/Bx` columns point at the GE schematic sheet to consult when wiring the
exact logic equation for a signal we promote from ☐/◑ to ✅.

### 4.7 Connector/channel selection & peripheral status (PC/PE/PU)

| Name | Ch | Bx | Meaning (EN) | gemu |
|------|----|----|--------------|------|
| `PCOV6` | 128 | 11 | Network output, external-condition examination | ☐ |
| `PC111`/`PC121`/`PC131`/`PC141` | 159,156,157 | — | Connector 1/2/3/4 selection, **channel 1** | ◑ |
| `PC211`/`PC221` | 160 | 5,1 | Connector 1/2 selection, **channel 2** | ◑ |
| `PC311`/`PC321`/`PC331`/`PC341` | 161,156,157 | — | Connector 1/2/3/4 selection, **channel 3** | ☐ |
| `PUC16`/`PUC26`/`PUC36` | 136 | 8,18,29 | **Channel 1/2/3 selection** | ◑ (`PUC2`) |
| `PUOO2` | 135 | 22 | Connector 2 selection | ◑ |
| `PUOOC`/`PUOOD` | 163,162 | 3,7 | Connector 3/4 selection | ☐ |
| `PELS1` | 156 | 15 | **Card reader connected to connector 2** | ◑ (the integrated reader) |
| `PELM6` | 161 | 4 | **Magnetic reader** connected to connector 2 (`PELM` in flowchart: selects `V4−1` vs `V4+1`) | ☐ (gemu = card reader, `+1`) |
| `PELSA` | 125 | 15 | "Out-of-service" condition of the **integrated reader** | ☐ (cf. `LUSEN`) |
| `PEST1` | 134 | 24 | Odd-parity error on input character | ☐ |
| `PEOOC`/`PEOOD` | 007,008 | 3 | "Availability" from connector 3/4 | ☐ |
| `PEBIA`/`PEBAA`/`PEBEA`/`PEBUA` | 134 | 19,10,13,22 | Selected connector 3/1/2/4 **busy** condition | ☐ |
| `PUBO6` | 134 | 14 | Selected-connector busy condition | ☐ |
| `PEC11`/`PEC21`/`PEC31`/`PEC41` | 137,136 | — | Stores **reset** conditions, channel 1/2/3/(1) | ☐ (cf. `REAB2`) |

> The page-36 flowchart's **`PC22`** ("reader, integrated controller, on channel
> 2") is the reader-present decode that branches the channel-2 transfer: after the
> input state `0C`, `PC22`-YES (reader) returns to `B8` for the next request;
> `PC22`-NO (printer) goes to `04|06`. It corresponds to the `PELS1`/`PELM6`
> reader-on-connector-2 selection (vs the printer). `PELM` selects the addresser
> direction (`V4−1` magnetic vs `V4+1` card/photo).

### 4.8 Arithmetic unit (UA)

| Name | Ch | Bx | Meaning (EN) | gemu |
|------|----|----|--------------|------|
| `UA001`…`UA071` | 091,094 | — | Output bits of the arithmetic unit | ✅ (ALU result) |
| `UAZO6` | 116 | 4 | **Decode UA 00+07 = "all zeroes"** (the `UAZO` in the ch-2 flowchart) | ◑ (ALU zero flag) |
| `URO31`/`URO71` | 090,093 | 21 | Carry from A.U. bits 00+03 / 04+07 | ◑ |
| `URPE6` | 091 | 3 | Carry going **into** the A.U. | ◑ |
| `URPU2` | 094 | 3 | Carry coming **out** of the A.U. | ◑ |

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
(input) is the next to implement — `CO14/CO41/CO04/CI34/CO31` are all available
commands; the reader drives it by asserting `RC02` (→`RES2`) and presenting the
byte on `NE_knot` (`integrated_reader.data` via the `PIB21` reader-input bit),
with the per-byte loop returning to `B8` and termination on the card-end (`FINI`
→ `RUF26`).
