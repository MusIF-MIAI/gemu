# GE-120 Unified Binary Format

The single interchange format shared by the toolchain and the (future) real-machine
loader. One small big-endian header carrying **origin** and **entry**, followed by
the flat machine-code image.

## Why it exists

Three tools must agree on one byte layout:

- **gasm** (assembler) — emits it (default output).
- **gdis** (disassembler/depuncher) — emits it (`--image`) and reads it (auto-detected).
- **gemu** (emulator) — loads it (positional `./ge prog.bin`).

It is also the format intended for injection into the real GE-120 through the
card-reader interface (binary mode): a minimal hardware/ROM bootstrap reads the
12-byte header to learn where to place the image and where to jump.

The single definition lives in `binimage.h` / `binimage.c`, compiled into all
three tools — there is no second copy of the layout.

## Layout

All multi-byte fields are **big-endian** (the machine's native order).

```
off  size  field     notes
0    4     magic     'G','E','1','2'  (0x47 0x45 0x31 0x32)
4    1     version   0x01  (BINIMAGE_VERSION)
5    1     flags     reserved, must be 0
6    2     origin    load address          (big-endian)
8    2     entry     initial PO            (big-endian)
10   2     length    image byte count      (big-endian)
12   …     image     flat machine code, `length` bytes
```

- 12-byte header, then exactly `length` image bytes.
- The header is **metadata only** — it is never placed in memory. The loader copies
  the `image` bytes to `mem[origin .. origin+length)` and sets the program counter
  to `entry`.
- `origin + length` must not exceed `0x10000` (64 KiB address space).

### Example — `halt.s` assembled at ORG 0

```
00000000: 4745 3132 0100 0000 0000 0006 0a00 47f0  GE12..........G.
00000010: 0000                                     ..
            └magic──┘ │  │ └org┘ └ent┘ └len┘ └─ image ─┘
                      ver flags
```

`origin=0x0000`, `entry=0x0000`, `length=6`; image = `0a 00 47 f0 00 00`
(`HLT` ; `JU 0x0000`).

## Producing it

```sh
gasm prog.s -o prog.bin            # unified format (default)
gasm --raw prog.s -o prog.bin      # headerless flat image (legacy)
```

- `ENTRY <expr>` directive sets the entry point; default = the load origin
  (lowest address emitted). `--org N` sets that origin.

```sh
gdis --image -o prog.bin deck.cap          # depunch a .cap deck to a loadable image
gdis --image --entry 0x0100 -o p.bin in    # override the entry point
```

## Loading it

```sh
./ge prog.bin                  # load at header origin, enter at header entry
./ge --raw --org 0x0200 blob   # load a headerless flat blob at 0x0200 (entry = origin)
./ge --deck deck.cap           # unrelated: Hollerith card-reader path (unchanged)
```

gemu reads the header (`binimage_read`), loads the image at `origin` via
`ge_load_image` (origin-aware, parity primed), then `ge_enter(entry)` seeds the
program counter and drops the sequencer into the alpha (fetch) phase — the direct
binary path does **not** run the peripheral LOAD bootstrap.

## Round-trip

The format is closed under the toolchain — verified byte-identical:

```
gasm src.s -o a.bin   →   gdis a.bin -o b.asm   →   gasm b.asm -o c.bin
                                                     cmp a.bin c.bin   # identical
```

and for real decks:

```
gdis --image deck.cap -o d.bin   →   ./ge d.bin   # bit-perfect depunch, executes
```

`gdis` auto-detects the GE12 magic on input, so a unified `.bin` is recognised
whether or not `--bin` is given; a non-matching file falls back to raw (`--bin`)
or `.cap` parsing.

## Notes / gotchas

- **Change-register window 0x00F0–0x00FF.** Reset (`ge_clear`) seeds the eight
  16-bit segment-base / change registers there (identity bases `N<<12`). A flat
  image that spans this window will overwrite them with its own bytes (or zeros in
  reconstructed gaps), which can break segment-base addressing. Programs that load
  across low memory must re-establish these bases (or the loader must preserve
  them). See `docs/ISA.md` §4 for the addressing model. This is the open item for
  running the large diagnostic decks through the binary path.
- **bit 15 of an address** is unused/unknown (masked); see `docs/ISA.md` §4.2.
- Version is checked on read; an unknown version is rejected rather than guessed.
