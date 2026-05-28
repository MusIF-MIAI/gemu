/*
 * tests/cardreader.c - Unit tests for the connector-2 card-reader peripheral.
 *
 * Tests use the utest.h framework.  No UTEST_MAIN here; the test runner
 * main() is provided by tests/main.c (auto-discovery).
 *
 * Two test suites:
 *
 *  cardreader.synthetic_4byte
 *    Writes a tiny synthetic .cap deck to /tmp that encodes exactly 4 bytes
 *    (0xAB, 0xCD, 0xEF, 0xAA) in TC_BINARY mode, then runs the full load
 *    sequence and checks that the machine reaches state 0xe3 (alpha) with
 *    the correct bytes packed into mem[].  This mirrors the manual hand-
 *    feeding in tests/initial-load.c, but done automatically by the
 *    cardreader peripheral.
 *
 *  cardreader.funktionalcpu_first_card
 *    Points at ../DUMP1/funktionalcpu.cap (the real oracle deck) and loads
 *    the first card using TC_NORMAL transcoding.  Checks that the machine
 *    reaches state 0xe3 and that mem[0..1] match the oracle bytes from
 *    transcode.c's known-good table.
 *
 * Feeding trigger/cadence implemented in cardreader.c:
 *   on_clock (TO00) is called once per machine cycle.
 *   - CR_IDLE:     if lu08==0, call reader_setup_to_send and go to CR_PRESENTED.
 *   - CR_PRESENTED: call reader_clear_sending, go to CR_IDLE, then immediately
 *                   attempt to present the next byte if lu08 is still 0.
 *   This mirrors the exact pattern from initial-load.c:
 *     reader_setup_to_send(...)   <- before b8/b9 cycle
 *     ge_run_cycle()              <- machine reads nibble
 *     reader_clear_sending(...)   <- before b1 cycle
 *     ge_run_cycle()              <- machine packs nibble
 */

#include "utest.h"
#include "../ge.h"
#include "../cardreader.h"
#include "../bit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/*
 * write_synthetic_cap - write a minimal .cap file to path with the given
 * column values (TC_BINARY passthrough: col[i] & 0xFF == byte[i]).
 *
 * All values are written as 4-hex-digit tokens on one line following the
 * "Card n. 1" header, matching the format the cap parser expects.
 */
static int write_synthetic_cap(const char *path,
                                const uint16_t *cols, int ncols)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "Synthetic deck for gemu cardreader test\n");
    fprintf(f, "Card n. 1\n");
    for (int i = 0; i < ncols; i++)
        fprintf(f, "%04X ", (unsigned)(cols[i] & 0xFFFFu));
    fprintf(f, "\n");
    fclose(f);
    return 0;
}

/* Run up to max_cycles cycles; return when halted or rSO reaches target_state
 * after it was previously not at target_state (edge detect), or on error.
 * Returns the last rSO seen. */
static int run_until_state(struct ge *g, uint8_t target, int max_cycles)
{
    for (int i = 0; i < max_cycles; i++) {
        int r = ge_run_cycle(g);
        if (r != 0)
            return -1;
        if (g->rSO == target)
            return g->rSO;
        if (g->halted)
            return g->rSO;
    }
    return g->rSO;
}

/* --------------------------------------------------------------------------
 * Test: 4-byte synthetic deck, TC_BINARY, expect state 0xe3
 *
 * Deck bytes: 0xAB, 0xCD, 0xEF, 0xAA  (same as initial-load.c manual test)
 *
 * After packing (two nibbles per memory byte):
 *   mem[0] = high-nibble(0xAB) | low-nibble(0xCD) = 0xBD
 *   mem[1] = high-nibble(0xEF) | low-nibble(0xAA) = 0xFA
 *
 * The initial-load.c reference test confirms these values.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, synthetic_4byte)
{
    static const char cap_path[] = "/tmp/gemu_test_4byte.cap";

    /* 4 column values: in TC_BINARY mode, low 8 bits become the byte */
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    int wr = write_synthetic_cap(cap_path, cols, 4);
    ASSERT_EQ(wr, 0);

    struct ge g;
    ge_init(&g);
    ge_clear(&g);
    ge_load_1(&g);   /* select connector 2 */
    ge_load(&g);

    int rc = cardreader_register(&g, cap_path, TC_BINARY);
    ASSERT_EQ(rc, 0);

    ge_start(&g);

    /* ------------------------------------------------------------------ */
    /* Run the full boot sequence.  The peripheral feeds bytes on-demand.  */
    /* We replicate the state checks from initial-load.c to confirm the    */
    /* same machine path is taken.                                          */
    /* ------------------------------------------------------------------ */

    /* State 0x00 Display */
    ASSERT_EQ(g.rSO, 0x00);
    ge_run_cycle(&g);

    /* State 0x80 Initialisation */
    ASSERT_EQ(g.rSO, 0x80);
    ge_run_cycle(&g);

    /* PER-PERI 1 */
    ASSERT_EQ(g.rSO, 0xc8);
    ge_run_cycle(&g);

    /* PER-PERI 2: rRE is set DURING the d8 cycle, so check it afterwards */
    ASSERT_EQ(g.rSO, 0xd8);
    ge_run_cycle(&g);
    ASSERT_EQ(g.rRE, 0x80); /* 0x80 == connector 2 (load_1 selected) */

    ASSERT_EQ(g.rSO, 0xd9);
    ge_run_cycle(&g);

    ASSERT_EQ(g.rSO, 0xda);
    ge_run_cycle(&g);

    ASSERT_EQ(g.rSO, 0xdb);
    ge_run_cycle(&g);

    /* PER-PERI 6 */
    ASSERT_EQ(g.rSO, 0xdc);
    ge_run_cycle(&g);

    /* PER-PERI 7: read command 0x40 is set DURING the cc cycle */
    ASSERT_EQ(g.rSO, 0xcc);
    ge_run_cycle(&g);
    ASSERT_EQ(g.rRE, 0x40);

    /* TPER-CPER 1 */
    ASSERT_EQ(g.rSO, 0xca);
    ge_run_cycle(&g);

    /* TPER-CPER 2 */
    ASSERT_EQ(g.rSO, 0xa8);
    ge_run_cycle(&g);

    /* TPER-CPER 3: length low == 0x80 (set DURING the a9 cycle) */
    ASSERT_EQ(g.rSO, 0xa9);
    ge_run_cycle(&g);
    ASSERT_EQ(g.rL1, 0x80);

    /* TPER-CPER 4 */
    ASSERT_EQ(g.rSO, 0xaa);
    ge_run_cycle(&g);

    /* TPER-CPER 5: rV1 is set DURING the ab cycle */
    ASSERT_EQ(g.rSO, 0xab);
    ge_run_cycle(&g);
    ASSERT_EQ(g.rV1, 0);

    /* TPER-CPER 6: machine enters input-wait loop (state b8).
     * From here on the cardreader peripheral takes over.
     * We run up to 2048 cycles to complete the 4-byte load sequence
     * and reach the end states. */
    ASSERT_EQ(g.rSO, 0xb8);

    /*
     * Run cycles until state 0xe3 (alpha) is reached.
     *
     * The peripheral automatically feeds:
     *   byte 0: 0xAB  (end=0)
     *   byte 1: 0xCD  (end=0)
     *   byte 2: 0xEF  (end=0)
     *   byte 3: 0xAA  (end=1)  <- sets RIG1/PEC1, drives load-end sequence
     *
     * After the last byte the machine transitions:
     *   b8 (wait) -> ea (TPER END 1) -> eb (TPER END 2) -> e3 (Alpha)
     */
    int final = run_until_state(&g, 0xe3, 2048);
    ASSERT_EQ(final, 0xe3);

    /*
     * Check memory contents.  initial-load.c verifies:
     *   mem[0] = 0xBD  (high nibble of 0xAB packed with low nibble of 0xCD)
     *   mem[1] = 0xFA  (high nibble of 0xEF packed with low nibble of 0xAA)
     */
    ASSERT_EQ(g.mem[0], 0xBD);
    ASSERT_EQ(g.mem[1], 0xFA);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test: real funktionalcpu.cap, TC_NORMAL, first card only.
 *
 * Points at ../DUMP1/funktionalcpu.cap (and funktionalcpu.bin for oracle).
 * Card 0 has 80 columns (80 nibbles = 40 packed bytes).  After a complete
 * channel-1 input load the machine must reach state 0xe3 (Alpha) with
 * mem[0..39] matching the nibble-packed oracle.
 *
 * Oracle packing formula (from the task specification):
 *   mem[i] = (bin[2*i] & 0x0F) << 4 | (bin[2*i+1] & 0x0F)  for i=0..39
 *
 * Verification:
 *   bin[0]=0xF4, bin[1]=0xF7  → mem[0] = (0x04)<<4 | 0x07 = 0x47
 *   bin[2]=0xF1, bin[3]=0xF4  → mem[1] = (0x01)<<4 | 0x04 = 0x14
 *   ... etc.
 *
 * NOTE: This test skips gracefully if either oracle file is not found.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, funktionalcpu_first_card)
{
    /* The test runner cwd is the gemu/ source dir */
    static const char cap_path[] = "../DUMP1/funktionalcpu.cap";
    static const char bin_path[] = "../DUMP1/funktionalcpu.bin";

    /* Check existence of both files */
    FILE *probe = fopen(cap_path, "r");
    if (!probe) {
        printf("  [SKIP] %s not found\n", cap_path);
        return;
    }
    fclose(probe);
    probe = fopen(bin_path, "rb");
    if (!probe) {
        printf("  [SKIP] %s not found\n", bin_path);
        return;
    }

    /* Read the first 80 oracle bytes (first card = 80 columns) */
    uint8_t bin[80];
    size_t nr = fread(bin, 1, 80, probe);
    fclose(probe);
    ASSERT_EQ((int)nr, 80);

    /* Compute oracle mem[0..39] */
    uint8_t oracle[40];
    for (int i = 0; i < 40; i++)
        oracle[i] = (uint8_t)(((bin[2*i] & 0x0Fu) << 4) | (bin[2*i+1] & 0x0Fu));

    struct ge g;
    ge_init(&g);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);

    int rc = cardreader_register(&g, cap_path, TC_NORMAL);
    ASSERT_EQ(rc, 0);

    ge_start(&g);

    /* Fast-forward through the peri-init states (rRE is set DURING the cycle) */
    ASSERT_EQ(g.rSO, 0x00); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0x80); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xc8); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xd8); ge_run_cycle(&g); ASSERT_EQ(g.rRE, 0x80);
    ASSERT_EQ(g.rSO, 0xd9); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xda); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xdb); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xdc); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xcc); ge_run_cycle(&g); ASSERT_EQ(g.rRE, 0x40);
    ASSERT_EQ(g.rSO, 0xca); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xa8); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xa9); ge_run_cycle(&g); ASSERT_EQ(g.rL1, 0x80);
    ASSERT_EQ(g.rSO, 0xaa); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xab); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xb8);

    /*
     * Run with the real deck until 0xe3 (Alpha).
     * Card 0 has 80 columns; the load terminates on end-of-card (FINI).
     * Allow enough cycles: 80 cols * ~4 cycles each + overhead ~ 4096 cycles.
     */
    int final = run_until_state(&g, 0xe3, 8192);
    ASSERT_EQ(final, 0xe3);

    /*
     * Verify all 40 loaded bytes against the oracle.
     * The oracle formula is: mem[i] = (bin[2i] & 0x0F)<<4 | (bin[2i+1] & 0x0F)
     * which corresponds to the GE nibble-packing scheme observed in initial-load.c.
     *
     * Key spot-check: oracle[0] = (0xF4 & 0x0F)<<4 | (0xF7 & 0x0F) = 0x40|0x07 = 0x47
     */
    ASSERT_EQ(g.mem[0], 0x47);  /* spot-check first byte */
    for (int i = 0; i < 40; i++) {
        if (g.mem[i] != oracle[i]) {
            printf("  [FAIL] mem[%d]: got=0x%02x expected=0x%02x\n",
                   i, g.mem[i], oracle[i]);
        }
        ASSERT_EQ((int)g.mem[i], (int)oracle[i]);
    }

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test: sequential two-card read (multi-card support).
 *
 * Synthetic deck has two cards in TC_BINARY mode:
 *   Card 0: 4 bytes 0xAB, 0xCD, 0xEF, 0xAA  (same as synthetic_4byte)
 *   Card 1: 4 bytes 0x11, 0x22, 0x33, 0x44
 *
 * After card 0: mem[0]=0xBD, mem[1]=0xFA, state=0xe3.
 * After card 1 (second load): mem[0]=0x12, mem[1]=0x34, state=0xe3.
 *
 * The second load is triggered by re-pressing LOAD+START (simulating a new
 * PER instruction from the loaded program).  The cardreader automatically
 * advances to card 1 after card 0's end-of-card is signalled.
 *
 * Packing (TC_BINARY, formula (a & 0x0F)<<4 | (b & 0x0F)):
 *   0x11, 0x22 → (0x1)<<4 | 0x2 = 0x12
 *   0x33, 0x44 → (0x3)<<4 | 0x4 = 0x34
 * -------------------------------------------------------------------------- */
UTEST(cardreader, sequential_two_cards)
{
    static const char cap_path[] = "/tmp/gemu_test_2cards.cap";

    /* Write a 2-card deck */
    {
        FILE *f = fopen(cap_path, "w");
        ASSERT_NE(f, NULL);
        fprintf(f, "Synthetic 2-card deck for gemu sequential_two_cards test\n");
        fprintf(f, "Card n. 1\n");
        /* Card 0: 0xAB 0xCD 0xEF 0xAA */
        fprintf(f, "00AB 00CD 00EF 00AA \n");
        fprintf(f, "Card n. 2\n");
        /* Card 1: 0x11 0x22 0x33 0x44 */
        fprintf(f, "0011 0022 0033 0044 \n");
        fclose(f);
    }

    struct ge g;
    ge_init(&g);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);

    int rc = cardreader_register(&g, cap_path, TC_BINARY);
    ASSERT_EQ(rc, 0);

    ge_start(&g);

    /* --- First card load (card 0) --- */
    ASSERT_EQ(g.rSO, 0x00); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0x80); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xc8); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xd8); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xd9); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xda); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xdb); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xdc); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xcc); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xca); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xa8); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xa9); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xaa); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xab); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xb8);

    int final = run_until_state(&g, 0xe3, 2048);
    ASSERT_EQ(final, 0xe3);

    /* Card 0 bytes: 0xAB,0xCD → 0xBD; 0xEF,0xAA → 0xFA */
    ASSERT_EQ(g.mem[0], 0xBD);
    ASSERT_EQ(g.mem[1], 0xFA);

    /* --- Second card load (card 1) --- */
    /* Re-initialise the machine to simulate the machine issuing another PER
     * instruction (via CLEAR+LOAD+START from the operator console, which is
     * what would happen if the loaded bootstrap issued a second PER read).
     * The cardreader peripheral is still registered with its pointer already
     * advanced to card 1, col 0 (cr_advance positioned it there after card 0).
     *
     * We also reset rSO=0 to bring the state machine back to the Display
     * state (state 00).  In the real machine this happens automatically when
     * CLEAR is pressed; in our emulator the initial rSO=0 comes from
     * ge_init()'s memset, so we mirror that here for the second run. */
    ge_clear(&g);
    g.rSO = 0x00;
    g.rSI  = 0x00;
    ge_load_1(&g);
    ge_load(&g);
    ge_start(&g);

    /* Run the full load sequence for card 1. */
    int final2 = run_until_state(&g, 0xe3, 4096);
    ASSERT_EQ(final2, 0xe3);

    /* Card 1 bytes: 0x11,0x22 → 0x12; 0x33,0x44 → 0x34 */
    ASSERT_EQ(g.mem[0], 0x12);
    ASSERT_EQ(g.mem[1], 0x34);

    ge_deinit(&g);
}
