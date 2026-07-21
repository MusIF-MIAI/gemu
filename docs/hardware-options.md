# Backplane straps and maintenance options

Source: cp06 **CHAPTER 002, "VARIANTI E OPZIONI / CHANGE AND OPTION"**,
drawing **140 130 65 6**, UCE 460. PDF page 79 of
`GE 120 CENTRAL PROCESSOR [6].pdf`.

The GE-120 is configured by plugging small jumper cards into three backplane
connector positions, plus one switch on the maintenance panel. Everything
below is on that one sheet.

## Where to look on the machine

Three **option connector positions** in the backplane, drawn on ch.002 as the
four `COIN` blocks and labelled by position:

| position | selects | signals |
|---|---|---|
| **E04** | which two connectors are enabled for the initial LOAD | `FUL26`, `FUL36` |
| **F03** | which connectors may raise an interruption | `INES3`, `INES4` |
| **F04** | machine version: cycle period and instruction set | `FEL06`, `FEL16`, `FUL4G` |

The jumper cards are **`PONT2N`** and **`PONT2P`** (and `PONT2H` on F03/F04);
"no card fitted" is itself a valid configuration and is what the tables print
as `/`.

One **maintenance panel switch**: **`S42`, labelled "LAMPS"**, whose `DIAG`
position overrides part of the strapping (see the note below).

## TAB. 1 — machine version (F03 / F04)

| version | cycle period | performances | interruptions | E03 | F04 | FEL06 | FEL16 | FUL4G |
|---|---|---|---|---|---|---|---|---|
| UCE 466 | 6 µsec | MIN | no | / | PONT2H | 1 | 1 | 0 |
| UCE 467 | 4 µsec | MAX | no | PONT2P | PONT2P | 0 | 1 | 1 |
| UCE 467 | 4 µsec | MAX | yes | PONT2P | / | 0 | 1 | 1 |
| UCE 468 | 2 µsec | MAX | no | PONT2H | PONT2P | 0 | 0 | 1 |
| UCE 468 | 2 µsec | MAX | yes | PONT2H | / | 0 | 0 | 1 |

So **`FUL4G` reads "this machine has the MAX instruction set"** — it is 0 only
on the slow 6 µsec UCE 466.

## TAB. 2 — interruption-enabled connectors (F03)

| connectors enabled | F03 | INES3 | INES4 |
|---|---|---|---|
| 3 and 4 | / | yes | yes |
| 3 | PONT2N | yes | no |
| 4 | PONT2P | no | yes |

## TAB. 3 — load-enabled connectors (E04)

| connectors enabled | E04 | FUL26 | FUL36 |
|---|---|---|---|
| 2 and 3 | / | 1 | 1 |
| 2 and 4 | PONT2N | 1 | 0 |
| 4 and 3 | PONT2P | 0 | 1 |

## The S42 "LAMPS" note

Printed on ch.002 in both languages:

> THE LEVEL OF THE SIGNALS FUL01 - FUL11 - FUL4F IS THE SAME OF THE SIGNALS
> FEL06 - FEL16 - FUL4G (RESPECTIVELY), WHEN THE SWITCH "LAMPS" (S42 IN THE
> MAINTENANCE PANEL) IS **NOT** IN THE POSITION "DIAG". AT THE CONTRARY, WHEN
> S42 IS IN THE POSITION "DIAG" THE LEVEL OF THE ABOVE MENTIONED SIGNALS
> BECOMES: FUL01=0 ; FUL11=0 ; FUL4F=1

So each of these has two forms: a `G`/`06`/`16` form driven by the straps, and
an `F`/`01`/`11` form that the DIAG switch can force. **`FUL4F` is the one the
timing charts cite as `{FUL4}`.**

## Why this matters to the timing charts

The LA, LPSR and register beta sheets carry a row `CI89 SET ALTO {FUL4}`, and
`CI89` sets the hardware stop flip-flop. Reading that with the table above:
those instructions **halt the machine when FUL4 is asserted**, and FUL4 is
asserted either on a MAX-performance machine or with the maintenance switch in
DIAG.

That is worth flagging as an open question rather than a settled reading,
because the obvious interpretation runs the wrong way: a MAX machine is the
one that *should* support these instructions. Two possibilities:

  * the printed condition is overbarred (`{/FUL4}`), so the MIN machine halts
    on an instruction it does not implement -- which is exactly what an
    unimplemented-operation trap looks like on a machine with no trap
    mechanism; or
  * the row really is the DIAG-mode maintenance stop, and reaching it on a
    MAX machine outside DIAG is prevented by something else on the sheet.

Resolving it needs the LA/LPSR beta sheets re-read at the CI89 row for an
overbar. Until then gemu carries the option model but not the row.

## The machine at Electric Dreams

Its card layout is public:
<https://docs.google.com/spreadsheets/d/19S23bxF4Ik-H6zl61luwYC_t-KFmUUzMeeC4uNQSRVo/>
sheet "Posizione schede su armadio CPU". Rows are listed in pairs (AB, CD,
EF, ...), so the option positions are all on the **EF** line:

    card 05:  0618034Z | 0618035V      <- row E | row F
    card 04:  (empty)
    card 03:  0618034Z | 0618035V
    card 06 and up: 47F 47F 47F 47F 11T 32H ... 44T 44T ...

Two things stand out. The sheet writes the FULL part number only at cards 03
and 05 -- every other position is the 3-character suffix (44T "44-tango", 53F,
40V, 47F...) -- which marks those two as the option cards rather than part of
the ordinary logic population. And **card 04 is empty in both rows**.

Reading that against the tables:

  * **E04 empty** -> TAB.3 row 1: FUL26 = FUL36 = 1, loading enabled on
    **connectors 2 and 3**. This is exactly the default gemu already assumed,
    now confirmed against a real machine rather than assumed.
  * **F04 empty** -> TAB.1: an empty F04 appears only on the
    interrupts-ENABLED rows, which are UCE 467 and UCE 468 -- both MAX
    performance, both **FUL4G = 1**.
  * **E03 populated** -> TAB.1 distinguishes the two: PONT2P is the 4 usec
    UCE 467, PONT2H the 2 usec UCE 468. So identifying whether `0618034Z` is
    PONT2P or PONT2H tells you which model the machine is.
  * **F03 populated** -> TAB.2: interruption enabled on ONE connector (3 with
    PONT2N, 4 with PONT2P), not both.

That F04 reading corrected a bug here: FUL4G had been derived as
`F04 == PONT2P`, which reads TAB.1 off the "no interrupts" rows only and gets
an empty F04 backwards. It is low for exactly one strap, F04 = PONT2H.

### Open question on that machine

The 44-tango (`0610044T`) is the commonest logic card in the CPU -- rows E and
F alone hold a dozen of them from card 18 upward -- so a 44-tango is not a
PONT jumper. If the option positions are found stuffed with 44-tangos rather
than with `061803xx` cards, that is a different configuration from the one
this sheet records, and worth re-checking against the sheet.

## In gemu

`struct ge_options` in `ge.h` holds `E04`, `F03`, `F04` (as `enum ge_pont`)
and `S42_diag`. `signals.h` derives `FUL26`, `FUL36`, `FUL4G`, `FUL4F` from
them, with `FUL2`/`FUL3`/`FUL4` as the names the charts use. The default --
all connectors empty, S42 not in DIAG -- is the E04-empty row of TAB.3, which
is what the initial-load tests assume.
