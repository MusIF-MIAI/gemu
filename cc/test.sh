#!/bin/sh
# End-to-end gec compiler tests: compile each example C program to gasm,
# assemble, run on gemu, and check main()'s return value (__rv).
set -e
cd "$(dirname "$0")"
make >/dev/null
GASM=../assembler/gasm
fail=0

check() {
    src="examples/$1"; want="$2"
    ./gec "$src" -o /tmp/gec_t.s
    "$GASM" -o /tmp/gec_t.bin /tmp/gec_t.s >/dev/null
    got=$(./runrv /tmp/gec_t.bin | sed -n 's/^__rv = \([0-9-]*\).*/\1/p')
    if [ "$got" = "$want" ]; then
        echo "  ok: $1 -> $got"
    else
        echo "  FAIL: $1 -> got $got, want $want"
        fail=1
    fi
}

echo "== gec end-to-end (C -> gasm -> gemu, checking __rv) =="
check sum.c    55
check fact.c   120
check fib.c    55
check array.c  16
check ptr.c    123
check divmod.c 16

if [ "$fail" = 0 ]; then echo "gec: all checks passed"; else echo "gec: FAILURES"; exit 1; fi
