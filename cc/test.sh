#!/bin/sh
# End-to-end gec compiler tests: compile each example C program to a card deck,
# put the deck in the reader, run the machine, and check main()'s return value
# (__rv). One command end to end -- the same .cap that would go to the iron.
set -e
cd "$(dirname "$0")"
make >/dev/null
fail=0

check() {
    src="examples/$1"; want="$2"
    ./gec "$src" -o /tmp/gec_t.cap >/dev/null
    got=$(./runrv /tmp/gec_t.cap | sed -n 's/^__rv = \([0-9-]*\).*/\1/p')
    if [ "$got" = "$want" ]; then
        echo "  ok: $1 -> $got"
    else
        echo "  FAIL: $1 -> got $got, want $want"
        fail=1
    fi
}

echo "== gec end-to-end (C -> .cap deck -> reader -> gemu, checking __rv) =="
check sum.c    55
check fact.c   120
check fib.c    55
check array.c  16
check ptr.c    123
check divmod.c 16

# -c stops at assembly and writes nothing else.
rm -f /tmp/gec_c.s /tmp/gec_c.cap
./gec examples/sum.c -c -o /tmp/gec_c.s
if [ -s /tmp/gec_c.s ] && [ ! -e /tmp/gec_c.cap ]; then
    echo "  ok: -c stops at assembly"
else
    echo "  FAIL: -c did not stop at assembly"
    fail=1
fi

# -E runs the preprocessor only. Checks object- and function-like macros, that a
# guarded header included twice contributes once, that comments are stripped
# before expansion, and that identifiers inside string literals are left alone.
cat > /tmp/gec_pp.c <<'EOF'
#include <ge.h>
#include <ge.h>
#define TWICE(x) ((x) + (x))
int main(void) {
    char *s = "GE_STATUS not expanded here";
    return TWICE(1) + GE_STATUS(0x00) + GE_QR_GT;   /* comment vanishes */
}
EOF
./gec /tmp/gec_pp.c -E -o /tmp/gec_pp.i
if   ! grep -q '_order((0x00), 0x80 | 0x40, 0x2E)' /tmp/gec_pp.i; then
    echo "  FAIL: -E did not expand GE_STATUS"; fail=1
elif ! grep -q '((1) + (1))' /tmp/gec_pp.i; then
    echo "  FAIL: -E did not expand a function-like macro"; fail=1
elif ! grep -q '"GE_STATUS not expanded here"' /tmp/gec_pp.i; then
    echo "  FAIL: -E expanded inside a string literal"; fail=1
elif grep -q 'comment vanishes' /tmp/gec_pp.i; then
    echo "  FAIL: -E left comments in the output"; fail=1
elif grep -q 'GE_H' /tmp/gec_pp.i; then
    echo "  FAIL: include guard did not suppress the second include"; fail=1
else
    echo "  ok: -E preprocesses only (macros, guards, comments, literals)"
fi

# Peripheral primitives: the device folds to an immediate, and the transfer
# length is emitted as the machine's length-1 field (80 bytes -> LL=79).
cat > /tmp/gec_dev.c <<'EOF'
#include <ge.h>
char card[80];
int main(void) {
    int rdr = _open_reader();
    _read(rdr, card, 80);
    if (GE_STATUS(rdr) != GE_QR_GT) lon();
    _close(rdr);
    return 0;
}
EOF
./gec /tmp/gec_dev.c -c -o /tmp/gec_dev.s
if   ! grep -q 'PER 0x00, __ord' /tmp/gec_dev.s; then
    echo "  FAIL: _read did not emit a PER with the folded unit byte"; fail=1
elif ! grep -q 'DW 79' /tmp/gec_dev.s; then
    echo "  FAIL: _read(.., 80) did not emit LL=79"; fail=1
elif ! grep -q 'DB 0xC0, 0x2E' /tmp/gec_dev.s; then
    echo "  FAIL: GE_STATUS did not emit a CPER|EPER order block"; fail=1
else
    echo "  ok: peripheral primitives (unit folded, LL=length-1, Z bits)"
fi

# The device must be a constant: the PER unit name is an instruction immediate.
cat > /tmp/gec_bad.c <<'EOF'
char b[4];
int main(void) { int d = 0; d = d + 1; _read(d, b, 4); return 0; }
EOF
if ./gec /tmp/gec_bad.c -c -o /tmp/gec_bad.s 2>/tmp/gec_bad.err; then
    echo "  FAIL: a non-constant device was accepted"; fail=1
elif ! grep -q 'must come from an _open_' /tmp/gec_bad.err; then
    echo "  FAIL: wrong diagnostic for a non-constant device"; fail=1
else
    echo "  ok: non-constant device rejected with a clear diagnostic"
fi

if [ "$fail" = 0 ]; then echo "gec: all checks passed"; else echo "gec: FAILURES"; exit 1; fi
