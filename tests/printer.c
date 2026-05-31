/*
 * tests/printer.c - Integrated printer / console-typewriter (channel 2) model.
 *
 * Locks in the pragmatic channel-2 completion behaviour (printer.c):
 *
 *   printer.present_completes_channel2_per
 *     The funktionalcpu report_and_end routine issues a channel-2 print PER
 *     (0x19D0, `PER 0x00,0x19EE`). Without a printer the CPU suspends in the
 *     org-phase external request-wait (state b8, rSA idle) forever, because
 *     gemu does not drive channel-2 timing at signal level. With the printer
 *     registered, printer_on_clock asserts PUC2 (-> DU97) and RC00, so the
 *     machine's own state_b8 microcode completes the PER and returns to alpha
 *     at the post-PER instruction (the final-verify CMC at 0x19d4). We assert
 *     the run reaches that post-PER code and that output was captured.
 *
 *   printer.absent_leaves_machine_waiting
 *     Same image, no printer: the machine never reaches the post-PER code
 *     within the budget (it parks at the b8 wait). This proves the model is
 *     inert unless registered (bootstrap/reader tests are unaffected).
 *
 *   printer.keyboard_queue
 *     printer_feed_key enqueues operator-keyboard bytes (two-way input side).
 *
 * Uses ../DUMP1/funktionalcpu.bin (the depunched oracle image); skips cleanly
 * if the fixture is absent.  No UTEST_MAIN here; the runner provides it.
 */

#include "utest.h"
#include "../ge.h"
#include "../printer.h"
#include "../binimage.h"
#include "../log.h"

#include <stdio.h>

static const char BIN_PATH[] = "../DUMP1/funktionalcpu.bin";

/* Load the funktionalcpu image, set the 0x40 (CPU functional + memory) test
 * option, and run with SWITCH 2 off. Returns 1 if the run progressed past the
 * channel-2 print PER (did NOT end parked in the b8 external request-wait),
 * 0 if it ended parked there. *out_len receives captured bytes.
 * Returns -1 if the fixture is missing.
 *
 * Discriminator note: rPO is the PER's already-set return address (0x19d4)
 * throughout the b8 wait, so a PO range can't tell "waiting" from "ran". The
 * real signal is the sequencer: a stalled print-wait sits with rSO==0xb8 and
 * rSA==0 (idle); a completed one has left that wait. */
static int run_option40(int with_printer, int *out_len)
{
    static uint8_t buf[MEM_SIZE];
    uint16_t org, ent, len;

    FILE *f = fopen(BIN_PATH, "rb");
    if (!f)
        return -1;
    int rc = binimage_read(f, &org, &ent, buf, sizeof buf, &len);
    fclose(f);
    if (rc != BINIMAGE_OK)
        return -1;

    struct ge g;
    ge_init(&g);
    ge_log_set_active_types(0);
    ge_clear(&g);
    ge_load_image(&g, buf, len, org);
    ge_seed_segment_bases(&g);

    /* Console test-selection byte: 0x40 = CPU functional + core-memory tests. */
    g.mem[0x0E00] = 0x40;
    g.mem_parity[0x0E00] = __builtin_parity(0x40) ? 0 : 1;
    g.mem_written[0x0E00] = 1;

    if (with_printer)
        printer_register(&g);

    ge_start(&g);
    ge_enter(&g, ent);

    /* The report_and_end print PER is reached around cycle ~656k; the printer
     * fires STALL_THRESHOLD cycles later. Budget comfortably past that. With a
     * printer we stop as soon as it captures (completion proof) so we never run
     * into the deck's continuous-test restart loop; without one we run the full
     * budget and observe the machine parked in the b8 wait. */
    for (long i = 0; i < 800000; i++) {
        g.JS2 = 0;
        if (ge_run_cycle(&g) != 0)
            break;
        if (g.halted)
            break;
        if (with_printer && printer_output_len(&g) > 0)
            break;   /* printer completed the PER and captured output */
    }

    int parked = (g.rSO == 0xb8 && g.rSA == 0x00);

    if (out_len)
        *out_len = with_printer ? printer_output_len(&g) : 0;

    ge_deinit(&g);
    return parked ? 0 : 1;
}

UTEST(printer, present_completes_channel2_per)
{
    int out_len = 0;
    int r = run_option40(1, &out_len);
    if (r < 0) {
        UTEST_SKIP("fixture ../DUMP1/funktionalcpu.bin not present");
        return;
    }
    /* The print PER completed and execution returned to the post-PER code. */
    ASSERT_EQ(r, 1);
    /* Something was captured into the paper-feed buffer. */
    ASSERT_GT(out_len, 0);
}

UTEST(printer, absent_leaves_machine_waiting)
{
    int r = run_option40(0, NULL);
    if (r < 0) {
        UTEST_SKIP("fixture ../DUMP1/funktionalcpu.bin not present");
        return;
    }
    /* No printer: the channel-2 print PER never completes, so the post-PER
     * code is never reached within the budget (parks at the b8 wait). */
    ASSERT_EQ(r, 0);
}

UTEST(printer, keyboard_queue)
{
    struct ge g;
    ge_init(&g);
    printer_register(&g);

    ASSERT_EQ(g.integrated_printer.kbd_head, g.integrated_printer.kbd_tail);
    printer_feed_key(&g, 'A');
    printer_feed_key(&g, 'B');
    /* tail advanced by two; head untouched (nothing consumed yet). */
    ASSERT_EQ(g.integrated_printer.kbd_head, 0);
    ASSERT_EQ(g.integrated_printer.kbd_tail, 2);
    ASSERT_EQ(g.integrated_printer.kbd[0], (uint8_t)'A');
    ASSERT_EQ(g.integrated_printer.kbd[1], (uint8_t)'B');

    ge_deinit(&g);
}

/* --------------------------------------------------------------------------
 * printer.channel2_output_transfer
 *
 * Drive the channel-2 OUTPUT data-transfer microcode directly: with the printer
 * registered (channel2.sink), a held channel-2 request (RC02) makes NA_knot route
 * each cycle to rSI state 0x02, which reads mem[V4] -> RO and hands it to the
 * printer via CE16 ("Load Printer Buffer"), advancing V4. The bytes are GE
 * internal graphic codes; the sink renders them through the glyph table. This is
 * the faithful memory->printer datapath (flow chart 14023130_1 sheet 36); the
 * org-phase routing that sets up rSI/RC02 from a real output PER is wired
 * separately.
 * -------------------------------------------------------------------------- */
UTEST(printer, channel2_output_transfer)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);
    printer_register(&g);   /* attaches channel2.sink */

    /* "HELLO" in the GE 100-series graphic code: H=0x58 E=0x55 L=0xA3 O=0xA6 */
    static const uint8_t s[5] = { 0x58, 0x55, 0xA3, 0xA3, 0xA6 };
    const uint16_t buf = 0x0200;
    for (int i = 0; i < 5; i++) {
        g.mem[buf + i] = s[i];
        g.mem_parity[buf + i] = __builtin_parity(s[i]) ? 0 : 1;
        g.mem_written[buf + i] = 1;
    }

    g.rV4 = buf;
    ge_start(&g);

    /* Five channel-2 (RES2) cycles; each runs rSI state 0x02 and emits one byte. */
    for (int i = 0; i < 5; i++) {
        g.RC02 = 1;      /* channel-2 request -> RIA2 -> RES2 */
        g.rSI  = 0x02;   /* channel-2 output transfer state */
        ge_run_cycle(&g);
    }

    ASSERT_EQ(printer_output_len(&g), 5);
    const char *o = printer_output(&g);
    ASSERT_EQ(o[0], 'H');
    ASSERT_EQ(o[1], 'E');
    ASSERT_EQ(o[2], 'L');
    ASSERT_EQ(o[3], 'L');
    ASSERT_EQ(o[4], 'O');

    ge_deinit(&g);
}
