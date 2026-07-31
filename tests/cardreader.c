/*
 * tests/cardreader.c - Unit tests for the connector-2 card-reader peripheral.
 *
 * Tests use the utest.h framework.  No UTEST_MAIN here; the test runner
 * main() is provided by tests/main.c (auto-discovery).
 *
 * The suites here cover the reader as a whole: the synthetic decks that pin the
 * handshake, and the real funktionalcpu deck that proves the whole self-load.
 *
 *  cardreader.synthetic_4byte
 *    Writes a tiny synthetic .cap deck to /tmp that encodes exactly 4 bytes
 *    (0xAB, 0xCD, 0xEF, 0xAA) in TC_BINARY mode, then runs the full load
 *    sequence and checks that the machine reaches state 0xe3 (alpha) with
 *    the correct bytes packed into mem[].  This mirrors the manual hand-
 *    feeding in tests/initial-load.c, but done automatically by the
 *    cardreader peripheral.
 *
 *  cardreader.tu03_triggers_the_card / no_feed_means_no_card
 *    The wire order: a read command latches and strobes nothing; TU03N is what
 *    starts the card moving. No LU08N may appear before the first TU03N.
 *
 *  cardreader.lupob_is_busy_for_the_whole_card / finin_stands_after_the_last_strobe
 *    The two status lines whose behaviour the bench settled in July 2026.
 *
 *  cardreader.funktionalcpu_loader_autodetect / _authentic_load_reaches_payload
 *    The real deck: find the bootstrap card, read it, and let its own code pull
 *    the remaining 106 cards in and relocate them.
 *
 * Feeding trigger/cadence implemented in cardreader.c (see its header comment):
 *   on_clock (TO00) is called once per machine cycle.
 *   - CR_ARMED_WAIT: a command latches; TU03N triggers the card.
 *   - CR_PRESENTING: present, then retire, then present again -- the gap is the
 *                    machine's b1 pack cycle.
 *   - CR_CARD_DONE:  FININ stands until something takes it down.
 *   This mirrors the exact pattern from initial-load.c:
 *     reader_setup_to_send(...)   <- before b8/b9 cycle
 *     ge_run_cycle()              <- machine reads nibble
 *     reader_clear_sending(...)   <- before b1 cycle
 *     ge_run_cycle()              <- machine packs nibble
 */

#include "utest.h"
#include "decks.h"
#include "../ge.h"
#include "../cardreader.h"
#include "../cap.h"
#include "../transcode.h"
#include "../bit.h"
#include "../reader.h"
#include "../log.h"

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
        if (ge_halted(g))
            return g->rSO;
    }
    return g->rSO;
}

static int run_until_mem_nonzero(struct ge *g, uint16_t addr, int max_cycles)
{
    for (int i = 0; i < max_cycles; i++) {
        int r = ge_run_cycle(g);
        if (r != 0)
            return -1;
        if (g->mem[addr] != 0)
            return g->mem[addr];
        if (ge_halted(g))
            return g->mem[addr];
    }
    return g->mem[addr];
}

static int run_until_mem_bytes(struct ge *g, uint16_t addr,
                               const uint8_t *bytes, size_t len,
                               int max_cycles)
{
    for (int i = 0; i < max_cycles; i++) {
        int r = ge_run_cycle(g);
        if (r != 0)
            return -1;
        if (memcmp(&g->mem[addr], bytes, len) == 0)
            return 1;
        if (ge_halted(g))
            return 0;
    }
    return memcmp(&g->mem[addr], bytes, len) == 0;
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
     *   byte 3: 0xAA  (end=1)  <- sets RIG1 and queues PEC1 for the TO50 latch,
     *                             driving the load-end sequence
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
 * Test (Phase 3): LUSEN out-of-service stalls the read.
 *
 * Identical setup to synthetic_4byte, but the reader is forced out-of-service
 * (lusen=1) once the machine reaches the input-wait (b8). cardreader_on_clock
 * then presents nothing, so the transfer never completes: the machine must NOT
 * reach 0xe3 and mem[0] stays 0. This is the unit-not-ready fault path.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, lusen_out_of_service_stalls)
{
    static const char cap_path[] = "/tmp/gemu_test_lusen.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    ge_start(&g);

    /* Advance to the input-wait, then yank the reader out of service. */
    int got = run_until_state(&g, 0xb8, 256);
    ASSERT_EQ(got, 0xb8);
    g.integrated_reader.lusen = 1;

    /* With the reader offline the load cannot complete. */
    int final = run_until_state(&g, 0xe3, 2048);
    ASSERT_NE(final, 0xe3);
    ASSERT_EQ(g.mem[0], 0x00);
    ASSERT_EQ((int)g.integrated_reader.lupor, 0);  /* not ready while offline */

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test (Phase 3): LUPOR ready behaviour + the PELEA safety invariant.
 *
 * A normal 4-byte read must still complete (LUPOR is only a status line, not a
 * gate that blocks the bootstrap). Two extra checks:
 *   - LUPOR is asserted at least once (reader reports ready during the wait).
 *   - LUPOR and LU08 are NEVER both 1 — this is what keeps
 *     PELEA = !(LU08 . LUPO1) == 1, i.e. the read data path is unchanged.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, lupor_ready_invariant)
{
    static const char cap_path[] = "/tmp/gemu_test_lupor.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    ge_start(&g);

    int saw_ready = 0;
    int reached_e3 = 0;
    for (int i = 0; i < 2048; i++) {
        ASSERT_EQ(ge_run_cycle(&g), 0);
        /* Invariant: a presented byte and "ready" are mutually exclusive. */
        ASSERT_FALSE(g.integrated_reader.lupor && g.integrated_reader.lu08);
        if (g.integrated_reader.lupor)
            saw_ready = 1;
        if (g.rSO == 0xe3) { reached_e3 = 1; break; }
        if (ge_halted(&g)) break;
    }

    ASSERT_TRUE(reached_e3);             /* normal load still completes */
    ASSERT_TRUE(saw_ready);              /* reader reported ready at least once */
    ASSERT_EQ(g.mem[0], 0xBD);
    ASSERT_EQ(g.mem[1], 0xFA);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test: TU03N (CE09) is the trigger, not the epilogue.
 *
 * The wire order the bench measured, and the one the Pico reader implements:
 * the CPU sends the read command on RE with a TU00N strobe, the reader latches
 * it and strobes NOTHING, and then TU03N arrives and the card starts moving.
 * Presenting on the command instead hands the card to a channel that is not
 * armed yet. So: no LU08N may appear before the first TU03N.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, tu03_triggers_the_card)
{
    static const char cap_path[] = "/tmp/gemu_test_tu03.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    ge_start(&g);

    int first_feed = -1;
    int first_strobe = -1;
    int feed_pulses = 0;
    int reached_e3 = 0;

    for (int i = 0; i < 2048; i++) {
        ASSERT_EQ(ge_run_cycle(&g), 0);
        if (g.integrated_reader.tu03) {
            feed_pulses++;
            if (first_feed < 0)
                first_feed = i;
        }
        if (g.integrated_reader.lu08 && first_strobe < 0)
            first_strobe = i;
        if (g.rSO == 0xe3) { reached_e3 = 1; break; }
        if (ge_halted(&g)) break;
    }

    ASSERT_TRUE(reached_e3);              /* the load completes */
    ASSERT_GT(feed_pulses, 0);            /* the machine did ask for the feed */
    ASSERT_GE(first_feed, 0);
    ASSERT_GE(first_strobe, 0);
    ASSERT_LT(first_feed, first_strobe);  /* and it asked BEFORE any data moved */

    ASSERT_EQ(g.mem[0], 0xBD);
    ASSERT_EQ(g.mem[1], 0xFA);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test: a reader with no feed presents nothing, then gives up and feeds itself.
 *
 * LENON ("not operable") suppresses TU03N at the reader input (reader.c). With
 * the feed gone the latched read command has nothing to start it, so no card
 * moves -- until the autofeed fallback fires, which is the Pico's D timeout and
 * exists so a wiring fault shows up as a late card rather than a dead machine.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, no_feed_means_no_card)
{
    static const char cap_path[] = "/tmp/gemu_test_nofeed.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    g.integrated_reader.lenon = 1;   /* not operable: the feed never arrives */
    ge_start(&g);

    int strobes = 0;
    for (int i = 0; i < 2048; i++) {
        ASSERT_EQ(ge_run_cycle(&g), 0);
        if (g.integrated_reader.lu08)
            strobes++;
        if (ge_halted(&g)) break;
    }

    ASSERT_EQ(g.integrated_reader.tu03, 0);   /* suppressed at the reader */
    ASSERT_EQ(strobes, 0);                    /* so nothing was ever presented */
    ASSERT_EQ((int)g.mem[0], 0);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test: FININ stands after the last strobe, and comes down on the next command.
 *
 * On the wire FININ rides the last presentation of a card and STAYS asserted
 * once the strobe has gone -- the presenter has stalled with the word still on
 * the pins. It is released by the next read command, by a TU03N, or by a
 * timeout. Clearing it together with the strobe (which is what gemu used to do)
 * hides a whole class of end-of-transfer timing from the model.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, finin_stands_after_the_last_strobe)
{
    static const char cap_path[] = "/tmp/gemu_test_finin.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    ge_start(&g);

    int saw_finin_without_strobe = 0;
    int reached_e3 = 0;

    for (int i = 0; i < 2048; i++) {
        ASSERT_EQ(ge_run_cycle(&g), 0);
        /* FININ up while LU08N is down: the end-of-card word left standing. */
        if (g.integrated_reader.fini && !g.integrated_reader.lu08)
            saw_finin_without_strobe = 1;
        if (g.rSO == 0xe3) { reached_e3 = 1; break; }
        if (ge_halted(&g)) break;
    }

    ASSERT_TRUE(reached_e3);
    ASSERT_TRUE(saw_finin_without_strobe);
    /* The deck is one card long, so nothing has released it yet. */
    ASSERT_EQ((int)g.integrated_reader.fini, 1);

    /* The next read command takes it down -- and, because the command latches
     * before any ready/busy update, without a ready front in between. */
    g.rRE = 0x40;
    reader_send_tu00(&g);
    ASSERT_EQ((int)g.integrated_reader.fini, 0);
    ASSERT_EQ((int)g.integrated_reader.cmd_pending, 1);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test: LUPOB is a per-card ready/busy line, not a per-character one.
 *
 * The Pico's predicate, which gemu now shares (feeder.c lupob_update):
 * ready means a deck is loaded, nothing is presenting, no command is waiting
 * for its feed, no FININ is standing, and we are parked before or after a card.
 * Everything else is busy -- including "no deck at all", which is why an
 * unarmed reader reads busy rather than ready.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, lupob_is_busy_for_the_whole_card)
{
    static const char cap_path[] = "/tmp/gemu_test_lupob.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    /* No deck registered: the reader is busy, never ready. */
    ASSERT_EQ((int)g.integrated_reader.lupor, 0);

    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    ge_start(&g);

    int ready_to_busy_fronts = 0;
    int busy_to_ready_fronts = 0;
    int prev_ready = -1;
    int ready_while_presenting = 0;
    int reached_e3 = 0;

    for (int i = 0; i < 2048; i++) {
        int ready;

        ASSERT_EQ(ge_run_cycle(&g), 0);
        ready = g.integrated_reader.lupor;

        /* Busy for every character of the card, not just the one on the pins. */
        if (ready && (g.integrated_reader.lu08 || g.integrated_reader.fini))
            ready_while_presenting = 1;

        if (prev_ready >= 0 && ready != prev_ready) {
            if (ready)
                busy_to_ready_fronts++;
            else
                ready_to_busy_fronts++;
        }
        prev_ready = ready;

        if (g.rSO == 0xe3) { reached_e3 = 1; break; }
        if (ge_halted(&g)) break;
    }

    ASSERT_TRUE(reached_e3);
    ASSERT_FALSE(ready_while_presenting);

    /* Exactly one busy front for the one card that was read: the reader goes
     * busy when the command latches and stays busy through the whole card. A
     * per-character LUPOB would produce one front per nibble. */
    ASSERT_EQ(ready_to_busy_fronts, 1);
    ASSERT_EQ(busy_to_ready_fronts, 0);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test (Phase 5): the RENIA length-count terminal is inert for a FININ read.
 *
 * The RENIA equation (L1 all-ones AND L204) is wired, but the current read
 * datapath keeps L204 clear on the bootstrap read, so the transfer must end
 * on FININ (RENIA = !(RL1U1 & L204) stays 1 regardless of L1). Since the
 * CI-phase counting network went live (audit round 3), L1 is no longer frozen
 * during the b1 packing states — the invariants are: L204 stays 0, the read
 * is FININ-bounded, and the packed data lands intact.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, renia_length_count_inert)
{
    static const char cap_path[] = "/tmp/gemu_test_renia.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    ge_start(&g);

    /* Reach the input-wait and capture the order length. */
    int got = run_until_state(&g, 0xb8, 256);
    ASSERT_EQ(got, 0xb8);
    ASSERT_EQ((int)g.rL1, 0x80);   /* order length captured at the wait */

    int reached_e3 = 0;
    for (int i = 0; i < 2048; i++) {
        ASSERT_EQ(ge_run_cycle(&g), 0);
        /* The length-count bit never sets on the bootstrap read, so the
         * terminal cannot gate RENIA: the read stays FININ-bounded. */
        ASSERT_EQ((int)((g.rL2 >> 4) & 1), 0);   /* L204 stays clear */
        if (g.rSO == 0xe3) { reached_e3 = 1; break; }
        if (ge_halted(&g)) break;
    }
    ASSERT_TRUE(reached_e3);
    ASSERT_EQ(g.mem[0], 0xBD);
    ASSERT_EQ(g.mem[1], 0xFA);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test (Phase 6): LUREN (error / jam) stalls the read like out-of-service.
 *
 * With LUREN asserted the reader cannot deliver data: the load must not
 * complete and no data lands. Mirrors the LUSEN fault path.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, luren_error_stalls)
{
    static const char cap_path[] = "/tmp/gemu_test_luren.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    ge_start(&g);

    int got = run_until_state(&g, 0xb8, 256);
    ASSERT_EQ(got, 0xb8);
    g.integrated_reader.luren = 1;          /* transcoder error / card jam */

    int final = run_until_state(&g, 0xe3, 2048);
    ASSERT_NE(final, 0xe3);
    ASSERT_EQ(g.mem[0], 0x00);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test (Phase 6): POM01 / PICON / BI20 observable feed-state lines.
 *
 * A packed (self-loading) binary deck presents each column as a hi-then-lo
 * nibble pair. Over the early part of the feed we must observe:
 *   - POM01 high while presenting (binary / by-pass read),
 *   - PICON high on column 0,
 *   - BI20  high on a low-nibble (2nd sub-read) presentation.
 * These are observability lines; nothing in the CPU logic consumes them, so we
 * just sample them at presentation cycles.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, feed_state_lines_pom_pico_bi20)
{
    static const char cap_path[] = "/tmp/gemu_test_feedlines.cap";
    {
        FILE *f = fopen(cap_path, "w");
        ASSERT_NE(f, NULL);
        fprintf(f, "Packed feed-line test deck\n");
        fprintf(f, "Card n. 1\n");
        fprintf(f, "0012 0034 0056 0078 \n");   /* full COLBIN bytes */
        fclose(f);
    }

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register_packed(&g, cap_path, TC_COLBIN), 0);
    ge_start(&g);

    int saw_pom_on_present = 0;
    int saw_picon_col0     = 0;
    int saw_bi20           = 0;
    for (int i = 0; i < 1024; i++) {
        ASSERT_EQ(ge_run_cycle(&g), 0);
        if (g.integrated_reader.lu08) {        /* a character is on the lines */
            if (g.integrated_reader.pom01) saw_pom_on_present = 1;
            if (g.integrated_reader.picon) saw_picon_col0 = 1;
            if (g.integrated_reader.bi20)  saw_bi20 = 1;
        }
        if (g.rSO == 0xe3) break;
        if (ge_halted(&g)) break;
    }

    ASSERT_TRUE(saw_pom_on_present);   /* binary-mode indicator asserted */
    ASSERT_TRUE(saw_picon_col0);       /* first-column check seen */
    ASSERT_TRUE(saw_bi20);             /* 2nd-nibble clock seen */

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * (removed) cardreader.funktionalcpu_first_card
 *
 * It asserted mem[0..39] after the one-card IPL against an oracle read from
 * ../DUMP1/funktionalcpu.bin -- a unified-format scatter image, not a card
 * dump, so the test had been guarding itself off and passing vacuously. .bin
 * is gone; bootstrap.card0_loads_to_alpha now makes the same 40-byte
 * assertion against an oracle decoded from the deck itself, live.
 * -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
 * Test: mixed funktionalcpu deck auto-detects the Hollerith bootstrap loader.
 *
 * The capture starts with several loader/control cards; the integrated reader
 * bootstrap must pick the CR10/Hollerith loader card, decode it in TC_HEX, and
 * land its first bytes exactly as punched.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, funktionalcpu_loader_autodetect)
{
    const char *cap_path = deck_funktionalcpu_cap();

    FILE *probe = fopen(cap_path, "r");
    if (!probe) {
        printf("  [SKIP] %s not found\n", cap_path);
        return;
    }
    fclose(probe);

    struct ge g;
    ge_init(&g);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);

    ASSERT_EQ(cardreader_register(&g, cap_path, TC_NORMAL), 0);
    ge_start(&g);

    ASSERT_EQ(run_until_state(&g, 0xe3, 8192), 0xe3);

    /* Decoded start of the actual Hollerith loader card (card 5 in the .cap):
     *   PER 0x80, 0x0022
     *   PER 0x80, 0x0020
     *   PER 0x80, 0x0026
     */
    ASSERT_EQ(g.mem[0], 0x9E);
    ASSERT_EQ(g.mem[1], 0x80);
    ASSERT_EQ(g.mem[2], 0x00);
    ASSERT_EQ(g.mem[3], 0x22);
    ASSERT_EQ(g.mem[4], 0x9E);
    ASSERT_EQ(g.mem[5], 0x80);
    ASSERT_EQ(g.mem[6], 0x00);
    ASSERT_EQ(g.mem[7], 0x20);
    ASSERT_EQ(g.mem[8], 0x9E);
    ASSERT_EQ(g.mem[9], 0x80);
    ASSERT_EQ(g.mem[10], 0x00);
    ASSERT_EQ(g.mem[11], 0x26);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test: authentic funktionalcpu deck loading progresses past the bootstrap.
 *
 * This covers the real reader flow: loader card in TC_HEX, subsequent program
 * cards in by-pass/COLBIN, with the loader relocating card payloads into their
 * embedded addresses. The first real code block reaches 0x0100 as
 * `43 F0 17 2A` in the reconstructed oracle image; waiting for "first nonzero"
 * at a later address is too weak because the loader writes transient bytes
 * while it is still unpacking the deck.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, funktionalcpu_authentic_load_reaches_payload)
{
    const char *cap_path = deck_funktionalcpu_cap();
    static const uint8_t expected[] = { 0x43, 0xF0, 0x17, 0x2A };

    FILE *probe = fopen(cap_path, "r");
    if (!probe) {
        printf("  [SKIP] %s not found\n", cap_path);
        return;
    }
    fclose(probe);

    struct ge g;
    ge_init(&g);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);

    ASSERT_EQ(cardreader_register(&g, cap_path, TC_NORMAL), 0);
    ge_start(&g);

    ASSERT_EQ(run_until_mem_bytes(&g, 0x0100, expected, sizeof(expected), 120000), 1);

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

/* --------------------------------------------------------------------------
 * cardreader.scatter_load_funktionalcpu
 *
 * cap_load_scattered places each self-addressed card's payload at its embedded
 * load address. For the funktionalcpu deck this populates the program: the
 * cpu_selftest entry sits at 0x0100 as `JC 0xF0, 0x172A` (43 F0 17 2A). This is
 * the working .cap default-input path (distinct from the cycle-faithful reader
 * bootstrap), and the regression guard for the IPL scatter-loader.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, scatter_load_funktionalcpu)
{
    const char *cap_path = deck_funktionalcpu_cap();

    FILE *probe = fopen(cap_path, "r");
    if (!probe) {
        printf("  [SKIP] %s not found\n", cap_path);
        return;
    }
    fclose(probe);

    static uint8_t img[MEM_SIZE];
    memset(img, 0, sizeof img);

    unsigned lo = 0xFFFF, hi = 0;
    int nc = cap_load_scattered(cap_path, TC_COLBIN, img, &lo, &hi);

    ASSERT_GT(nc, 0);
    ASSERT_EQ((int)lo, 0x0000);
    ASSERT_GT((int)hi, 0x1000);

    /* cpu_selftest entry scattered to 0x0100: JC 0xF0, 0x172A */
    ASSERT_EQ((int)img[0x0100], 0x43);
    ASSERT_EQ((int)img[0x0101], 0xF0);
    ASSERT_EQ((int)img[0x0102], 0x17);
    ASSERT_EQ((int)img[0x0103], 0x2A);
}

/* --------------------------------------------------------------------------
 * Test: the CPU->reader command lines are strobes, not levels.
 *
 * TU00N (command clock), COCON (mode-select clock) and TU03N (feed) are raised
 * by the CE commands late in a cycle and must be down again after the reader
 * has seen them -- on_clock, at TO00 of the next cycle. A strobe left standing
 * is a stuck pin: it is what anyone reading a trace (or the Pico, sampling the
 * same wires at the COCA slots) would call a fault.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, command_strobes_do_not_stand)
{
    static const char cap_path[] = "/tmp/gemu_test_strobe.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);
    ge_start(&g);

    /* REGEN went up with the CLEAR above; one cycle and the reader has it. */
    ASSERT_EQ(ge_run_cycle(&g), 0);
    ASSERT_EQ((int)g.integrated_reader.regen, 0);

    /* A mode-selecting read command: RE + TU00N, and COCON with it. */
    g.rRE = 0x20;                                  /* read binary / by-pass */
    reader_send_tu00(&g);
    ASSERT_EQ((int)g.integrated_reader.tu00, 1);
    ASSERT_EQ((int)g.integrated_reader.cocon, 1);
    ASSERT_EQ((int)g.integrated_reader.active_valid, 1);   /* the mode is latched */

    ASSERT_EQ(ge_run_cycle(&g), 0);
    ASSERT_EQ((int)g.integrated_reader.tu00, 0);
    ASSERT_EQ((int)g.integrated_reader.cocon, 0);
    ASSERT_EQ((int)g.integrated_reader.active_valid, 1);   /* but the mode stands */

    /* And the same for the feed, which the reader consumes as it drops it. */
    reader_send_tu10(&g);
    ASSERT_EQ((int)g.integrated_reader.tu03, 1);
    ASSERT_EQ(ge_run_cycle(&g), 0);
    ASSERT_EQ((int)g.integrated_reader.tu03, 0);

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * Test: what is in the hopper, as the panel reports it.
 *
 * cardreader_deck_cards is the deck as captured; cardreader_cards_left counts
 * down as the reader feeds, and both say -1 with no reader on the connector.
 * -------------------------------------------------------------------------- */
UTEST(cardreader, hopper_reports_what_is_left)
{
    static const char cap_path[] = "/tmp/gemu_test_hopper.cap";
    uint16_t cols[4] = { 0x00AB, 0x00CD, 0x00EF, 0x00AA };
    ASSERT_EQ(write_synthetic_cap(cap_path, cols, 4), 0);

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);

    ASSERT_EQ(cardreader_deck_cards(&g), -1);      /* nothing on the connector */
    ASSERT_EQ(cardreader_cards_left(&g), -1);

    ge_load_1(&g);
    ge_load(&g);
    ASSERT_EQ(cardreader_register(&g, cap_path, TC_BINARY), 0);

    ASSERT_EQ(cardreader_deck_cards(&g), 1);
    ASSERT_EQ(cardreader_cards_left(&g), 1);       /* still waiting at the throat */

    ge_start(&g);
    for (int i = 0; i < 2048; i++) {
        ASSERT_EQ(ge_run_cycle(&g), 0);
        if (g.rSO == 0xe3) break;
    }

    ASSERT_EQ(cardreader_deck_cards(&g), 1);       /* the deck does not shrink */
    ASSERT_EQ(cardreader_cards_left(&g), 0);       /* the hopper does */

    ge_deinit(&g);
}
