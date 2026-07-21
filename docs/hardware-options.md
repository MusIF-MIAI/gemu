# Backplane straps and maintenance options

Source: cp06 **CHAPTER 002, "VARIANTI E OPZIONI / CHANGE AND OPTION"**,
drawing **140 130 65 6**, UCE 460. PDF page 79 of
`GE 120 CENTRAL PROCESSOR [6].pdf`.

The GE-120 is configured by plugging small jumper cards into three backplane
connector positions, plus one switch on the maintenance panel. Everything
below is on that one sheet.

## Memory capacity is a separate sheet

cp06 **CHAPTER 001, "SELEZIONE CAPACITA' MEMORIA / MEMORY CAPABILITY
SELECTION"**, dwg 14013 065 6, PDF page 78 -- the sheet immediately before
ch.002. Same mechanism, two more connector positions, **E05** and **F05**:

| version | memory | E05 | F05 | VAMA2 | VEMB6 | VAMC2 |
|---|---|---|---|---|---|---|
| UCE 460 | 8K  | / | / | 1 | 1 | 1 |
| UCE 461 | 12K | PONT2N | / | 1 | 1 | 0 |
| UCE 462 | 16K | / | PONT2N | 1 | 0 | 1 |
| UCE 463 | 24K | PONT2P | PONT2N | 0 | 0 | 1 |
| UCE 464 | 32K | PONT2N | PONT2P | 0 | 0 | 0 |

The connector blocks: E05 carries `VAMC2` on pin 4 and `VAMA2` on pin 3; F05
carries `VAMA2` on pin 3 and `VEMB6` on pin 1. The same S42 "LAMPS" note
applies -- `VAMA1`/`VAMB1`/`VAMC1` follow `VAMA2`/`VEMB6`/`VAMC2` unless S42
is in DIAG, when they become 1 / 0 / 1.

So the UCE numbering runs on two independent axes: **460-464 is the memory
capacity** (ch.001) and **466-468 is the processor version** (ch.002). A
machine is one of each.

gemu currently allocates a flat 64K (`MEM_SIZE` in ge.h), above every
documented capacity, and does not model the selection.

## Where to look on the machine

Three **option connector positions** in the backplane, drawn on ch.002 as the
four `COIN` blocks and labelled by position:

| position | selects | signals |
|---|---|---|
| **E04** | which two connectors are enabled for the initial LOAD | `FUL26`, `FUL36` |
| **F03** | which connectors may raise an interruption | `INES3`, `INES4` |
| **F04** | machine version: cycle period and instruction set | `FEL06`, `FEL16`, `FUL4G` |

The jumper cards are **`PONT2N`** and **`PONT2P`** -- those two types only;
"no card fitted" is itself a valid configuration and is what the tables print
as `/`.

> **Correction, 2026-07-21.** Earlier revisions of this file (and of
> signals.h/ge.c) named a third type, "PONT2H".  It does not exist: every
> "PONT2H" was a misread of **PONT2N** in the 1968 typewriter face of
> ch.001/ch.002, re-read at 400 dpi after the physical cards were identified
> at Electric Dreams: **0618034Z reads PONT2N on the board**, and
> **0618035V is electrically a PONT2N as well** (different part code, same
> strap function).  The identification question this file used to pose --
> "is 0618034Z a PONT2P or a PONT2H?" -- was therefore answered *neither*.

One **maintenance panel switch**: **`S42`, labelled "LAMPS"**, whose `DIAG`
position overrides part of the strapping (see the note below).

## TAB. 1 — machine version (E03 / F04)

| version | cycle period | performances | interruptions | E03 | F04 | FEL06 | FEL16 | FUL4G |
|---|---|---|---|---|---|---|---|---|
| UCE 466 | 6 µsec | MIN | no | / | PONT2N | 1 | 1 | 0 |
| UCE 467 | 4 µsec | MAX | no | PONT2P | PONT2P | 0 | 1 | 1 |
| UCE 467 | 4 µsec | MAX | yes | PONT2P | / | 0 | 1 | 1 |
| UCE 468 | 2 µsec | MAX | no | PONT2N | PONT2P | 0 | 0 | 1 |
| UCE 468 | 2 µsec | MAX | yes | PONT2N | / | 0 | 0 | 1 |

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

Sharpened by the 2026-07-21 identification: on a UCE 468 the strap levels are
FEL06/FEL16/FUL4G = 0/0/1 and the DIAG override forces 0/0/1 -- **identical**,
so S42 is a no-op for these signals on this machine and `FUL4` is asserted
unconditionally. There is no switch position in which a verbatim
`CI89 SET ALTO {FUL4}` would NOT halt this machine, which leans further
toward the overbar reading.

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
    UCE 467, PONT2N the 2 usec UCE 468.
  * **F03 populated** -> TAB.2: interruption enabled on ONE connector (3 with
    PONT2N, 4 with PONT2P), not both.

That F04 reading corrected a bug here: FUL4G had been derived as
`F04 == PONT2P`, which reads TAB.1 off the "no interrupts" rows only and gets
an empty F04 backwards. It is low for exactly one strap, F04 = PONT2N.

### Physical identification, 2026-07-21 (settles E03/F03)

Read off the machine: **E03 carries 0618034Z, which is printed PONT2N**, and
**F03 carries 0618035V, which is electrically a PONT2N too** despite the
different part code (identified by its trace pattern against the confirmed
2N).  Against the tables:

  * **E03 = PONT2N -> UCE 468**: 2 usec, MAX instruction set.  gemu's assumed
    model was right, but for the wrong reason -- it had 34Z down as "PONT2H",
    a card type that does not exist (see the correction above).
  * **F03 = PONT2N -> interruption enabled on connector 3 only**
    (INES3 = 1, INES4 = 0).  gemu had modelled connector 4; fixed in ge.c.

### The card-05 pair: an off-table combination

Both part numbers being PONT2N applies at card 05 too (E05 = 34Z,
F05 = 35V), so the machine's capacity straps are **E05 = N, F05 = N -- a
combination the ch.001 table never defines**.  The five printed rows are
`/ /` (8K), `N /` (12K), `/ N` (16K), `P N` (24K), `N P` (32K).

**RESOLVED (2026-07-21): the per-pin reading wins.**  Requiring one fixed
shorted-pin set per card type to reproduce every printed row of TAB.1, TAB.2,
TAB.3 and the ch.001 table simultaneously over-determines the answer:

    PONT2N shorts pins {1, 4}          PONT2P shorts pins {1, 3}

(and drops the F04 pin assignment out as a bonus: FUL4G on pin 4, the
interrupt inhibit on pin 1, explaining why both card types disable interrupts
in F04).  The trace-side photos of the machine's two cards show identical
etch and identical solder patterns, corroborating that both are the same
type.

So the machine's N+N at card 05 reads **(VAMA2, VEMB6, VAMC2) = (1, 0, 0)**:
VAMA2 is grounded only by a PONT2P, and there is none.  That is no printed
row.  gemu now computes the selection signals from the pin mechanism
(signals.h), straps N+N as found, and `ge_memory_capacity_k()` reports the
combination as off-table (0) rather than guessing; the startup log says so
explicitly.  What the memory bound logic actually does with (1, 0, 0) is the
remaining open question -- the ch.001 fan-outs point at (077-x)/(309-x)/
(318-x)/(321-x) for the consumers, and a memtest on the machine above 16K
would answer it empirically.

**Falsifiable check available**: two spare boards stamped `18036` exist, with
a different connector style and a different staple pattern -- plausibly the
pre-upgrade strap cards, i.e. PONT2P.  If so, buzzing (or photographing the
trace side of) a 18036 against a 18034 must show the difference of exactly
one short: pin 4 on the 2N vs pin 3 on the 2P, with pin 1 common.  Any other
difference falsifies the {1,4}/{1,3} derivation.

### The physical card (photographed 2026-07-21)

One of the machine's strap cards, in hand: an orange single-sided board in a
blue carrier, 17-pin gold edge connector -- matching cp08's PIEDINI DEL
CONNETTORE 01-17 -- date-stamped 19 APR 1971.  Three findings:

  * **Two cards photographed, and the stamps differ while the annotation is
    the same**: one board is stamped `18035`, the other `18034` (over-struck),
    and BOTH are hand-marked "PONT 2N" with `618034` written on -- reading as
    the PONT2N assembly's catalogue number being annotated onto whichever
    bare board carries it.  The stamped numbers are the boards' own part
    markings, which is where the layout sheet's 0618034Z / 0618035V split
    comes from.  The two cards were pulled from **E03 and E05** -- both row-E
    option positions physically confirmed PONT2N -- and one of them is the
    `18035`-stamped board, so the layout sheet's per-row stamp assignment is
    itself approximate.  F05 remains unverified (strapped PONT2N on the
    both-codes-are-2N instruction); the buzz-out is deferred, so the N+N
    capacity reading stays flagged as open.
  * **The strap type is set at jumpering time, not in the etch**: the board
    is a generic PONT2 with three full-width rows of plated holes, and the
    type is selected by where bare-wire staples are soldered ("PONT 2N" is
    handwritten on this one).  Four staples visible: two adjacent at the
    left and one at the right bridging the top row to the middle row, one at
    the lower right bridging middle to bottom.  **The two photographed cards
    carry this same staple pattern**, which is the electrical identification:
    same jumpering = same strap type, regardless of the stamp.  (The second
    photo also shows the connector housing's molded pin numbering along the
    17-pin edge -- the reference for a buzz-out.)
  * **The card in hand can settle the N+N capacity question**: buzz the 17
    edge pins pairwise with a tester to get the card's short map, then apply
    it to the ch.001 connector assignments (E05: VAMC2 pin 4, VAMA2 pin 3;
    F05: VAMA2 pin 3, VEMB6 pin 1).  Whether a PONT2N shorts pin 3 decides
    between the (0,0,0) = 32K reading and the (1,0,0) = off-table reading
    above.  A photo of the trace side would serve equally.

### F03 momentarily absent

The physical F03 card is mislaid (last seen in a 2018 photo).  While the
socket is empty the real machine runs the TAB.2 `/` row -- interruptions
enabled on BOTH connectors -- and will return to connector-3-only when the
card is restuffed.  gemu models the intended, restuffed configuration.

### Not to be confused with the 44-tango

The 44-tango (`0610044T`) is the commonest logic card in the CPU -- rows E and
F alone hold about a dozen from card 18 upward, including E36 and E37 -- and
it is a LOSE23/LOSE2S logic board, not a PONT jumper. The option positions are
at the other end of the row, cards 03 to 05.

## In gemu

`struct ge_options` in `ge.h` holds `E04`, `F03`, `F04` (as `enum ge_pont`)
and `S42_diag`. `signals.h` derives `FUL26`, `FUL36`, `FUL4G`, `FUL4F` from
them, with `FUL2`/`FUL3`/`FUL4` as the names the charts use. The default --
all connectors empty, S42 not in DIAG -- is the E04-empty row of TAB.3, which
is what the initial-load tests assume.
