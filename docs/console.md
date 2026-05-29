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
must not be pressed during machine operation.

---

## 3. Operator controls

| Control | Type | Behaviour (CPU[4] §3.3, fo.31–33) |
|---------|------|-----------------------------------|
| **CLEAR** | key | Stops everything in the subsystem, clears all error conditions, presets CPU + peripherals to a defined state. Required after `MEM CHECK` and after power-on. No lamp. |
| **LOAD 1 / LOAD 2** | switch | Selects one of two peripheral units enabled at install time for program loading (Conn.2/3, Conn.4/3, or Conn.2/4 — CPU[4] fo.43). |
| **LOAD** | key | *Arms* the bootstrap: sets the `AINI` flip-flop so the next `START` reads an initial program block (≤129 words) from the selected unit and begins executing it. No lamp. |
| **START** (HALT) | key | Starts operation. The white **HALT** lamp shows the machine is stopped. First `START` after `CLEAR`: runs the program if no other switch is set; runs the **load** if `LOAD` was pressed. |
| **STEP-BY-STEP** | switch | Executes one instruction per `START`. Mid-run, it makes the program stop at the end of the current instruction. **`INS` inhibits it; `ENS`, `CLEAR`, or the maintenance-panel `STOC` switch re-enable it.** White lamp = inserted. |
| **SWITCH 1 / SWITCH 2** | switch | Two general-purpose switches the program reads via the `JS1` / `JS2` instructions. |

> **Loading sequence (CPU[4] §3.3 / §5.3):** `CLEAR` → select unit (`LOAD1`/`LOAD2`)
> → `LOAD` → `START`.

> **gemu note / step-by-step:** the panel's STEP-BY-STEP is implemented by the
> maintenance-panel **PAPA** switch (§4) — `console.lamps.STEP_BY_STEP` follows
> `PAPA`. The documented `INS`-inhibit / `STOC`-override interaction is modelled:
> `ge.c:fsn_last_clock` gates the PAPA halt by `STOC || !ADIR` (`ADIR` set by
> `INS`/`CI77`, cleared by `ENS`/`CI78` and CLEAR). See §7.

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
| 8 | `RS_V1_SCR` | V1 | AM → **storage** from address V1 onward (memory key-in) |
| 9 | `RS_V1_LETT` | V1 | **reads** memory at V1, **+1** into V1; the byte read shows on the **RO** lamps |
| 10 | `RS_NORM` | PO | *(normal operating position — no forcing)* |
| 11 | `RS_PO` | PO | AM → PO |
| 12 | `RS_FI_UR` | *(none)* | AM → FI register and URPE |
| 13 | `RS_SO` | *(none)* | AM → SO, SI |
| 14 | `RS_FO` | *(none)* | AM → FO |

In position 8, `AM08` forces the memory check bit (even if incorrect) when the
`INCE` switch is inserted.

### 4.3 Maintenance switches (CPU[4] §4.2, fo.35–36)

| Switch | gemu flag | Behaviour |
|--------|-----------|-----------|
| **PAPA** | `PAPA` | Step-by-step execution of the **microsequences** (stops after each), without disturbing peripheral transfers. `START` runs one step. *(This is the panel STEP-BY-STEP.)* |
| **PATE** | `PATE` | Stops the timing after **every delay-line cycle**; `START` runs one cycle. Finer than PAPA. |
| **RICI** | `RICI` | Disables loading of the *next* status — repeats execution of the current Status. |
| **ACOV** | `ACOV` | Stops the machine when a jump condition **is verified** at the end of reading a jump instruction. |
| **ACON** | `ACON` | Stops the machine when a jump condition is **not verified**. |
| **STOC** | `STOC` | Lets STEP-BY-STEP stop the CPU **even if** the program inhibited step-by-step (via `INS`). |
| **INAR** | `INAR` | **Inhibits the error stop** on a memory check error or on addressing a non-existent address. Used during console forcing so a key-in to unwritten memory does not trip `MEM CHECK`/`INV ADD`. |
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
| **CLI (headless)** | `./ge prog.bin` or `./ge --deck deck.cap` | Drives CLEAR→LOAD→START for you and runs to HLT; `--trace` for logs. |
| **ncurses TUI** | `./ge --tui` | Implies `--console`; spawns `console/curses/console.py` against the `/tmp/gemu.console` socket. |
| **WebAssembly** | `make wasm && make wasm-run` | Browser panel; exports `press_clear/press_load/press_start`, `set_switches(flags, am)`, `set_register_selector(s)`. |

The WebAssembly `set_switches` packs the maintenance switches into a flags word:

```
bit 0 SITE   bit 3 STOC   bit 6 RICI
bit 1 INCE   bit 4 ACON   bit 7 PATE
bit 2 INAR   bit 5 ACOV   bit 8 PAPA
```

### 5.1 The WebAssembly "simulator gadget" (deck loading)

A real GE-120 has no file dialog — a program enters through a deck of cards
physically loaded into the reader. The browser panel reproduces this faithfully:
a small **simulator gadget**, drawn deliberately apart from the authentic console
(dashed border, monospace) so it never reads as a real control, lets you pick a
`.cap` deck and "insert it in the reader hopper". Internally it writes the deck
into the emscripten in-memory filesystem and calls `mount_deck()`, which attaches
it to the card reader on connector 2 and selects `LOAD1` — exactly the `--deck`
CLI path. You then run the **real** bootstrap on the console buttons:

```
(gadget) choose deck → Insert deck in reader
(console) CLEAR → LOAD → START
```

No code is teleported into RAM: the documented `80 → c8` load sequence reads the
deck through the reader, just like the hardware.

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
| **`INS` inhibits step-by-step; `ENS`/`STOC` re-enable** | implemented — `fsn_last_clock` gates the `PAPA` halt by `STOC \|\| !ADIR` (`CI77`/`CI78`); test `console_fidelity.step_by_step_inhibit` |
| **SWITCH 1 / SWITCH 2 lamps** ← `JS1`/`JS2` | implemented — `console.c`; test `console_fidelity.switch_lamps` |
| **PATE** (single delay-line-cycle step) | implemented — `fsn_last_clock` halts after every delay-line cycle; test `console_fidelity.pate_single_cycle` |
| `ACOV` / `ACON` jump-condition stop | implemented in `CI38` (`AVER && ACOV`, `!AVER && ACON`); verified by inspection |
| **`INCE` parity forcing (`AM08` as check bit)** | implemented — `pulse.c` stores `AM08` as the parity bit during V1-SCR forcing; test `console_fidelity.ince_forces_check_bit` |
| `SITE` (ignore peripheral availability) | partial — open |

The one remaining open item, `SITE` (don't wait for peripheral availability),
reaches into the peripheral handshaking rather than the console proper, and is
the next target.
