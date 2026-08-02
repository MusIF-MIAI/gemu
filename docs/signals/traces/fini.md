### 1.2b FINI signal trace — card-end through the Capitolo to RIG1

Traced 2026-08-01 via `ge120-follow-signal` skill. Source: cp06 schematics (ch.158 → ch.140 → ch.138).

**Role:** FINI is the "end of read" signal from the card reader. It bounds the transfer at the physical card boundary: drives the load-end sequence (`b9→ea→eb→e3`) instead of continuing. On the wire it rides the last character presentation and stays standing after the LU08 strobe drops.

**Signal family** (from signals index, cp06 p44):

| Signal | Generator (chapter-pin) | Role |
|--------|------------------------|------|
| FINI1 | 165-1 | Primary end-of-row pulse |
| FINI2 | 165-5 | Secondary end-of-row pulse |
| FINA1 | 165-3 | End-of-row acknowledge |
| FINA3 | 166-1 | End-of-row extended |
| FINE3 | 166-1 | End-of-row to connector logic |
| FINE4 | 166-4 | End-of-row final |

**Atlas backplane positions** (from `atlas/row_G_pinout_verified.csv`, `atlas/row_I_pinout_verified.csv`):

| Row | Slot | Board | Pin | Signal |
|-----|------|-------|-----|--------|
| G | 7 | R11N2A | 01 | FIN01 |
| G | 7 | R11N2A | 03 | FINAA |
| G | 7 | R11N2A | 05 | FINIB |
| G | 7 | R11N2A | 09 | FINOA |
| G | 7 | R11N2A | 11 | FINI |
| G | 7 | R11N2A | 16 | FINI2 |
| I | 7 | R11N2A | 24 | FINE3 |
| I | 7 | R11N2A | 26 | FINE4 |

**Signal path (hardware logic chain):**

1. **Origin — card reader sensors.** The card reader photoelectric sensors detect the end of each punched card row. FINI1/FINI2 are generated at chapter 165 (cap:165), board R11N2A. FINE3/FINE4 are generated at chapter 166.

2. **Chapter 158 (cp06 p227) — connector logic.** FINE3 and FINE4 enter chapter 158 as inputs to NAND gates that combine them with channel-select signals:
   - Gate 8 (NAND): `FINE4 + PC142 → PF34A → (148-4)` — routes to ch.148
   - Gate 9 (NAND): `FINE3 + PCI31 → PF13A → (140-4)` — routes to ch.140
   - Gate 11 (NAND): `FINE3 + PC331 → PF33A → (140-4)` — routes to ch.140

3. **Chapter 140 (cp06 p213) — Channel 1 Logic.** Gate 4 (NAND) combines the FINI-derived signals with other connector-end signals:
   - `OF12A (160-9) + PF13A (158-9) + PF14A (158-10) → RF10I → (138-3)`
   - RF10I = "OR of END condition from P.U. for channel 1" (official name: RF101)
   - RF10I is the signal that tells channel 1: "the peripheral unit says we've reached the end."

4. **Chapter 138 (cp06 p211) — Channel 1 end logic.** Gate 3 (NAOR) combines RF10I with RIME1 (length-based end condition):
   - `RF10I + RIME1 → RIG1A`
   - Gate 4 (NAND): `RIG1A → RIG16 → (009-3), (311-1), (311-5), (272-11), (207-5)`
   - RIG16 is the final "End from controller on channel 1" signal that terminates the transfer.

**In gemu:** The `RF101()` helper in `signals.h` is wired from the reader's FINI/FINE signals. The `RIG1` signal is the final latch that ends the channel-1 transfer. The current model short-circuits the full RF10I→RIG1A chain — `FINI1` directly drives `RIG1` via `reader.c`. The faithful path through ch.158→ch.140→ch.138 would add the channel-select decode (`PC142`, `PCI31`, `PC331`) and the length-based alternative (`RIME1`) to the end condition.

**Tags on ge120.xyz:** `FINI`, `FINE3`, `FINE4`, `RF101`, `RIG1`, `channel1`, `end-condition`, `connector`

---

