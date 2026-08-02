# LU08 / PELEA trace — character strobe and peripheral error

LU08N is the character-ready strobe. PELEA is the peripheral error signal derived from LUPO+LU08.

## LU08 path (ch.150, ch.125)

| Stage | Signal | Gate | Role |
|-------|--------|------|------|
| COCA input | `LU08B` | — | Strobe from reader (connector 2) |
| Buffer | `LU082` | RIIN | Buffered strobe, active-low |
| PELEA decode | `LUPO6 + LU082` | NAND (ch.125 gate 15) | `PELEA = !(LUPO · LU08)` |
| Error path | `PELEA → REL3B` | (ch.127 gate 15) | Routes to (155-13), (124-6) |
| Status decode | `RG001` (LUPO), `RG011` (LURE) | RO decode | LU08 participates in peripheral status examination |

**PELEA meaning:** `PELEA = !(LUPO · LU08)` — fires when the controller is NOT available (LUPO=0) during a strobe, or when the strobe arrives without a ready controller. This is the "out-of-service on channel-2" condition.

**In gemu:** `LU08` is the primary handshake signal (`integrated_reader.lu08`). PELEA is not yet explicitly modeled — the ready/busy invariant (`LUPOR` always set when presenting) makes PELEA inert in the current model.

## Tags on ge120.xyz

`LU08B`, `LU082`, `LUPO6`, `PELEA`, `REL3B`, `peripheral-error`
