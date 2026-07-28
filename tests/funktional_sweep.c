/*
 * tests/funktional_sweep.c - the cycle-exact CPU functional sweep.
 *
 * This is the regression that catches timing-chart drift, and nothing else in
 * the suite does. The failure mode it exists for is invisible in results: an
 * opcode that runs both a hybrid one-shot AND its real executive states still
 * produces the right bytes, just in far fewer cycles. So the assertion is the
 * cycle count, exactly.
 *
 *   A genuine fidelity change SHOULD move these numbers. Update them
 *   deliberately, having checked the step trace is unchanged.
 *
 * It used to live in tests/roundtrip.sh, driven through `gdis --image` into a
 * .bin and `ge --poke 0x0E00=...`. Both are gone -- the machine takes programs
 * on cards and nothing else. What remains here is scaffolding, openly: the deck
 * is decoded in-process with cap_load_scattered (a test oracle, never a load
 * path) and the console test-selection byte is written the way the operator
 * keys it in at the maintenance panel (rotary pos 8, V1 SCR -- docs/console.md
 * 6.3), because reaching that state through the panel is a separate test.
 *
 * The reader-fed path for this same deck is covered by
 * cardreader.funktionalcpu_authentic_load_reaches_payload.
 */

#include "utest.h"
#include "decks.h"
#include "../ge.h"
#include "../cap.h"
#include "../transcode.h"
#include "../log.h"
#include "../printer.h"

#include <stdio.h>
#include <string.h>

struct sweep_result {
    int    halted;
    long   cycles;
    uint16_t po;
    int    decode_errors;
};

static int run_sweep(uint8_t option, long max_cycles, struct sweep_result *r)
{
    static uint8_t scat[MEM_SIZE];
    const char *cap_path = deck_funktionalcpu_cap();
    unsigned lo = 0, hi = 0;
    struct ge g;
    long i;

    memset(scat, 0, sizeof scat);
    if (cap_load_scattered(cap_path, TC_COLBIN, scat, &lo, &hi) <= 0)
        return -1;

    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    if (ge_load_image(&g, scat + lo, (uint16_t)(hi - lo + 1), (uint16_t)lo) != 0) {
        ge_deinit(&g);
        return -1;
    }
    ge_seed_segment_bases(&g);

    /* The operator's key-in: console test selection. */
    ge_mem_store8(&g, 0x0E00, option);

    /* The sweep ends by reporting over the channel-2 print PER. Without a
     * printer the machine parks in the b8 external request-wait forever and
     * never reaches its HLT (see tests/printer.c). */
    printer_register(&g);

    ge_start(&g);
    ge_enter(&g, (uint16_t)lo);

    for (i = 0; i < max_cycles && !ge_halted(&g); i++) {
        if (ge_run_cycle(&g) != 0)
            break;
    }

    r->halted = ge_halted(&g);
    r->cycles = i;
    r->po     = g.rPO;
    r->decode_errors = 0;

    ge_deinit(&g);
    return 0;
}

/* Option 0x00: no test selected. The deck converges straight to its documented
 * idle halt. */
UTEST(funktional_sweep, idle_halt_when_no_test_selected)
{
    struct sweep_result r;

    if (run_sweep(0x00, 200000, &r) != 0) {
        printf("  [SKIP] funktionalcpu deck not found\n");
        return;
    }

    ASSERT_TRUE(r.halted);
    ASSERT_EQ((int)r.po, 0x175A);
}

/* Option 0x40: the CPU functional sweep across the whole instruction set. */
UTEST(funktional_sweep, cpu_functional_sweep_is_cycle_exact)
{
    struct sweep_result r;

    if (run_sweep(0x40, 3000000, &r) != 0) {
        printf("  [SKIP] funktionalcpu deck not found\n");
        return;
    }

    ASSERT_TRUE(r.halted);
    ASSERT_EQ((int)r.po, 0x1427);
    ASSERT_EQ(r.cycles, 1341373L);
}
