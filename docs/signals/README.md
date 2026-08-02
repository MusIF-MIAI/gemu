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

**Backplane (card·pin) column.** Each signal table now carries the signal's
location(s) on the CPU **card-layout Atlas** (`atlas/row_*_pinout_verified.csv`
in the gemu-audit workspace, alongside this repo; backplane rows A–T = drawing
*14026 136*, cabinet **section 2A**). Format:
`<row><slot>·<pin>` — e.g. `G7·07` = row G, slot 7, pin 07. Entries are matched on
the 4-character signal **base** (mnemonic+number), so every suffix/polarity variant
is listed with its actual backplane code (e.g. `LU08B` the conn-2 line and `LU082`
its companion). All 20 Atlas rows (A–T) are now alphabet-cleaned to `[A-Z0-9]`, so
the index covers every legible pin on the 2A layout. `—` means the signal is
**not on the 2A card-layout pins**: it lives on another cabinet section not in this
Atlas, or the only pin that would carry it was illegible on the scan and left blank.
A handful of located cells still carry residual OCR glyph slips (O↔0 / B↔8 / I↔1 /
S↔B, e.g. `LU01S` for `LU01B`) — the **position** (card·pin) is correct; the suffix
may need a glance. The microcode-command tables (§6 CO/CI/CU/CA) are mostly `—`:
those are timed micro-commands, not pin-labelled backplane lines.

---

## Contents

| File | Section | Description |
|------|---------|-------------|
| [01-coca-connector.md](01-coca-connector.md) | §1 | COCA connector — reader ↔ CPU pin-by-pin reference |
| [02-channel-sequencer.md](02-channel-sequencer.md) | §2 | CPU channel & sequencer signals (transfer machinery) |
| [03-model-notes.md](03-model-notes.md) | §3 | Current model vs pin-by-pin target |
| [04-signal-index.md](04-signal-index.md) | §4 | Official GE-120 signal index (registers, channel, peripheral, etc.) |
| [05-channel2-microcode.md](05-channel2-microcode.md) | §5 | Channel-2 data-transfer microcode |
| [06-microcode-commands.md](06-microcode-commands.md) | §6 | Microcode command dictionary (CO/CI/CE/CU/CA) |

### Signal Traces

Detailed hardware traces of COCA signals through the Capitolo logic.

| File | Signal group | Chapters | Description |
|------|-------------|----------|-------------|
| [traces/fini.md](traces/fini.md) | FINI/FINE | 165→158→140→138 | Card-end signal: sensors → RIG1 transfer termination |
| [traces/lu-data.md](traces/lu-data.md) | LU00-LU07 | 150→151→152 | Reader data lines: COCA → PIB bus → NE → RO |
| [traces/lu08-pelea.md](traces/lu08-pelea.md) | LU08, PELEA | 125 | Character strobe + peripheral error |
| [traces/status-signals.md](traces/status-signals.md) | LUPO/LURE/LUSE/LENO → PCOV6 | 124→125→126→127→128 | Reader status → RO decode → external ops |
| [traces/register07.md](traces/register07.md) | Register 07 (link register) | 229→236→225→235→248→199→252→219→221→209→213→206→212→205→239→036→052→053→186→185→193→090→380→412 | JRT/SR/SL link register write path (complete) |

Traces still to be written, listed here so the gaps are visible rather than
implied: `fiden` (FIDE/FISE end-of-sequence, ch.125→123), `re-data` (RE00-RE08
CPU→reader command data, ch.120→153→154), `tu-timing` (TU10/TU20/TU30 strobes,
ch.149→157), `pomo-mode` (POMO, N001/N002, DEBI, MI01/MI02 mode selection,
ch.149→156).

