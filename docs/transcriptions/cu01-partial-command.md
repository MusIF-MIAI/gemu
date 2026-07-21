# CM01 / CU01 partial-command network (cp06 ch.199/229/236/248/252)

Read from source at 600dpi, audit round 3 (2026-07-21), tracing why gemu's
`DE00A` had to be stubbed to a constant. Result: **the transcription of
`DE00A` was never wrong — it was incomplete.** `DE00A` is one of FOUR
active-low leaves of an OR feeding the partial command, and gemu modelled it
as if it were the whole condition.

## The chain, gate by gate

| Chapter (PDF pg) | Title | Gate | Equation |
|---|---|---|---|
| 229 (289) | FUNCTION DECODING | 8 | `DO01A = NAND(FO066?, FO03F, FO07F)` |
| 229 (289) | FUNCTION DECODING | 10 | `DO00A = NAND(FO07F, ...)` |
| 229 (289) | FUNCTION DECODING | 11 | `DO001 = /DO00A` |
| 236 (296) | STATUS DECODING | 6 | `DO011 = /DO01A` |
| 248 (307) | FUNCTION AND STATUS CODES ANDS | 9 | `DE00A = NAND(DO011, DI062)` |
| 248 (307) | FUNCTION AND STATUS CODES ANDS | 4 | `DE11A = NAND(DI061, DO001)` |
| 248 (307) | FUNCTION AND STATUS CODES ANDS | 8 | `DE06A = NAND(DO021, DI062)` |
| 199 (259) | COMMAND C164 GEN. AND PARTIAL | 6 | `DE001 = /DE00A` (dual-input inverter) |
| **252 (311)** | **PARTIAL COMMANDS GENERATION** | **7** | **`CM011 = NAND(DE00A, DE11A, DE13A, DE06A)`** |
| 252 (311) | PARTIAL COMMANDS GENERATION | 10 | `CM01A = /CM011` -> (219-1)(221-3) |

Page = chapter + 60 up to ch.238, chapter + 59 from ch.240 (cp06 is missing
chapter 239 / foglio 217 — see `expert/knowledge/cache/cp06/_scan_defects.md`).

## What each leaf covers

`CM011` is a 4-input NAND over active-low inputs, i.e. the command asserts
when ANY leaf goes low. The four leaves partition the opcode space by
function class ANDed with a state decode:

- `DE00A = DO011 · DI062` — `DO01` class (`FO6·/FO3·/FO7`: JCC 0x40, JRT 0x41,
  JC 0x43, JU 0x47, JIE/JS1/JS2 0x53) in the beta band (`DI062` = SA 0110 01xx).
- `DE06A = DO021 · DI062` — `DO02` class (`FO3·FO6·/FO7`) in beta. This is the
  **LA** leaf (0x68 = 0110 1000: bit 6 and bit 3 both set).
- `DE11A = DI061 · DO001` — the `DO00` class, decoded from `FO07F` at ch.229
  gate 10. This is the **console/control** leaf: INS/ENS/LON/LOFF/LOLL (all
  FO 0x02), NOP2 (0x07), HLT (0x0A) — every opcode with FO bit 7 clear that
  `DO011` cannot reach.
- `DE13A` — from (243-5), not yet transcribed. LPSR (0x9d) is the obvious
  remaining single-beta opcode, so this is the likely candidate.

## Why the stub existed, and what it papered over

gemu's chart row is `{ TI06, CU01, not_per_peri, DE00A0 }`, treating `DE00A0`
as a necessary AND term. Evaluated honestly, `DE00A0` is FALSE for the whole
console group (FO bit 6 clear => `DO011` = 0), so the row would never fire for
them — hence `SIG(DE00A) { return 0; ... }` with the note "doesn't work for
nop/lon/loff ecc". The constant-0 makes `DE00A0` constant 1, which restores
those opcodes at the cost of making the term inert for everyone.

`not_per_peri` was then added to stop the constant-1 leaking `CU01` onto
PER/PERI/RDC. It works because the union of the four real leaves happens to
exclude exactly the external opcodes and the families that continue into the
executive band — but it reproduces the union by complement rather than by
construction, which is why it could not be un-stubbed.

## What this unblocks

Modelling the four leaves properly removes the last obstacle to folding the
64|65|66 variants into a single chart (see `msl-timings.h`): the exit rows
stop depending on the variant match to narrow the opcode set, because the
gates narrow it themselves. `beta_64_undocumented` then disappears — an
unknown FO fails every leaf, performs no datapath rows and falls through,
which is what the `aa7ed63` comment already claims the real machine does.

Signals still to add: `DO00A`, `DO001`, `DI061`, `DE06A`, `DE11A`, `DE13A`,
`CM011`/`CM01A`. Only `DO021` exists today.

## Scan notes

- The cp06 signal index lists `DO00A` at 229-10, but the parser dropped the
  row: OCR read the zeros as letter O (`DOOOA`), so the whole `DO0x` band is
  absent from `_signals_index.md`. Fix the parser before trusting that band.
- ch.236's title block renders as "256" below ~300dpi; at 600dpi it is
  unambiguously 236, and the fan-outs (gate 6 -> 248-9, gate 14 -> 248-10)
  corroborate it. Not a numbering defect.
- ch.229 gate 8 pin 10 and gate 10 pin 1 take unlabelled bus lines; the
  `FO066` attribution for gate 8 is inferred from the resulting opcode
  partition, not read off the sheet.
