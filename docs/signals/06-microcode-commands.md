## 6. Microcode command dictionary (CO / CI / CE / CU / CA), from the index

The per-clock commands the MSL states issue (`msl-commands.c`), transcribed from
the official command index (name · chapter · box · EN). gemu uses the base
mnemonic (`CO30`, `CI34`, `CE16`, …) for the indexed `COxx1`/`CIxx1`/`CExx1`.

### 6.1 CO — output-clock (TOxx) commands
| Cmd | Ch | Meaning (EN) | Backplane (card·pin) |
|-----|----|--------------|---|
| CO00 | 205 | NI → PO | — |
| CO01 | 206 | NI → V1 | — |
| CO02 | 207 | NI → V2 | — |
| CO03 | 217 | NI → V3 | — |
| CO04 | 190 | **NI → V4** (advance the ch-2/V4 addresser) | — |
| CO06 | 206 | NI(00÷07) → L2 | — |
| CO10 | 208 | PO → NO | — |
| CO11 | 206 | **V1 → NO** (ch-1 mem address) | — |
| CO12 | 205 | V2 → NO | — |
| CO13 | 205 | V3 → NO | — |
| CO14 | 208 | **V4 → NO** (ch-2 mem address) | — |
| CO16 | 207 | L2 → NO(00÷07) | — |
| CO18 | 209 | enable forcing in NO(00÷07) | — |
| CO30 | 180 | **memory READ → RO** (commits TO50) | — |
| CO31 | 207 | **memory WRITE** (RO → mem; commits TO65) | — |
| CO35 | 210 | internal-error reset | — |
| CO40 | 193 | counts minus | — |
| CO41 | 200 | **counts from bit 00** (NI = +1) | — |
| CO48/CO49 | 211/212 | set / reset URPE & URPU | — |
| CO90–CO97 | 205–213 | force "1"/value into NO halves | — |

### 6.2 CI — input-clock (TIxx) commands
| Cmd | Ch | Meaning (EN) | Backplane (card·pin) |
|-----|----|--------------|---|
| CI00–CI07 | 180–207 | NI → PO/V1/V2/V3/V4/L1/L2(00÷07)/L3, NI(00÷07)→FO | — |
| CI09 | 205 | NI(08÷15) → RI | — |
| CI10–CI17 | 185–223 | PO/V1/V2/L1/L2/L3 → NO | — |
| CI19/CI20 | 185/190 | enable NO(08÷15) forcing / console forcing | — |
| CI21 | 191 | **RI → NO(08÷15)** | — |
| CI32 | 192 | NO(08÷15) → RO | — |
| CI33 | 185 | NO(00÷07) → RO | — |
| **CI34** | 193 | **NE → RO** (the channel input read — reader byte) | — |
| CI38/CI39 | 211/183 | enable set AVER&ALTO / reset AVER | — |
| CI40–CI44 | 193–215 | counts minus / from bit 00 / from bit 04 / block carry 03 / block carry 07 | — |
| CI45–CI47 | 196/190 | logic ops / decimal-or-AND / subtract-or-XOR | — |
| CI50/CI51 | 215/181 | operate only ALU bits 00÷03 / 04÷07 | — |
| CI60–CI67 | 181–199 | RO(04÷07 or 00÷03) → NI quartets | — |
| CI68/CI69 | 200/181 | U.A. → NI(08÷15)/(00÷07) | — |
| CI70–CI76 | 201–202 | **set FI00..FI06** | — |
| CI77/CI78 | 222/204 | set / reset ADIR | — |
| CI80–CI86 | 201–204 | reset FI00..FI06 | — |
| CI87/CI88/CI89 | 224/203/182 | set ALAM / reset ALAM / set ALTO | — |

### 6.3 CE — external / channel commands (the reader-relevant ones)
| Cmd | Ch | Meaning (EN) | gemu | Backplane (card·pin) |
|-----|----|--------------|------|---|
| CE00 | 214 | RO → RA | ✅ | CE001: H17·05, I13·07, N20·07 |
| CE01 | 214 | **RO → RE** (load the connector-name / command register) | ✅ | CE011: H17·03, M20·14 |
| CE02 | 215 | enable external-channel selection | ◑ | CE021: I11·07, I12·04, I13·05, L10·16 |
| CE03 | 216 | I/O logic reset | ◑ | CE031: E13·02, G12·09, H15·06, L10·11, L11·06 |
| CE05 | 214 | enable set of external error | ◑ | CE051: H17·01, I12·07 |
| CE06 | 215 | enable set of **channel-1 error** | ◑ | CE061: C29·05, G18·07, L10·10 |
| CE07 | 215 | **I/O logic set** (gemu also sets RASI here) | ✅ | CE07I: G14·07; CE071: G18·11, I13·13 |
| CE08 | 214 | **VICU issue** | ◑ Implemented as `CE08()` setting `RAVI` on `TO19 && RETO` and then `RACI` on `RB111`; wider VICU handling is still incomplete. | CE081: H17·06 |
| **CE09** | 214 | **Sends TU10 of channel 1** (gemu `reader_send_tu10`; the feed/advance hook) | ◑ | CE091: H17·04, I16·07 |
| **CE10** | 214 | **Sends TU20 of channel 1** (gemu `reader_send_tu00`; the read-strobe hook) | ◑ | CE101: H17·02, I11·09 |
| CE11 | 215 | Sends TU30 of channel 1 | ☐ | CE111: G18·14, I16·04, L11·05 |
| CE12/CE13/CE14 | 215/210 | Sends TU10/TU20/TU30 of channel 3 | ☐ | CE121: G18·13, I16·05; CE131: G15·09, H18·06; CE141: F22·16, H15·05, I16·03 |
| CE15 | 215 | issue FIRU | ◑ | CE151: H9·05, H18·12, L10·07 |
| CE16 | 217 | **Load printer buffer** (channel-2 OUTPUT emit) | ✅ | CE161: E12·14 |
| CE17 | 217 | stop printing | ☐ | CE171: E12·07, M17·06 |
| CE18 | 214 | enable cycle-request reset | ✅ | CE181: G13·05, H17·15 |
| CE19 | 215 | reset channel-3 selection | ☐ | CE191: H10·05, H18·11 |

> **Plan correction (CAN1):** the COCA `TU00N`/`TU03N` map onto these channel-1
> timing strobes — gemu emits **TU10 at `CE09`** and **TU20 at `CE10`** (the gemu
> function names `reader_send_tu10`/`reader_send_tu00` are off-by-name vs the
> official TU10/TU20). The card-feed/advance is the `CE09` (TU10) hook; the
> read-strobe is the `CE10` (TU20) hook. Exact TU00N↔TU10 / TU03N↔TU30 pin
> correspondence needs the controller schematic.

### 6.4 CU — status-register (SO/S1) commands
| Cmd | Ch | Meaning (EN) | Backplane (card·pin) |
|-----|----|--------------|---|
| CU00–CU07 | 218–222 | **set SO register** (build the future status) | CU00A: B5·14, C5·09 |
| CU10–CU17 | 220–224 | **reset SO register** | CU10A: B3·14, C5·12 |
| CU20 | 192 | **load future status into SO and S1** | CU20A: B5·02, H38·03 |

### 6.5 CA — timed commands; CAGU/CAPE
| Cmd | Ch | Meaning (EN) | Backplane (card·pin) |
|-----|----|--------------|---|
| CA10–CA21 | 036–039 | timed command pairs (CO/CI 10..21) | CA101: L33·16, M31·11, M32·11, M33·11, M34·11 |
| CA40–CA44 | 038 | timed command pairs (CO/CI 40..44) | CA401: L34·15, P20·04, P21·04 |
| CAGU7 | 141 | **External-operation general reset** (the REGEN-class clear) | CAGUF: C29·01, G16·15, L10·06, M17·07, N18·09; CAGU7: H15·02, L10·02, M6·11; CAGUC: L2·01, L6·06; CAGUA: L3·01, M3·01, N6·02; CAGUD: L6·07, M2·01; CAGUB: L6·09 |
| CAPEC/CAPED | 007/008 | command-rejection condition from UP connector 3/4 | CAPEC: G7·06, I2·09, M14·10, N7·05; CAPE7: G7·13, G8·07; CAPEB: I8·03; CAPED: L7·06, M14·09, N2·09, P8·05; CAPE8: L7·11 |

### 6.6 Registers added by this index page
- `BO002`…`BO152` (ch.073–076): **BO** register bits; `BOCO1` (116) = permission
  to load BO. `BU001`…`BU151` (ch.096–099): **counting-network output** bits.
  `BASI1` (218) = enables loading the **SI** register. `BIFEC/BIFED/BIFUA/BIFUC/
  BIFUD` (007/008/153) = info bit in/out, connectors 3/4/1.
