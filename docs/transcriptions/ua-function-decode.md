# UA function decode — CI45/CI46/CI47 (cp06 ch.087, PDF p163)

Sheet: "ARITHMETICAL UNIT, CONCENTRATOR / UNITA ARITMETICA, CONCENTRATORE",
dwg 14013 0651, CHAPTER 087. Page = chapter + 76 in this band (confirmed
against the cartiglios of p163/164/170/171 = ch.087/088/094/095).

Wires traced on the sheet by the maintainer, 2026-07-21. PARTIAL: `UCO01`
and `UCO2A` are still unread.

## Entry inverters

All three use the tied-input NAND idiom this drawing favours (the same shape
as `DE001` on ch.199 and `CI50B` on ch.094 — a NAND with both inputs on one
net is how the sheets draw an inverter).

    gate 22  U25   CI451  from (196-2) P25-04   ->  CI45D = /CI45
    gate 26  U25   CI461  from (196-1) P25-05   ->  CI46B = /CI46
    gate 31  U25   CI471  from (190-3) P25-06   ->  CI47B = /CI47
                   (U25.4 and U25.5 both on CI471)

## Function lines

    gate 32  U20.1=CI46B U20.2=CI47B -> U20.3
             UCOA1 = NAND(/CI46, /CI47) = CI46 + CI47        <- NO CI45 term
    gate 33  U20.4=UCOA1 U20.5=CI45D
             UCO21 = NAND(/CI45, CI46+CI47) = CI45 + /(CI46+CI47)
    gate 23  U25.1=CI451 U25.2=CI471 -> U25.3
             UCO4A = NAND(CI45, CI47) = /(CI45 . CI47)
    gate 24  U20, inverter of UCO4A, out pin 11
             UCO41 = CI45 . CI47      -> (025-13) (091-26) (094-26)
    gate 27  U15.1=CI45D U15.13=CI47B
             UCO0A = NAND(/CI45, /CI47) = CI45 + CI47
    gate 29  U19.2=CI45D U19.1=CI471 -> U19.3
             UCO1A = NAND(/CI45, CI47) = CI45 + /CI47
    gate 30  U19, inverter of UCO1A (U19.13 + U19.14 tied), out pin 11
             UCO11 = /CI45 . CI47     -> (087-5) (087-8) (087-19)
                                         (088-5) (088-14) (088-15)

## Still unread

    gates 25 + 28   U13, outputs pins 6 AND 8 wired onto one net -> UCO01
                    a two-term wired OR, inputs unknown
                    -> (087-3) (088-21) (088-3) (088-10) (088-11)
                       (088-16) (088-17)
                    destinations U15.4, U15.11, U09.5, U12.2, U12.3
                    (= gates 20, 15, 9, 14, 13 on this sheet)
    gate 34         U20 -> UCO2A, inputs unknown
                    -> (087-16) (088-8).  Probably /UCO21.

## The code, evaluated

| operation        | CI45 CI46 CI47 | UCOA1 | UCO21 | UCO41 | UCO0A | UCO11 |
|------------------|----------------|-------|-------|-------|-------|-------|
| binary add       | 0 0 0          | 0     | 1     | 0     | 0     | 0     |
| binary subtract  | 0 0 1          | 1     | 0     | 0     | 1     | 1     |
| decimal add      | 0 1 0          | 1     | 0     | 0     | 0     | 0     |
| decimal subtract | 0 1 1          | 1     | 0     | 0     | 1     | 1     |
| AND              | 1 1 0          | 1     | 1     | 0     | 1     | 0     |
| XOR              | 1 0 1          | 1     | 1     | 1     | 1     | 0     |
| OR               | 1 1 1          | 1     | 1     | 1     | 1     | 0     |

The CI45/CI46/CI47 combinations come from the timing sheets: fo.43 (NI/XI/
CI/TM), fo.146 (XC/OC/NC), fo.39 (register), fo.78 (CMI/CMC), fo.142
(AB/SB/AD/SD/MVQ/CMQ).

## The finding, and what it implies

`UCO11` is the SUBTRACT line (`/CI45 . CI47`), not a binary/decimal
discriminator -- an earlier guess that gate 29 took CI46B was wrong, it takes
CI471.

So **decimal add is distinguishable from binary add** (UCOA1 and UCO21 both
flip) but **decimal subtract is NOT distinguishable from binary subtract** --
identical on all five traced lines. That asymmetry is real, not a gap in the
tracing.

Where the missing decimal-ness probably lives: on fo.142 `CI46` and `CI50`
are driven by the SAME gate with the SAME family set,

    CI46 = DE99A0 {(AD+SD+CMQ)}     decimal
    CI50 = DE99A0 {(AD+SD+CMQ)}     "OPERA SOLO UA1"

so they always fire together for the decimal family, and ch.094 shows CI50
inhibiting `UZE71`/`UZE81`, the inter-zone enables. That suggests decimal
behaviour is carried by the zone/carry gating rather than wholly by the
function code -- which would explain the SD/SB degeneracy directly.

`UCO01` is the last place in the code where a binary/decimal distinction
could hide. If it does not carry one either, the conclusion is that the UA
computes the same FUNCTION for SD and SB and the decimal correction comes
from CI50 restricting propagation to a single quartet. In that case gemu
should implement CI50 as a width gate and drive the existing alu_dec.c from
CI46 and CI50 together, exactly as fo.142 pairs them.
