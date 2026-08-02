# LU00-LU07 data lines trace — reader data through the Capitolo

Reader data lines `LU00N`-`LU07N` carry the transcoded character bits (8) from the card reader to the memory register RO via the NE network.

**Path:** COCA connector → ch.150/151/152 (channel buffers) → PIB bus → NE input → RO → memory

## Signal flow (per bit)

### LU00-LU02 (ch.150, cp06 p223) — channel 1 input, bits 00-02

| Stage | Signal | Gate | Role |
|-------|--------|------|------|
| COCA input | `LU00B`, `LU01B`, `LU02B` | — | Data from reader (connector 2) |
| Buffer | `LU002`, `LU012`, `LU022` | RIIN gates | Buffered copy, active-low |
| NE input decode | `FU00A→FU001`, `FU01A→FU011`, `FU02A→FU021` | NAND | NE register write enable |
| NE latch | `AIFE3→AIFE4→NE006`, `BIFE3→BIFE4→NE016` | NAND chain | Latched data in NE |
| PIB enable | `PIB21` (ch.150-24) | NAND | `= !(PB12A·PB22A·PB32A)` — enables NE input from conn 2 |
| Data bus | `PI11A`, `PI12A`, `PIB11`, `PIB31` | fan-out | Carries data to NE register |
| Auxiliary | `PITE1→PITEA`, `MIFE3→MIFE4→MIFE5` | — | Parity/error decode |

**PIB21 equation:** `PIB21 = !(PB12A · PB22A · PB32A)` where:
- `PB12A = !(RESI1 · PC121)` — channel-1 + connector-2 selected
- `PB22A = !(RET21 · PC221)` — channel-2 + connector-2 selected
- `PB32A = !(RES31 · PC321)` — channel-3 + connector-2 selected

### LU03-LU05 (ch.151, cp06 p222) — channel 1 input, bits 03-05

| Stage | Signal | Gate | Role |
|-------|--------|------|------|
| COCA input | `LU03B`, `LU04B`, `LU05B` | — | Data from reader |
| Buffer | `LU032`, `LU042`, `LU052` | RIIN gates | Buffered copy |
| NE input | `FU03A→FU031`, `FU04A→FU041`, `FU05A→FU051` | NAND | NE register write enable |
| NE latch | `DIFE3→DIFE4→NE036`, `EIFE3→EIFE4→NE046` | NAND chain | Latched data in NE |
| Data bus | `PI31A`, `PI32A`, `PIB11`, `PIB21`, `PIB31` | fan-out | Carries data to NE |

### LU06-LU07 (ch.152, cp06 p225) — channel 1 input, bits 06-07

| Stage | Signal | Gate | Role |
|-------|--------|------|------|
| COCA input | `LU06B`, `LU07B` | — | Data from reader |
| Buffer | `LU062`, `LU072` | RIIN gates | Buffered copy |
| NE input | `FU06A→FU061`, `FU07A→FU071` | NAND | NE register write enable |
| NE latch | `LIFE3→LIFE4→PI71A`, `GIFE3→GIFE4→PI61A` | NAND chain | Latched data |
| NE register | `NE066`, `NE076` | — | NE bits 06/07 |
| RES fan-out | `RES16→RB14A`, `RES36→RB34A` | — | RES signals → PIB41 |
| Data bus | `PIB11`, `PIB21`, `PIB31`, `PIB41` | fan-out | Carries data to NE |

## Destination: NE register → RO → memory

The NE register receives data when `PIB21` is asserted (connector 2 enabled) and the channel-1 or channel-2 cycle is assigned (`RESI`/`RES2`). The `CI34` command (`NE→RO`) moves NE into RO, then `CO31` (`RO→Mem`) stores to memory.

**Tags on ge120.xyz:** `LU00B-LU07B`, `LU002-LU072`, `FU00A-FU07A`, `NE006-NE076`, `PIB11-PIB41`, `reader-data`, `channel1-input`
