# Register 07 (Link Register) — Signal Trace

**Role:** Register 07 (R7) is the link register stored at memory addresses
254/255 (0xFE/0xFF). It holds the return address for JRT (Jump Return)
instructions and the found address for SR/SL (Search Right/Left) instructions.

**Primary users:**
- JRT (0x41) — deposit return address, jump to target
- SR (0xD9) — deposit found search address
- SL (0xDB) — deposit found search address
- PER (0x9E) / PERI (0x9C) — peripheral I/O reference

## Register 07 address generation

The change registers (0-15) reside at memory addresses 240-255. Register 07
is at 254/255 (0xFE/0xFF), the last register in the block.

Address construction for register 07 write (EA/EB states, cp07 fo.34/35):
- **CO18** opens the NO21 forcing, so the forced byte alone drives the knot
- **CO90-CO97** force all eight bits of quartets 2,1 to 1111 1111, giving 0x00FF
- **CO90** forces bit 0, the odd (low) byte of the cell; in EA|EB it is gated by
  DI11A0, the EA|EB state decode (the DI65A0 gate belongs to the ED|EC indexing
  states, which build the same kind of address for a different purpose)
- **EA** writes first, at the forced 0xFF, and deposits the datum's LOW byte;
  CO40/CO41 then walk the address down into **V2**, so **EB** writes 0xFE with
  the HIGH byte

Traced clock-by-clock in `tests/register07.c`: with `V1 = 0x1234` the walk ends
as `mem[255] = 0x34`, `mem[254] = 0x12`.

### Address forcing signal table

| Signal | Source Chapter | Pin | PDF Page | NO bit |
|--------|---------------|-----|----------|--------|
| CO901  | 239 | 5 | 298 | NO00 (low byte, odd address) |
| CO911  | 209 | 4 | 269 | NO01 |
| CO921  | 213 | 5 | 273 | NO02 |
| CO931  | 213 | 2 | 273 | NO03 |
| CO941  | 213 | 4 | 273 | NO04 |
| CO951  | 206 | 6 | 266 | NO05 |
| CO961  | 212 | 4 | 272 | NO06 |
| CO971  | 205 | 5 | 265 | NO07 |

## JRT instruction signal path (cp07 fo.33 → fo.34/35)

### 1. Function decode — DO01A (ch.229 gate 8)

**Chapter 229 (cp06 p289)** — FUNCTION DECODING:
- Gate 8 (NAND 2): `DO01A = !(FOO2F · FOO7F)` → D38-13
  - FOO2F = function bit decode (from ch.104-20, ch.104-2)
  - FOO7F = function bit decode (from ch.104-2, ch.105-18)
- DO01A identifies D0 hex digit (function class 0x0x, 0x4x)

### 2. DO011 — jump function class (ch.236 gate 6)

**Chapter 236 (cp06 p296)** — STATUS DECODING:
- Gate 6 (NAND 2): `DO011 = !(DO01A · C27-12)` → C27-13
  - DO01A = D0 hex digit from ch.229-8
  - C27-12 = additional function class select
- DO011 identifies jump function class (FO6 · /FO3 · /FO7)

### 3. Status decode — DI06A (ch.225 gate 6)

**Chapter 225 (cp06 p285)** — STATUS DECODING:
- Gate 6 (NAND 2): `DI06A = !(SAO65 · SAO26)` → A35-12
  - SAO65/SAO26 = status code bits (from ch.110-4, ch.111-4)
- DI06A = beta band decode (SA 0110 01xx, i.e., 0x40-0x5F)

### 4. DI062 — beta band decode (ch.235 gate 6)

**Chapter 235 (cp06 p295)** — STATUS DECODING:
- Gate 6 (NAND 2): `DI062 = !DI06A` → C26-13
  - D106A = DI06A continuation from ch.225-6
- DI062 is the inverted beta band signal for JRT decode

### 5. JRT decode — DE00A (ch.248 gate 9)

**Chapter 248 (cp06 p307)** — FUNCTION AND STATUS CODES ANDS:
- Gate 9 (NAND): `DE00A = !(DO011 · DI062)`
  - DO011 = jump function class from ch.236-12
  - DI062 = beta band decode from ch.235-13
- DE00A active when: function=D0 AND band=0x4x → matches JRT (0x41)

### 6. DE001 generation (ch.199 gate 6)

**Chapter 199 (cp06 p259)** — COMMAND C164 GEN. AND PARTIAL:
- Gate 6 (NAND): `DE001 = /DE00A` (dual-input inverter)
- DE001 → (199-5), (248-12), (270-12)

### 7. Partial command — CM011 (ch.252 gate 7)

**Chapter 252 (cp06 p311)** — PARTIAL COMMANDS GENERATION:
- Gate 7 (NAND): `CM011 = !(DE00A · DE11A · DE13A · DE06A)`
  - DE11A = console/control codes in beta band
  - DE13A = LPSR in beta band
  - DE06A = LA in beta band
- CM011 → CM01A (inverted) → downstream state control

### 8. State transition — CU031 (ch.219 gate 3)

**Chapter 219 (cp06 p279)** — COMMANDS CU01, 03, 04, GEN.:
- Gate 3 (NAND 4): `CU031 = !(CM04A · CM05A · DE63A · ED67A · ED65A · DE07A · DE40A · EG49A)` → B04-16
- CU031 sets S003 bit → triggers EA state transition
- Gate 4 (NAND 1): `CU03A = /CU031` → B04-15

### 9. Exit state — CU071 (ch.221 gate 3)

**Chapter 221 (cp06 p281)** — COMMANDS CU05, 07, 10 GEN.:
- Gate 3 (NAND 4): `CU071 = !(CMO1A · CMO3A · EG49A · EG52A · EG55A · EG29A · EG35A · CMIOA)` → B03-15
- CU071 sets S007 bit → triggers exit from EA/EB states
- Gate 4 (NAND 1): `CU07A = /CU071` → B03-15

### 10. Change register address — CO181 (ch.209 gate 1)

**Chapter 209 (cp06 p269)** — COMMANDS CO18, 91 GEN.:
- Gate 1 (NAND 4): `CO181 = !(DA26A · DE69A · DE07A · DI13A · DE02A)` → H31-14
  - DE69A/DE07A/DI13A/DE02A = status decode signals
  - DE001 from ch.199-3 feeds into DA26A
- CO181 = change-register address high bit (active for JRT/SR/SL EA/EB states)
- Gate 4 (NAND 4): `CO911 = !(DI35A · EG60A · ED07A · DI11A · EG63A · EG68A)` → H31-01

### 11. Address forcing — CO921/CO931/CO941 (ch.213 gates 5,2,4)

**Chapter 213 (cp06 p273)** — COMMANDS CO92, 93, 94, CI62 GEN.:
- Gate 2 (NAND 3): `CO931 = !(EG61A · EG28A · DE69A · CB17A)` → H32-15
- Gate 4 (NAND 3): `CO941 = !(EC06A · DI11B · DI13A · DE02A)` → H32-14
- Gate 5 (NAND 3): `CO921 = !(EG59A · ED08A · CB17A · EG62A)` → H32-16

### 12. Address forcing — CO951 (ch.206 gate 6)

**Chapter 206 (cp06 p266)** — COMMANDS CO01, 06, 11, 95 GEN.:
- Gate 6 (NAND 1): `CO951 = !(CB18A · EC03A)` → H33-16

### 13. Address forcing — CO961 (ch.212 gate 4)

**Chapter 212 (cp06 p272)** — COMMANDS CO49, 96 GEN.:
- Gate 4 (NAND 3): `CO961 = !(CB18A · ED65A · DI26A · DE88A)` → H29-14

### 14. Address forcing — CO971 (ch.205 gate 5)

**Chapter 205 (cp06 p265)** — COMMANDS CO00, 12, 13, 97, CI09 GEN.:
- Gate 5 (NAND 2): `CO971 = !(CB18A · ED75A · ED68A)` → H37-01

### 15. CO901 — odd byte select (ch.239)

**Chapter 239** — STATUS DECODING (DI65A source):
- DI65A generated at ch.239 pin 5
- CO901 forces NO bit 0, the odd (low) byte of the change-register address

gemu carries `CO90` in two different address builds, under the state decode each
sheet prints:

- `msl-states.c: { TO10, CO90, 0, DI65A0 }` — the **ED|EC** indexing states,
  which resolve a modified address through a change register.
- `msl-states.c: { TO10, CO90, 0, DI11A0 }` — **EA|EB**, the register-07 link
  write this trace follows, and what the EA table at the end of this file
  describes.

The two decodes are disjoint: `DI11A0` is true exactly for `SA` in `e8..eb`, and
`DI65A0` is false across that whole band. Gating EA's `CO90` on `DI65A0`
therefore suppresses it outright, forcing 0xFE instead of 0xFF — the link lands
in `mem[254]/mem[253]` and every JRT return goes to the wrong address. Inside
EA|EB the `DI11A0` gate discriminates nothing, since it holds throughout those
states; it is transcribed because the sheet prints it, and `CO90` is effectively
unconditional there, a cell's low byte always being odd.

### 16. Data path — CI111 (ch.186 gate 1)

**Chapter 186 (cp06 p246)** — COMMANDS CI01, 02, 03, 11, 14, 17, 21, 22, 23 GEN.:
- Gate 1 (NAND 3): `AM031 = !DI10A · !DI10B · !DI11A` → H28-02
- CI111 = V1 → NO data transfer command (return address in V1)

### 17. Data path — CI331 (ch.185 gate 7)

**Chapter 185 (cp06 p245)** — COMMANDS CI10, 19, 33, 66 GEN.:
- Gate 7 (NAND 4): `CI331 = !(G27-11 · G27-12 · G27-15 · G27-13 · G27-16 · G27-14 · H27-02)` → H27-01
- CI331 = NO21 → RO data transfer command

### 18. Data path — CI33A/CI332 (ch.193 gates 7, 2)

**Chapter 193 (cp06 p253)** — COMMANDS CI34, 42, 43, CO40 GEN.:
- Gate 7 (NAND 1): `CI33A = !CI331` → H22-06
- Gate 2 (NAND 1): `CI332 = !CI33A` → H22-04
- CI332 = final data transfer signal (NO21 → RO)

### 19. Memory write — UCO31 (ch.090 gate 3)

**Chapter 090 (cp06 p166)** — ARITHMETICAL UNIT:
- Gate 2 (NAND): UBI01 → UCO3A
- Gate 3 (NAND): `UCO31 = !UCO3A` → feeds ch.091-26, ch.091-29, ch.091-10, ch.091-13
- UCO31 = RO → MEM write command (used in EA/EB states)

### 20. Timed commands — CA2O1, CA191 (ch.036 gate 6)

**Chapter 036 (cp06 p113)** — TIMED COMMANDS CA 11, 12, 15, 17, 20:
- Gate 6 (NAND): `CA2O1 = !CT2O1` → L33-11 (feeds ch.052-14, ch.053-14)
  - CT2O1 from gate 5: `CT2O1 = NOFA1 | CI2O1 | T0302`
- CA2O1 = NO register forcing gate (combines CO18 + timed commands)
- CA191 = gating signal for upper byte forcing (ch.053)
- These signals enable the CO9x1 forcings to take effect on the NO register

### 21. NO register forcing (ch.052 + ch.053)

**Chapter 052 (cp06 p129)** — FORCING IN NO BIT 00÷07:
- Gate 2 (NAOR 1): `NOG0A = CA2O1 | CA181 | CO901` → O32-03 (ch.054-1)
- Gate 6 (NAOR 1): `NOG1A = CA2O1 | CA181 | CO911` → P32-06 (ch.054-5)
- Gate 8 (NAOR 1): `NOG5A = CA2O1 | CA181 | CO951` → P32-02 (ch.054-7)
- Gate 10 (NAOR 1): `NOG2A = CA2O1 | CA181 | CO921` → P32-05 (ch.054-9)
- Gate 12 (NAOR 1): `NOG6A = CA2O1 | CA181 | CO961` → P32-01 (ch.054-11)
- Gate 4 (NAOR 1): `NOG4A = CA2O1 | CA181 | CO941` → P32-03 (ch.054-3)
- Gate 14 (NAOR 1): `NOG3A = CA2O1 | CA181 | CO931` → P32-04 (ch.054-13)
- Gate 16 (NAOR 1): `NOG7A = CA2O1 | CA181 | CO971` → O32-15 (ch.054-15)

**Chapter 053 (cp06 p130)** — FORCING IN NO BIT 08÷15:
- Gate 2 (NAOR 1): `NOG8A = CA2O1 | CA191 | CO901` → O31-03 (ch.055-1)
- Gate 6 (NAOR 1): `NOG9A = CA2O1 | CA191 | CO911` → P31-06 (ch.055-5)
- Gate 10 (NAOR 1): `NOGAA = CA2O1 | CA191 | CO921` → P31-05 (ch.055-9)
- Gate 14 (NAOR 1): `NOGBA = CA2O1 | CA191 | CO931` → P31-04 (ch.055-13)
- Gate 4 (NAOR 1): `NOGCA = CA2O1 | CA191 | CO941` → P31-03 (ch.055-3)
- Gate 8 (NAOR 1): `NOGDA = CA2O1 | CA191 | CO951` → P31-02 (ch.055-7)
- Gate 12 (NAOR 1): `NOGEA = CA2O1 | CA191 | CO961` → P31-01 (ch.055-11)
- Gate 16 (NAOR 1): `NOGFA = CA2O1 | CA191 | CO971` → O31-15 (ch.055-15)

The pattern: each NOG bit is `CA2O1 | CA181/CA191 | CO9x1`. When CO9x1 is active,
it forces the corresponding NO register bit high via the NAOR gate.

Bits 0 and 7 of each byte sit one card over from the rest: `NOG0A`/`NOG7A` on
**O32**, `NOG8A`/`NOGFA` on **O31**, with the middle six on P32/P31. Sixteen
distinct pins for sixteen bits, no sharing — which is what the machine requires,
since ED|EC and EF|EE build the same change-register address with `CO90` on and
then off (241+2N, then 240+2N) while `CO94` stays on throughout. Confirmed on
the card-layout Atlas: `atlas/row_O_pinout_verified.csv` slot 32 carries `NOG0A`
at pin 03 and `NOG7A` at pin 15, and `row_P_pinout_verified.csv` slot 32 carries
`NOG6A`/`NOG5A`/`NOG4A`/`NOG3A`/`NOG2A`/`NOG1A` on pins 01–06; rows O/P slot 31
mirror it for the high byte.

The same two cards carry the gating signals: `CA181` on **O32-13** and `CA191` on
**O31-13**, with `CA2O1` on pin 14 of both — so `CA181` is the low-byte enable
and `CA191` the high-byte one, one per chapter, as the equations above have it.

## EA/EB state signal sequences

### EA state (fo.34) — link write byte 1

| Timing Point | Signal | Hardware Source | Purpose |
|--------------|--------|-----------------|---------|
| TO10 | CO18 | ch.209 gate 1 | Change-register address high bit |
| TO10 | CO97 | ch.205 gate 5 | Address forcing bit 7 |
| TO10 | CO96 | ch.212 gate 4 | Address forcing bit 6 |
| TO10 | CO95 | ch.206 gate 6 | Address forcing bit 5 |
| TO10 | CO94 | ch.213 gate 4 | Address forcing bit 4 |
| TO10 | CO93 | ch.213 gate 2 | Address forcing bit 3 |
| TO10 | CO92 | ch.213 gate 5 | Address forcing bit 2 |
| TO10 | CO91 | ch.209 gate 4 | Address forcing bit 1 |
| TO10 | CO90 | ch.239 — gated `DI11A0` here, **not** `DI65A0` (see §15) | Address forcing bit 0 (odd byte) |
| TO25 | CO31 | ch.090 gate 3 | RO → MEM write |
| TO30 | CI11 | ch.186 gate 1 | V1 → NO (datum) |
| TO50 | CI33 | ch.185 → ch.193 | NO21 → RO |
| TI06 | CU00 | — | Transition to EB |

### EB state (fo.35) — link write byte 2

| Timing Point | Signal | Hardware Source | Purpose |
|--------------|--------|-----------------|---------|
| TO10 | CO12 | ch.129 (RCO12) | V2 → NO (address) |
| TO10 | CO97-CO90 | ch.205-213 | Address forcing (continued) |
| TO25 | CO31 | ch.090 gate 3 | RO → MEM write |
| TO30-TO40 | CI11 | ch.186 | Data transfer |
| TI06 | CU07 | ch.221 gate 3 | Exit state |

## SR/SL path to register 07

SR/SL (cp07 fo.152-155) share the EA/EB exit with JRT:
- fo.155 exit: EA path when `{(dRO = 0i) + (L1_2,1 = 1i)}` (found or exhausted)
- EA/EB write the found address into register 07 via `{SR+SL+JRT}` forcing
- CO18 is gated by `ea_co18 = !is_la(ge)` (SR/SL get it, LA does not)

SR/SL decode signals:
- DI08A (ch.225 gate 7) — alpha band decode component
- DI09A (ch.225 gate 11) — status decode for 0xDx function
- DE02A (ch.248 gate 11) — function+status AND
- DE06A (ch.248 gate 8) — LA in beta band
- DE11A (ch.248 gate 4) — console/control codes

## Addresser signals (LA7x)

The LA7x signals generate the addresser bits for register 07 (modifier field
0111 binary). These are the RETE D flip-flop outputs:

| Signal | Chapter | Pin | PDF Page |
|--------|---------|-----|----------|
| LA701  | 412 | 4 | 417 |
| LA702  | 380 | 6 | 397 |
| LA70A  | 412 | 4 | 417 |
| LA70B  | 380 | 6 | 397 |
| LA711  | 412 | 29 | 417 |
| LA712  | 381 | 6 | 398 |
| LA71A  | 412 | 35 | 417 |
| LA71B  | 381 | 6 | 398 |
| LA721  | 412 | 54 | 417 |
| LA722  | 382 | 6 | 399 |
| LA72A  | 412 | 60 | 417 |
| LA72B  | 382 | 6 | 399 |
| LA731  | 412 | 24 | 417 |
| LA732  | 383 | 6 | 403 |
| LA73A  | 412 | 30 | 417 |
| LA73B  | 383 | 6 | 403 |
| LA741  | 413 | 4 | — |
| LA742  | 384 | 6 | 400 |
| LA751  | 412 | 29 | 417 |
| LA752  | 385 | 6 | 401 |
| LA761  | 413 | 6 | — |
| LA762  | 386 | 6 | 402 |
| LA771  | 413 | 6 | — |
| LA772  | 387 | 6 | 404 |

**Chapter 380 (cp06 p397)** — MEMORY READING AMPLIFIERS:
- RETE D flip-flops (VO0-VO9) with LAxx1/LAxxA inputs and LAxx2/LAxxB outputs
  - VO4 gate 6: LA731/LA73A → LA732/LA73B (register 4 bit)
  - VO7 gate 18: LA131/LA13A → LA132/LA13B (register 1 bit)
- AMPL A circuits (LEBx1) amplify signals to R27 outputs:
  - LEB71 → R27-02 (register 4 bit)
  - LEB11 → R27-13 (register 1 bit)

**Chapter 412 (cp06 p417)** — 1ST STACK READING AND INHIBITION CONNECTORS:
- COME D: LA711/LA71A from (381-11)/(381-6), LA701/LA70A from (380-11)/(380-6)
- COME F: LA731/LA73A from (383-6)/(383-1), LA721/LA72A from (382-6)/(382-1)
- All connectors to MEM 470 (ch.510 position J1, ch.511 position J2)

## Memory address bit 7 (V007)

| Signal | Chapter | Pin | PDF Page |
|--------|---------|-----|----------|
| V0071  | 070 | 17 | 147 |
| V007A  | 070 | 13 | 147 |
| V007B  | 070 | 18 | 147 |
| V007D  | 068 | 24 | 145 |

V007 is the high bit of the memory address (addresses 128-255 have V007 set).
For register 07 at 254/255, V007 is always active.

## Operating simultaneity (ch.129)

**Chapter 129 (cp06 p202)** — OPERATING SIMULTANEITY LOGIC:
- Gate 4 (NAND 2): `RCO11 = !RA11A · !RA11M` → feeds RCO1A
- Gate 6 (NAND 3): `RCO1A = !RA12A · !RAS12` → feeds RCO12
- Gate 7 (NAND 1): `RCO12 = !RCO1A` → H13-06 (feeds ch.131-9, ch.009-2)
- RCO12 = V2 → NO data transfer enable (used in EB state for address write)

## Complete signal flow diagram

```
JRT (0x41)
    │
    ├── Function decode: FOO2F + FOO7F → DO01A (ch.229 gate 8)
    │       DO01A + C27-12 → DO011 (ch.236 gate 6)
    │
    ├── Status decode: SAO65 + SAO26 → DI06A (ch.225 gate 6)
    │       DI06A → DI062 (ch.235 gate 6)
    │
    ├── JRT decode: DO011 + DI062 → DE00A (ch.248 gate 9)
    │       DE00A → DE001 (ch.199 gate 6)
    │
    ├── Partial command: DE00A + DE11A + DE13A + DE06A → CM011 (ch.252 gate 7)
    │
    ├── State set: CM04A + CM05A + ... → CU031 (ch.219 gate 3) → S003 (EA)
    │       CMO1A + CMO3A + ... → CU071 (ch.221 gate 3) → S007 (exit)
    │
    ├── Address forcing:
    │       CO181 (ch.209 gate 1) → H31-14 → ch.036-12
    │       CO901 (ch.239 DI65A) → ch.052-2, ch.053-2
    │       CO911 (ch.209 gate 4) → ch.052-6, ch.053-6
    │       CO921 (ch.213 gate 5) → ch.052-10, ch.053-10
    │       CO931 (ch.213 gate 2) → ch.052-14, ch.053-14
    │       CO941 (ch.213 gate 4) → ch.052-4, ch.053-4
    │       CO951 (ch.206 gate 6) → ch.052-8, ch.053-8
    │       CO961 (ch.212 gate 4) → ch.052-12, ch.053-12
    │       CO971 (ch.205 gate 5) → ch.052-16, ch.053-16
    │
    ├── Timed commands:
    │       NOFA1 + CI2O1 + T0302 → CT2O1 → CA2O1 (ch.036 gate 6)
    │       CA2O1 + CA181 + CO9x1 → NOGx (ch.052/053 NAOR gates)
    │
    ├── Data path:
    │       CI111 (ch.186 gate 1) → V1 → NO (datum in V1)
    │       CI331 (ch.185 gate 7) → CI33A → CI332 (ch.193 gates 7,2) → NO21 → RO
    │       UCO31 (ch.090 gate 3) → RO → MEM write
    │
    └── Memory: MEM[0xFE] ← RO (byte 1, EA state), MEM[0xFF] ← RO (byte 2, EB state)
```

## gemu implementation

gemu models the link register write in `msl-states.c`:
- `state_ea[]` chart: CO18 forces address, CO90-CO97 force 0xFF, CO31 writes RO→MEM
- `state_eb[]` chart: V2 → NO, CO31 writes second byte
- `ea_co18() = !is_la(ge)` — gates CO18 for JRT/SR/SL only
- `ea_writes_back() = is_jrt_or_la(ge) || L207_output_writeback(ge)`
- `is_jrt_or_la() = is_jrt(ge) || is_la(ge)` — JRT (0x41) or LA (0x68)
- `is_jrt() = ge->rFO == JRT_OPCODE`
- `CO90() = SET_BIT(ge->kNO.forcings, 0)` — forces NO bit 0
- `DI65A0() = !DI65A(ge)` — inverted state decode for odd byte select

## ge120.xyz markers

Relevant markers for register 07 trace:
- cp06-lc-p113 (ch.036) — Timed commands CA11/12/15/17/20, CA2O1
- cp06-lc-p129 (ch.052) — Forcing in NO bit 00-07
- cp06-lc-p130 (ch.053) — Forcing in NO bit 08-15
- cp06-lc-p145 (ch.068) — V007D address bit
- cp06-lc-p147 (ch.070) — V0071/V007A/V007B
- cp06-lc-p166 (ch.090) — Arithmetical unit, UCO31
- cp06-lc-p182 (ch.106) — S0 register (state set)
- cp06-lc-p202 (ch.129) — Operating simultaneity, RCO12
- cp06-lc-p245 (ch.185) — CI331 generation
- cp06-lc-p246 (ch.186) — CI111 generation
- cp06-lc-p253 (ch.193) — CI33A/CI332
- cp06-lc-p259 (ch.199) — DE001 generation
- cp06-lc-p265 (ch.205) — CO971
- cp06-lc-p266 (ch.206) — CO951
- cp06-lc-p269 (ch.209) — CO181, CO911
- cp06-lc-p272 (ch.212) — CO961
- cp06-lc-p273 (ch.213) — CO921, CO931, CO941
- cp06-lc-p279 (ch.219) — CU031
- cp06-lc-p281 (ch.221) — CU071
- cp06-lc-p285 (ch.225) — DI06A
- cp06-lc-p289 (ch.229) — DO01A
- cp06-lc-p295 (ch.235) — DI062
- cp06-lc-p296 (ch.236) — DO011
- cp06-lc-p298 (ch.238) — Status decoding
- cp06-lc-p299 (ch.240) — Status decoding (DI65A area)
- cp06-lc-p307 (ch.248) — DE00A generation
- cp06-lc-p311 (ch.252) — CM011 partial command
- cp06-lc-p397 (ch.380) — LA addresser flip-flops
- cp06-lc-p417 (ch.412) — LA signal connectors
