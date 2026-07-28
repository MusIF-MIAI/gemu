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

if [ "$fail" = 0 ]; then echo "gec: all checks passed"; else echo "gec: FAILURES"; exit 1; fi
