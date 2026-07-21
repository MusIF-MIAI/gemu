# AB-SB-AD-SD-MVQ-CMQ timing sheets (cp07 fo.140-143, PDF p126-128)

Read from source at 600dpi, audit round 3 (2026-07-21). Conversion DEFERRED:
needs the UA decimal mode (CI46 decimal + CI50 "work only UA1") and the
digit/quartet length-count decode (CI42 count-from-04 in the loop; CU01 gated
{(L1_1 = 1i)} — the X bit is a *quartet* counter here, not a pass counter).
OVERBAR(x) marks a printed negation bar. Family walks V2 DESCENDING (CO40 in
60|62): arithmetic runs LSB-first for the carry chain.

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
