# GE-120 C ABI

A calling convention and data model for compiling C to the GE-120 / GE-130, as
implemented by the in-tree compiler `software/gemu/cc` (`gcc.c`) emitting `gasm`
assembly.

This document is **normative for our toolchain**: it is a convention *we* define,
not one recovered from a GE C compiler (none existed — the machine predates C).
Every choice is justified against the verified ISA (`docs/ISA.md`). Where the
hardware forces a decision it is marked **[hw]**; where we chose freely among
valid options it is marked **[abi]**.

---

## 0. Summary

| Decision | Value |
|---|---|
| `char` / `signed char` / `unsigned char` | **1 byte** |
| `short` / `int` | **2 bytes**, big-endian, two's complement |
| `long` | **4 bytes** |
| `long long` | **8 bytes** |
| pointer (all) | **2 bytes** (16-bit address space) |
| `float` / `double` | **not supported** (no FP hardware; reserved for a future soft-float) |
| Endianness | **big-endian** [hw] — `AB/SB` and address fields are big-endian |
| Alignment | **none** — memory is byte-addressable; every type is byte-aligned [hw] |
| Char/string constants | GE-100 internal graphic code (ISA §2.1), **not ASCII** |
| Link register | **R7** [hw] — `JRT` writes the return address here |
| Stack pointer | **R6** [abi] |
| Frame pointer | **R5** [abi] |
| Globals/abs base | **R0** [abi] — held at identity `0x0000` |
| Scratch address regs | **R1–R4** [abi] |
| Stack growth | **upward** (toward higher addresses) [abi] |
| Return value | fixed cell `__rv` (16 bytes) [abi] |

> **Why `int` = 2 and not 1.** `AB`/`SB` add and subtract big-endian binary
> fields of **1–16 bytes** in a single instruction (ISA §6.6), so wide integers
> cost nothing extra for `+ - & | ^` and compares. A 2-byte `int` also matches
> the 16-bit pointer/register width, so an `int` can hold a pointer — essential
> for idiomatic C. Only **multiply, divide, and shift** need software help, and
> that is independent of width (the ISA has no binary `×`/`÷` and **no shift
> instruction** — ISA §9). A 1-byte `int` is available via `-mint=1` but is a
> curiosity: it cannot hold a pointer and caps loop counters at 256.

---

## 1. Memory map

16-bit address space; **bit 15 of an encoded address field is unused** (ISA §4.2),
so directly-encoded addresses span `0x0000–0x7FFF`. Higher memory is reachable
only by loading a base register, which we do not do — **the C model lives in the
low 32 KiB.**

```
0x0000 .. 0x000F   reserved / low scratch
0x0010 .. 0x001F   __rv      return-value cell (16 bytes)
0x0020 .. 0x002F   __a, __b  runtime-helper operand cells (mul/div/shift)
0x0030 .. 0x00EF   reserved (incl. interrupt zone 0x0300+ if ever enabled)
0x00F0 .. 0x00FF   change registers R0..R7  (R_N at 0xF0+2N)  [hw]
0x0100 .. ......    .text  (crt0 first, then compiled functions)
......             .data  (initialised globals)
......             .bss   (zeroed globals)
0x6000 .. 0x7FFF   stack  (R6 starts at 0x6000, grows up)     [abi]
```

The split between `.bss`/heap and the stack base (`0x6000`) is a linker constant
(`crt0`), adjustable per program. Programs that the integrated card reader
bootstraps load `.text` at `0x0100` (the `DUMP1` convention, ISA §4.1); unit
tests that use `ge_load_program` load at `0x0000`.

---

## 2. Registers (the eight change registers)

The machine has no general arithmetic registers; the eight **change registers**
(memory-mapped 16-bit words at `0xF0+2N`, ISA §4.3) are address/index registers.
The ABI assigns them fixed roles:

| Reg | Addr | Role | Preserved across call? |
|----|------|------|----|
| **R0** | `0xF0` | Globals / absolute base, held at `0x0000` (identity). Modifier 0 ⇒ absolute addressing. | yes (never changed) |
| **R1** | `0xF2` | Scratch address register A (lhs pointer / dereference) | no (caller-saved) |
| **R2** | `0xF4` | Scratch address register B (rhs pointer) | no (caller-saved) |
| **R3** | `0xF6` | Scratch / array-index base | no (caller-saved) |
| **R4** | `0xF8` | Scratch | no (caller-saved) |
| **R5** | `0xFA` | **Frame pointer (FP)** | **yes (callee-saved)** |
| **R6** | `0xFC` | **Stack pointer (SP)** | **yes (callee-saved)** |
| **R7** | `0xFE` | **Link register (LR)** — written by `JRT` [hw] | **yes (callee-saved if non-leaf)** |

Because computation is memory-to-memory, R1–R4 are used only to *form effective
addresses* (pointer deref, array indexing) via the `disp(N)` address form
(`EA = disp + R_N`, ISA §4.2). All actual data lives in memory.

---

## 3. Calling convention

### 3.1 Call and return [hw-anchored]

Verified from `CPU/GE 120 CENTRAL PROCESSOR [4].pdf` §5.5.6.2 / §5.6.5.1 and the
APS manual (EDV-AFL 03) p.123–134, and implemented + tested in `gemu`:

- **Call:** `JRT 0xF0, callee` — opcode `0x41`, mask `0xF0` (unconditional).
  Deposits the address of the *next* instruction into **R7** and jumps. One
  instruction. (`SUB`/`*N…N` in APS assemble to exactly this.)
- **Return:** `JU 0x000(7)` — `JU` with modifier 7, displacement 0; effective
  address `= 0 + R7` = the saved return address. Encodes as `47 F0 70 00`.
- **Nesting:** `JRT` clobbers R7, so a **non-leaf** function must spill R7 into
  its frame before making any call and restore it before returning. A **leaf**
  function may leave R7 alone (MIPS/ARM-style).

### 3.2 Stack frame

The stack grows **upward**. On entry, R6 (SP) points at the base of the new
frame — which is also where the **caller has already written the outgoing
arguments**. Within a frame, everything is addressed as `disp(5)` (FP-relative),
and `disp` is unsigned 0–`0xFFF` (ISA §4.2), so every slot is at a **non-negative**
offset from FP — the reason the stack grows up and args sit at the bottom.

Frame layout (offsets from **FP = R5**), with `A` = total bytes of incoming args:

```
 FP+0      incoming argument 0          \
 FP+...    incoming argument 1..n-1     |  written by caller (= bytes [SP..] before the call)
 FP+(A-?)  last incoming arg            /
 FP+A      saved caller FP   (2 bytes)
 FP+A+2    saved LR          (2 bytes)  (present iff function is non-leaf)
 FP+A+4    local variables / spilled temporaries ...
 FP+F      (top of frame; new SP)        F = A + 4 + locals_size
```

**Prologue** (non-leaf shown; a leaf omits the LR save):

```
    STR 5, A(6)          ; save caller FP at [SP+A]
    STR 7, (A+2)(6)      ; save LR at [SP+A+2]
    LA  5, 0x000(6)      ; FP = SP
    LA  6, F(6)          ; SP = SP + F   (allocate the frame)
```

**Epilogue:**

```
    LR  7, (A+2)(5)      ; LR  <- saved (non-leaf only)
    LA  6, 0x000(5)      ; SP  = FP      (deallocate)
    LR  5, A(5)          ; FP  <- saved caller FP
    JU  0x000(7)         ; return
```

All of `STR/LR/LA/JU/JRT` are ✅-wired in `gemu`. (`AMR`/`SMR` exist for
register arithmetic but the prologue/epilogue only need add-via-`LA`, since the
stack grows up.)

### 3.3 Arguments

- Arguments are pushed **left-to-right** into the caller's outgoing-argument
  area, i.e. written to `[SP+0], [SP+k0], [SP+k1], …` where each `k` is the
  running sum of prior argument sizes. After the `JRT`, that area is the
  callee's `FP+0…` incoming-argument block.
- Each argument occupies its type's natural size (char 1, int/ptr 2, long 4).
- Small structs are passed by value byte-for-byte in the arg area; large structs
  and any struct return use a **hidden pointer** (caller allocates, passes its
  address as an implicit first argument). *(struct-by-value: planned, not in the
  first compiler slice.)*
- Variadic functions: arguments are laid out the same way; the callee reads
  successive slots. (No register args means varargs is uniform and simple.)

### 3.4 Return values

- Scalars (≤16 bytes) are returned in the fixed cell **`__rv`** (`0x0010`),
  big-endian, right-justified for integers. The caller copies `__rv` to its
  destination **before any subsequent call** (the cell is not reentrant, but a
  return value is always consumed before the next call — the same discipline GE
  system subroutines use with their "fixed store areas," APS p.134).
- `void` functions write nothing.
- Struct returns use the hidden-pointer mechanism (§3.3).

### 3.5 Caller/callee responsibilities

- **Caller:** evaluate and write arguments to the outgoing area; `JRT 0xF0,f`;
  after return, copy `__rv` if the value is used. Must assume R1–R4 are
  destroyed; spill any live scratch first (the compiler keeps live values in
  memory anyway, so this is usually a no-op).
- **Callee:** run the prologue; body addresses locals/args/temps as `disp(5)`;
  write the result to `__rv`; run the epilogue; `JU 0x000(7)`.

---

## 4. Data representation

- **Integers** — big-endian two's complement, the width of their type. Stored at
  the address of their **most-significant byte**; `AB/SB` process right-to-left
  from the least-significant (rightmost) byte (ISA §4.5), so the operand address
  passed to `AB/SB` is `field_addr + size - 1`.
- **Pointers** — 2-byte big-endian absolute addresses (`0x0000–0x7FFF`).
- **Arrays** — contiguous, row-major; `a[i]` ⇒ `EA = &a + i*sizeof(elem)`,
  computed into a scratch register (R1–R4) with `LA`/`AMR`.
- **Structs** — fields laid out in declaration order, no padding (byte-aligned).
- **`char` / strings** — encoded in the **GE-100 internal graphic set**
  (ISA §2.1), e.g. `'0'`=`0x40`, `'A'`=`0x51`, space=`0x50` — **not ASCII**. The
  compiler translates C character/string literals through this table so that
  console/printer output matches the machine. `"\0"` terminator stays `0x00`.

---

## 5. Runtime helpers (the ops the ISA lacks)

The ISA has **no binary multiply/divide and no shift** (ISA §9). The compiler
emits calls to assembly helpers in `crt0`/`libgc`. Helper convention (fast path,
not the general stack ABI): operands in fixed cells **`__a`**, **`__b`**
(2 bytes each at `0x0020`/`0x0022`); result in **`__rv`**.

| Helper | C operator | Algorithm (wired instructions only) |
|---|---|---|
| `__mul` | `*` | 16-iteration shift-add: walk bits of `__b` with `TM` masks; for each set bit add the running double of `__a` (`AB acc,acc`) into the result. No shift-right needed. |
| `__divu` / `__modu` | `/` `%` (unsigned) | restoring division via `SB`/`CMC`/`JC` bit-walk; quotient→`__rv`, remainder kept for `__mod`. |
| `__div` / `__mod` | signed | sign-adjust operands, call unsigned core, fix sign. |
| `__shl` | `<<` | `n` self-adds (`AB x,x`) — left shift = repeated doubling. |
| `__shru` | `>>` (unsigned) | `__divu` by `2^n`. |
| `__shr` | `>>` (signed) | arithmetic: sign-extend then `__divu`, adjust. |

`crt0` responsibilities: set `R0 = 0x0000`, `R6 = 0x6000` (stack base),
`R5 = R6`, then `JRT 0xF0, main`; on return, leave `__rv` in place and `HLT`
(so a test harness or the console can read `main`'s exit value from `__rv`).

---

## 6. Worked example

```c
int add(int a, int b) { return a + b; }   /* leaf */
int main(void)       { return add(2, 40); }
```

`main` (non-leaf, 0 args, frame = saved-FP + saved-LR + outgoing-arg area):

```
main:   STR 5, 0(6)          ; save caller FP
        STR 7, 2(6)          ; save LR (main calls add)
        LA  5, 0x000(6)      ; FP = SP
        LA  6, F(6)          ; allocate frame (F = 4 + outgoing-arg space)
        ; --- write args for add() at the outgoing area [FP+4], [FP+6] ---
        MVI ... ; a=2  -> two bytes 00 02 at outgoing slot 0
        MVI ... ; b=40 -> 00 28 at outgoing slot 1
        ; SP currently points at the outgoing area base -> that's add's FP
        JRT 0xF0, add        ; call; R7 = return addr
        ; add wrote its result to __rv; copy __rv -> main's __rv (already there)
        LR  7, 2(5)          ; restore LR
        LA  6, 0x000(5)      ; SP = FP
        LR  5, 0(5)          ; restore caller FP
        JU  0x000(7)         ; return (to crt0, which HLTs)

add:    ; leaf: a at FP+0, b at FP+2
        AB  2,2, __rv+14, FP+1 ... ; __rv = a ; then AB __rv += b  (2-byte fields)
        JU  0x000(7)         ; return (R7 untouched -> still caller's)
```

(The exact `AB` field addressing and the arg-write sequence are what `gcc.c`
emits; this sketch shows the frame mechanics.) After `crt0` `HLT`s, `__rv`
holds `42`.

---

## 7. Limits / non-goals (first compiler)

- No floating point.
- No `goto` into blocks, no VLAs, no `alloca`.
- `long long`/64-bit and full `long` arithmetic rely on `AB/SB` width (≤16
  bytes, free) but mul/div on widths >2 bytes are not yet in the helper set.
- Recursion works (frames + R7 spill); deep recursion is bounded by the
  `0x6000–0x7FFF` stack window (8 KiB).
- Single translation unit at first (no separate-compilation linker yet); `crt0`
  + program assembled together by `gasm`.
- **Addressing (current toolchain):** the emulator resolves every operand
  address as base+displacement (`disp(N)` → `change_register[N] + disp`) and does
  not yet honor the architectural bit-15 absolute/modified flag (ISA.md §4.2,
  §8). Local/stack access via `disp(5)`/`disp(6)` is exact; but an **absolute**
  address ≥ `0x1000` would index a (possibly reloaded) base register, so **keep
  compiled code and globals in low memory (< 0x1000)** until the bit-15 modified
  indexing micro-cycle is transcribed. Small programs are unaffected.

> **Status:** the compiler (`cc/gec.c`) implements this ABI and is verified
> end-to-end on `gemu` (`cc/test.sh`, run by `make check`): arithmetic, `* / %`,
> comparisons, `&& || !`, `if/while/for`, recursion (R7 spill), pointers
> (`& *`, array decay) and arrays. The native `JRT` call/return and the SS
> operand-fetch fix (V1 keeps its full field) make it work.

These are implementation limits of `cc`, not of the ABI — the convention above
is complete enough to target more of C as the compiler grows.
