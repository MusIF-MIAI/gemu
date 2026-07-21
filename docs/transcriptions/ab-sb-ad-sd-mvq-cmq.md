# AB-SB-AD-SD-MVQ-CMQ timing sheets (cp07 fo.140-143, PDF p126-128)

Read from source at 600dpi, audit round 3 (2026-07-21). OVERBAR(x) marks a
printed negation bar. Family walks V2 DESCENDING (CO40 in 60|62): arithmetic
runs LSB-first for the carry chain.

> **Read "Second read" at the end before using the row list below.** Two of
> its conditions do not survive contact with the gates: fo.143's
> `{(L1_1 = 1i)}` against CU01 is a sheet error (CU01 is unconditional), and
> fo.143's EMPTY condition cell on CI73 is also wrong (CI73 is gated on an L1
> signal, and that gate is the family's zero-extension). The first read's
> conclusion that "the X bit is a quartet counter, not a pass counter" was
> drawn from the first of those and is WRONG -- X is a pass counter here,
> exactly as in the register family.
>
> Both were traced to their gates and corrected; nothing is unknown any more.
> Prerequisites are met (the UA decimal mode landed in fdc1afe), so the
> conversion can be written straight from the end of this document.

## fo.140 — 64|65 beta (SA 0110 0101)
Rows: TO50 NO->BO; TO65 CO49 = DI06A0 (RES URPE E URPU); TI06 CU10 = DI06A0;
TI06 CU12 = DI06A0. No conditions. Exit: 60+62 (0110 00X0) unconditional.

## fo.141 — 60|62 (SA 0110 00X0)
- TO10 CO12 = DE16A0                        V2 -> NO
- TO10 CO41 = DI05B0                        count from 00
- TO10 CO40 = CB12A0 (DE17A0)               DECREASING (V2 walks DOWN)
- TO25 CO30 = EG09A0 {OVERBAR(FA03)}        MEM -> RO
- TO30 CI15 = CD03A0 (DI05A0)               L1 -> NO
- TO30 CI41 = CD08A0 (DE17A0)               CI count from 00
- TO30 CI42 = CD08A0 (DE17A0)               CI count from 04
- TO30 CI40 = DI05B0                        CI decreasing
- TO30 CI44 = DI05B0                        CI stop count 07
- TO40 CO02 = DE17A0                        NI -> V2
- TO50 NO -> BO
- TO70 CI65 = CD11A0 (DE17A0)               RO1 -> NI3
- TO70 CI60 = CD11A0 (DE17A0)               RO2 -> NI4
- TI05 CI05 = DE89A0                        NI -> L1
- TI06 CI85 = CD20A0 (DE91A0)               RESET FI05 (unconditional)
- TI06 CI84 = CD19A0 (DE90A0) {OVERBAR(SA01)}  RESET FI04
- TI06 CU04 = CM06A0 (DI05A0); TI06 CU15 = DI05A0
Exit: 50+52 (0101 00X0) unconditional.

## fo.142 — 50|52 (SA 0101 00X0)
- TO10 CO11 = EG15A0                        V1 -> NO
- TO25 CO30 = CB07A0 (DI04A0)               MEM -> RO
- TO30 CI15 = CD02A0 (DA19A0)               L1 -> NO
- CI46 = DE99A0 {(AD+SD+CMQ)}               decimal/AND mode
- CI47 = DE98A0 {(SD+SB+CMQ)}               subtract/OR mode
- TO50 CO48 = DE97A0 {(SD+CMQ+SB).OVERBAR(SA01)}  SET URPE E URPU
- TO50 NO -> BO
- CI50 = DE99A0 {(AD+SD+CMQ)}               "OPERA SOLO UA1 / WORK ONLY UA1"
- TO70 CI60 = DA01A0 {(AD+SD+CMQ+MVQ)}      RO2 -> NI4
- TO70 CI68 = DA20A0 {(AD+SD+AB+SB+CMQ)}    UA -> NI43
- TI05 CI05 = CD02A0 (DA19A0)               NI -> L1
- TI06 CU14 = CM09A0 (DI04A0)
Exit: 40+42 (0100 00X0) unconditional.

## fo.143 — 40|42 (SA 0100 00X0)
- TO10 CO11 = CB01A0 (DI49A0); TO10 CO41 = CB14A0 (DA31A0);
  TO10 CO40 = CB12A0 (DE95A0)               (V1 also walks DOWN)
- TO25 CO31 = CB09A0 (DE96A0) {(AD+SD+MVQ+AB+SB)}   RO -> MEM (CMQ absent)
- TO30 CI15 = CD21A0 (DA21A0); TO40 CO01 = CB01A0 (DI49A0);
  TO50 CI32 = CD21A0 (DA21A0)
- TI06 CI75 = CD18A0 (EG46A0)
  {OVERBAR(dRO=0i).(AB+SB) + OVERBAR(dRO_1=0i).(AD+SD+CMQ+MVQ)}   SET FI05
  (bars span each full equality; the family factors are unbarred)
- TI06 CI74 = DE92A0 {(URPE)}               SET FI04 (no bar)
- TI06 CI73 = EG43A0                        SET FI03 (unconditional)
- TI06 CU01 = CM02A0 (DI49A0) {(L1_1 = 1i)} SET S001  <- quartet-1 counter!
- TI06 CU05 = DI49A0
- TI06 CU07 = CM03A0 (EG45A0) {(L1_2 = 1i)} SET S007
Exit: 60+62 {OVERBAR(L1_2 = 1i)} | E2+E3 {(L1_2 = 1i)}.

Scan notes: fo.140 command cell prints "CC49" (=CO49); fo.142 CI46's command
glyph resembles CI45 (equation CI4611 settles it); CO11 schema 205-7/206-7
ambiguous on fo.142 (fo.143 reads 206-7).


## UA control: what CI50 does (RESOLVED 2026-07-21)

Read from cp06 **chapter 094, PDF page 170** ("ARITHMETICAL UNIT / UNITA'
ARITMETICA", dwg 14013 0650). Page = chapter + 76 in this band; confirmed
against the cartiglios of p163/164/170/171 = ch.087/088/094/095.

    gate 8  (NAND1 U30)  CI50B = /CI501      inputs tied, the inverter idiom
    gate 9  (NAND3 U25)  UZE71 = NAND(UR071, TI051, CI50B, ...)
    gate 12 (NAND3 U25)  UZE81 = NAND(..., CI50B, ...)

`CI50B` is the *complement* of the command, so asserting CI50 drives CI50B low
and **inhibits UZE71 and UZE81** -- the enables for the upper zones of the
arithmetic unit. That is the sheet's "OPERA SOLO UA1 / WORK ONLY UA1" in
gates: CI50 does not select a mode, it *disables the high zones* so only the
low unit participates. Which is what a digit-at-a-time decimal operation
needs.

gemu has no CI50 command at all today. Adding it means gating the UA width
rather than its function.

## What is still unread

**The CI45/CI46/CI47 -> UA function table.** cp06 **chapter 087, PDF page
163** ("ARITHMETICAL UNIT, CONCENTRATOR", dwg 14013 0651) carries the mode
decode in its bottom-left block:

    gate 22 (U25)  CI451 (from 196-2) -> CI45D
    gate 26 (U25)  CI461 (from 196-1) -> CI46B      <- the index's 087-26
    gate 31 (U25)  CI471 (from 190-3) -> CI47B
    gate 23        UCO4A = NAND(CI471, CI451)
    gate 27        UCO0A = NAND(CI45D, CI47B)
    gates 24/25/28/29/30/32/33/34 -> UCO01 UCO11 UCO21 UCO41 UCOA1 + /A forms

Those UCOxx lines are what the concentrator gates (1-21 on the same sheet,
continuing onto ch.088 / p164) use to build the adder function. gemu models
CI46 as `ua_controls.decimal_and`, a logic-mode flag, which cannot be right
for both roles: fo.142 has `CI46 = DE99A0 {(AD+SD+CMQ)}` selecting DECIMAL,
while fo.43/fo.146 use the same command as part of the AND/OR code.

Deriving the full table means tracing ~20 concentrator gates across two
sheets. Before doing that by hand, check cp04 (the prose volume) for a
printed UA function table -- it would give the same answer as data.

## Second read (2026-07-21, 600 dpi page images)

### Settled

**Clock placement.** The rows with an empty Mastro-Clock cell are continuation
rows and inherit the clock printed above them, so the commands the first read
left unplaced are: **CI46 and CI47 at TO30** (they follow `TO30 CI15`) and
**CI50 at TO50** (it follows the `TO50 NO->BO` row). No inference needed.

**fo.142 prints no CI45.** The command cell whose glyph resembles CI45 carries
equation `CI4611`, so it is CI46; there is no CI45 row on the sheet at all.
The family therefore runs the UA in ARITHMETIC mode (logic low) with CI46
selecting decimal — exactly the case `ge_ua_decimal` was written for.

**CO48's bar covers SA01 only**, not the product: `{(SD+CMQ+SB)·/SA01}`.
Confirmed at high zoom; the bar starts after the dot.

**fo.141's CO30 bar is real**: `{/FA03}`, matching the `not_FA03` gemu already
uses for XC-OC-NC.

**Beta enters the executive band with X = 0.** fo.140's state cell is
`0 1 1 0 0 1 0 1` = 0x65, so S001 = 0, and beta issues only CU10/CU12 (S000,
S002), leaving S001 untouched. The first executive pass is 60, not 62.

**CI75's decimal test is quartet-local.** fo.143 prints
`{/(dRO=0i)(AB+SB) + /(dRO_1=0i)(AD+SD+CMQ+MVQ)}` — subscript 1 on the second
term. The binary ops test the whole result byte; the decimal ops test only the
DIGIT quartet, so the preserved zone nibble cannot make a zero result look
nonzero.

**CI60 supplies the zone.** fo.142's `TO70 CI60 = DA01A0 {(AD+SD+CMQ+MVQ)}`
routes RO2 (the operand's high nibble) into NI4 while CI50 keeps the UA in its
low zone. That corroborates, from the sheet, the pass-through that
`ge_ua_decimal` had to assume when CI50 is asserted: the high quartet of the
result is RO's, not the UA's.

**L1_2 is the high quartet, L1_1 the low** — three independent confirmations:
the MVQ/CMQ field length is already known to be the high nibble (funktionalcpu
step 0x1B), fo.143 makes `{L1_2 = 1i}` the exit gate, and the SS2 layout the
disassembler uses puts the operand-1 length in the high nibble.

**fo.143's exit box**: `60+62 {/(L1_2 = 1i)}` (bar present) and
`E2+E3 {(L1_2 = 1i)}`.

### What the deck can and cannot arbitrate

The 0x40 sweep's algebra cases (deck steps 0x14-0x1C) are:

    AD  1, 1, 0x04F2, 0x04F3        AB  1, 2, 0x0513, 0x0515
    AD  1, 2, 0x04F2, 0x04F5        AB  1, 2, 0x0513, 0x051B
    AD  2, 1, 0x04FB, 0x04FC        SD  1, 2, 0x0521, 0x0523
    MVQ 2,    0x0531, 0x0533        SB  1, 2, 0x0529, 0x052B
    CMQ 2,    0x0534, 0x0536

Working the quartet counters through those: the loop count is governed by the
operand-1 quartet, so **every subtract in the sweep runs exactly one
iteration**. `AD 2, 1` is the only multi-iteration case and it is an add.
So the deck exercises the loop arc but **cannot** adjudicate anything about
how the borrow behaves across iterations. Do not treat a green deck as
evidence on that question.

### RESOLVED by tracing the gates (2026-07-21)

Both blockers dissolved the same way, and both dissolve into the same lesson:
**a timing sheet's condition cell is not the gate.** The condition lives in
the family's leaf of a multi-leaf command OR, and the sheet sometimes prints
it in the wrong cell or not at all.

**CU01 has no counter term.** fo.143 prints `{(L1_1 = 1i)}` against CU01.
Traced end to end in cp06 and there is no L1 signal anywhere in the chain:

    ch.219 g1   CU011 = NAND(CM01A, CM02A, DE53A, ED36A,
                             ED10A, ED84A, ED66A, ED50A)      8 leaves
    ch.252 g8   CM021 = NAND(DI49A, DI36B, DI57B, DI94A,
                             DI60A, DI13A, DI50A)             7 leaves, audit below
    ch.222 g1   DI49A = NAND(DI481, SA066)                    the 40|42 decode
    ch.241 g3   DI48A = NAND(SA04F, SA05F, SA07F)             bits 4,5,7 zero
    ch.241 g5   DI481 = /DI48A                                -> (222-1)

ch.252 g8 pin detail (user-corrected): DI50A drives BOTH pins 5 and 13;
pins 6 and 10 are floating -- open TTL inputs read high, so a floating NAND
input is transparent, the dual of the tied-input inverter idiom.

**CM021 leaf audit** (2026-07-21, via the cp06 signal index at
`marco-kb-architecture/_signals_index.tsv`, each generator read at the gate):

    DI49A  ch.222 g1    pure 40-4F decode (chain above, complete)
    DI13A  ch.225 g18   NAND(D1101, SA036, SA026), D1101 = SA076.SA056 - state
    DI36B  ch.231 g8    buffer of NAND(D1591, SA066, D1341) - state
    DI50A  ch.233 g3    NAND(DI521, DI481, SA026) = states 04-07
    DI57B  ch.236 g13   buffer of ch.232 g23 (SA01F + bus); every input
                        reaching ch.232 is SAxxx or status-band D1xxx - state
    DI94A  ch.248 g1    NAND(SA006, DI931), DI931 = ch.241 g7 from status
                        decodes.  NB ch.248 is titled "FUNCTION AND STATUS
                        CODES ANDS" and other gates there DO take F0xx inputs;
                        this one happens not to.
    DI60A  ch.239 g12   UNVERIFIABLE: ch.239 is missing from the scan
                        (p298 = ch.238 jumps to p299 = ch.240)

So six of seven leaves are verified state decodes and one cannot be checked.
The conclusion does not depend on the seventh: CM021 is a NAND of active-low
leaves, i.e. an OR -- a leaf can only ADD states where CU01 fires, never veto
one.  DI49A alone guarantees the firing in 40|42.

**CU01 therefore fires unconditionally in 40|42**, exactly as gemu's common
row already had it, and the X bit means "not the first iteration" -- the same
pass encoding the register family uses. Which makes `CO48
{(SD+CMQ+SB)·/SA01}` fire on the first iteration ONLY, so the borrow is
preset once and propagates correctly through URPE for the rest of the loop.
Contradiction 1 was an artifact of believing the brace.

(Corroboration picked up on the way: ch.236 g3 prints `D1493 = /D149A`, so
the `DI493` input of CI73's leaf gate EG43A (ch.264 g5) is the active-high
form of the same 40-band decode -- CI73's leaf and CU01's leaf hang off the
same state decode, differing only in the L1U16 term.)

**CI73 is conditional, on an L1 signal the sheet does not print.** fo.143
shows an empty condition cell for `CI73A0 = EG43A0`. But CI73 is another
multi-leaf OR (ch.203 g8, seven leaves) and this family's leaf is built as

    ch.264 g5   EG43A = NAND(DI493, DO211, L1U16, DO211)
                DI493 <- (236-3)  the 40|42 state decode
                DO211 <- (229-7)  the opcode-group decode (tied to two pins)
                L1U16 <- (068-5)  an L1 REGISTER signal, from the ch.068 band

So FI03 is set in 40|42 only when that L1 condition holds, and `CO30 {/FA03}`
in 60|62 then stops fetching the source. **That is the zero-extension**, and
it is where it should be: once operand 2's quartet is exhausted the source
read is inhibited and the remaining destination digits are processed against
nothing. Contradiction 2 was an artifact of believing an empty cell.

Reading the two together, the family's mechanism is coherent and complete:
the LOW quartet counts operand 2 and drives CI73 -> FA03 -> source-fetch
inhibit (zero-extend); the HIGH quartet counts operand 1 and drives CU07, the
loop exit; the X bit is just "not the first iteration" and gates the one-shot
borrow preset. Every piece of fo.140-143 has a job.

**`L1U16` CONFIRMED.** cp06 **ch.068** is titled "DECODING OF VO AND L1 /
DECODIFICA DI VO E L1" and builds both quartet terminals explicitly:

    g4  (NAND3 U08)  L1UIF = NAND(L1006, L1016, L1026, L1036)   bits 0..3
    g5  (NAND1 U15)  L1UI6 = /L1UIF                             -> N22-13
                             fans out to (240-6) (275-8) (264-5) (276-3)
    g9  (NAND4 U10)  L1UMF = NAND(L1046, L1056, L1066, L1076)   bits 4..7
    g10 (NAND1 U15)  L1UM6 = /L1UMF                             -> N22-11
                             fans out to (275-6) (271-4) (258-6) (258-2) (128-3)

`L1UI6` is high exactly when L1 bits 0-3 are ALL ONES, and `(264-5)` in its
fan-out is precisely the EG43A input traced above. So:

    L1UI6  =  "L1_1 = 1i"   the LOW quartet, I = inferiore
    L1UM6  =  "L1_2 = 1i"   the HIGH quartet

which independently confirms the quartet assignment (L1_1 low, L1_2 high) for
the fourth time, and settles the family completely:

    CI73  SET FI03  {L1_1 = 1i}     <- the brace fo.143 prints against CU01
    CU01  SET S001  unconditional
    CU07  SET S007  {L1_2 = 1i}

**fo.143's `{(L1_1 = 1i)}` brace is printed one row too low.** It belongs to
CI73, the row directly above it. With it in the right place every row of
fo.140-143 has a job and the family is coherent:

  * the LOW quartet counts operand 2; when it runs out CI73 sets FI03, FA03
    inhibits `CO30` in 60|62, and the source stops being fetched -- the
    zero-extension;
  * the HIGH quartet counts operand 1 and CU07 ends the loop;
  * the X bit is "not the first iteration", so CO48 presets the borrow once.

Nothing is left unknown. The conversion can be written from this document.

### The contradictions as originally found (kept for the record)

**1. CO48 re-presets the borrow on every iteration.** `{(SD+CMQ+SB)·/SA01}`
fires whenever the X bit is clear, and X is set by `CU01 {(L1_1 = 1i)}`, i.e.
only once operand 2's quartet is exhausted. For equal-length operands (the
common case, e.g. `SD 3,3` -> L1 = 0x22) the two quartets underflow on the
SAME iteration, so X stays 0 for the whole loop and the carry-in is forced to
1 before every digit — which destroys borrow propagation. Nothing else in the
loop touches URPE, so the chain otherwise works by default (that is how
`AD 2,1` propagates its carry). Read literally the sheet computes multi-digit
SD/SB wrongly; the deck cannot tell us, because its SD/SB are single-digit.

The first suspicion was that `CO4811 = DE97A0` (schema 211-3) hides a leaf,
the same shape as the DE00A/CM011 defect. **Traced, and it does not.**

cp06 **ch.211** ("COMMANDS CO48, 90, CI03, 38 GEN.", p271) gate 3 (NAND4 U08):

    CO481 = NAND(EC71A, DA03A, DE56A, DE97A, DA17A, EG21A, EG22A)

so CO48 is indeed a seven-leaf OR and DE97A is only this family's leaf — but
that is the expected shape (one leaf per instruction family's sheet), not a
defect. Following DE97A back, cp06 **ch.238** ("STATUS DECODING", p298) gate
10 (NAND3 U06, pins 13/2/1):

    DE97A = NAND(SA01L, DI042, DO451)
            SA01L  <- (256-15) B30-04   the S001 state bit, low form
            DI042  <- (235-11) B30-02   the 50|52 state decode
            DO451  <- (202-6)  A30-02   the {SD+CMQ+SB} opcode decode

Three terms, exactly the three the timing sheet prints. **fo.142's condition
is complete and correctly transcribed.** So the puzzle is not a missing OR
leaf; it is in what makes SA01 rise on the second iteration, which is not on
any of these four sheets.

Note the deck's own choice of cases is suggestive: `AD 2,1` (source shorter
than destination) is the only multi-iteration algebra test in the sweep, and
in exactly that shape the mechanism works — operand 2's quartet underflows at
the end of iteration 1, CU01 sets X, and CO48 is blocked from iteration 2
onward. It is the EQUAL-length case that has no mechanism to raise X early,
and the deck never exercises one.

**2. CI73 SET FI03 is unconditional, and FA03 inhibits the source fetch.**
fo.143 sets FI03 at TI06 of every 40|42 (`CI73A0 = EG43A0`, no condition, no
parenthesised term). ge.h has FI unloaded into FA at TO10, so FA03 is high by
TO10 of the next 60|62 — before that state's `TO25 CO30 {/FA03}`. Taken at
face value the source byte is read on the FIRST iteration only, and since
40|42's CI32 overwrites RO with the result byte, later iterations would stage
a stale result byte as their "source". That cannot be what the machine does,
so either FA03 means something other than a plain latched FI03 here, or the
FI->FA handoff clears FI and the timing works out differently, or CI73's set
is gated by something not printed.

**3. What FI03/FA03 actually is.** ge.h documents ffFA as fault bits
("Faults, pp. 139-141"); docs/console.md only lists FA00-FA03 as the low four
bits of FA. gemu models FI03 as a plain flip-flop (CI73 set / CI83 reset) and
never sets it in an executive state. If FA03 is a fault line then `{/FA03}`
on CO30 is just "read unless faulted" and is irrelevant to the loop — which
would dissolve contradiction 2 but leaves CI73's unconditional set unexplained.

(1) and (2) are independent of each other; either one alone blocks a faithful
conversion, because guessing on either produces silently wrong arithmetic
rather than a visible failure.

## ch.239 reconstructed from the physical card layout (2026-07-21)

The one unverifiable CM021 leaf (DI60A, generated in the scan-missing ch.239)
has been reconstructed from cp08 (card layout, dwg 14026136) without the
schematic page, by Marco's method: find the chapter's physical board, then
rebuild the logic from a same-type board whose chapter we do have.

**Board lookup.** cp08 p7 (RIGA A) / p8 (RIGA B) list one card per line with
its 17 connector-pin net names. Card A16+B16 = `0610047F LOSE2M`, and its
pins carry exactly the twelve ch.239 outputs. The same part number sits at
A17+B17 — which is ch.248, a chapter we can read. Cross-validation: ch.248's
printed pin labels match the cp08 A17/B17 rows pin-for-pin (16 of 16 nets),
so the LOSE2M pin->gate template is trusted:

    side A: g1 out 01 (in 02,03)   g3 out 05 (in 04,03)   g2 out 07 (in 09,10)
            g4 out 12 (in 11,10)   g10 out 15 (in 14,13)  g8 out 16 (in 06,13)
    side B: g11 out 02 (in 01,03)  g9 out 04 (in 05,03)   g5 out 07 (in 06,12)
            g12 out 11 (in 09,10)  g6 out 14 (in 13,12)   g7 out 15 (in 16,12)

**ch.239 = A16+B16, all twelve gates** (output pin = index gate number, 12/12
match, which is itself a strong check):

    g1  DI70A = NAND(SA006, D1391)     g7  DI66A = NAND(SA016, D1131)
    g2  DI72A = NAND(SA006, D1621)     g8  DI89A = NAND(SA01L, D1501)
    g3  DI71A = NAND(D1391, SA00F)     g9  DI82A = NAND(SA006, D1111)
    g4  DI73A = NAND(D1621, SA00F)     g10 DI90A = NAND(SA016, D1501)
    g5  DI65A = NAND(SA01M, D1131)     g11 DI83A = NAND(SA00F, D1111)
    g6  DI67A = NAND(D1031, D1131)     g12 DI60A = NAND(DI121, SA028)

Every input is an SAxxx state-register bit or a status-band DIxxx buffer:
the whole board is state decoding, like its DESA2x neighbours.

**DI60A specifically:** `NAND(DI121, SA028)` at B16 pins 09/10 -> 11.
Independent confirmation from a page that was never missing: ch.252 g11
prints its DI121 output fanning to **(239-12)** -- the exact gate. Unwinding
DI121 = /DI12A, DI12A = NAND(SA03F, D1101) (ch.225 g25), D1101 = SA076·SA056:

    DI60A asserted  <=>  SA07 · SA05 · /SA03 · SA02    (bits 6,4,1,0 free)

**Pure state decode. All seven CM021 leaves are now verified; the CU01
conclusion stands with no unverified links.**

Residual uncertainties, stated: cp08 cells were read from 300-dpi renders
(not OCR); `SA028`'s last glyph could be read as B (`SA02B`) -- either way it
is an SA02 rail form and the conclusion is unaffected. The gate template
assumes A16's backplane follows the same LOSE2M internal wiring as A17,
which the 12/12 output-pin/gate-number match makes near-certain.
