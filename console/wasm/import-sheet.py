#!/usr/bin/env python3
"""Export the restoration workbook's card layout to a TSV the generator reads.

    ./import-sheet.py ~/Downloads/GE-120\\ restoration.xlsx

The "SCHEDE CPU" sheet is the layout as checked against the real machine: one
cell per TIER per column (AB, CD, ... ST down the side; slots 40 to 1 across),
holding the last three characters of the card's part number -- 53F for
0610053F.  It is the authority the Atlas CSVs are not: the CSVs are OCR of the
factory layout drawing and disagree with it at eleven positions, and they leave
the OPZIONE sockets blank where the workbook says what is actually fitted.

Two things live in the formatting rather than the values, and both are read
here so the TSV carries everything: the tan fill marks the footprint of the two
MEM470 core stores, and a cell holding two part numbers on separate lines is a
pair of cards in one position.

Nothing here depends on a spreadsheet library -- an xlsx is a zip of XML.
"""

import os
import re
import sys
import zipfile
import xml.etree.ElementTree as ET

NS = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
TIER_ROWS = {2: "AB", 3: "CD", 4: "EF", 5: "GH", 6: "IL",
             7: "MN", 8: "OP", 9: "QR", 10: "ST"}
MEM_FILL = "FFFFD966"          # the tan the workbook fills the core stores with


def col_number(letters):
    n = 0
    for ch in letters:
        n = n * 26 + ord(ch) - 64
    return n


def read_sheet(path):
    z = zipfile.ZipFile(path)
    shared = []
    if "xl/sharedStrings.xml" in z.namelist():
        for si in ET.fromstring(z.read("xl/sharedStrings.xml")):
            shared.append("".join(t.text or "" for t in si.iter(NS + "t")))
    styles = ET.fromstring(z.read("xl/styles.xml"))
    fills = []
    for f in styles.find(NS + "fills"):
        pf = f.find(NS + "patternFill")
        fg = pf.find(NS + "fgColor") if pf is not None else None
        fills.append(fg.get("rgb") if fg is not None else None)
    xfs = [int(x.get("fillId", "0")) for x in styles.find(NS + "cellXfs")]

    ws = ET.fromstring(z.read("xl/worksheets/sheet1.xml"))
    value, fill = {}, {}
    for c in ws.iter(NS + "c"):
        m = re.match(r"([A-Z]+)(\d+)", c.get("r"))
        row, col = int(m.group(2)), col_number(m.group(1))
        v = c.find(NS + "v")
        inline = c.find(NS + "is")
        if inline is not None:
            text = "".join(t.text or "" for t in inline.iter(NS + "t"))
        elif v is None:
            text = ""
        elif c.get("t") == "s":
            text = shared[int(v.text)]
        else:
            text = v.text or ""
        value[(row, col)] = text.strip()
        if c.get("s"):
            fill[(row, col)] = fills[xfs[int(c.get("s"))]]

    slots = {}
    for (row, col), text in value.items():
        if row == 1:
            try:
                slots[col] = int(float(text))
            except ValueError:
                pass
    return value, fill, slots


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.expanduser("~/Downloads/GE-120 restoration.xlsx")
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[2] if len(sys.argv) > 2 else \
        os.path.join(here, "../../../atlas/tier_layout.tsv")
    if not os.path.exists(src):
        sys.exit(f"no workbook at {src}")

    value, fill, slots = read_sheet(src)
    lines, counts = [], {}
    for row, tier in TIER_ROWS.items():
        for col, slot in sorted(slots.items(), key=lambda kv: -kv[1]):
            text = value.get((row, col), "")
            kind = ""
            if fill.get((row, col)) == MEM_FILL:
                kind = "MEM470"          # inside a core store's footprint
            if not text and not kind:
                continue
            # a cell with two part numbers is two cards in the one position
            parts = [p.strip() for p in re.split(r"[\n/]+", text) if p.strip()]
            lines.append(f"{tier}\t{slot}\t{kind}\t{' '.join(parts)}")
            counts[kind or "card"] = counts.get(kind or "card", 0) + 1

    with open(out, "w") as fh:
        fh.write("# Card layout from the GE-120 restoration workbook, sheet\n"
                 "# \"SCHEDE CPU\" -- the layout as checked against the real\n"
                 "# machine. One line per tier per occupied slot:\n"
                 "#   tier <TAB> slot <TAB> kind <TAB> value\n"
                 "# kind is MEM470 where the workbook's tan fill marks a core\n"
                 "# store's footprint, empty otherwise. value is the part\n"
                 "# number's last three characters (53F = 0610053F), or a word\n"
                 "# like CONN/FILTRI, or full part numbers space-separated.\n"
                 "# Written by console/wasm/import-sheet.py -- do not hand-edit.\n")
        fh.write("\n".join(lines) + "\n")
    print(f"{len(lines)} cells -> {out}")
    for k, n in sorted(counts.items()):
        print(f"    {k}: {n}")


if __name__ == "__main__":
    main()
