#!/bin/sh
# roundtrip.sh — toolchain end-to-end checks for the unified binary format.
#
# Run from the gemu directory (where `make check` runs). Exercises the three
# tools together, which the in-process utest suite cannot:
#
#   1. Each assembler example:  gasm -> .bin,  gemu runs it to HLT,
#      and gasm -> gdis -> gasm is byte-identical (round-trip).
#   2. A real .cap deck:  gdis --image -> .bin,  gemu loads + executes it
#      with no decode errors ("no timing charts" / "implement command").
#
# Exits non-zero if any check fails.

GE=./ge
GASM=assembler/gasm
GDIS=disassembler/gdis
CAP=../DUMP1/funktionalcpu.cap

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0

echo "== unified-format round-trip =="

for s in assembler/examples/*.s; do
    name=$(basename "$s" .s)
    if ! "$GASM" -o "$TMP/$name.bin" "$s" >/dev/null 2>&1; then
        echo "FAIL: $name: gasm error"; fail=1; continue
    fi

    out=$("$GE" "$TMP/$name.bin" --max-cycles 100000 2>/dev/null)
    if ! echo "$out" | grep -q 'halted=1'; then
        echo "FAIL: $name: did not halt ($out)"; fail=1; continue
    fi

    "$GDIS" -o "$TMP/$name.asm" "$TMP/$name.bin" >/dev/null 2>&1
    "$GASM" -o "$TMP/$name.2.bin" "$TMP/$name.asm" >/dev/null 2>&1
    if cmp -s "$TMP/$name.bin" "$TMP/$name.2.bin"; then
        echo "  ok: $name (halts; gasm->gdis->gasm identical)"
    else
        echo "FAIL: $name: round-trip differs"; fail=1
    fi
done

echo "== .cap depunch -> unified -> run =="

if [ -f "$CAP" ]; then
    if "$GDIS" --image -o "$TMP/fk.bin" "$CAP" >/dev/null 2>&1; then
        errs=$("$GE" "$TMP/fk.bin" --trace err --max-cycles 50000 2>&1 \
               | grep -c 'no timing charts\|implement command')
        if [ "$errs" -eq 0 ]; then
            echo "  ok: funktionalcpu depunch loads + executes (no decode errors)"
        else
            echo "FAIL: funktionalcpu run produced $errs decode error(s)"; fail=1
        fi
    else
        echo "FAIL: gdis could not depunch $CAP"; fail=1
    fi
else
    echo "  skip: $CAP not found"
fi

if [ "$fail" -eq 0 ]; then
    echo "roundtrip: all checks passed"
else
    echo "roundtrip: FAILURES"
fi
exit $fail
