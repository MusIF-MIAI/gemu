# Hybrid → per-clock conversion checklist

Standing protocol for replacing a `EXEC_*` one-shot with transcribed per-clock
executive states. Written down so it is not re-derived (or re-argued) once per
family. Companion to `flowchart-sheets.md` (what is left) and
`transcriptions/` (the raw sheet reads).

## Ground rules

- **Never co-author commits. Never push.** Commit each step locally.
- Scanned decks are **"available separately"** — that is the only phrasing to
  use in commits, comments or docs. `../DUMP1` is the working drop and the
  source of record; `tests/decks.h` resolves it, `Site_Acceptance_Test/` is the
  fallback.
- A row that is **not** a transcription gets a comment saying so, in place.
  Currently two: `ss_hybrid_exit`, `beta_unclaimed` (`msl-states.c`).
- Inferences (clock placement, pass-through behaviour, overbar reads) get
  labelled as inferences at the definition site. Do not launder them into
  transcriptions.

## Per-family conversion sequence

1. **Transcribe first, convert second.** Sheet → `docs/transcriptions/<family>.md`
   with fo. numbers, before any code. Commit the transcription on its own if the
   conversion is going to be deferred.
2. **Check the datapath prerequisites exist.** The sheet names commands; the
   commands must already do the right thing. This is where AB/SB/AD/SD died
   once: `CI68` had no decimal path, so the conversion would have produced
   binary results where the machine does BCD. Grep the command implementations
   for every `CO/CI/CU/CE` the sheet issues before writing a single row.
3. **Add the family predicate** next to the others in `msl-states.c`, and
   **remove those opcodes from `ss_hybrid_family`** in the same commit — the
   two must move together (see traps).
4. **Common vs family rows.** Rows with no family term on any sheet go in the
   state's common chart; family rows carry their own gate. A test enforces that
   no family row repeats a `(clock, command)` pair already in common.
5. **Check for common-row conflicts.** If the family's sheet gates a command
   that the common chart fires unconditionally, the *common* row must gain a
   term. Known pending pair:
   - `exec_50` common `{TO70, CI68}` — fo.142 gates it `{AD+SD+AB+SB+CMQ}`,
     **MVQ excluded**.
   - `exec_40` common `{TI06, CU01}` — fo.143 gates it `{L1₁ = 1i}` for the
     algebra family (selects 62 over 60 for the zero-extension pass).
   These silently change *other* working families. One deck diff each.
6. **Extend the fetch/exit predicates** the family needs (e.g.
   `exec60_fetches_source`).
7. Update `flowchart-sheets.md` (mark the family `*`) and the memory note.

## Acceptance criteria

Every one of these, every conversion:

- `make clean && make check` green. **`make clean` is not optional** — a stale
  `.o` once produced a six-test phantom regression that bisected to nothing.
- funktionalcpu `*0x0E00=0x40` terminates **`HALT PO=0x1427 step=27 mstep=65`**.
- **The cycle count will move**, and that is the point (1.31M → 1.41M → 1.59M →
  1.83M → 2.05M across prior conversions). Update `want_cyc` in
  `tests/roundtrip.sh` *deliberately*, only after confirming the step trace is
  unchanged.
- Step-trace diff against the previous commit:
  ```sh
  git archive HEAD | tar -x -C "$T" && make -C "$T" libge.a
  # link funkharness against both, then filter -- raw stdout is ~7 GB
  ... | grep -E "^\[[0-9]+\] (step|mstep|HALT)"
  ```
  All 159 step/mstep/HALT lines must match, cycle numbers included, unless the
  conversion is expected to change them — in which case say which and why.

## Traps, learned the expensive way

- **First-match exclusivity is gone.** The old variant matrix was ordered and
  silently shadowed overlapping predicates. `is_ss_data_op` still lists
  converted opcodes; only `ss_hybrid_family`'s `!ss_byte_loop` guard keeps them
  from running the one-shot *and* the executive loop. That failure produces
  **correct results in far fewer cycles** — no assertion in the suite notices.
  The pinned `want_cyc` is the only thing that catches it.
- **Forward declarations.** `msl-states.c` is order-sensitive; predicates used
  by the beta chart must be declared above it.
- **Read the whole gate.** Two corrections came from mis-read arity/inputs
  (gate 27's third input `CI461` flipped `UCO01` from binary to decimal add).
  When a signal's meaning hinges on a gate, have the trace confirmed.
- **TAB.1 rows are per-configuration.** `FUL4G` was wrong because it was read
  off the "no interrupts" rows only.

## Roadmap

Order is roughly cheapest-first; each needs its own transcription + prerequisite
check.

- [ ] **AB/SB/AD/SD/MVQ/CMQ** (fo.126-128) — transcribed; UA decimal landed
      (`fdc1afe`). Blocked only on the two common-row conflicts above.
- [ ] **SR/SL** (fo.132-134) — transcribed; needs the search datapath. EA/EB
      tail is ready.
- [ ] MVP/CMP/AP/SP (fo.96-105) — AP/SP has the recomplement tail.
- [ ] MP/DP (fo.106-118) — 4-state codes, largest family.
- [ ] PK/UPK/PKS/UPKS (fo.118-126).
- [ ] TL/EDT (fo.130-132).
- [ ] **TR** (fo.134-136) — absent from `opcodes.h` entirely; needs the opcode
      added before the states.

## Open questions, parked

- `CI89 SET ALTO {FUL4}` at TO80 is printed with **no overbar**. Taken
  verbatim it halts a MAX machine at `PO=09b8`. Unresolved; not committed.
- `beta_unclaimed` cannot retire until CU10/CU12 partial commands are
  transcribed.
- `CI` → `OI` rename to match the manual's naming.
- 32K memory capacity is reported at startup but not enforced.
