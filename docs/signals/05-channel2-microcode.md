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

