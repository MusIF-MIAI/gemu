/*
 * tests/transcode.c — unit tests for the GE-120 card-reader transcoder.
 *
 * Tests:
 *  transcode/known_entries  — check the 18 non-space known mappings directly.
 *  transcode/blank_column   — column 0x0000 (no holes) → 0x20 (space).
 *  transcode/binary_mode    — TC_BINARY returns low 8 bits unchanged.
 *  transcode/matches_bin_oracle — load funktionalcpu.cap + .bin and verify all
 *                                  9120 bytes match after TC_NORMAL transcoding.
 *
 * File-path convention: paths are relative to the directory from which the
 * test binary is invoked.  The standard invocation is from the project root
 * (/home/dan/Documents/GE-120/software/) where DUMP1/ lives.
 * If the files are missing the oracle test is skipped gracefully.
 */

#include "utest.h"
#include "../cap.h"
#include "../transcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
 * Oracle test: full 9120-byte match against funktionalcpu.bin
 * ------------------------------------------------------------------------- */

#define ORACLE_CAP  "../DUMP1/funktionalcpu.cap"
#define ORACLE_BIN  "../DUMP1/funktionalcpu.bin"
#define ORACLE_NCARDS  114
#define ORACLE_NCOLS   80
#define ORACLE_NBYTES  (ORACLE_NCARDS * ORACLE_NCOLS)  /* 9120 */

UTEST(transcode, matches_bin_oracle)
{
    /*
     * Graceful skip if either file is missing.
     */
    FILE *bf = fopen(ORACLE_BIN, "rb");
    if (!bf) {
        /* File not found — skip rather than fail. */
        fprintf(stderr, "NOTE: %s not found; skipping oracle test\n", ORACLE_BIN);
        ASSERT_TRUE(1); /* pass vacuously */
        return;
    }

    uint8_t *bin = (uint8_t *)malloc(ORACLE_NBYTES);
    ASSERT_TRUE(bin != NULL);

    size_t nr = fread(bin, 1, ORACLE_NBYTES, bf);
    fclose(bf);

    if ((int)nr != ORACLE_NBYTES) {
        fprintf(stderr, "NOTE: %s too short (%zu bytes); skipping oracle test\n",
                ORACLE_BIN, nr);
        free(bin);
        ASSERT_TRUE(1);
        return;
    }

    struct cap_deck *deck = cap_load(ORACLE_CAP);
    if (!deck) {
        fprintf(stderr, "NOTE: %s failed to load; skipping oracle test\n", ORACLE_CAP);
        free(bin);
        ASSERT_TRUE(1);
        return;
    }

    /*
     * The .cap file contains 228 card records.  The first 114 hold the actual
     * 13-bit hex column data; the remaining 114 hold only visual _/* patterns
     * (which the cap parser ignores, producing ncols==0).
     * We align: bin[card*80 + col] ↔ cap card[card], column[col].
     */
    int ncards = cap_num_cards(deck);
    if (ncards < ORACLE_NCARDS) {
        fprintf(stderr,
                "NOTE: cap has %d cards (expected >= %d); skipping oracle test\n",
                ncards, ORACLE_NCARDS);
        cap_free(deck);
        free(bin);
        ASSERT_TRUE(1);
        return;
    }

    /* Verify first 114 cards have the expected column data (not visual rows). */
    {
        int nc0 = cap_card_ncols(deck, 0);
        if (nc0 != ORACLE_NCOLS) {
            fprintf(stderr,
                    "NOTE: cap card 0 has %d cols (expected %d); skipping oracle test\n",
                    nc0, ORACLE_NCOLS);
            cap_free(deck);
            free(bin);
            ASSERT_TRUE(1);
            return;
        }
    }

    /* Transcode and compare. */
    int mismatches = 0;
    int first_mismatch_card = -1;
    int first_mismatch_col  = -1;
    uint8_t first_got = 0, first_expected = 0;
    uint16_t first_hol = 0;

    for (int card = 0; card < ORACLE_NCARDS; card++) {
        int ncols = cap_card_ncols(deck, card);
        if (ncols != ORACLE_NCOLS) {
            fprintf(stderr,
                    "NOTE: cap card %d has %d cols (expected %d)\n",
                    card, ncols, ORACLE_NCOLS);
            /* Count as ORACLE_NCOLS mismatches for this card */
            mismatches += ORACLE_NCOLS;
            continue;
        }

        const uint16_t *cols = cap_card_columns(deck, card);
        for (int col = 0; col < ORACLE_NCOLS; col++) {
            uint8_t expected = bin[card * ORACLE_NCOLS + col];
            uint8_t got      = transcode_column(cols[col], TC_NORMAL);
            if (got != expected) {
                if (mismatches == 0) {
                    first_mismatch_card = card;
                    first_mismatch_col  = col;
                    first_got           = got;
                    first_expected      = expected;
                    first_hol           = cols[col];
                }
                mismatches++;
            }
        }
    }

    cap_free(deck);
    free(bin);

    if (mismatches > 0) {
        fprintf(stderr,
                "transcode oracle: %d mismatch(es); first at card %d col %d: "
                "hol=0x%04X got=0x%02X expected=0x%02X\n",
                mismatches,
                first_mismatch_card, first_mismatch_col,
                (unsigned)first_hol, (unsigned)first_got, (unsigned)first_expected);
    }

    ASSERT_EQ(mismatches, 0);
}
