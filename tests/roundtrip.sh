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
# The scanned decks are available separately; ../DUMP1 is the working drop
# holding them and is the source of record, same as for the other decks
# checked below. Site_Acceptance_Test/ is the fallback without the drop.
CAP=../DUMP1/funktionalcpu.cap
[ -f "$CAP" ] || CAP=Site_Acceptance_Test/funktionalcpu.cap

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
        # The deck reads its test selection from mem[0x0E00], which the console
        # operator sets before pressing START.  --poke applies between load and
        # ge_start(), which is exactly where the operator's write lands.
        #
        # Option 0 exercises almost nothing -- the deck finds no test selected
        # and converges straight to the documented idle halt.  Option 0x40 is
        # the CPU functional sweep, ~1.3M cycles across the whole instruction
        # set, and is what actually regressions the timing charts.
        for opt in 0x00 0x40; do
            case "$opt" in
                0x00) want=175a; cyc=200000;   what="idle HLT (no test selected)" ;;
                0x40) want=1427; cyc=3000000;  what="CPU functional sweep" ;;
            esac
            run=$("$GE" "$TMP/fk.bin" --poke 0x0E00=$opt --trace err \
                        --max-cycles $cyc 2>&1)
            errs=$(echo "$run" | grep -c 'no timing charts\|implement command')
            last=$(echo "$run" | tail -1)
            if [ "$errs" -ne 0 ]; then
                echo "FAIL: funktionalcpu $opt produced $errs decode error(s)"
                fail=1
            elif echo "$last" | grep -q 'halted=1' &&
                 echo "$last" | grep -q "PO=$want"; then
                echo "  ok: funktionalcpu *0x0E00=$opt -> HLT 0x$want ($what)"
            else
                echo "FAIL: funktionalcpu $opt did not reach HLT 0x$want ($last)"
                fail=1
            fi
        done
    else
        echo "FAIL: gdis could not depunch $CAP"; fail=1
    fi
else
    echo "  skip: $CAP not found"
fi

echo "== isolation deck (--iso) extraction =="

ISO=../DUMP1/isolationcpu01.cap
if [ -f "$ISO" ]; then
    if "$GDIS" --image -o "$TMP/iso.bin" "$ISO" >/dev/null 2>&1; then
        # 210 valid cards x 76 cols = 15960 payload bytes + 12-byte header.
        sz=$(wc -c < "$TMP/iso.bin")
        if [ "$sz" -eq 15972 ]; then
            echo "  ok: isolationcpu01 auto-family load -> 15960-byte stream (210 cards x 76)"
        else
            echo "FAIL: isolationcpu01 --iso image is $sz bytes (expected 15972)"; fail=1
        fi
    else
        echo "FAIL: gdis could not extract $ISO"; fail=1
    fi
else
    echo "  skip: $ISO not found"
fi

echo "== additional real deck families =="

PRT=../DUMP1/printermechanicaltest.cap
if [ -f "$PRT" ]; then
    if "$GDIS" --image -o "$TMP/prt.bin" "$PRT" >/dev/null 2>&1; then
        sz=$(wc -c < "$TMP/prt.bin")
        if [ "$sz" -eq 3040 ]; then
            echo "  ok: printermechanicaltest scatter-loads to a 3028-byte image"
        else
            echo "FAIL: printermechanicaltest image is $sz bytes (expected 3040)"; fail=1
        fi
    else
        echo "FAIL: gdis could not depunch $PRT"; fail=1
    fi
else
    echo "  skip: $PRT not found"
fi

CPCR=../DUMP1/control-program-cr.cap
if [ -f "$CPCR" ]; then
    if "$GDIS" --image -o "$TMP/cpcr.bin" "$CPCR" >/dev/null 2>&1; then
        sz=$(wc -c < "$TMP/cpcr.bin")
        if [ "$sz" -eq 4158 ]; then
            echo "  ok: control-program-cr scatter-loads to a 4146-byte image"
        else
            echo "FAIL: control-program-cr image is $sz bytes (expected 4158)"; fail=1
        fi
    else
        echo "FAIL: gdis could not depunch $CPCR"; fail=1
    fi
else
    echo "  skip: $CPCR not found"
fi

if [ "$fail" -eq 0 ]; then
    echo "roundtrip: all checks passed"
else
    echo "roundtrip: FAILURES"
fi
exit $fail
