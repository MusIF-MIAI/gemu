# gdis — GE-120 / GE-130 disassembler & "depuncher"

`gdis` reverses the two transformations that stand between a punched deck and
readable code:

1. **Depunch** — read a `.cap` punch-card capture, column-binary ("by-pass")
   decode each program card, and reassemble the self-describing data-card
   records into a flat memory image. This is the inverse of punching a deck.
2. **Disassemble** — linear-sweep the image (or a raw `.bin`) into readable
   GE-120 assembly. The output re-assembles with [`gasm`](../assembler/).

The depunch path **reuses the emulator's own** `../cap.c` (the `.cap` parser)
and `../transcode.c` (`transcode_column`, the COLBIN decoder), so it is
bit-identical to how `gemu` reads a deck. The card format and the data-card
layout it relies on are documented in [`../docs/punchcards.md`](../docs/punchcards.md)
— that findings document is the source of the whole depunch recipe.

## Build

```sh
make            # produces ./gdis  (compiles gdis.c + ../cap.c + ../transcode.c)
make clean
```

## Usage

```sh
gdis [options] input
```

`input` is a `.cap` deck (default) or a raw machine-code image (`--bin`).

| Option | Meaning |
|---|---|
| `-o FILE` | write output to FILE (default stdout) |
| `--bin` | treat input as a raw image instead of a `.cap` deck |
| `--org 0xNNNN` | origin for `--bin` (default `0x0000`) |
| `--mode M` | `.cap` decode mode: `colbin` (default) · `binary` · `normal` · `hex` |
| `--prefix "HH HH …"` | force the 8-byte data-card prefix (default: auto-detect per deck) |
| `--loose` | accept any self-consistent card record (ignore the prefix gate) |
| `--cards` | dump each card's decoded bytes + record parse, then exit |
| `--hex` | dump the reconstructed image as hex, then exit |
| `-v` | verbose, card-by-card report on stderr |

## How the depunch works (per `docs/punchcards.md`)

1. `cap_load()` parses the `.cap` into per-card arrays of 80 × 13-bit columns.
   (Each deck holds two dumps — the hex dump and the ASCII hole-art; only the
   hex cards carry column values, so the art cards are naturally skipped.)
2. Each program card is **COLBIN**-decoded one column → one byte
   (`B2R = {9,8,7,6,3,2,1,0}`; row 0 is the MSB).
3. Each decoded card is a **self-describing record**:

   ```
    col:  0 1 2 3 4 5 6 7 | 8  | 9 10 | 11 ............... | 79
          └── ID prefix ──┘  LL  └addr┘  └── LL+1 payload ──┘  seq
   ```

   `LL` = payload length − 1, `addr` = big-endian load address. Payload bytes go
   to `image[addr .. addr+LL]`. Cards are **order-independent** — each carries
   its own address — so a shuffled deck still reconstructs correctly.

**Prefix auto-detection.** The 8-byte ID prefix in cols 0–7 is constant across a
deck's data cards but is **per-deck** (`punchcards.md` §5) — `funktionalcpu` uses
`00 04 40 00 20 40 40 42`, `control-program-cr` uses `00 04 80 04 02 40 40 04`,
etc. So by default `gdis` scans the deck, picks the **dominant** prefix, reports
it, and loads only cards bearing it (loader/title/control cards are skipped).
Override with `--prefix "HH HH …"`, or bypass the gate entirely with `--loose`.
If the detected prefix doesn't start `00 04`, `gdis` warns that the deck likely
uses a different card framing.

## Which decks work

| Deck family | Result |
|---|---|
| `funktionalcpu` | **verified** — the reverse-engineered oracle behind `punchcards.md`; depunches to a clean `0x0100…` CPU image. |
| `control-program-cr`, `auto-kontr`, `printermechanicaltest`, `reading-test`, `semi-manuale` | same `00 04 ..` record framing, auto-detected and loaded; disassembly is plausible but **not oracle-verified**. |
| `ls600-*`, `sat-ls600` | **peripheral test decks** — share the framing but their payloads are test patterns, not `0x0100` CPU code (records overlap a small region). |
| `isolationcpu0x`, `isolat-dsu-erganz` | **different card framing** (no `00 04` prefix; code punched more directly). Not yet cracked — `gdis` warns and the result is poor. Inspect with `--cards`. |

Only `funktionalcpu` is ground-truth-verified; for the rest, treat the depunched
image and disassembly as a starting point for reverse engineering, and
cross-check with `--cards` / `--hex`.

## Examples

Depunch + disassemble the bundled functional-test deck:

```sh
$ gdis -o funktionalcpu.s ../../DUMP1/funktionalcpu.cap
gdis: …/funktionalcpu.cap: 228 cards, 106 records loaded, 122 skipped;
      image 0x0100..0x1C53 (contiguous)
```

(106 payload cards, reconstructing a single gap-free block `0x0100…0x1C53` —
matching the layout documented in `docs/punchcards.md` §5.)

Inspect the raw cards / image without disassembling:

```sh
gdis --cards ../../DUMP1/funktionalcpu.cap      # per-card LL/addr/bytes
gdis --hex   ../../DUMP1/funktionalcpu.cap      # the reconstructed image, hex
```

Disassemble a raw image straight from `gasm`:

```sh
gasm -o prog.bin prog.s
gdis --bin --org 0x0000 prog.bin
```

## Round-trip

`gdis` is the exact inverse of `gasm` for every instruction class:
`gasm → gdis --bin → gasm` reproduces byte-identical output (verified across P,
branch, sense, register, immediate, peripheral, and both SS forms — including
`SR`/`SL`).

**Scope.** That guarantee holds for **pure-code** images. Like any linear-sweep
disassembler, `gdis` cannot tell code from data: in a mixed image (a real deck
is roughly half data tables), data bytes get decoded as whatever opcode they
happen to be — e.g. a run of `0xFF` fill decodes as a run of `SB` instructions
whose "operands" are nonsense addresses. Such lines disassemble fine for reading
but will **not** re-assemble (out-of-range addresses, etc.). For reverse
engineering, use `--hex`/`--cards` to see the raw image, and treat the assembly
as a readable view that is authoritative only over the genuine code regions.

## Disassembly conventions

- Format is chosen by the top two opcode bits (P = 2 B, PM = 4 B, SS = 6 B), the
  same rule the emulator decodes by.
- Address fields are printed as absolute `0x%04X` (which re-encodes identically),
  and branch targets that land inside the image get `L_xxxx:` labels.
- Conditional branches print the raw mask plus the matching alias as a comment,
  e.g. `JC 0x20, L_0140   ; JE/JEQ/JZ`.
- Bytes that are not a recognised opcode (data regions, or a misaligned sweep)
  fall back to `DB 0xNN` and the sweep resyncs on the next byte.
- Gaps in a sparse image emit a fresh `ORG`. `SR`/`SL` are shown as SS ops with
  the caveat that their length encoding is unconfirmed (see ISA.md §6.10).
```
