# Operating the GE-120 Console

A modern operator's guide to the GE-120 front panel and to the three consoles
shipped with **gemu** (the headless CLI, the ncurses `--tui` client, and the
in-browser WebAssembly panel).

Every control, lamp and procedure below is drawn from the original GE
documentation. Sources are cited inline as
`CPU[n] <section> fo.<folio>` where `CPU[n]` is the descriptive-manual volume and
`fo.` is the drawing/folio number; confidence and OCR caveats are noted where the
scan is unreliable.

> **Primary sources**
> - `CPU/GE 120 CENTRAL PROCESSOR [4].pdf` §3 *Operator Panel*, §4 *Maintenance
>   Panel* — drawing **30004122**, fo.30–37.
> - `CPU/GE 120 CENTRAL PROCESSOR [1].pdf` — diagnostic organisation / loading.
> - Reproduced and regression-tested in gemu: `tests/cpu.c`
>   (`cpu_isolation.test_k`, `cpu_isolation.oper_call_by_register_forcing`).

---

## 1. The two panels

The GE-120 console is physically two panels (CPU[4] dwg 30004122, fo.30–34):

| Panel | Audience | Purpose |
|-------|----------|---------|
| **Operator panel** | normal operation | load + run programs, observe HALT / OPER CALL, set the two program-readable switches |
| **Maintenance panel** | field engineering | force and display every internal register and memory, single-step the microsequencer, stop on jump conditions, inhibit error stops |

The maintenance panel only comes alive when one of its switches is inserted **or**
the `LAMPS` switch is at `ON`/`DIAG` (CPU[4] §4.3, fo.37). In `DIAG` the unit is
put in diagnostic mode and `MAINT ON` is lit.

**`MAINT ON` — observed on the restored machine, 2026-07-29.** The lamp says the
maintenance panel has the machine, and it is lit when

```
(any maintenance switch inserted  OR  register selector off NORM)  AND  stopped
```

The nine maintenance switches are the ones in §4.3 — `PAPA`, `PATE`, `RICI`,
`ACOV`, `ACON`, `INAR`, `STOC`, `INCE`, `SITE` — **and the sixteen `AM` forcing
toggles**, which are switches on that panel too: setting one up is the engineer
preparing a value to force. The operator panel's `SWITCH 1` /
`SWITCH 2` are **not** part of it: those are program-readable (`JS1`/`JS2`) and
belong to the running program, not to the engineer. Turning the rotary off `NORM`
is enough on its own — that is what arms the forcing cycle `START` would perform
(§4.2), which is exactly when an operator wants telling. A running machine keeps
the lamp dark whatever the panel is doing.

Modelled in `console.c` (`ge_fill_console_data`); test
`console_fidelity.maint_on_lamp`.

---

## 2. Lamps and indicators

### 2.1 Operator-panel lamps (CPU[4] §3, fo.31–33)

| Lamp | Colour | Meaning | gemu field |
|------|--------|---------|------------|
| **HALT** | white | machine stopped — by `HLT`, by STEP-BY-STEP, or from the maintenance panel | `ALTO` |
| **OPER CALL** | — | operator-call request raised by the program (`ALAM`, set by the `ALAM`/`LON` path → `CI87`) | `ALAM` |
| **SWITCH 1 / SWITCH 2** | white | position of the two program-readable switches; lit when the switch reads logic `1` (the value that makes `JS1`/`JS2` jump) | `JS1` / `JS2` |
| **I** | — | interrupt enabled | `INTE` |
| **C1 / C2 / C3** | — | channel-busy / peripheral-connector status | `PUC1/2/3` |
| **OF / NZ / IM / JE** | — | condition flags (overflow, non-zero, …) and jump-enable | `FA` bits / `JE` |
| **LOAD 1 / LOAD 2** | white (double) | which of the two install-time load units is selected | `ALOI` |

### 2.2 Maintenance-panel display (CPU[4] §4.2, fo.34)

Bit lamps mirror internal registers directly:

| Lamps | Register |
|-------|----------|
| `R000`–`R008` | **RO** |
| `S000`–`S007` | **SO** (microsequencer state) |
| `FA00`–`FA03` | low 4 bits of **FA** |
| `UR` | the **URPE** flip-flop |
| `B1`–`B4` | selection of the four connectors |
| `SA00`–`SA07` | **SA** (next-state latch) |

The **BO** bus drives the rotary-selected register onto the display while the
machine is stopped (see §5). gemu surfaces it as `ADD_reg` (`ge->rBO`), `OP_reg`
(`ge->rFO`) and `RO`.

### 2.3 LAMPS CHECK

A momentary key that lights every console lamp for a bulb test (CPU[4] §3.2). It
must not be pressed during machine operation. On the panel it shares a button
with the `MAINT ON` lamp — `MAINT.ON` above, `LAMPS CHECK` below — so pressing
that button lights the whole console for as long as it is held.

Modelled as `ge->lamps_test`: `ge_fill_console_data` overwrites every lamp while
it is set, so the ncurses panel and the browser panel test the same lamps from
the same place, and nothing in the CPU is disturbed. Test
`console_fidelity.lamps_check_lights_everything`.

---

## 3. Operator controls

| Control | Type | Behaviour (CPU[4] §3.3, fo.31–33) |
|---------|------|-----------------------------------|
| **CLEAR** | key | Stops everything in the subsystem, clears all error conditions, presets CPU + peripherals to a defined state. Required after `MEM CHECK` and after power-on. No lamp. In gemu: `AINI`/`ALAM`/`PODI`/`ADIR`, `ALTO` set, `RC00`-`RC03`, the reader's command/mode latches and `LUREN`, the **fault latches MEM CHECK and INV ADD**, the `FI`/`FA` condition flip-flops, and the **sequencer, preset to the display state** (§4.2). Not core, and not `PO` — the first `START` after `CLEAR` runs the program from where its addresser is parked. |
| **LOAD 1 / LOAD 2** | switch | Selects one of two peripheral units enabled at install time for program loading (Conn.2/3, Conn.4/3, or Conn.2/4 — CPU[4] fo.43). |
| **LOAD** | key | *Arms* the bootstrap and does nothing else: it sets the `AINI` flip-flop. No card moves, no lamp lights. The next `START` is what reads. |
| **START** (HALT) | key | Starts operation. The white **HALT** lamp shows the machine is stopped. First `START` after `CLEAR`: runs the program if no other switch is set; runs the **load** if `LOAD` was pressed. |
| **STEP-BY-STEP** | switch | Executes one instruction per `START`. Mid-run, it makes the program stop at the end of the current instruction. **`INS` inhibits it; `ENS`, `CLEAR`, or the maintenance-panel `STOC` switch re-enable it.** White lamp = inserted. This is the operator panel's own switch, signal `ASIN`, and it is **not** the maintenance panel's `PAPA` — see the note below. |
| **SWITCH 1 / SWITCH 2** | switch | Two general-purpose switches the program reads via the `JS1` / `JS2` instructions. |

> **Loading sequence (CPU[4] §3.3 / §5.3):** `CLEAR` → select unit (`LOAD1`/`LOAD2`)
> → `LOAD` → `START`.
>
> **What `START` actually reads: one card.** The `80 → c8 → … → e3` walk issues a
> single "read unchanged" order with `L1 = 0x80` to `V1 = 0x0000`, and the
> channel packs two presented nibbles per byte — so 80 columns become 40 bytes
> at address 0, and the machine executes them. It does not read a deck. The card
> does: `software/loader.txt` is the original bootstrap listing and it is exactly
> 40 bytes (`0x0000-0x0027`) of two `PER` reads and a `JU 0x0028`. Verified on
> the bench, July 2026.

> **STEP-BY-STEP and PAPA are two separate circuits.** They have the same
> apparent effect — the machine stops and each `START` advances it — but they are
> independent, and only one of them has a lamp. From the `ALTO` set conditions
> (CPU[4] fo.115):
>
> | | signal | path | stops after | program can inhibit? | lamp |
> |---|---|---|---|---|---|
> | **STEP-BY-STEP** (operator panel) | `ASIN` | `CI891` at `E2`/`E3` of alpha — the command `HLT` itself uses | each **instruction** | yes: `INS` sets `ADIR`, `ENS`/`CLEAR` clear it, `STOC` overrides | **yes** |
> | **PAPA** (maintenance panel) | `AMICB` | `ALS71` at the end of a CPU work cycle | each **microsequence** | **no** | no |
>
> Because `ASIN` goes through `CI891`, the stop lands after the function code is
> read with the program addresser still on the OP code of the instruction just
> read (CPU[4] §5.1 b). `PAPA` steps the microsequences "without interfering
> with the transfers from peripheral unit" (CPU[4] §2.4); the same `ALS71` term
> also stops the machine whenever the rotary is off `NORM` and off position 8.
>
> gemu modelled the two as one thing until 2026-07-29, with the lamp following
> `PAPA` and the `INS` inhibit wrongly applied to it — so any program that had
> issued `INS` could silently ignore an inserted `PAPA`. They are now separate:
> `ge.h` `ASIN`, `msl-states.c state_E2_E3_TO80_CI89`, `ge.c fsn_last_clock`.
> Tests `console_fidelity.step_by_step_and_papa_are_separate` and
> `console_fidelity.step_by_step_stops_earlier_than_the_program_end`.

---

## 4. Maintenance panel

### 4.1 AM switches

Sixteen toggles `AM00`–`AM15` that **load** (force) or **display** configurations
on the main registers (CPU[4] §4.2, fo.34). In rotary positions 8, 12, 13 and 14
only `AM00`–`AM07` are used.

### 4.2 Rotary register selector

With the **machine stopped** each position routes a register onto **BO** for
display — possible because the Logic Sequence Matrix is still clocked by the free-
running delay line. Pressing **START** instead runs a *forcing cycle*, writing the
`AM` switches into the selected register (CPU[4] §4.2, fo.35–37).

| Pos | gemu `RS_*` | Display | `START` forces |
|-----|-------------|---------|----------------|
| 1 | `RS_V4` | V4 | AM → V4 |
| 2 | `RS_L3` | L3 | AM → L3 |
| 3 | `RS_V3` | V3 | AM → V3 |
| 4 | `RS_R1_L2` | RI-L2 | AM → RI-L2 |
| 5 | `RS_V2` | V2 | AM → V2 |
| 6 | `RS_L1` | L1 | AM → L1 |
| 7 | `RS_V1` | V1 | AM → V1 |
| 8 | `RS_V1_SCR` | V1 | AM → **storage** from address V1 onward (memory key-in) — see the note below: this one does **not** stop after a cycle |
| 9 | `RS_V1_LETT` | mem[V1] on **RO** | **reads** memory at V1 and steps V1 by 1; the lamps show the byte at V1 for as long as the position is selected |
| 10 | `RS_NORM` | PO | *(normal operating position — no forcing)* |
| 11 | `RS_PO` | PO | AM → PO |
| 12 | `RS_FI_UR` | *(none)* | AM → FI register and URPE |
| 13 | `RS_SO` | *(none)* | AM → SO, SI |
| 14 | `RS_FO` | *(none)* | AM → FO |

In position 8, `AM08` forces the memory check bit (even if incorrect) when the
`INCE` switch is inserted.

> **The storage key-in does not stop, and that is not a bug.** fo.98's
> end-of-cycle stop (`ALSOA=0`) exempts position 8 along with `NORM`, so with
> neither `PAPA` nor a step switch inserted a `START` at position 8 keys the
> `AM` switches into address after address, straight through core, until the
> addresser walks off the installed memory and **INV ADD** stops the machine.
> That is what the machine at Electric Dreams does. Insert `PAPA` and each
> `START` stores one byte, leaving V1 advanced for the next.
>
> The stop is the point: a memory fault does not merely light a lamp, it stops
> the subsystem — which is why `CLEAR` is *required* after `MEM CHECK` (§3.3)
> and why `INAR` exists to inhibit it. gemu raises the lamp either way and sets
> `ALTO` unless `INAR` is inserted (`pulse.c mem_fault`). Test
> `console_fidelity.storage_key_in_runs_until_the_error_stop`.
>
> **The fault lamps are conditions, not latches.** Each reports the memory
> cycle it belongs to, and a good cycle puts it out again — which you only see
> with the stop inhibited. Nothing loaded, `INAR` in, `START`: the machine
> walks zeroes up through core and **INV ADD** comes on as the addresser passes
> the installed 32K, then goes out as it wraps to `0x0000`, blinking once per
> lap (~0.2 s each way at nominal speed). With `INAR` out the machine stops ON
> the faulting cycle, so the lamp stands there lit until `CLEAR` — the same
> behaviour seen from the other side. Observed on the restored machine,
> 2026-07-31; test `console_fidelity.inv_add_follows_the_cycle_it_reports`.
>
> **Position 9 is the read-out.** `RO` is cleared at `TO20` of every cycle
> (fo.142), so anything on those lamps is put there by the cycle you are
> looking at: with the rotary at `V1-LETT` the display sequence fetches
> `mem[V1]` and the byte stands on the `RO` lamps for as long as the position
> is selected; each `START` steps V1 to the next byte. (gemu's display chart
> gained that read, and `CI33` — which would otherwise reload `RO` with the low
> half of the address — is excluded for this position; `msl-states.c`
> `state_00`. Whether the iron instead freezes the byte just read is the one
> part of this that wants a bench check.)

> **Keying a start address into PO** (position 11) is the ordinary way to run a
> program that is already in core, and it has to survive the return to `NORM`:
>
>     CLEAR -> rotary PO -> AM = address, INAR in -> START (forcing cycle)
>            -> rotary NORM, INAR out -> START (runs from it)
>
> gemu got this wrong in two places until 2026-07-31, and the symptom was the
> plain one — the machine ran from somewhere else. First, a `HLT` parks the
> sequencer mid-phase with the halted instruction still in `FO`, and `ge_clear`
> did not preset it: the next `START` finished the OLD instruction and ate the
> forced address as its operand. `CLEAR` now presets the sequencer to the
> display state (`00`), which is where a stopped machine sits and which walks
> `00 -> 80 -> alpha`, i.e. straight into a fetch at `PO`. Second, the display
> sequence's own `CI15` row (`NO <- L1`) was firing for every rotary position
> and overwriting the `PO` that `TO10` had just routed into the knot;
> Initialisation's `CO00` (`PO <- NI`) then wrote that back into the program
> addresser, so the machine started from the last program's `L1`. Conditioning
> `CI15` on the `L1` position — which is what `CI33`'s own exclusion list says
> — leaves the address positions alone. Test
> `console_fidelity.po_can_be_forced_and_run_from`.

### 4.3 Maintenance switches (CPU[4] §4.2, fo.35–36)

| Switch | gemu flag | Behaviour |
|--------|-----------|-----------|
| **PAPA** | `PAPA` | Step-by-step execution of the **microsequences** (stops after each), without disturbing peripheral transfers. `START` runs one step. *(This is the panel STEP-BY-STEP.)* |
| **PATE** | `PATE` | Stops the timing after **every delay-line cycle**; `START` runs one cycle. Finer than PAPA. |
| **RICI** | `RICI` | Disables loading of the *next* status — repeats execution of the current Status. |
| **ACOV** | `ACOV` | Stops the machine when a jump condition **is verified** at the end of reading a jump instruction. |
| **ACON** | `ACON` | Stops the machine when a jump condition is **not verified**. |
| **STOC** | `STOC` | Lets STEP-BY-STEP stop the CPU **even if** the program inhibited step-by-step (via `INS`). |
| **INAR** | `INAR` | **Inhibits the error stop** on a memory check error or on addressing a non-existent address — the lamp still lights, the machine does not stop (`pulse.c mem_fault`). Without it, a fault sets `ALTO`: that is what ends a runaway storage key-in, and what makes `CLEAR` "required after MEM CHECK". |
| **INCE** | `INCE` | Inhibits check-bit correction for characters from external units. During a console storage forcing it stores `AM08` as the (possibly wrong) odd-parity bit, suppressing parity generation for `AM07`–`AM00`. |
| **SITE** | `SITE` | The CPU no longer waits for availability / triggers from external units — the program evolves as if peripherals are always ready. |
| **LAMPS** | — | 3-position: `OFF` (all maintenance lamps off) / `ON` (lamps powered) / `DIAG` (lamps + diagnostic mode + `MAINT ON`). |

---

## 5. gemu front-ends

gemu drives the *same* internal model through three consoles. The C API
(`console.h`) is the common substrate:

```c
ge_clear(&g);                          /* CLEAR key                     */
ge_load_1(&g); / ge_load_2(&g);        /* LOAD1 / LOAD2 select          */
ge_load(&g);                           /* LOAD key (arms AINI)          */
ge_start(&g);                          /* START key                     */
ge_set_console_rotary(&g, RS_SO);      /* rotary register selector      */
ge_set_console_switches(&g, &sw);      /* AM + PAPA/PATE/.../INAR/...    */
ge_fill_console_data(&g, &console);    /* read back all lamps           */
ge_run_cycle(&g);                      /* advance the delay-line clock  */
```

| Front-end | How to start | Notes |
|-----------|--------------|-------|
| **CLI (headless)** | `./ge deck.cap` (`--deck` is an alias) | Drives CLEAR→LOAD1→LOAD→START for you and runs to HLT; `--trace` for logs. A `.cap` deck is the only input it takes. |
| **ncurses TUI** | `./ge --tui` | Implies `--console`; spawns `console/curses/console.py` against the `/tmp/gemu.console` socket. |
| **WebAssembly** | `make wasm && make wasm-run` | Browser panel; exports `press_clear/press_load/press_start`, `press_power_off`, `set_switches(flags, am)`, `set_register_selector(s)`, `set_switch_1_2(s1, s2)` (the program-readable switches → `JS1`/`JS2`), `set_load_unit(load1)` (LOAD1/LOAD2 selector), `set_speed(mult)` (run-speed multiplier), `mount_deck` (deck loader) with `deck_cards`/`deck_cards_left` (what is in the hopper), `refresh_lamps` (after LAMPS CHECK). The page's only input is a `.cap` deck chosen from the operator's disk — picking the file mounts it, exactly as the CLI mounts a positional `.cap`; nothing is vendored beside the page and there is no second format. The run loop is `requestAnimationFrame`-driven and **paces the cycle count to nominal GE-120 wall-clock time** (one `ge_run_cycle` = one 4 µs elementary cycle → 250 cycles/ms; CPU[4] "memory cycle of nominal 2/4/6 µs for 130/120/115/3"), with a simulator-toolbar speed selector (default real time). For instruction-level inspection use PAPA single-step instead. A live gdb-style disassembly window (shared `disasm.c`, driven from `opcodes.h`) tracks the program counter — `AAAA: <bytes>  MNEM ops`, current instruction highlighted. |

The WebAssembly `set_switches` packs the maintenance switches into a flags word:

```
bit 0 SITE   bit 3 STOC   bit 6 RICI
bit 1 INCE   bit 4 ACON   bit 7 PATE
bit 2 INAR   bit 5 ACOV   bit 8 PAPA
```

### 5.1 The WebAssembly "simulator gadget" (the reader hopper)

A real GE-120 has no file dialog — a program enters through a deck of cards
physically loaded into the reader. The browser panel keeps that distinction
visible with a small **simulator gadget**, drawn deliberately apart from the
authentic console (dashed border, monospace) so it never reads as a real
control. The gadget does exactly one thing, and it is the one thing a browser
cannot do for you: it carries cards to the hopper.

```
(gadget)  choose a .cap deck → Put In Hopper      [mount_deck()]
(console) CLEAR → LOAD1 → LOAD → START
```

There is no staging path and no image path. `LOAD` sets `AINI` and nothing else;
`START` runs the `80 → c8 → … → e3` bootstrap, which reads **one** card, packs
its 80 columns into 40 bytes at `0x0000` and executes them. Every card after
that is pulled by a `PER` read the loaded program issues itself.

The operator's window between `LOAD` and `START` is still there and still
useful: the rotary register dials can force values into memory (e.g. the
diagnostic test-select byte at `0x0E00` — dial `V1 ← 0x0E00`, `V1 SCR ← 0x40`)
and they survive into the run.

---

## 6. Example procedures

Each procedure is taken from the original manuals and is reproduced verbatim in
gemu's tests. Confidence is **high** where a regression test passes.

### 6.1 Bootstrap a program from a peripheral (normal load)

**Source:** CPU[4] §3.3 / §5.3 (fo.31–32, fo.43). **Confidence:** high.

```
1. CLEAR                       presets CPU + peripherals
2. LOAD1 (or LOAD2)            select the load unit (connector 2/3/4)
3. LOAD                        arm the bootstrap (sets AINI)
4. START                       state 80 → c8: read ≤129 words, then execute
```

In gemu this is exactly what `./ge --deck funktionalcpu.cap` does
(`main.c:192`): `ge_load_1` → `ge_load` → attach the deck to the reader on
connector 2 → run. The natural `00 → 80 → c8 → alpha` bootstrap leaves the entry
address as the machine defines it.

### 6.2 Single-step a running program

**Source:** CPU[4] §3.3 (STEP-BY-STEP) + §4 (PAPA). **Confidence:** high.

```
1. CLEAR
2. insert PAPA (STEP-BY-STEP)  white lamp lights; STEP_BY_STEP lamp on
3. START                       advance exactly one operation, then HALT
   …repeat START to walk forward, watching SO/SA/PO/FO on the lamps
```

During step-by-step the address (PO) and function code (FO) of the **next**
instruction are displayed (CPU[4] §3.3).

### 6.3 Key a byte into memory and read it back

**Source:** CPU[4] §4.2 rotary pos 8 / pos 9; reproduced as
`cpu_isolation.test_k` ("diag fo.82"). **Confidence:** high.

```
WRITE (pos 8, V1 SCR):
  CLEAR
  rotary → V1 SCR (pos 8), AM = 0x00FF, insert INAR
  START                      forces 0xFF into storage at the V1 address
                             (→ mem[0] == 0xFF)

READ  (pos 9, V1 LETT):
  rotary → V1 (pos 7), AM = 0x0000, START   (set the V1 address = 0)
  rotary → V1 LETT (pos 9)
  START                      reads mem[V1], +1 into V1; byte shows on RO lamps
                             (→ RO == 0xFF, MEM CHECK off)
```

`INAR` is inserted so writing into never-before-written memory does not raise
`MEM CHECK` / `INV ADD`.

### 6.4 Key an instruction into the registers and light OPER CALL

**Source:** CPU[4] §4 "Maintenance Panel", dwg 30004122 fo.35–37; reproduced as
`cpu_isolation.oper_call_by_register_forcing`. **Confidence:** high (validated on
gemu against the real-machine procedure).

This keys a 2-character `LON` instruction (opcode `0x02`, second char `0x80`)
straight into the CPU registers and single-steps it through every fetch phase
until it executes and raises **OPER CALL**.

```
1. CLEAR; insert PAPA (step-by-step)
2. rotary SO  (pos 13): force 0xE2   → sequencer enters the alpha (fetch) phase
3. rotary PO  (pos 11): force 0x00   → program counter = 0  (SO is preserved)
4. NORM, START                        → alpha 0xE2 steps to operand-fetch 0xE0
5. rotary FO  (pos 14): force 0x02    → the LON opcode      (SO held at 0xE0)
6. NORM, START                        → 0xE0 decodes a 2-byte op → beta 0x64
7. rotary L1  (pos 6):  force 0x80    → LON 2nd char        (SO held at 0x64)
8. NORM, START                        → beta executes LON → CI87 sets ALAM
                                       → OPER CALL lamp on
```

**Key fidelity point (gemu `ge.c:fsn_last_clock`):** a forcing cycle preserves the
program sequencer **SO** — only forcing SO itself (pos 13) changes it. Without
this, each register-force would clobber the sequencer and the phase walk could
never complete. This was the bug fixed in commit *"preserve the program sequencer
during console forcing (CPU[4] fo.35-37)"*.

### 6.5 Display a register without disturbing the program

**Source:** CPU[4] §4.2 (fo.36). **Confidence:** medium (display path verified;
"non-disturbing" guarantee relies on the delay line still clocking the LSM).

```
1. (machine stopped — e.g. on HALT)
2. rotary → the register of interest (V1…FO)
3. read it on the BO display lamps; do NOT press START (START would force)
```

Because the rotary only *forces* on `START`, simply turning it to a register and
reading the lamps is non-destructive.

---

## 7. Fidelity status & open items

| Behaviour | gemu status |
|-----------|-------------|
| CLEAR / LOAD / START / LOAD1-2 bootstrap | implemented (§6.1) |
| PAPA step-by-step + STEP_BY_STEP lamp | implemented (§6.2) |
| Rotary force/display, SO-preserving forcing | implemented (§6.3, §6.4) |
| OPER CALL via keyed `LON` | implemented + regression-tested (§6.4) |
| `INAR` inhibits the error stop during forcing | exercised by `test_k`; full stop-on-fault model partial |
| **STEP-BY-STEP (`ASIN`) and PAPA are independent circuits** | implemented — only `ASIN` lights the lamp and only `ASIN` is inhibitable by `INS`; tests `console_fidelity.step_by_step_and_papa_are_separate`, `…_stops_earlier_than_the_program_end` (§3) |
| **`INS` inhibits step-by-step; `ENS`/`STOC` re-enable** | implemented — `state_E2_E3_TO80_CI89` gates `ASIN` by `STOC \|\| !ADIR` (`CI77`/`CI78`) |
| **SWITCH 1 / SWITCH 2 lamps** ← `JS1`/`JS2` | implemented — `console.c`; test `console_fidelity.switch_lamps` |
| **PATE** (single delay-line-cycle step) | implemented — `fsn_last_clock` halts after every delay-line cycle; test `console_fidelity.pate_single_cycle` |
| `ACOV` / `ACON` jump-condition stop | implemented in `CI38` (`AVER && ACOV`, `!AVER && ACON`); verified by inspection |
| **`INCE` parity forcing (`AM08` as check bit)** | implemented — `pulse.c` stores `AM08` as the parity bit during V1-SCR forcing; test `console_fidelity.ince_forces_check_bit` |
| `SITE` (ignore peripheral availability) | partial — open |
| **Keys reach the machine over the console socket** | implemented — `console_socket.c` parses the client frame and dispatches CLEAR / LOAD1-2 / LOAD / START edge-triggered; tests `console_socket.*`. Before this the ncurses client's key presses were received and discarded. |
| **`LOAD` arms only; `START` reads one card** | implemented and regression-tested — `bootstrap.card0_loads_to_alpha` compares all 40 bytes against the deck's own loader card |

The one remaining open item, `SITE` (don't wait for peripheral availability),
reaches into the peripheral handshaking rather than the console proper, and is
the next target.
