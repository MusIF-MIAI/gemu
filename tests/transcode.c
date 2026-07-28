/*
 * tests/transcode.c — unit tests for the GE-120 card-reader transcoder.
 *
 * Tests:
 *  transcode/known_entries  — check the 18 non-space known mappings directly.
 *  transcode/blank_column   — column 0x0000 (no holes) → 0x20 (space).
 *  transcode/binary_mode    — TC_BINARY returns low 8 bits unchanged.
 *  transcode/hex_and_holeart_agree — a .cap carries the same deck twice, as a
 *                                  hex column dump and as ASCII hole art. Parse
 *                                  the hole art here, independently of cap.c,
 *                                  and require every one of the 114x80 columns
 *                                  to agree with the hex section.
 *
 * File-path convention: paths are relative to the directory from which the
 * test binary is invoked (the project root, where ../DUMP1 lives).
 * If the deck is missing the oracle test is skipped gracefully.
 */

#include "utest.h"
#include "../cap.h"
#include "../transcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Smoke-test: known non-space entries
 * ------------------------------------------------------------------------- */

UTEST(transcode, known_entries)
{
    /* 18 non-space entries derived from the oracle. */
    struct { uint16_t hol; uint8_t ge; } known[] = {
        { 0x0001, 0xF0 }, /* digit 0 */
        { 0x0002, 0xF1 }, /* digit 1 */
        { 0x0004, 0xF2 }, /* digit 2 */
        { 0x0008, 0xF3 }, /* digit 3 */
        { 0x0010, 0xF4 }, /* digit 4 */
        { 0x0020, 0xF5 }, /* digit 5 */
        { 0x0040, 0xF6 }, /* digit 6 */
        { 0x0080, 0xF7 }, /* digit 7 */
        { 0x0100, 0xF8 }, /* digit 8 */
        { 0x0200, 0xF9 }, /* digit 9 */
        { 0x0003, 0x4C }, /* '<' */
        { 0x0005, 0x7C }, /* '@' */
        { 0x0009, 0x7B }, /* '#' */
        { 0x0041, 0xE7 }, /* 'X' */
        { 0x0081, 0xE8 }, /* 'Y' */
        { 0x0101, 0x6B }, /* ',' */
        { 0x0201, 0xBA }, /* special (0xBA) */
        { 0x1001, 0x4E }, /* '+' */
    };

    for (size_t i = 0; i < sizeof(known)/sizeof(known[0]); i++) {
        uint8_t got = transcode_column(known[i].hol, TC_NORMAL);
        ASSERT_EQ((int)got, (int)known[i].ge);
    }
}

/* -------------------------------------------------------------------------
 * Smoke-test: blank column
 * ------------------------------------------------------------------------- */

UTEST(transcode, blank_column)
{
    /* A blank column (no holes) must produce a space character (0x20). */
    ASSERT_EQ((int)transcode_column(0x0000, TC_NORMAL), 0x20);
}

/* -------------------------------------------------------------------------
 * Smoke-test: unobserved column → space
 * ------------------------------------------------------------------------- */

UTEST(transcode, unobserved_defaults_to_space)
{
    /*
     * Pick a value that was never in the corpus — for example 0x0FFF.
     * All unobserved 13-bit values should yield 0x20.
     */
    ASSERT_EQ((int)transcode_column(0x0FFF, TC_NORMAL), 0x20);
    ASSERT_EQ((int)transcode_column(0x0006, TC_NORMAL), 0x20);
}

/* -------------------------------------------------------------------------
 * Smoke-test: binary mode
 * ------------------------------------------------------------------------- */

UTEST(transcode, binary_mode)
{
    /* TC_BINARY must return the low 8 bits of the column value unchanged. */
    ASSERT_EQ((int)transcode_column(0x0042, TC_BINARY), 0x42);
    ASSERT_EQ((int)transcode_column(0x10FF, TC_BINARY), 0xFF);
    ASSERT_EQ((int)transcode_column(0x0000, TC_BINARY), 0x00);
    ASSERT_EQ((int)transcode_column(0x1234, TC_BINARY), 0x34);
}

/* -------------------------------------------------------------------------
 * Bits above 12 are masked in TC_NORMAL
 * ------------------------------------------------------------------------- */

UTEST(transcode, high_bits_ignored_normal)
{
    /*
     * Bits 13..15 of the uint16_t input should be ignored in TC_NORMAL.
     * 0x0001 and 0x8001 both produce 0xF0.
     */
    ASSERT_EQ((int)transcode_column(0x0001, TC_NORMAL),
              (int)transcode_column(0x6001, TC_NORMAL));
}


/* -------------------------------------------------------------------------
 * Oracle: the deck checks itself
 *
 * A .cap capture holds the same deck twice over -- once as 80 four-hex-digit
 * column samples per card, once as 12 rows of ASCII hole art. cap.c reads only
 * the hex half. Parsing the hole art here, by hand, gives a second opinion on
 * every column the transcoder is ever handed.
 *
 * (This replaces an oracle that compared against ../DUMP1/funktionalcpu.bin.
 * That fixture is a 7264-byte scatter image, not a 9120-byte card dump, so the
 * comparison had been skipping itself and passing vacuously. .bin is gone now.)
 * ------------------------------------------------------------------------- */

#define ORACLE_CAP     "../DUMP1/funktionalcpu.cap"
#define ORACLE_NCARDS  114
#define ORACLE_NCOLS   80

/* hole-art row order is the physical one, top to bottom: 12, 11, 0, 1 ... 9.
 * A cap value's bit b is card row b for b in {0-9, 11, 12}; bit 10 is
 * structurally absent (GPIO 10 was never wired). */
static const int holeart_row_bit[12] = { 12, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

UTEST(transcode, hex_and_holeart_agree)
{
    struct cap_deck *deck;
    FILE *f;
    char line[512];
    /* [card][col] accumulated from the hole art. */
    static uint16_t art[ORACLE_NCARDS][ORACLE_NCOLS];
    int seen[ORACLE_NCARDS];
    int card = -1, row = 0;
    int cards_with_art = 0;
    int mismatches = 0;

    f = fopen(ORACLE_CAP, "rb");
    if (!f) {
        fprintf(stderr, "NOTE: %s not found; skipping oracle test\n", ORACLE_CAP);
        return;
    }

    memset(art, 0, sizeof art);
    memset(seen, 0, sizeof seen);

    while (fgets(line, sizeof line, f)) {
        int n;
        unsigned long num;
        char *p = line;

        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, "Card n.", 7) == 0) {
            num = strtoul(p + 7, NULL, 10);
            /* Card numbers are 1-based; the hex section comes first, so the
             * second sighting of a number is its hole art. */
            card = (num >= 1 && num <= ORACLE_NCARDS) ? (int)(num - 1) : -1;
            row = 0;
            continue;
        }
        if (card < 0)
            continue;

        /* A hole-art row is exactly 80 characters of '*' (hole) and '_'. */
        n = 0;
        while (p[n] == '*' || p[n] == '_')
            n++;
        if (n != ORACLE_NCOLS || (p[n] != '\n' && p[n] != '\r' && p[n] != '\0'))
            continue;
        if (row >= 12)
            continue;

        for (int c = 0; c < ORACLE_NCOLS; c++)
            if (p[c] == '*')
                art[card][c] |= (uint16_t)(1u << holeart_row_bit[row]);
        if (row == 0)
            cards_with_art++;
        seen[card] = 1;
        row++;
    }
    fclose(f);

    if (cards_with_art < ORACLE_NCARDS) {
        fprintf(stderr, "NOTE: %s has hole art for %d/%d cards; skipping\n",
                ORACLE_CAP, cards_with_art, ORACLE_NCARDS);
        return;
    }

    deck = cap_load(ORACLE_CAP);
    ASSERT_TRUE(deck != NULL);
    ASSERT_GE(cap_num_cards(deck), ORACLE_NCARDS);

    for (int i = 0; i < ORACLE_NCARDS; i++) {
        const uint16_t *cols = cap_card_columns(deck, i);
        ASSERT_TRUE(seen[i]);
        ASSERT_EQ(cap_card_ncols(deck, i), ORACLE_NCOLS);
        for (int c = 0; c < ORACLE_NCOLS; c++) {
            if (cols[c] != art[i][c]) {
                if (mismatches == 0)
                    fprintf(stderr,
                            "hole art disagrees at card %d col %d: "
                            "hex=0x%04X art=0x%04X\n",
                            i, c, cols[c], art[i][c]);
                mismatches++;
            }
        }
    }

    cap_free(deck);
    ASSERT_EQ(mismatches, 0);
}
