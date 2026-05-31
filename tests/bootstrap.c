/*
 * tests/bootstrap.c - Channel-level bootstrap load test for the GE-120 emulator.
 *
 * Loads the real diagnostic deck ../DUMP1/funktionalcpu.cap through the
 * emulated card-reader peripheral (cardreader_register) and verifies that:
 *
 *   1. The machine reaches state 0xe3 (Alpha phase) after loading exactly
 *      one card (80 columns = 40 packed bytes).
 *
 *   2. mem[0..39] exactly match the oracle nibble-packing of the first 80
 *      bytes of ../DUMP1/funktionalcpu.bin:
 *
 *        oracle[i] = (bin[2*i] & 0x0F) << 4 | (bin[2*i+1] & 0x0F)
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
#include "../ge.h"
#include "../cardreader.h"
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
        if (g->halted)
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
    static const char cap_path[] = "../DUMP1/funktionalcpu.cap";
    static const char bin_path[] = "../DUMP1/funktionalcpu.bin";

    /* Skip gracefully if oracle files are not available */
    {
        FILE *p = fopen(cap_path, "r");
        if (!p) {
            printf("  [SKIP] %s not found\n", cap_path);
            return;
        }
        fclose(p);
        p = fopen(bin_path, "rb");
        if (!p) {
            printf("  [SKIP] %s not found\n", bin_path);
            return;
        }
        fclose(p);
    }

    /* Load oracle: first 80 bytes of .bin = first card (80 columns).
     *
     * STALE-ORACLE GUARD: this oracle assumes funktionalcpu.bin is a raw,
     * headerless dump of card 0's nibbles. That is no longer true — the .bin is
     * now a unified-format image (12-byte "GE12" header + a *scatter* image
     * placing each card's payload at its embedded `41 addr` target), so its
     * first bytes are the header/scatter-origin region, NOT card 0's loader.
     * The authentic card-0 load is exercised (and passes) by
     * bootstrap.channel_state_sequence and cardreader.* against the .cap deck.
     * Skip here until this test is repointed to a .cap-decode oracle. */
    uint8_t bin_card0[80];
    {
        FILE *f = fopen(bin_path, "rb");
        ASSERT_NE(f, NULL);
        size_t nr = fread(bin_card0, 1, 80, f);
        fclose(f);
        ASSERT_EQ((int)nr, 80);
        if (bin_card0[0] == 'G' && bin_card0[1] == 'E' &&
            bin_card0[2] == '1' && bin_card0[3] == '2') {
            printf("  [SKIP] funktionalcpu.bin is a unified-format scatter image; "
                   ".bin-derived card-0 oracle is stale (see channel_state_sequence)\n");
            return;
        }
    }

    /* Compute oracle mem[0..39]:
     *   oracle[i] = (bin[2i] & 0x0F) << 4 | (bin[2i+1] & 0x0F)
     * This matches the GE nibble-packing observed in tests/initial-load.c:
     *   two consecutive nibbles (low nibbles of two successive bytes) are
     *   packed into one 8-bit memory cell. */
    uint8_t oracle[40];
    for (int i = 0; i < 40; i++)
        oracle[i] = (uint8_t)(((bin_card0[2*i] & 0x0Fu) << 4) |
                              (bin_card0[2*i+1] & 0x0Fu));

    /* Spot-check: bin[0]=0xF4, bin[1]=0xF7 → (4)<<4 | 7 = 0x47 */
    ASSERT_EQ((int)oracle[0], 0x47);

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
    /* These are identical to tests/cardreader.c :: funktionalcpu_first_card
     * and tests/initial-load.c :: load_1_button.                          */
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

    /* First spot-check */
    ASSERT_EQ((int)g.mem[0], 0x47);

    /* Full 40-byte comparison */
    for (int i = 0; i < 40; i++) {
        if (g.mem[i] != oracle[i]) {
            printf("  [FAIL] mem[%2d]: got=0x%02x expected=0x%02x "
                   "(bin[%d]=0x%02x bin[%d]=0x%02x)\n",
                   i, g.mem[i], oracle[i],
                   2*i, bin_card0[2*i], 2*i+1, bin_card0[2*i+1]);
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
    static const char cap_path[] = "../DUMP1/funktionalcpu.cap";

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
    ASSERT_EQ((int)g.halted, 0);

    ge_deinit(&g);
}
