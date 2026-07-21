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
| 243 (302) | FUNCTION AND STATUS CODES ANDS | 5 | `DE13A = NAND(DO061, DI061)` |
| 241 (300) | (status inverters) | 11 | `DI061 = /DI06A` — same net value as `DI062` |
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
- `DE13A = DO061 · DI061` — the `DO06` class in beta. `DI061` is a second
  driver of the same net as `DI062`, so all four leaves share the shape
  "function class AND beta band", differing only in the class: DO00, DO01,
  DO02, DO06.

## The two gates that needed human eyes

`DO00A` and `DO06A` take their inputs on unlabelled bus lines rather than at
labelled stubs, so they could not be read from crops. Traced on the sheet by
the maintainer, 2026-07-21:

- **U09 pin 1** (gate 10, `DO00A`) wires to margin stub **(105-8) C38-12 =
  `FO06F`** — the same net that feeds U08 pin 10 and U14 pin 1, both of which
  ARE labelled `FO06F`, which corroborates the trace.
- **U11 pin 1** (gate 4, `DO06A`) — the same `FO06F` net.
- **U11 pin 4** = **`FO036`**, from (104-16) D38-03.
- **U11 pin 5** = **`DO041`**, from U07 pin 6 — not an `FO` bit at all, but
  the DO04 class decode fed back in. This is the one that could not have been
  guessed from the opcode partition.
- **U14 pin 10** (gate 8, `DO01A`) = **`FO066`**, confirming the earlier
  inference.

Giving:

    DO001 = /FO7 . /FO6
    DO011 = FO6 . /FO3 . /FO7
    DO021 = FO3 . FO6 . /FO7
    DO061 = /FO6 . FO3 . FO0 . DO041      (DO041 = /FO5 . FO7)

The partition over the ISA is exact, with no exceptions across 49 opcodes:
DO00 takes 0x02/0x07/0x0A, DO01 the jumps and JRT, DO02 takes LA, DO06 takes
LPSR alone, and every family that continues into the executive band plus all
of PER/PERI/RDC falls outside all four. That the four independently-read
gates reproduce exactly the required set is the strongest available check
that the trace is right.

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

All of it is now in `signals.h`: `DO00A`/`DO001`, `DO06A`/`DO061`, `DI061`,
`DE06A`, `DE11A`, `DE13A`, `CM011`/`CM01A`/`CM01A0`, and `DE00A` un-stubbed.
The beta charts for the control, JRT, LA and LPSR sheets now gate `CU01` and
`CU07` on `CM01A0` instead of on the invented `not_per_peri` complement.

`not_per_peri` survives on exactly two sheets, both of which are hybrids
rather than transcriptions: `beta_64_undocumented` (the swept-core guard from
aa7ed63) and `beta_64_ss`, whose one-shot has to synthesise the return that
the executive loop would otherwise make — `CM01A0` correctly withholds `CU01`
from every SS opcode, because in the real machine beta is not what returns
them to alpha. Both uses disappear with the last hybrid.

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
