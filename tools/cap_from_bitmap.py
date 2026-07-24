#!/usr/bin/env python3
"""cap_from_bitmap.py — reconstruct the hex card section of a .cap file from
its visual bitmap section.

The Pico/Burroughs card readers emit two representations per card in a .cap:

  1. a HEX section  — `Card n. N` followed by one line of 80 four-hex-digit
     column tokens (13-bit hole patterns).  This is what gemu's cap.c parses.
  2. a VISUAL section — `Card n. N` followed by 12 lines of 80 chars, `*` for a
     punched hole and `_` for none: the 12 punch rows of the card, top to
     bottom (IBM row order 12, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9).

A scan that produced only the visual section is "incomplete": gemu cannot load
it, because the bitmap lines carry no hex.  This tool regenerates the hex
section from the bitmap and writes a full dual-section .cap in the same layout
as the good DUMP decks.

Row -> bit mapping (empirically derived and verified against funktionalcpu.cap,
which ships BOTH sections for all 114 cards — 9120 columns, zero mismatch):

    bitmap row  0 (card row 12) -> bit 12 (0x1000)
    bitmap row  1 (card row 11) -> bit 11 (0x0800)
    bitmap row  2 (card row  0) -> bit  0
    bitmap row  3 (card row  1) -> bit  1
    ...
    bitmap row 11 (card row  9) -> bit  9
    (bit 10 / 0x400 is unused by this reader's encoding)

Run --selftest to re-verify the mapping against a dual-section oracle deck.
"""

import argparse
import re
import sys

# bitmap row index (top=0) -> bit position in the 13-bit column value
ROW_TO_BIT = {0: 12, 1: 11, 2: 0, 3: 1, 4: 2, 5: 3,
              6: 4, 7: 5, 8: 6, 9: 7, 10: 8, 11: 9}

CARD_RE = re.compile(r'\s*Card n\.\s*(\d+)\s*$')
BITMAP_RE = re.compile(r'[_*]{80}\Z')
HEXLINE_RE = re.compile(r'([0-9A-Fa-f]{4} ?)+\Z')


def parse_cards(path):
    """Return (preamble_lines, [(num, kind, payload)]).

    kind is 'bmp' (payload = list of 80-char rows) or 'hex' (payload = list of
    int column values). Cards are yielded in file order; a deck that carries
    both sections yields each card number twice, once per kind.
    """
    preamble, cards = [], []
    num, kind, buf = None, None, []
    seen_card = False

    def flush():
        nonlocal num, kind, buf
        if num is not None and buf:
            if kind == 'bmp':
                cards.append((num, 'bmp', buf))
            else:
                vals = [int(t, 16) for t in ' '.join(buf).split()]
                cards.append((num, 'hex', vals))
        buf, kind = [], None

    for line in open(path, errors='replace'):
        s = line.rstrip('\n')
        m = CARD_RE.match(s)
        if m:
            flush()
            num = int(m.group(1))
            buf, kind = [], None
            seen_card = True
            continue
        if not seen_card:
            preamble.append(s)
            continue
        if BITMAP_RE.match(s):
            kind = 'bmp'
            buf.append(s)
        elif s.strip() and HEXLINE_RE.match(s.strip()):
            kind = 'hex'
            buf.append(s.strip())
        # everything else (blank lines, "Total cards:", stray text) ends the
        # current payload but is not itself card data
    flush()
    return preamble, cards


def bitmap_to_columns(rows):
    """Convert 12 bitmap rows (80 chars each) to 80 column values."""
    if len(rows) != 12 or any(len(r) != 80 for r in rows):
        raise ValueError(f"expected 12 rows of 80, got {len(rows)} rows "
                         f"of lengths {sorted({len(r) for r in rows})}")
    cols = []
    for c in range(80):
        v = 0
        for r in range(12):
            if rows[r][c] == '*':
                v |= (1 << ROW_TO_BIT[r])
        cols.append(v)
    return cols


def fmt_hexline(cols):
    """80 column values -> the reader's hex line ('XXXX ' * 80, uppercase)."""
    return ''.join(f'{v:04X} ' for v in cols)


def selftest(oracle_path):
    """Verify ROW_TO_BIT against a deck carrying both sections."""
    _, cards = parse_cards(oracle_path)
    hexmap = {n: p for n, k, p in cards if k == 'hex'}
    bmpmap = {n: p for n, k, p in cards if k == 'bmp'}
    common = [n for n in hexmap if n in bmpmap and len(hexmap[n]) == 80]
    if not common:
        print(f"selftest: {oracle_path} has no dual-section cards", file=sys.stderr)
        return 1
    bad = 0
    for n in common:
        got = bitmap_to_columns(bmpmap[n])
        if got != hexmap[n]:
            bad += sum(1 for a, b in zip(got, hexmap[n]) if a != b)
    print(f"selftest: {len(common)} dual cards, {bad} column mismatches "
          f"({'OK' if bad == 0 else 'FAIL'})")
    return 0 if bad == 0 else 1


def convert(src, dst):
    preamble, cards = parse_cards(src)
    bmp = [(n, p) for n, k, p in cards if k == 'bmp']
    has_hex = any(k == 'hex' for _, k, _ in cards)
    if has_hex:
        print(f"warning: {src} already contains a hex section; "
              f"rebuilding it from the bitmap", file=sys.stderr)
    if not bmp:
        print(f"error: {src} has no bitmap cards to convert", file=sys.stderr)
        return 1

    converted = []
    for n, rows in bmp:
        try:
            converted.append((n, bitmap_to_columns(rows)))
        except ValueError as e:
            print(f"error: card {n}: {e}", file=sys.stderr)
            return 1

    with open(dst, 'w') as f:
        # preamble, minus any stale "Total cards:" (we emit the real one below)
        for line in preamble:
            if line.strip().lower().startswith('total cards'):
                continue
            f.write(line + '\n')
        # hex section — the part gemu actually loads
        for n, cols in converted:
            f.write(f'Card n. {n}\n')
            f.write(fmt_hexline(cols) + '\n')
        f.write(f'Total cards: {len(converted)}\n\n\n')
        # visual section, preserved verbatim from the scan
        for n, rows in bmp:
            f.write(f'Card n. {n}\n')
            for r in rows:
                f.write(r + '\n')

    print(f"{src} -> {dst}: {len(converted)} cards "
          f"({len(preamble)} preamble lines)")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('src', nargs='?', help='input .cap (bitmap section)')
    ap.add_argument('dst', nargs='?', help='output .cap (dual section)')
    ap.add_argument('--selftest', metavar='ORACLE',
                    help='verify the row->bit mapping against a dual-section deck')
    args = ap.parse_args()

    if args.selftest:
        return selftest(args.selftest)
    if not args.src or not args.dst:
        ap.error('need src and dst (or --selftest ORACLE)')
    return convert(args.src, args.dst)


if __name__ == '__main__':
    sys.exit(main())
