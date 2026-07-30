# gec — a small C compiler for the GE-120 / GE-130

`gec` compiles a useful subset of C to **`gasm` assembly**, and drives `gasm` to
punch a **card deck**. That deck is the whole output: it is what the emulator
runs and what goes in the real machine's reader, the same file either way. It
implements the calling convention and data model in
[`../docs/ABI.md`](../docs/ABI.md).

```
make                       # builds ./gec and ./runrv
./gec prog.c -o prog.cap   # C -> assembly -> card deck, one command
../ge prog.cap             # put it in the hopper and run it
./runrv prog.cap           # same, and print main()'s return value (__rv)

./gec prog.c -c -o prog.s  # compile only, stop at gasm assembly
./gec prog.c -E -o prog.i  # preprocess only, stop at expanded C
```

## Preprocessor

`#define` (object- and function-like), `#undef`, `#include` (`<...>` and
`"..."`), and `#ifdef`/`#ifndef`/`#else`/`#endif` — enough for a guarded header.
Comments are stripped before expansion, and identifiers inside string and
character literals are left alone. There is no `#if` arithmetic and no `#`/`##`.

Includes are searched next to the source file, then in `<gec-binary-dir>/include`.
`#include <stdio.h>` keeps its old meaning of enabling `printf`/`scanf`.

`-E` stops after this stage and writes the expanded C, which is the quickest way
to see what a macro really turned into.

## Peripheral access

`<ge.h>` gives C a device interface without exposing the `PER` instruction or its
order blocks:

```c
#include <ge.h>

char card[80];

int main(void) {
    int rdr = _open_reader();
    _read(rdr, card, 80);
    if (GE_STATUS(rdr) != GE_QR_GT)
            lon();
    _close(rdr);
    return card[0];
}
```

| Primitive | Meaning |
|---|---|
| `_open_reader()`, `_open_reader_bypass()` | yield a device's unit byte; emit no code |
| `_read(dev, buf, n)` / `_write(dev, buf, n)` | transfer `n` bytes to/from a global buffer |
| `_order(dev, z, x)` | issue a command/status order; returns the qualitative result |
| `_close(dev)` | no-op, for symmetry |

Two things the compiler enforces, both because the order block is *data* the
instruction points at:

- **The device must come from an `_open_*()` call.** The `PER` unit name is an
  immediate inside the instruction, so it cannot be a computed value. A local
  initialised from `_open_*()` is folded; reassigning it invalidates that.
- **The length must be a constant**, and it is written in **bytes**. The
  machine's `LL` field is *length−1*, and that subtraction happens in exactly one
  place — `_read(dev, buf, 80)` emits `LL=79`, so it cannot be got wrong by hand.

Only unit values this project has established are exposed. The rest of the device
list needs CPU[1]'s channel-code table, which has not been transcribed.

## Language subset

- Types: `char` (1 byte, unsigned), `short`/`int` (2 bytes, big-endian),
  pointers (2 bytes), 1-D arrays. No `float`.
- Functions, parameters, locals, globals (with constant initialisers), string
  and char literals (translated to the GE-100 internal graphic set).
- Operators: `+ - * / %`, `== != < <= > >=`, `&& || !`, `& | ^`, `<< >>`,
  unary `-`, `&` (address-of), `*` (deref), `[]`, assignment, function calls.
- Statements: `if/else`, `while`, `for`, `return`, blocks, declarations.
- **Recursion** works (link register R7 spilled per the ABI).

The ISA has no binary multiply/divide and no shift, so `* / % << >>` are emitted
as calls to runtime helpers (`__mul`, `__divu`, `__modu`, `__shl`, `__shru`)
generated into a `crt0` preamble together with the stack/return-value setup.

## How it works (see ABI.md for detail)

- **Call** = `JRT 0xF0, fn` (deposits the return address in index register 7);
  **return** = `JU 0x000(7)`. Non-leaf functions spill R7 into their frame.
- Memory-to-memory code generation: every scalar value is computed in a 2-byte
  frame temporary; the change registers are used only to form addresses
  (R5 = frame pointer, R6 = stack pointer, R7 = link, R0 = globals base).

## Tests

`./test.sh` (also run by `make check` in the parent) compiles each program in
`examples/` to a deck, feeds the deck through the card reader, and checks
`main()`'s return value:

| example | checks | `__rv` |
|---|---|---|
| `sum.c` | `for` loop, `+` | 55 |
| `fact.c` | recursion, `*` | 120 |
| `fib.c` | double recursion (R7 spill) | 55 |
| `array.c` | global array, pointer decay, indexing | 16 |
| `ptr.c` | address-of, write-through pointer | 123 |
| `divmod.c` | runtime `/` and `%` | 16 |

## Limitations

See `../docs/ABI.md` §7. The toolchain now honors the architectural bit-15
absolute/modified flag (the operand-fetch indexing micro-cycle is implemented),
so absolute code/globals are used verbatim and `gec` places them above `0x1000`
without aliasing the reprogrammed base registers; frame/stack use `disp(5)`/
`disp(6)` (modified). No floats, no separate compilation yet.

## Driver mode (gcc-style)

`gec` now drives `gasm` itself (found next to the binary at
`../assembler/gasm`, else in PATH):

    gec prog.c -o prog.cap        # the default: a card deck carried by the
                                  #   ORIGINAL IPL scatter loader, embedded
                                  #   verbatim from the SAT decks and proven
                                  #   on the real machine -- 66-byte LL/II
                                  #   relocation cards plus a jump-to-origin
                                  #   termination card
    gec prog.c --boot -o prog.cap # deck led by the boot.s template instead:
                                  #   boot card + program as body cards
    gec prog.c --card -o prog.cap # one 40-byte IPL boot card (ORG 0)
    gec prog.c -c -o prog.s       # compile only, stop at gasm assembly
    gec prog.c -o prog.s          # same (back-compat: .s output implies -c)

`--boot` works because crt0 (`__start`) is emitted first at the origin
(0x1100), so image entry == origin -- the boot card's exact contract. The
boot card leaves change-register 0 dirty; gec-generated code never uses R0.
