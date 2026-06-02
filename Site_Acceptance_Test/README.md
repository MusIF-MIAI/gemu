# Site Acceptance Test Decks

This directory vendors the original `.cap` card-deck captures into the `gemu`
repository so the acceptance-test media used during GE-120/130 restoration
work can be versioned and referenced in-tree.

Only the original captured `.cap` decks are copied here. Derived artifacts such
as `funktionalcpu.bin`, `funktionalcpu.s`, and the synthetic
`integrated-funktional-memtest.cap` are intentionally excluded.

## Provenance

The `.cap` files preserved here were extracted from real punched cards using a
restored and modernized **Burroughs punched card reader**. The capture hardware
and firmware are documented in:

- `software/burroughs-card-reader/`
- <https://github.com/MusIF-MIAI/Burroughs-card-reader>

The deck ordering and loading notes below come from the local GE manuals:

- `CPU/GE 120 CENTRAL PROCESSOR [1].pdf`
  - section `2.2 Card deck configuration`
  - the advised test sequence at OCR lines `926..941`
- `CRZ/CRZ 105 -111-112-120-121 [1].pdf`
  - `CONTROL PROGRAM CR` preparation at OCR lines `36466..36546`
  - `SITE ACCEPTANCE TEST LS 600 / SEQUENCER PROGRAM` preparation at OCR lines
    `44076..44503`

The manuals attribute the software listings to **GENERAL ELECTRIC INFORMATION
SYSTEMS ITALIA**. The scanned copies inspected here do not carry a separate
modern license grant; they preserve the original program headings and sheet
credits such as:

- `GENERAL ELECTRIC INFORMATION SYSTEMS ITALIA`
- `L. Coreggia, 15 nov. 66` on `Reading test on channel A 1591011`
- `A. Chinni, Oct. 30, 69` on `SEQUENCER PROGRAM`
- `Pregnana, Sep 15, 1969` on `130 CPU ISOLATION TEST`

Treat these captures as archival material reproduced for restoration,
documentation, and emulator-validation work.

## Manual-Documented Test Order

`CPU[1]` says the advised order of tests is:

1. `C.P.U. test`
2. `Card reader test`
3. `Printer test`
4. `Reader-printer overlapping test`
5. `Punch test`
6. `Magnetic Tape Handler controller test`
7. `Eventual peripheral connected`
8. `LP300B test`

That is the authoritative **test sequence**. It is not a one-file-per-bullet
table in the manuals, so the filename mapping below is split by confidence.

## Capture Set

These original captures are preserved here:

- `funktionalcpu.cap`
- `semi-manuale-test-i5i.cap`
- `isolat-dsu-erganz-cpu.cap`
- `isolationcpu01.cap`
- `isolationcpu02.cap`
- `isolationcpu03.cap`
- `auto-kontr-i51.cap`
- `sat-ls600.cap`
- `reading-test-chain-01a.cap`
- `ls600-controller-test.cap`
- `ls600-transcoder-test.cap`
- `ls600-doe.cap`
- `control-program-cr.cap`
- `control-program-cr-copia.cap`
- `printermechanicaltest.cap`

## Filename Mapping

### Confirmed from divider labels, deck titles, or manual headings

- `funktionalcpu.cap`
  - `CPU test`
  - the functional CPU deck
- `isolat-dsu-erganz-cpu.cap`
  - `CPU isolation / DSU supplement to CPU`
  - companion CPU-memory/isolation deck
- `reading-test-chain-01a.cap`
  - `Card reader test`
  - matches `Reading test on channel A 1591011`
- `printermechanicaltest.cap`
  - `Printer test`
- `ls600-controller-test.cap`
  - `LS600 Controller Test 1592070`
- `ls600-transcoder-test.cap`
  - `LS600 Transcoder IBM Test 1592050`
- `ls600-doe.cap`
  - `D.O.E. LS600 Test`
- `sat-ls600.cap`
  - `SITE ACCEPTANCE TEST LS 600`
  - specifically the `SEQUENCER PROGRAM` deck used ahead of another diagnostic

### Present in the box/capture set, but exact SAT-sequence role still not proven

- `semi-manuale-test-i5i.cap`
- `auto-kontr-i51.cap`
- `control-program-cr.cap`
- `control-program-cr-copia.cap`
- `isolationcpu01.cap`
- `isolationcpu02.cap`
- `isolationcpu03.cap`

Notes:

- `control-program-cr.cap` is documented in the CRZ manual as a **control
  program** placed before another CR diagnostic deck after deck surgery; that is
  not the same thing as proving it is the `reader-printer overlapping test`
  named in `CPU[1]`.
- `isolationcpu01/02/03.cap` appear to be earlier or partial captures of the
  CPU isolation family; `isolat-dsu-erganz-cpu.cap` is the stronger candidate
  for the complete divider-labelled CPU supplement deck.

## How The Manuals Say To Load Them

### Standalone diagnostic decks

For a standalone diagnostic deck, the physical procedure is the usual GE
console flow: select the proper load unit, press `CLEAR`, put the deck on the
reader, set the device to `OPERATE`, then `LOAD` + `START`.

In `gemu`, the practical equivalents are:

- fast scatter-family path:
  - `./ge Site_Acceptance_Test/<deck>.cap`
- authentic reader/bootstrap path:
  - `./ge --deck Site_Acceptance_Test/<deck>.cap`

Current emulator support is not identical for every family:

- scatter-family decks such as `funktionalcpu.cap`,
  `printermechanicaltest.cap`, and `control-program-cr.cap` load best through
  the positional `.cap` path
- isolation/SMAC-family decks need their own family-aware handling and are not
  yet generally executable end-to-end in `gemu`

### `CONTROL PROGRAM CR`

The CRZ manual says to prepare a diagnostic this way:

1. In the `CONTROL PROGRAM CR` deck, keep the title card, then choose the one
   loader card matching the attached reader, discarding the other three loaders.
2. Find the target diagnostic's summary card and move it to the end of that
   diagnostic deck.
3. Put `CONTROL PROGRAM CR` before the diagnostic to be checked.
4. Load the resulting combined deck on the reader.

So `control-program-cr.cap` is not just "run this alone"; it is a
**front-of-deck utility program** for checking another CR diagnostic.

### `SITE ACCEPTANCE TEST LS 600`

The LS600 SAT manual documents a two-deck arrangement:

1. Prepare the `SEQUENCER PROGRAM` deck:
   - remove the first card (title)
   - remove the last card (summary)
   - among the first four remaining cards, keep only the loader that matches the
     actual reader type
2. Prepare the diagnostic deck to be checked:
   - remove the first card
   - remove the last card
   - remove cards after the first that do not belong to the regular numbering
3. Put the `SEQUENCER PROGRAM` deck first, followed by the diagnostic deck.
4. Load the combined deck with the normal console `CLEAR` -> `LOAD` -> `START`
   sequence.

In this capture set, `sat-ls600.cap` is that sequencer deck; it is intended to
run **before** one of the LS600 diagnostic decks, not by itself.

## Suggested Working Order In This Repo

Given the evidence above, the safest run order for restoration work is:

1. `funktionalcpu.cap`
2. `isolat-dsu-erganz-cpu.cap`
3. `reading-test-chain-01a.cap`
4. `printermechanicaltest.cap`
5. `control-program-cr.cap` plus the CR diagnostic it is intended to check
6. `sat-ls600.cap` followed by one of:
   - `ls600-controller-test.cap`
   - `ls600-transcoder-test.cap`
   - `ls600-doe.cap`

Everything after step 4 still needs more tool support in `gemu` to reproduce
the manual deck surgery and multi-deck chaining automatically. This README
records the documented order and the preserved media so that work can proceed
from the original captures instead of ad-hoc slices.
