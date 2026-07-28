/*
 * tests/bootstrap.c - Channel-level bootstrap load test for the GE-120 emulator.
 *
 * Loads the real diagnostic deck ../DUMP1/funktionalcpu.cap through the
 * emulated card-reader peripheral (cardreader_register) and verifies that:
 *
 *   1. The machine reaches state 0xe3 (Alpha phase) after loading exactly
 *      one card (80 columns = 40 packed bytes).
 *
 *   2. mem[0..39] exactly match the deck's own bootstrap loader card, decoded
 *      in TC_HEX and nibble-packed the way the channel-1 transfer packs it:
 *
 *        oracle[i] = HEX(col[2*i]) << 4 | HEX(col[2*i+1])
 *
 * SCOPE NOTE:
 *   This test exercises the CHANNEL-LEVEL LOAD path only:
 *     - Peripheral initialisation (states 00→80→c8→d8→d9→da→db→dc→cc→ca)
 *     - Transfer setup         (states a8→a9→aa→ab)
 *     - Nibble-input loop      (states b8→b9→b1 × 80 iterations)
 *     - End-of-transfer        (states ea→eb→e3)
 *
 *   It does NOT test execution of the loaded loader program (MVC/JU/etc.
 *   instructions), which requires CPU instruction execution not yet
 *   implemented in this wave of emulation.
 *
 * Uses:
 *   - cardreader_register() for automatic byte feeding
 *   - ge_load_1() + ge_load() + ge_start() for the boot sequence
 *   - utest.h for assertions (no UTEST_MAIN; main() is in tests/main.c)
 */

#include "utest.h"
#include "decks.h"
#include "../ge.h"
#include "../cardreader.h"
#include "../cap.h"
#include "../transcode.h"
#include "../bit.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Helper: run up to max_cycles, stop when rSO == target or halted.
 * Returns the final rSO value.
 * -------------------------------------------------------------------------- */
static int bootstrap_run_until(struct ge *g, uint8_t target, int max_cycles)
{
    for (int i = 0; i < max_cycles; i++) {
        if (ge_run_cycle(g) != 0)
            return -1;
        if (g->rSO == target)
            return g->rSO;
        if (ge_halted(g))
            return g->rSO;
    }
    return g->rSO;
}

/* --------------------------------------------------------------------------
 * bootstrap.card0_loads_to_alpha
 *
 * Full channel-level load of funktionalcpu.cap card 0, asserting:
 *   - state 0xe3 reached after one card
 *   - all 40 packed bytes match the bin oracle
 * -------------------------------------------------------------------------- */
UTEST(bootstrap, card0_loads_to_alpha)
{
    const char *cap_path = deck_funktionalcpu_cap();
    uint8_t oracle[40];
    uint16_t art_cols[80];

    /* Skip gracefully if the deck is not available */
    {
        FILE *p = fopen(cap_path, "r");
        if (!p) {
            printf("  [SKIP] %s not found\n", cap_path);
            return;
        }
        fclose(p);
    }

    /* The oracle is the deck itself. Find the bootstrap loader card the way the
     * reader does -- a row-8 punch in column 3 -- decode its 80 columns in
     * TC_HEX, and pack nibble pairs into 40 bytes, which is precisely what the
     * IPL's channel-1 packing transfer produces (see tests/initial-load.c).
     *
     * This replaces an oracle taken from the first 80 bytes of
     * ../DUMP1/funktionalcpu.bin. That file is a unified-format *scatter*
     * image, not a card dump, so the comparison had been skipping itself; .bin
     * is gone now, and the deck was always the better witness. */
    {
        struct cap_deck *deck = cap_load(cap_path);
        int loader = -1;

        ASSERT_TRUE(deck != NULL);
        for (int i = 0; i < cap_num_cards(deck) && i < 16; i++) {
            const uint16_t *cols = cap_card_columns(deck, i);
            if (cap_card_ncols(deck, i) >= 80 && cols && cols[2] == 0x0100) {
                loader = i;
                memcpy(art_cols, cols, sizeof art_cols);
                break;
            }
        }
        cap_free(deck);
        if (loader < 0) {
            printf("  [SKIP] %s carries no row-8 loader card\n", cap_path);
            return;
        }
    }

    for (int i = 0; i < 40; i++) {
        uint8_t hi = transcode_column(art_cols[2 * i],     TC_HEX) & 0x0f;
        uint8_t lo = transcode_column(art_cols[2 * i + 1], TC_HEX) & 0x0f;
        oracle[i] = (uint8_t)((hi << 4) | lo);
    }

    /* The loader's own first order: PER 0x80 (`9E 80`). */
    ASSERT_EQ((int)oracle[0], 0x9E);
    ASSERT_EQ((int)oracle[1], 0x80);

    /* ------------------------------------------------------------------ */
    /* Set up emulator                                                       */
    /* ------------------------------------------------------------------ */
    struct ge g;
    ge_init(&g);
    ge_clear(&g);
    ge_load_1(&g);   /* select connector 2 (card reader on channel 1) */
    ge_load(&g);     /* set AINI: on state 80 → c8 (load sequence)     */

    int rc = cardreader_register(&g, cap_path, TC_NORMAL);
    ASSERT_EQ(rc, 0);

    ge_start(&g);    /* clear ALTO: let the CPU run                     */

    /* ------------------------------------------------------------------ */
    /* Walk through the peri-init and transfer-setup states.               */
    /* These are identical to tests/initial-load.c :: load_1_button.       */
    /* ------------------------------------------------------------------ */

    /* State 00 - Display */
    ASSERT_EQ(g.rSO, 0x00);
    ge_run_cycle(&g);

    /* State 80 - Initialisation (AINI=1 → will go to c8) */
    ASSERT_EQ(g.rSO, 0x80);
    ge_run_cycle(&g);

    /* PER-PERI 1: peripheral type bits loaded into rL2 */
    ASSERT_EQ(g.rSO, 0xc8);
    /* rL2=0: input, non-output, increasing address, with packing, ch1/2 */
    ASSERT_EQ((int)g.rL2, 0x00);
    ge_run_cycle(&g);

    /* PER-PERI 2: peripheral unit name loaded into rRE (0x80 = connector 2) */
    ASSERT_EQ(g.rSO, 0xd8);
    ge_run_cycle(&g);
    ASSERT_EQ(g.rRE, 0x80);

    ASSERT_EQ(g.rSO, 0xd9); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xda); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xdb); ge_run_cycle(&g);

    /* PER-PERI 6 */
    ASSERT_EQ(g.rSO, 0xdc);
    ge_run_cycle(&g);

    /* PER-PERI 7: read command 0x40 loaded into rRE */
    ASSERT_EQ(g.rSO, 0xcc);
    ge_run_cycle(&g);
    ASSERT_EQ(g.rRE, 0x40);

    /* TPER-CPER 1: RIG1 cleared */
    ASSERT_EQ(g.rSO, 0xca);
    ge_run_cycle(&g);
    ASSERT_EQ((int)g.RIG1, 0);

    /* TPER-CPER 2-5: transfer parameters loaded */
    ASSERT_EQ(g.rSO, 0xa8); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xa9); ge_run_cycle(&g);
    ASSERT_EQ(g.rL1, 0x80); /* length = 0x80 = 128 nibbles */
    ASSERT_EQ(g.rSO, 0xaa); ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xab); ge_run_cycle(&g);
    ASSERT_EQ(g.rV1, 0);    /* load address = 0 */
    ASSERT_NE((int)g.RASI, 0); /* channel 1 in transfer */

    /* TPER-CPER 6: machine enters the input-wait loop (state 0xb8).
     * From here the cardreader peripheral feeds the 80 card columns
     * automatically, one byte per b8/b9 cycle pair. */
    ASSERT_EQ(g.rSO, 0xb8);

    /* ------------------------------------------------------------------ */
    /* Run the nibble-input loop until state 0xe3 (Alpha phase).           */
    /*                                                                      */
    /* Card 0 has 80 columns = 80 nibbles = 40 packed bytes.  Each byte    */
    /* pair takes ~4 machine cycles (b8→b9→b1→b8).  Allow extra headroom   */
    /* for the end-of-card sequence (b8→ea→eb→e3).                          */
    /* ------------------------------------------------------------------ */
    int final_state = bootstrap_run_until(&g, 0xe3, 8192);

    ASSERT_EQ(final_state, 0xe3);

    /* ------------------------------------------------------------------ */
    /* Verify loaded memory against oracle                                  */
    /* ------------------------------------------------------------------ */

    /* Full 40-byte comparison: one card, nibble-packed to address 0. */
    for (int i = 0; i < 40; i++) {
        if (g.mem[i] != oracle[i]) {
            printf("  [FAIL] mem[%2d]: got=0x%02x expected=0x%02x "
                   "(cols[%d]=0x%04x cols[%d]=0x%04x)\n",
                   i, g.mem[i], oracle[i],
                   2*i, art_cols[2*i], 2*i+1, art_cols[2*i+1]);
        }
        ASSERT_EQ((int)g.mem[i], (int)oracle[i]);
    }

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * bootstrap.channel_state_sequence
 *
 * Lighter-weight test: verify only the state transitions of the load
 * sequence without asserting memory contents.  This serves as a regression
 * guard for the peripheral state machine independently of the oracle data.
 * -------------------------------------------------------------------------- */
UTEST(bootstrap, channel_state_sequence)
{
    const char *cap_path = deck_funktionalcpu_cap();

    FILE *p = fopen(cap_path, "r");
    if (!p) {
        printf("  [SKIP] %s not found\n", cap_path);
        return;
    }
    fclose(p);

    struct ge g;
    ge_init(&g);
    ge_clear(&g);
    ge_load_1(&g);
    ge_load(&g);

    int rc = cardreader_register(&g, cap_path, TC_NORMAL);
    ASSERT_EQ(rc, 0);

    ge_start(&g);

    /* Walk to the start of the input loop */
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

    /* Run to alpha — machine must reach 0xe3 */
    int s = bootstrap_run_until(&g, 0xe3, 8192);
    ASSERT_EQ(s, 0xe3);

    /* mem[0] must be non-zero (first oracle byte is 0x47) */
    ASSERT_NE((int)g.mem[0], 0x00);

    /* Machine must not have halted before reaching alpha */
    ASSERT_EQ((int)ge_halted(&g), 0);

    ge_deinit(&g);
}
