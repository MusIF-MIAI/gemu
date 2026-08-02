# Status signals trace — LUPO/LURE/LUSE/LENO → RO decode → PCOV6

The reader status signals (LUPO, LURE, LUSE, LENO, FIDE) flow through external condition examination → RO register decode → external operations logic.

## ch.124 (cp06 p197) — EXTERNAL CONDITIONS EXAMINATION

| Signal | Origin | Buffer | Decode | PC-line |
|--------|--------|--------|--------|---------|
| `LUPOB` | reader ready | `LUPO2` | `RG001` (LUPO) | `PC1AA`, `PC3AA`, `PC5AA`, `PC4AA`, `PC6AA` |
| `LUREB` | reader error | `LURE2` | `RG011` (LURE) | `PC2BA`, `PC14A` |
| `FUPOA` | conn-1 available | `FUPO1` | — | `PCOCA` |
| Status | `SEGED`, `MATED`, `CAPEB/C` | — | — | `PC1A1`, `PC2A1`, `PC1B1`, `PC2B1` |
| Error | `PE00C/D/3/4`, `PITE1/C`, `PC12F/4/3A/4` | — | — | — |
| Output | — | — | — | `PCOCA`, `PCODA`, `PCOBA`, `PCOGA` → ch.128 |

**Intermediate signals:** `LUPO2`, `LURE2`, `FUPO1`, `PC4AA`, `PC6AA`, `PC1AA`, `PC3AA`, `PC5AA`, `PC2BA`, `PC14A`, `PC1A1`, `PC2A1`, `PC1B1`, `PC2B1`, `SEGED`, `SECEC`, `SEGE3/4`, `MATED`, `MATEC`, `MATE3/4`, `CAPEC`, `CAPE3/4`, `PE00C/D/3/4`, `PITE1/C`, `PC12F/4/3A/4`

## ch.125 (cp06 p198) — EXTERNAL CONDITIONS EXAMINATION

| Signal | Origin | Buffer | Decode | PC-line |
|--------|--------|--------|--------|---------|
| `FIDEB` | end-of-file (conn 2) | `FIDE2` | `RG021` (FIDE) | `PC1CA`, `PC3CA`, `PC5CA` |
| `LUPO6+LU082` | reader status | — | NAND gate 15 | `PELEA = !(LUPO·LU08)` |
| `FISED` | P.U. condition (conn 4) | `FISEC`, `FISE3` | — | `PC4GA`, `PC6GA`, `PC2CA`, `PC14A` |
| Output | — | — | — | `PC1C1`, `PC2C1`, `PC1D1`, `PC2D1`, `PCOEA`, `PCOFA` → ch.128 |

## ch.126 (cp06 p199) — EXTERNAL CONDITIONS EXAMINATION

| Signal | Origin | Buffer | Decode | PC-line |
|--------|--------|--------|--------|---------|
| `LUSEB` | out-of-service | `LUSE2` | `RG061` (LUSE) | `PC14B`, `PC13B`, `PC12C`, `PC11C` |
| `FUSEA` | conn-1 out-of-service | `FUSE1` | — | `PC5EA`, `PC4EA` |
| Status | `TESED`, `TESEC`, `TESE3/4` | — | — | `PC1E1`, `PC2E1`, `PC1F1`, `PC2F1` |
| Output | — | — | — | `PCOEA` → ch.128 |

**Intermediate signals:** `LUSE2`, `FUSE1`, `TESED`, `TESEC`, `TESE3/4`, `PC14B`, `PC13B`, `PC12C`, `PC11C`, `PC5EA`, `PC4EA`, `PC1E1`, `PC2E1`, `PC1F1`, `PC2F1`

## ch.127 (cp06 p200) — EXTERNAL CONDITIONS EXAMINATION

| Signal | Origin | Buffer | Decode | PC-line |
|--------|--------|--------|--------|---------|
| `LENOB` | not-operable | `LENO2` | `RG131` (LENO) | `PC14B`, `PC13B`, `PC12C`, `PC11C` |
| `FUZZA` | conn-1 status | `FUZ21` | — | `PC5GA`, `PC4GA` |
| Error | `ERCAD`, `ERCAC`, `ERCA3/4`, `ERCEA→ERCE1` | — | — | `PC14A`, `PC13A`, `PC2GA` |
| Output | — | — | — | `PC1G1`, `PC2G1`, `PC1L1`, `PC2L1`, `PCOGA`, `PCOLA` → ch.128 |

**Intermediate signals:** `LENO2`, `FUZZA`, `FUZ21`, `ERCAD`, `ERCAC`, `ERCA3/4`, `ERCEA`, `ERCE1`, `PC14B`, `PC13B`, `PC12C`, `PC11C`, `PC5GA`, `PC4GA`, `PC1G1`, `PC2G1`, `PC1L1`, `PC2L1`

## ch.128 (cp06 p201) — DECODING FOR EXTERNAL OPERATIONS

This chapter combines all the PCOx lines from ch.124-127 into the final external-ops output.

| Gate | Inputs | Output | Destination |
|------|--------|--------|-------------|
| Gate 11 (NAND) | `PCOCA + PCOAA + PCODA + PCOFA + PCOGA + PCOLA + PCOEA + PCOBA` | `PCOV6` | `(311-17), (311-21), (273-6), (217-2)` |
| Gates 1-4 | Various PCOx | `PCO1A`, `PCO16`, `PCO3A`, `PCO36` | decoded external operations |

**Intermediate signals:** `PCOV6`, `PCO1A/16`, `PCO3A/36`, `RL1PA/1`, `RL1UA/1`, `RL3DA/1`

**PCOV6 destinations:** interrupt logic (`311-17`, `311-21`), external operation counter (`273-6`), general reset (`217-2`).

## Full chain: LUPO example

```
LUPOB (COCA) → ch.124: LUPO2 (RIIN buffer)
  → RG001 decode → PC1AA/PC3AA/PC5AA/PC4AA/PC6AA
  → PCOCA → ch.128 gate 11 (NAND of 8 PCOx inputs) → PCOV6
  → (311-17, 311-21, 273-6, 217-2) — interrupt/status logic
```

## Tags on ge120.xyz

`LUPOB`, `LUPO2`, `LUREB`, `LURE2`, `LUSEB`, `LUSE2`, `LENOB`, `LENO2`, `FIDEB`, `FIDE2`, `PELEA`, `REL3B`, `PC1AA-PC6AA`, `PC1A1-PC2L1`, `PCOCA`, `PCODA`, `PCOFA`, `PCOGA`, `PCOLA`, `PCOEA`, `PCOBA`, `PCOV6`, `PCO1A`, `PCO16`, `PCO3A`, `PCO36`, `external-condition`, `interrupt`, `external-ops`, `reader-status`, `peripheral-error`
