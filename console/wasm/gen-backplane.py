#!/usr/bin/env python3
"""Bake the CPU card-layout Atlas into backplane.js for the wasm console.

The Atlas CSVs (drawing 14026 136, cabinet section 2A) live in the gemu-audit
workspace alongside this repo, not inside it, so the generated file is committed
and this script is only re-run when the scans are re-read.

    ./gen-backplane.py [--atlas ../../../atlas] [-o backplane.js]

Layout, from backplane_layout_verified.md: 18 rows lettered A-T in the Italian
alphabet (J and K do not exist), 40 card positions per row, 17 pins per
connector.  A physical board usually spans two rows at one column -- A+B, C+D,
and so on -- which is why the rows pair up into nine tiers.

Signal glosses come from docs/signals/, so a pin the operator clicks can say
what the net is called and which schematic chapter draws it.
"""

import argparse
import csv
import glob
import json
import os
import re
import sys

ROWS = list("ABCDEFGHI") + list("LMNOPQRST")      # no J, no K
TIERS = [(ROWS[i], ROWS[i + 1]) for i in range(0, len(ROWS), 2)]
SLOTS = 40
PINS = 17


def read_atlas(atlas_dir):
    """-> cards[row][slot] = {code,type,notes}, pins[row][slot] = [17 names]."""
    cards, pins = {}, {}
    for row in ROWS:
        path = os.path.join(atlas_dir, f"row_{row}_pinout_verified.csv")
        if not os.path.exists(path):
            sys.exit(f"missing Atlas row: {path}")
        cards[row], pins[row] = {}, {}
        with open(path) as fh:
            for rec in csv.DictReader(fh):
                if not rec.get("slot"):
                    continue
                slot = int(rec["slot"])
                names = [(rec.get(f"pin_{i:02d}") or "").strip()
                         for i in range(1, PINS + 1)]
                code = (rec.get("card_code") or "").strip()
                ctype = (rec.get("card_type") or "").strip()
                notes = (rec.get("notes") or "").strip()
                if any(names) or code or (ctype and ctype != "//////"):
                    pins[row][slot] = names
                    cards[row][slot] = {"code": code, "type": ctype,
                                        "notes": notes}
    return cards, pins


# ---------------------------------------------------------------- glosses

CELL = re.compile(r"`([^`]+)`")
# "052", "205-213", "104,105", "081–086" (en dash) -- keep as printed
CH_OK = re.compile(r"^[0-9]{2,3}[0-9,–—/ .-]*$")


def read_glosses(docs_dir):
    """Signal/command name -> {ch, meaning} scraped from the markdown tables.

    Every table in docs/signals/ that has a name column, a chapter column and a
    meaning column contributes.  Names are the backticked tokens in the first
    cell; a cell listing several (`RC001`,`RC011`) gives the same gloss to each.
    Ranges written with an ellipsis are not expanded -- the endpoints are real
    signals, the middle is the reader's inference, and inventing the members
    would put names in the index that no sheet prints.
    """
    out = {}
    for path in sorted(glob.glob(os.path.join(docs_dir, "**/*.md"),
                                 recursive=True)):
        header = None
        for line in open(path):
            if not line.lstrip().startswith("|"):
                header = None
                continue
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if set("".join(cells)) <= set("-: "):        # separator row
                continue
            low = [c.lower() for c in cells]
            if header is None:
                if low and low[0] in ("name", "cmd", "signal"):
                    ch = next((i for i, c in enumerate(low)
                               if c in ("ch", "ch/bx", "chapter")), None)
                    mean = next((i for i, c in enumerate(low)
                                 if c.startswith("meaning")), None)
                    header = (ch, mean) if ch is not None else None
                continue
            ch_i, mean_i = header
            if ch_i >= len(cells):
                continue
            ch = cells[ch_i].strip("*` ")
            if not CH_OK.match(ch):
                continue
            meaning = ""
            if mean_i is not None and mean_i < len(cells):
                meaning = re.sub(r"[*`]", "", cells[mean_i]).strip()
            for name in CELL.findall(cells[0]):
                name = name.strip()
                if re.fullmatch(r"[A-Z0-9]{4,6}", name) and name not in out:
                    out[name] = {"ch": ch, "m": meaning}
    return out


# "LU08B: G7·07, H8·10" inside a Backplane cell.  The docs locate a signal by
# position; the Atlas names the signal at a position.  Two independent readings
# of the same backplane, so where both speak they can be compared.
LOCS = re.compile(r"\b([A-Z0-9]{4,6})\s*:\s*((?:[A-Z]\d{1,2}[·.]\d{2}\s*,?\s*)+)")
ONE_LOC = re.compile(r"([A-Z])(\d{1,2})[·.](\d{2})")

# The gate-by-gate traces are prose, not tables: a chapter heading, then bullets
# carrying the gate equation and where its output lands.
#   **Chapter 052 (cp06 p129)** - FORCING IN NO BIT 00/07:
#   - Gate 2 (NAOR 1): `NOG0A = CA2O1 | CA181 | CO901` -> O32-03 (ch.054-1)
CHAP = re.compile(r"\*\*Chapter\s+0*(\d{2,3})\b")
EQN = re.compile(r"`([A-Z0-9]{4,6})\s*=\s*([^`]+)`")
TRACE_LOC = re.compile(r"→\s*([A-Z])(\d{1,2})[-·.](\d{2})")


def read_traces(docs_dir):
    """-> equations[name] = {ch, eq}, and the positions those bullets name.

    The equations are the closest thing we have to what a net *is*, so a pin
    the operator clicks can show the gate that drives it and not just a gloss.
    """
    equations, located = {}, {}
    for path in sorted(glob.glob(os.path.join(docs_dir, "traces/*.md"))):
        chapter = ""
        for line in open(path):
            hit = CHAP.search(line)
            if hit:
                chapter = hit.group(1).zfill(3)
            for name, expr in EQN.findall(line):
                if name not in equations:
                    equations[name] = {"ch": chapter,
                                       "eq": re.sub(r"\s+", " ", expr).strip()}
                for row, slot, pin in TRACE_LOC.findall(line):
                    if row in ROWS and 1 <= int(slot) <= SLOTS \
                            and 1 <= int(pin) <= PINS:
                        key = f"{row}{int(slot)}-{int(pin):02d}"
                        located.setdefault(key, [])
                        if name not in located[key]:
                            located[key].append(name)
    return equations, located


def read_locations(docs_dir):
    """position key "G7-07" -> [signal names the docs place there]."""
    out = {}
    for path in sorted(glob.glob(os.path.join(docs_dir, "**/*.md"),
                                 recursive=True)):
        for line in open(path):
            if not line.lstrip().startswith("|"):
                continue
            for name, locs in LOCS.findall(line):
                for row, slot, pin in ONE_LOC.findall(locs):
                    if row not in ROWS or not 1 <= int(slot) <= SLOTS:
                        continue
                    if not 1 <= int(pin) <= PINS:
                        continue
                    key = f"{row}{int(slot)}-{int(pin):02d}"
                    out.setdefault(key, [])
                    if name not in out[key]:
                        out[key].append(name)
    return out


# Glyph pairs the 2A scans confuse, as docs/signals/README.md warns: O with 0,
# I with 1, B with 8, S with B, and O with D on the worst cells.  A pin reading
# N0G0A is the NOG0A the schematics name, and F1N1B is FINIB.
CONFUSED = ["O0D", "I1", "B8S"]
GLYPH = {c: set(group) for group in CONFUSED for c in group}
MAX_VARIANTS = 256


def variants(name):
    """Every reading of a token under those confusions, nearest first.

    Bounded: a token where most characters are ambiguous would otherwise
    explode, and a match found past a few hundred candidates is coincidence
    rather than a reading.  The token itself always comes first, so an exact
    hit never loses to a glyph-swapped one.
    """
    out = [name]
    seen = {name}
    frontier = [name]
    while frontier and len(out) < MAX_VARIANTS:
        nxt = []
        for word in frontier:
            for i, ch in enumerate(word):
                for alt in GLYPH.get(ch, ()):
                    if alt == ch:
                        continue
                    cand = word[:i] + alt + word[i + 1:]
                    if cand not in seen:
                        seen.add(cand)
                        out.append(cand)
                        nxt.append(cand)
                        if len(out) >= MAX_VARIANTS:
                            return out
        frontier = nxt
    return out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--atlas", default=os.path.join(here, "../../../atlas"))
    ap.add_argument("--docs", default=os.path.join(here, "../../docs/signals"))
    ap.add_argument("-o", "--out", default=os.path.join(here, "backplane.js"))
    args = ap.parse_args()

    cards, pins = read_atlas(args.atlas)
    glosses = read_glosses(args.docs)
    located = read_locations(args.docs)
    equations, trace_locs = read_traces(args.docs)
    for key, names in trace_locs.items():
        located.setdefault(key, [])
        for n in names:
            if n not in located[key]:
                located[key].append(n)
    # A gate equation is a better answer than a one-line gloss, so it wins the
    # chapter where both know the net, and stands alone where only it does.
    for name, eq in equations.items():
        g = glosses.setdefault(name, {"ch": eq["ch"], "m": ""})
        g["eq"] = eq["eq"]
        if not g.get("ch"):
            g["ch"] = eq["ch"]

    # Only ship glosses for nets that actually appear on a pin, keyed by the
    # spelling the Atlas uses -- the page looks up what it reads off the card.
    used = {}

    def want(name):
        if not name or name in used:
            return
        for cand in variants(name):
            if cand in glosses:
                g = dict(glosses[cand])
                if cand != name:
                    g["as"] = cand              # the schematics' spelling
                used[name] = g
                return

    for row in ROWS:
        for names in pins[row].values():
            for name in names:
                want(name)
    for names in located.values():
        for name in names:
            want(name)

    data = {
        "rows": ROWS,
        "tiers": [list(t) for t in TIERS],
        "slots": SLOTS,
        "pinCount": PINS,
        "cards": cards,
        "pins": pins,
        "sigs": used,
        "docPins": located,
    }

    filled = sum(len(pins[r]) for r in ROWS)
    named = sum(1 for r in ROWS for n in pins[r].values() for x in n if x)

    # Where both sources speak, do they agree?  Reported, not reconciled: the
    # Atlas is OCR of a card layout and the docs are hand-read from schematics,
    # and which one is right at a given pin is a question for the scans.
    agree = clash = 0
    for key, names in located.items():
        row, rest = key[0], key[1:]
        slot, pin = rest.split("-")
        atlas = pins.get(row, {}).get(int(slot))
        if not atlas:
            continue
        got = atlas[int(pin) - 1]
        if not got:
            continue
        if any(v in names for v in variants(got)):
            agree += 1
        else:
            clash += 1
    print(f"docs locate {len(located)} pins; {agree} agree with the Atlas, "
          f"{clash} disagree")
    with open(args.out, "w") as fh:
        fh.write("/* Generated by gen-backplane.py from the card-layout Atlas\n"
                 " * (drawing 14026 136, cabinet section 2A). Do not edit by\n"
                 " * hand -- re-run the generator against the Atlas CSVs. */\n")
        fh.write("window.GE_BACKPLANE = ")
        json.dump(data, fh, separators=(",", ":"), sort_keys=True)
        fh.write(";\n")

    print(f"{filled} populated positions, {named} named pins, "
          f"{len(used)} glossed nets -> {args.out} "
          f"({os.path.getsize(args.out) // 1024} KB)")


if __name__ == "__main__":
    main()
