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

