#!/bin/sh
# roundtrip.sh — toolchain end-to-end checks, on cards.
#
# Run from the gemu directory (where `make check` runs). Exercises the three
# tools together, which the in-process utest suite cannot:
#
#   1. Every assembler example: gasm -> a .cap deck, the deck goes in the
#      reader, and the machine runs it with no decode errors. Exactly the file
#      and exactly the path that would be used against the real GE-120.
#   2. The assembler is deterministic: the same source twice gives the same
#      deck, byte for byte.
#   3. gasm and gdis are inverses: every byte the assembler reports in its own
#      listing comes back off the cards through the depuncher, at the same
#      address, so neither the encoder nor the decoder is drifting.
#   4. The captured decks still depunch to the byte counts they always have.
#
# The cycle-exact CPU functional sweep that used to live here moved into
# tests/funktional_sweep.c when the .bin path was removed; it needs to write the
# console test-selection byte, which is scaffolding a test may do and a command
# line should not.
#
# Exits non-zero if any check fails.

GE=./ge
GASM=assembler/gasm
GDIS=disassembler/gdis

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0

# Examples that ORG at 0x0000 are boot cards: the machine reads ONE card,
# nibble-packs its 80 columns to 40 bytes at address 0, and executes them.
# Everything else is carried by the scatter loader and lives above 0x0086.
card_example() {
    case "$1" in
        halt|bootcard) return 0 ;;
        *)             return 1 ;;
    esac
}

# What the deck should do once it is running. Free-running demos never halt --
# that is the point of them.
expect_halt() {
    case "$1" in
        halt|mvc|regs)  return 0 ;;
        *)              return 1 ;;
    esac
}

echo "== assembler examples: source -> deck -> reader -> machine =="

for s in assembler/examples/*.s; do
    name=$(basename "$s" .s)
    mode=""
    card_example "$name" && mode="--card"

    if ! "$GASM" $mode -o "$TMP/$name.cap" "$s" >/dev/null 2>&1; then
        echo "FAIL: $name: gasm error"; fail=1; continue
    fi

    # Determinism: the same source must give the same deck.
    "$GASM" $mode -o "$TMP/$name.again.cap" "$s" >/dev/null 2>&1
    if ! cmp -s "$TMP/$name.cap" "$TMP/$name.again.cap"; then
        echo "FAIL: $name: gasm is not deterministic"; fail=1; continue
    fi

    run=$("$GE" "$TMP/$name.cap" --trace err --max-cycles 300000 2>&1)
    errs=$(echo "$run" | grep -c 'no timing charts\|implement command')
    last=$(echo "$run" | tail -1)

    if [ "$errs" -ne 0 ]; then
        echo "FAIL: $name: $errs decode error(s) running the deck"; fail=1; continue
    fi
    if expect_halt "$name"; then
        if ! echo "$last" | grep -q 'halted=1'; then
            echo "FAIL: $name: deck did not halt ($last)"; fail=1; continue
        fi
        echo "  ok: $name (deck loads and halts)"
    else
        if echo "$last" | grep -q 'error=1'; then
            echo "FAIL: $name: deck run errored ($last)"; fail=1; continue
        fi
        echo "  ok: $name (deck loads and free-runs)"
    fi
done

echo "== gasm <-> gdis agree on the punched bytes =="

# A boot card is a hex card, not a scatter record, so gdis cannot read one back;
# this runs on the loader decks. --loose because a synthesized deck carries no
# per-deck identifier prefix on its data cards.
#
# The check is byte identity: every byte gasm reports in its own listing must
# come back out of the deck through gdis's depuncher, at the same address. That
# is the scatter-card encoder and the depuncher checked against each other.
for s in assembler/examples/*.s; do
    name=$(basename "$s" .s)
    card_example "$name" && continue
    [ -f "$TMP/$name.cap" ] || continue

    "$GASM" -o "$TMP/$name.chk.cap" -l "$TMP/$name.lst" "$s" >/dev/null 2>&1
    if ! "$GDIS" --loose --hex "$TMP/$name.cap" > "$TMP/$name.hex" 2>/dev/null; then
        echo "FAIL: $name: gdis could not depunch the deck"; fail=1; continue
    fi

    if python3 - "$TMP/$name.lst" "$TMP/$name.hex" <<'PY'
import re, sys

def load(path, holes_ok):
    out = {}
    for line in open(path):
        m = re.match(r'^([0-9A-Fa-f]{4}):\s*(.*)$', line.rstrip())
        if not m:
            continue
        addr = int(m.group(1), 16)
        for i, tok in enumerate(m.group(2).split()):
            if tok == '--':
                if holes_ok:
                    continue
                return None
            if not re.fullmatch(r'[0-9A-Fa-f]{2}', tok):
                break
            out[addr + i] = int(tok, 16)
    return out

want = load(sys.argv[1], False)
got  = load(sys.argv[2], True)
missing = [a for a in want if a not in got]
wrong   = [a for a in want if a in got and got[a] != want[a]]
if missing or wrong:
    if missing:
        print("  missing %d byte(s), first at 0x%04X" % (len(missing), min(missing)))
    if wrong:
        a = min(wrong)
        print("  %d byte(s) differ, first at 0x%04X: punched 0x%02X, assembled 0x%02X"
              % (len(wrong), a, got[a], want[a]))
    sys.exit(1)
PY
    then
        echo "  ok: $name (every assembled byte comes back off the cards)"
    else
        echo "FAIL: $name: deck bytes disagree with the assembler listing"; fail=1
    fi
done

echo "== captured decks still depunch =="

# gdis reports the span it reconstructed on stderr:
#   "... image 0xLO..0xHI (N bytes)"
span_bytes() {
    "$GDIS" $2 -o /dev/null "$1" 2>&1 >/dev/null |
        sed -n 's/.*image 0x[0-9A-Fa-f]*\.\.0x[0-9A-Fa-f]* (\([0-9]*\) bytes).*/\1/p' |
        tail -1
}

check_deck() {
    path="$1"; want="$2"; extra="$3"; label=$(basename "$path" .cap)
    if [ ! -f "$path" ]; then
        echo "  skip: $path not found"
        return
    fi
    got=$(span_bytes "$path" "$extra")
    if [ "$got" = "$want" ]; then
        echo "  ok: $label depunches to a $got-byte image"
    else
        echo "FAIL: $label depunched to $got bytes (expected $want)"; fail=1
    fi
}

check_deck ../DUMP1/isolationcpu01.cap 15960 --iso
check_deck ../DUMP1/printermechanicaltest.cap 3028
check_deck ../DUMP1/control-program-cr.cap 4146

if [ "$fail" -eq 0 ]; then
    echo "roundtrip: all checks passed"
else
    echo "roundtrip: FAILURES"
fi

exit $fail
