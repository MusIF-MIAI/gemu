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


# Card types the scans read with an H where the machine has an M -- the 1968
# typeface puts the two close enough that the OCR takes one for the other.
# Each of these is corroborated: ALAM2A and LOSE2M appear under their M
# spelling elsewhere in the layout itself (7 and 18 times), AMPL2A and TEME2A
# are written that way in docs/hardware-options.md and
# backplane_layout_verified.md, and MAME2A is the machine's owner reading the
# card. Listed one by one rather than replacing every H, so a type that really
# does carry one is not quietly rewritten.
TYPE_FIX = {
    "AHPL2A": "AMPL2A",
    "ALAH2A": "ALAM2A",
    "HAME2A": "MAME2A",
    "LOSE2H": "LOSE2M",
    "TEHE2A": "TEME2A",
}


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
                ctype = TYPE_FIX.get(ctype, ctype)
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


# ---------------------------------------------------------------- the manual

# "**Chapter 052 (cp06 p129)**" -- a page a human read off the scan, which is
# worth more than anything derived below.
CHAP_PAGE = re.compile(r"\*\*Chapter\s+0*(\d{1,3})\s*\(cp06\s*p(\d{1,4})\)")
CHAPTER_IN_TEXT = re.compile(r"CHAPTER\s*0*(\d{1,3})\b")


def longest_rising(pairs):
    """Keep the longest run of (page, chapter) that rises with the page.

    The gate sheets are bound in chapter order, so a page whose OCR'd chapter
    number goes backwards is a misread, not a sheet. Dropping those costs a
    few real pages and keeps the index from pointing anywhere confidently
    wrong.
    """
    if not pairs:
        return []
    best = [1] * len(pairs)
    prev = [-1] * len(pairs)
    for i in range(len(pairs)):
        for j in range(i):
            if pairs[j][1] < pairs[i][1] and best[j] + 1 > best[i]:
                best[i], prev[i] = best[j] + 1, j
    i = best.index(max(best))
    out = []
    while i >= 0:
        out.append(pairs[i])
        i = prev[i]
    return out[::-1]


def read_manual(manual_dir, docs_dir):
    """Index the scanned manuals on ge120.xyz: which page draws a chapter, and
    which page catalogues a card.

    Wants a directory holding the explorer's own data, fetched once:

        for f in manuals data/text/cp06 data/text/cp09 data/text/cp10F; do
            curl -sO --create-dirs --output-dir <dir> https://ge120.xyz/data/$f.json
        done

    Without it the page still works; it just cannot offer the sheets.
    """
    def load(name):
        p = os.path.join(manual_dir, name)
        return json.load(open(p)) if os.path.exists(p) else None

    manuals = load("manuals.json")
    if not manuals:
        return None
    out = {"site": "https://ge120.xyz/", "pdf": "manuals/",
           "vols": {}, "chapters": {}, "cards": {}}
    for m in manuals.get("manuals", []):
        out["vols"][m["slug"]] = {
            "t": m.get("title", ""),
            "n": m.get("pages"),
            # the scan itself: the explorer renders it with pdf.js, but the
            # file is served plainly and with byte ranges, so a frame can open
            # it at one page without pulling the whole binder
            "f": m.get("file", ""),
        }

    # chapters: hand-read pairs win; OCR fills the rest where it stays in order
    cp06 = load("cp06.json")
    if cp06:
        pages = cp06["pages"]
        out["_flat_cp06"] = {n: t.upper() for n, t in enumerate(pages) if t}
        seen = []
        for n, t in enumerate(pages):
            if not t:
                continue
            got = {int(x) for x in CHAPTER_IN_TEXT.findall(t.upper())}
            if len(got) == 1:
                seen.append((n, got.pop()))
        for page, ch in longest_rising(seen):
            out["chapters"][f"{ch:03d}"] = {"p": page, "src": "ocr"}
    for path in sorted(glob.glob(os.path.join(docs_dir, "**/*.md"), recursive=True)):
        for ch, page in CHAP_PAGE.findall(open(path).read()):
            out["chapters"][f"{int(ch):03d}"] = {"p": int(page), "src": "read"}

    # Fill the gaps between anchors, but only across a stretch where the sheets
    # clearly run one page per chapter -- the chapter gap and the page gap
    # agree.  Where they disagree the binder has skipped or doubled up and
    # there is nothing to interpolate along, so those chapters stay unknown
    # rather than get a number that looks authoritative and is not.
    #
    # Held out against the hand-read anchors this predicts 24 of 25 exactly;
    # the miss (chapter 158) drifts four pages, which is why the result is
    # marked "between" and the page is offered as approximate.
    # A page whose own text names one plausible chapter and it is not the one
    # being interpolated onto it is a page that says otherwise; leave it alone.
    # The threshold drops readings like "CHAPTER 3", which are OCR noise off a
    # drawing rather than a sheet title.
    says = {}
    if cp06:
        for n, t in enumerate(cp06["pages"]):
            if not t:
                continue
            got = {int(x) for x in CHAPTER_IN_TEXT.findall(t.upper()) if int(x) >= 20}
            if len(got) == 1:
                says[n] = got.pop()

    anchors = sorted((int(c), v["p"]) for c, v in out["chapters"].items())
    for (c1, p1), (c2, p2) in zip(anchors, anchors[1:]):
        if c2 - c1 > 1 and c2 - c1 == p2 - p1:
            for k in range(1, c2 - c1):
                ch, page = c1 + k, p1 + k
                if says.get(page, ch) != ch:
                    continue
                out["chapters"].setdefault(f"{ch:03d}",
                                           {"p": page, "src": "between"})

    # cards: the catalogues print the part number on the entry's own pages
    for slug in ("cp09", "cp10F"):
        vol = load(f"{slug}.json")
        if not vol:
            continue
        for n, t in enumerate(vol["pages"]):
            if not t:
                continue
            flat = re.sub(r"[^A-Z0-9]", "", t.upper())
            out.setdefault("_flat", {})[(slug, n)] = flat
            if slug == "cp10F":
                out.setdefault("_flat_cp10F", {})[n] = t.upper()
    return out


# "ge->options.E05 = PONT_2N;   /* UCE 464: 32K core, TAB.1's {N,P} row */"
STRAP = re.compile(r"ge->options\.([EF])0(\d)\s*=\s*PONT_(\w+)\s*;"
                   r"(?:\s*/\*(.*?)\*/)?")


def read_straps(root):
    """How this machine is strapped, read from ge_init in ge.c.

    Positions E03/E04/E05 and F03/F04/F05 are the OPTION sockets -- the Atlas
    prints their card_type as ****** (OPZIONE) and gives them two or three pins
    apiece, which are the strap signals the ch.001/ch.002 tables read.  What
    card is in each is a property of this machine, not of the layout, and gemu
    already carries it because the whole emulator is bounded by it.  Reading it
    from ge.c keeps one source of truth: change the strap there and the drawing
    follows.
    """
    path = os.path.join(root, "ge.c")
    if not os.path.exists(path):
        return {}
    out = {}
    for col, num, kind, note in STRAP.findall(open(path).read()):
        out[f"{col}{int(num)}"] = {
            "pont": "" if kind == "NONE" else f"PONT{kind}",
            "note": re.sub(r"\s+", " ", note).strip(),
        }
    return out


def index_straps(manual, straps):
    """Where the strap tables and the card's own drawing are printed."""
    if not straps:
        return
    flat6 = manual.get("_flat_cp06") or {}
    flat10 = manual.get("_flat_cp10F") or {}
    pages = {}
    # TAB.1 memory capacity: the only page printing the E05/F05 signal VAMC2
    hit = sorted(n for n, t in flat6.items() if re.search(r"VAMC\s*2", t))
    if hit:
        pages["capacity"] = {"v": "cp06", "p": hit[0],
                             "t": "TAB.1 — memory capacity (E05/F05)"}
    # version / cycle period / interruptions / loading, the ch.002 tables
    ch2 = manual["chapters"].get("002")
    if ch2:
        pages["version"] = {"v": "cp06", "p": ch2["p"],
                            "t": "TAB.1–3 — version, cycle, interrupts, loading"}
    # the strap card itself, by the drawing number ge.c cites
    hit = sorted(n for n, t in flat10.items() if re.search(r"015\s*433\s*91", t))
    if hit:
        pages["card"] = {"v": "cp10F", "p": hit[0],
                         "t": "PONT card — bridge recipe (dwg 015 433 91)"}
    manual["straps"] = {"at": straps, "pages": pages}


def read_tier_layout(atlas_dir):
    """The restoration workbook's layout, exported by import-sheet.py.

    One entry per tier per occupied slot.  This is the layout as checked
    against the real machine, so where it and the Atlas CSVs disagree it wins:
    the CSVs are OCR of the factory drawing and differ from it at eleven
    positions, and they leave the OPZIONE sockets blank where the workbook
    says what is fitted.
    """
    path = os.path.join(atlas_dir, "tier_layout.tsv")
    if not os.path.exists(path):
        return {}
    out = {}
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        parts = line.rstrip("\n").split("\t")
        if len(parts) < 4:
            continue
        tier, slot, kind, value = parts[0], parts[1], parts[2], parts[3]
        out[(tier, int(slot))] = {"kind": kind, "value": value}
    return out


def apply_tier_layout(cards, layout):
    """Set each position's part number from the workbook.

    A tier cell covers both of its rows -- that is what a dual-slot board is --
    so both rows take the code, which is also what makes the front view draw
    them as one board.  Three-character values are part-number tails (53F ->
    0610053F); anything else (CONN, FILTRI, a pair of full part numbers) is
    carried through as it is written.
    """
    fixed = disagreed = filled = 0
    for (tier, slot), cell in layout.items():
        if cell["kind"] == "MEM470":
            continue
        value = cell["value"]
        if re.fullmatch(r"[0-9A-Z]{3}", value):
            code, label = "06100" + value, ""
        else:
            code, label = "", value
        for row in tier:
            card = cards.get(row, {}).get(slot)
            if card is None:
                continue
            card["sheet"] = value
            if not code:
                card["sheetLabel"] = label
                continue
            if not card.get("code"):
                filled += 1
            elif card["code"] != code:
                disagreed += 1
                card["wasCode"] = card["code"]
            else:
                fixed += 1
            card["code"] = code
    return fixed, disagreed, filled


def pin_shape(pins):
    """A position's wiring with the numbers masked out.

    Two sockets carrying the same card are wired the same way and differ only
    in which bit or which stack they serve, so LZ861/LZ841 and LZ461/LZ441 have
    one shape.  That makes the shape a fingerprint for the card type.
    """
    return tuple(re.sub(r"\d", "#", p) if p else "" for p in pins)


def guess_option_card(cards, pins, row, slot):
    """Name the card in an option socket from the layout, not from a guess.

    The Atlas leaves card_code and card_type blank at the OPZIONE sockets --
    the OCR read the slot number into the type cell -- but it did read their
    pins, and the machine wires a socket the same way whatever build fills it.
    So: match the socket's pin shape against the named positions in its own
    row.  An exact shape match is taken outright; otherwise the best match by
    signal-family overlap is taken when it is decisive.

    Checked against the layout spreadsheet: this returns 0610029W for S/T
    26-29 and 0610030U for S/T 30-31, which is what the sheet's ST row prints
    (29W at 26-29, 30U at 30-31), and 0610002J across Q/R 28-31, which is the
    "eight identical 02J boards at 24-31" hardware-options.md reads off the
    layout sheet.
    """
    mine = pins.get(row, {}).get(slot)
    if not mine:
        return None
    named = [(s, c) for s, c in cards.get(row, {}).items()
             if c.get("code") and not (c.get("type") or "").startswith("*")]
    if not named:
        return None

    want = pin_shape(mine)
    exact = {(c["code"], c["type"]) for s, c in named
             if pin_shape(pins[row][s]) == want}
    if len(exact) == 1:
        code, ctype = exact.pop()
        return {"code": code, "type": ctype, "how": "same wiring"}

    def families(ps):
        out = {}
        for p in ps:
            m = re.match(r"[A-Z]+", p or "")
            if m:
                out[m.group(0)] = out.get(m.group(0), 0) + 1
        return out

    wf = families(mine)
    scored = []
    for s, c in named:
        gf = families(pins[row][s])
        keys = set(wf) | set(gf)
        inter = sum(min(wf.get(k, 0), gf.get(k, 0)) for k in keys)
        union = sum(max(wf.get(k, 0), gf.get(k, 0)) for k in keys)
        if union:
            scored.append((inter / union, c["code"], c["type"]))
    if not scored:
        return None
    scored.sort(reverse=True)
    best = scored[0]
    runner = next((x for x in scored if (x[1], x[2]) != (best[1], best[2])), None)
    # decisive: a strong match, and clearly ahead of the next different card
    if best[0] >= 0.85 and (not runner or best[0] - runner[0] >= 0.1):
        return {"code": best[1], "type": best[2], "how": "matching signals"}
    return None


def index_fitted(manual, cards, pins):
    """Option sockets this machine's build actually fills.

    The memory rows Q to T are laid out for the largest build and marked
    OPZIONE below it, so whether a socket is populated is a function of the
    capacity strap rather than of the layout.  This machine straps the 32K row
    ({E05,F05} = {PONT2N, PONT2P}), and on a 32K build rows Q to T are filled.

    For Q and R the card is known: docs/hardware-options.md reads the layout
    sheet as eight identical 02J AMPL2A boards at positions 24-31, and the
    Atlas shows 24-27 are exactly that, so 28-31 take the same card.  Rows S
    and T have no uniform run to read from -- their neighbours are 1NTE23 and
    PATE24 -- so those are recorded as fitted with the card left unnamed
    rather than guessed from the position next door.
    """
    straps = (manual.get("straps") or {}).get("at") or {}
    cap = (straps.get("E5", {}).get("pont"), straps.get("F5", {}).get("pont"))
    if cap != ("PONT2N", "PONT2P"):
        return                                   # not the 32K row; leave them
    fitted = {}
    why = ("fitted on the GE-130 / 32K build — the memory rows are laid out for "
           "the largest machine and marked OPZIONE below it")
    for row in ("Q", "R", "S", "T"):
        for slot, c in cards.get(row, {}).items():
            if not (c.get("type") or "").startswith("*"):
                continue
            entry = {"why": why}
            named = guess_option_card(cards, pins, row, slot)
            if named:
                entry.update(named)
            fitted[f"{row}{slot}"] = entry
    if fitted:
        manual["fitted"] = fitted


def index_types(manual, types, docs_dir):
    """Pages for card types that carry no part number at all.

    COCA is the case this exists for: thirty positions along the edges of rows
    G to N whose card_code is blank on every layout page, because a COCA is not
    a circuit card -- it is the connector the cable loom lands on, and it is
    drawn on cp06's connector sheets rather than catalogued in cp09/cp10F.
    """
    flat = manual.get("_flat_cp06") or {}
    if not flat:
        return
    for name in sorted(types):
        if len(name) < 4:
            continue
        pages = sorted(n for n, txt in flat.items()
                       if re.search(r"\b" + re.escape(name) + r"\b", txt))
        if pages:
            manual.setdefault("types", {})[name] = {
                "v": "cp06", "p": pages[0], "n": len(pages)}


def index_cards(manual, codes):
    """code -> {v, p}: the first catalogue page printing that part number."""
    flat = manual.pop("_flat", {})
    if not flat:
        return
    for code in sorted(codes):
        norm = re.sub(r"[^A-Z0-9]", "", code.upper())
        if len(norm) < 6:
            continue
        for cand in variants(norm):
            hit = next(((s, n) for (s, n), txt in sorted(flat.items())
                        if cand in txt), None)
            if hit:
                entry = {"v": hit[0], "p": hit[1]}
                if cand != norm:
                    entry["as"] = cand
                manual["cards"][code] = entry
                break


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--atlas", default=os.path.join(here, "../../../atlas"))
    ap.add_argument("--docs", default=os.path.join(here, "../../docs/signals"))
    ap.add_argument("--manual", default=os.path.join(here, "../../../manual-cache"),
                    help="directory of ge120.xyz data JSONs (see read_manual)")
    ap.add_argument("-o", "--out", default=os.path.join(here, "backplane.js"))
    args = ap.parse_args()

    cards, pins = read_atlas(args.atlas)
    layout = read_tier_layout(args.atlas)
    if layout:
        agree, differ, filled = apply_tier_layout(cards, layout)
        print(f"layout: workbook agrees with the Atlas at {agree} positions, "
              f"overrides {differ}, fills {filled} the Atlas left blank")
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

    # Where the two MEM 470 boxes stand, given by the machine's owner: slots
    # 39 down to 32, and 9 down to 2, across the memory rows.  The workbook's
    # tan fill is a slot wider at each outer end (40..32 and 10..2) because it
    # marks the bay rather than the box, so the fill is used only to find which
    # rows are memory rows, not how wide the box is.
    MEM_BAYS = [(32, 39), (2, 9)]
    mem_fill = sorted({(t, sl) for (t, sl), c in layout.items()
                       if c["kind"] == "MEM470"})
    mem_tiers = sorted({t for t, _ in mem_fill})
    mem = [(t, sl) for t in mem_tiers
           for lo, hi in MEM_BAYS for sl in range(lo, hi + 1)]
    if mem_fill:
        print(f"memory: two MEM 470 boxes over "
              f"{', '.join(f'{hi}-{lo}' for lo, hi in MEM_BAYS)} "
              f"({', '.join(str(hi - lo + 1) for lo, hi in MEM_BAYS)} slots each) "
              f"in tiers {'/'.join(mem_tiers)}")

    manual = read_manual(args.manual, args.docs)
    if manual:
        codes = {c["code"] for row in ROWS for c in cards[row].values()
                 if c.get("code")}
        # a type that never carries a part number is not a catalogued card
        coded, uncoded = set(), set()
        for row in ROWS:
            for c in cards[row].values():
                if c.get("type") and c["type"] != "//////":
                    (coded if c.get("code") else uncoded).add(c["type"])
        index_types(manual, uncoded - coded, args.docs)
        index_straps(manual, read_straps(os.path.join(here, "../..")))
        index_fitted(manual, cards, pins)
        index_cards(manual, codes)
        manual.pop("_flat_cp06", None)
        manual.pop("_flat_cp10F", None)

    data = {
        "rows": ROWS,
        "tiers": [list(t) for t in TIERS],
        "slots": SLOTS,
        "pinCount": PINS,
        "cards": cards,
        "pins": pins,
        "sigs": used,
        "docPins": located,
        # the two core stores, as slot runs per tier
        "mem": sorted({sl for _, sl in mem}),
        "memTiers": sorted({t for t, _ in mem}),
    }
    if manual:
        data["manual"] = manual
        by = {}
        for c in manual["chapters"].values():
            by[c["src"]] = by.get(c["src"], 0) + 1
        print(f"manual: {len(manual['chapters'])} chapters located "
              f"({by.get('read', 0)} read off the page, {by.get('ocr', 0)} from "
              f"the OCR layer, {by.get('between', 0)} interpolated between "
              f"anchors), {len(manual['cards'])} card codes found in a catalogue")
        st = manual.get("straps", {})
        if st:
            fitted = [k for k, v in st["at"].items() if v["pont"]]
            print(f"straps: {len(st['at'])} option sockets read from ge.c "
                  f"({', '.join(k + '=' + st['at'][k]['pont'] for k in sorted(fitted))}"
                  f"; the rest empty), tables at "
                  f"{', '.join(f'{v[chr(118)]} p{v[chr(112)]}' for v in st['pages'].values())}")
        fit = manual.get("fitted") or {}
        if fit:
            named = sum(1 for v in fit.values() if v.get("code"))
            kinds = {}
            for v in fit.values():
                if v.get("code"):
                    kinds[v["code"]] = kinds.get(v["code"], 0) + 1
            print(f"fitted: {len(fit)} option sockets in rows Q-T populated by "
                  f"the 32K build, {named} named from the layout "
                  f"({', '.join(f'{k}x{n}' for k, n in sorted(kinds.items()))})")
    else:
        print(f"manual: no cache at {args.manual} — the sheets will not be "
              f"offered (see read_manual for the fetch)")

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
