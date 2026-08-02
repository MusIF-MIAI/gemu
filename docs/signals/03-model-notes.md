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

### 3.1 Where the model stops: commands, not gates

gemu models the machine at the level the **timing sheets** print: per clock
phase, a list of micro-commands with the gate each is printed under. It does not
model the combinatorial network *underneath* a command. Raising `CO18` sets the
NO-knot forcing mode directly; the real chain
`CO181 → CT2O1 → CA2O1 → NOG0A..NOGFA → NO` has no counterpart in the code, and
neither do the per-bit NAOR gates that OR a forced bit onto whatever the register
selection is driving. The knot's forcing is a whole-value substitution instead
(see `NO_knot()` in `signals.h`).

That boundary is deliberate. [traces/register07.md](traces/register07.md) §20–21
now gives most of the network — `CA2O1 = !CT2O1`, `CT2O1 = NOFA1 | CI2O1 | T0302`,
and all sixteen `NOGxA = CA2O1 | CA181/CA191 | CO9x1` — but **`CA191` is still
only named, not defined** ("gating signal for upper byte forcing"), and the
`CO9x1` gates it does give are wide NANDs over decodes we do not model either
(`CO911 = !(DI35A · EG60A · ED07A · DI11A · EG63A · EG68A)`). Transcribing half a
network and guessing the rest would produce something that looks gate-accurate
and is not; the simplification is at least honest about its edge.

The boundary is safe as long as **no state needs a forcing to survive past the
`BO`/`VO` latch at TO20**, where `pulse.c:on_TO20` clears `kNO.forcings` and
`kNO.force_mode`. Every current state builds its forced address at TO10 and reads
it out at TO20; the datum commands (`CI11` at TO30, `CI33`/`CI32` at TO50) run
afterwards on an unforced knot, which is exactly what makes JRT deposit `V1`
rather than `0xFF`. That invariant is pinned by
`tests/register07.c:forcings_do_not_outlive_the_vo_latch` — if a future sheet does
need a forcing later in the cycle, that is the test to make fail first,
deliberately.

**A trap worth naming.** `CO90` (force NO bit 0 = the odd, low byte of a register
cell) is gated by `DI65A0` in the **ED|EC** indexing state and by `DI11A0` in
**EA|EB**. Those are different state decodes, not a transcription slip: `DI11A0`
is true exactly for `SA` in `e8..eb`, and `DI65A0` is false across that whole
band. Re-gating EA's `CO90` on `DI65A0` drops bit 0 from the forced address, so
JRT's link goes to `mem[254]/mem[253]` instead of `mem[255]/mem[254]` and every
subroutine return lands somewhere else. Verified by mutation; guarded by
`tests/register07.c:the_ea_eb_forcing_gate_is_a_state_decode`, and set out in
[traces/register07.md](traces/register07.md) §15.

Note also that inside EA|EB the `DI11A0` gate discriminates nothing (it is a pure
state decode, true throughout). It is transcribed because the sheet prints it,
and kept for traceability back to the page.

---

