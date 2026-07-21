# Counting network (rete di conta) — scope for the quartet length count

Sheets: cp06 **chapters 096-099 = PDF pages 172-175** ("COUNTING NETWORK /
RETE DI CONTA", dwg 14013 0650). Offset page = chapter + 76, same band as the
arithmetic unit; confirmed on all four cartiglios.

Read at 300dpi 2026-07-21. This file scopes what the decimal-family
conversion actually needs; it is NOT a full transcription of the adder.

## Why this matters

fo.141 configures the length count with FOUR commands at TO30, all from one
gate:

    CI40 = DI05B0          decreasing
    CI41 = CD08A0 (DE17A0) count from 00
    CI42 = CD08A0 (DE17A0) count from 04
    CI44 = DI05B0          stop count 07

and fo.143 exits with TWO conditions:

    CU01 {(L1_1 = 1i)}     operand-1 quartet exhausted
    CU07 {(L1_2 = 1i)}     operand-2 quartet exhausted

So L1's low byte holds TWO independent counters, one per quartet -- which is
the SS instruction format, where each operand carries its own length. That is
the "two-quartet parallel count" the earlier notes called a blocker.

## What turns out NOT to be a blocker

The exit conditions need no sheet. The manual's notation is positional and
already established by MVC, where `{L1_2,1 = 1i}` is the whole low byte and
gemu implements it as `(rL1 & 0xff) == 0xff`. So

    {L1_1 = 1i}  ==  (rL1 & 0x0f) == 0x0f
    {L1_2 = 1i}  ==  (rL1 & 0xf0) == 0xf0

CI42 is already implemented too (`counting_network.ci_cmds.from_04`, delta
+= 0x10 in ge_counting_network_output).

## What IS the open question

With CI41 and CI42 both raised, gemu computes a single byte-local
subtraction of 0x11. That is only equivalent to "decrement each quartet by
one" while quartet 1 does NOT underflow:

    0x35 - 0x11 = 0x24     both quartets -1        correct
    0x30 - 0x11 = 0x1F     q1 wrapped AND borrowed into q2, so q2 went -2

If the two counters are genuinely independent, the hardware must BLOCK the
borrow crossing bit 03 into bit 04 -- a quartet-boundary twin of what CI44
("stop count 07") does at the byte boundary. gemu models the byte block
(`stop_07`) but has no quartet block, so its 0x11 subtract ripples.

Whether that block exists decides the implementation:

  * block exists  -> add a `stop_03` analogue; each quartet counts alone and
                     the two exit conditions are independent, as fo.143 reads
  * no block      -> the ripple is intended, and the family must be relying
                     on operand lengths that never let quartet 1 underflow
                     before its exit fires -- which would need justifying

## What chapter 096 shows

Bit slice for **bits 04-07** (quartet 2). Inputs are the BO true/complement
pairs BO042/BO04B, BO052/BO05B, BO062/BO06B, BO072/BO07B; outputs BU051,
BU061, BU071 to P20-11/10/09 -> (101-16)(101-20)(101-21), plus BURMA ->
(099-5).

Command lines visible on the sheet: **CA401 / CA40A** (the "decreasing"
command) and **CA441 -> gate 17 (U23) -> CA44B** (the "stop 07" command).

Carry/lookahead terms: BUCA1 (gate 1), BUCE1 (5), BUC11 (10), BUC01 (11),
BUCU1 (16), BUDA1 (19), and BUDO1/BUDI1/BUDE1 arriving from (097-24),
(097-22), (097-17). BIO4A/BIO41, BIO5A/BIO51, BIO6A/BIO61 are the per-bit
terms.

**No count-from-04 command line appears on this sheet.** There is no CA42x
anywhere on ch.096, which is where the injection at bit 04 would be expected.

## RESOLVED

`CA421` is generated at **ch.038 gate 19 (U24.3)** and fans out to L34-12,
**097-23** and 325-7. Chapter 097 (PDF p173, dwg 14013 0652) is the bits
00-03 slice, the twin of ch.096:

    gate 23  U23     CA421 from (038-19) 020-03  ->  CA42B
                     an inverter -- the exact twin of ch.096's
                     gate 17 turning CA441 into CA44B
    gate 24  U13     "NAOR 1", one pin floating
                     U13.5 = CA41A   from (038-20)
                     U13.4 = U13.3 = CA42B  (tied across both pairs)
                     U13.2 = CA431   from (038-6)
                     ->  BUD01  ->  (096-2) (096-7) (096-13) (096-18)

`BUD01` is the term that drives quartet 2's carry chain over on ch.096, and
it is built from **command lines only** -- no carry or propagate term out of
the bits 00-03 chain appears in it. So quartet 2 counts because it was told
to, not because quartet 1 borrowed into it, and a borrow crossing bit 03 must
not disturb it. The quartets are structurally independent.

(The block is therefore not a "stop 03" gate as guessed. It is the absence of
a path: quartet 2's chain is simply not fed from quartet 1's.)

### What the exits actually do

Re-reading fo.143 with this in hand corrects an earlier misreading. From
state 40, CU05 alone gives 0x60 and CU05+CU01 gives 0x62, so

    CU01 {(L1_1 = 1i)}    does NOT terminate -- it selects 62 over 60
    CU07 {(L1_2 = 1i)}    is the only exit, to E2/E3

L1's low byte holds one length per SS operand: quartet 1 for operand 1,
quartet 2 for the destination. When operand 1 is exhausted the loop keeps
running in the X variant, zero-extending the shorter operand, until the
destination is full. Which is exactly why the counters must not interfere:
if quartet 1's borrow reached quartet 2, every unequal-length operand pair
would finish short.

## Implemented

`ge_counting_network_output` now treats from_zero + from_04 as two
independent nibble counters. Verified: suite 303/303, and the funktionalcpu
0x40 deck is identical to baseline on all 159 transitions including cycle
numbers -- the register family raises CI41+CI42 as well, but its two-pass
loop never wraps quartet 1, so nothing observable changes there.
