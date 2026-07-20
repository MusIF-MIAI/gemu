# SR-SL timing sheets (cp07 fo.152-155, PDF p132-134)

Read from source at 600dpi, audit round 3 (2026-07-21). Conversion DEFERRED:
SR/SL currently execute outside the per-clock model, and their EA/EB tail
(the found-address write into change register 7 via the {SR+SL+JRT} forcing,
already variant-ready in state_ea_instr) needs the search datapath below
first. OVERBAR(x) marks a printed negation.

## fo.152 — 64|65 beta (SA 0110 010X)
TO50 NO->BO; TO65 CO49 = DI06A0; TI06 CU10 = DE20A0 (note: DE20A0 here, not
DI06A0); TI06 CU12 = DI06A0. Exit: 60+62 unconditional.

## fo.153 — 60|62 (SA 0110 00X0)
- TO10 CO12 = DE16A0                        V2 -> NO
- TO10 CO41 = DI05B0                        count from 00 (V2 ASCENDS)
- TO25 CO30 = EG09A0 {OVERBAR(FA03)}        MEM -> RO
- TO30 CI15 = CD03A0 (DI05A0)               L1 -> NO
- TO30 CI40 = DI46A0                        CI decreasing (gate DI46A0!)
- TO30 CI44 = DI05B0                        CI stop 07
- TO30 CI41 = CD09A0 (DE18A0)               CI count from 00
- TO50 NO -> BO
- TO70 CI65 = CD11A0 (DE17A0); TO70 CI60 = CD11A0 (DE17A0)   stage byte
- TI05 CI05 = DE89A0                        NI -> L1
- TI06 CI74 = DE94A0                        SET FI04 (unconditional)
- TI06 CI85 = DE94A0                        RESET FI05 (unconditional)
- TI06 CU04 = CM06A0 (DI05A0); TI06 CU15 = DI05A0
Exit: 50+52 unconditional.

## fo.154 — 50|52 (SA 0101 00X0)
- TO10 CO11 = EG15A0                        V1 -> NO
- TO25 CO30 = CB07A0                        MEM -> RO (the search KEY byte)
- TO30 CI15 = CD02A0 (DA19A0)               L1 -> NO
- TO50 CO48 = CB15A0 (DE41A0)               SET URPE E URPU (unconditional!)
- TO50 NO -> BO
- TO70 CI68 = DA20A0                        UA -> NI43 (the compare)
- TI05 CI05 = CD02A0                        NI -> L1
- TI06 CU14 = CM09A0 (DI04A0)
Exit: 40+42 unconditional.

## fo.155 — 40|42 (SA 0100 00X0)
- TO10 CO11 = CB01A0 (DI49A0)               V1 -> NO
- TO10 CO41 = CB14A0 (DA31A0)               count from 00
- TO10 CO40 = CB12A0 (DE95A0) {SL}          DECREASING {SL}: SL walks V1
                                            down, SR up (direction switch)
- TO30 CI15 = CD21A0 (DA21A0)               L1 -> NO
- TO40 CO01 = CB01A0 (DI49A0)               NI -> V1
- TO50 row prose: "TRASFERISCE NO IN BO SE MANCA CI32 / IF CI32 IS ABSENT,
  TRANSFER NO -> BO" (the TO50_1 relatch rule, printed!)
- TO50 CI32 = CD21A0 (DA21A0)               NO43 -> RO
- TI06 CI75 = CD18A0 (EG48A0) {(dRO = 0i)}  SET FI05 (found; no bar)
- TI06 CU01 = CM02A0 (DI49A0)               SET S001
- TI06 CU03 = ED67A0 {(dRO=0i) + (L1_2,1 = 1i)}   SET S003 (-> EA path)
- TI06 CU05 = DI49A0                        SET S005
- TI06 CU07 = CM03A0 (EG48A0) {(L1_2,1 = 1i)}     SET S007
- TI06 CU07 = EG49A0 {(dRO = 0i)}           SET S007 (second row, alt gate)
Exit: 60+62 {OVERBAR(dRO=0i) . OVERBAR(L1_2,1=1i)}  (not-found AND not-
exhausted, both terms individually barred) | EA (1110 1010)
{(dRO=0i) + (L1_2,1 = 1i)} (found-or-exhausted -> EA writes the address
into cr7 via the {SR+SL+JRT} forcing).
