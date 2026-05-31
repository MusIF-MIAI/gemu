/*
 * printer.c - Integrated printer / console-typewriter peripheral (channel 2).
 *
 * See printer.h for the model rationale. Summary of the completion mechanism,
 * derived empirically against the funktionalcpu report_and_end PER (0x19D0,
 * `PER 0x00,0x19EE`) and the channel-2 / TPER flow charts:
 *
 *   A channel-2 print/typewriter PER runs the CPU through the external-operation
 *   organisation phase (c8 d8 d9 .. a8 a9 aa ab) and parks it at state b8, the
 *   org-phase external request-wait. There the CPU suspends: RC00 drops, so the
 *   NA knot yields 0 (idle) and rSA stays 0 while rSO holds b8 pending. gemu has
 *   no channel-2 timing model to satisfy the wait, so it hangs.
 *
 *   The faithful exit is gated on DU97 = PUC2 ^ L2.3 (signals.h). With PUC2
 *   asserted (and RC00 raised so NA_knot routes rSO=b8 into rSA), state_b8's own
 *   CU01/CU13/CU14/CU06 commands build the PER-completion future_state and the
 *   sequencer returns to alpha at the post-PER instruction with PO/registers
 *   intact (verified: it then runs the real final-verify CMC at 0x19d4 cleanly).
 *
 * Discriminating the channel-2 print-wait from the channel-1 reader input-wait:
 *   State b8 is SHARED with the card reader (its inter-byte "waiting for next
 *   column" gap looks identical: rSO=b8, rSA=0, RASI=1, lu08=0, RC00=0). The
 *   reader peripheral presents a byte within a couple of cycles, breaking the
 *   wait; a genuine channel-2 print-wait persists. We therefore debounce with a
 *   stall counter: only after the wait has held unbroken for STALL_THRESHOLD
 *   on_clock calls do we treat it as a printer op and complete it. The reader's
 *   inter-byte gap never approaches that, so a concurrently-registered reader is
 *   never disturbed.
 */

#include "printer.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "gecode.h"

/* Consecutive idle b8-wait cycles before we conclude it is a channel-2 print
 * op and not a reader inter-byte gap. The reader presents within a few cycles;
 * 256 is comfortably above any inter-byte gap yet imperceptible to the user. */
#define STALL_THRESHOLD 256

/* Bound on bytes read from the print order block per completed PER. A printer
 * line is at most ~132 columns; cap well above that and stop at a NUL. */
#define CAPTURE_MAX 160

struct printer_ctx {
    int            stall;     /* consecutive idle channel-2 wait cycles */
    struct ge_peri peri;
};

/* Is the machine parked in the channel-2 org-phase external request-wait?
 * rSO=b8 pending, rSA=0 (CPU suspended/idle), CPU-active request not raised,
 * and the integrated reader is not presenting a byte (so this is not a reader
 * inter-byte gap that the reader is about to break). */
static int in_channel2_print_wait(struct ge *ge)
{
    return ge->rSO == 0xb8 &&
           ge->rSA == 0x00 &&
           !ge->RC00 &&
           ge->integrated_reader.lu08 == 0;
}

/* Best-effort capture of the print order block at completion: read up to
 * CAPTURE_MAX bytes from the channel-2 operand addresser (rV2), rendering each
 * through the GE 100-series internal graphic set (gecode.c) — the machine's own
 * character code, NOT ASCII — and stopping at a NUL. Non-graphic codes render
 * '.'. Honest about its limits — see printer.h. */
static void capture_line(struct ge *ge)
{
    struct ge_integrated_printer *p = &ge->integrated_printer;
    uint16_t addr = ge->rV2;
    int n = 0;

    for (n = 0; n < CAPTURE_MAX; n++) {
        uint16_t a = (uint16_t)(addr + n);
        uint8_t b = ge->mem[a];
        if (b == 0x00)
            break;
        if (p->out_len >= (int)sizeof(p->out) - 2)
            break;
        p->out[p->out_len++] = ge_glyph(b);
    }
    if (p->out_len < (int)sizeof(p->out) - 1)
        p->out[p->out_len++] = '\n';
    p->out[p->out_len] = '\0';

    ge_log(LOG_PERI,
           "printer: completed channel-2 PER, captured %d byte(s) from V2=0x%04x\n",
           n, addr);
}

/*
 * on_clock: called at TO00, the first clock of every machine cycle, before the
 * MSL chart for the cycle runs. We watch for the channel-2 print-wait; once it
 * has stalled past the debounce threshold we capture the line and assert the
 * unit-ready / CPU-active conditions so the machine's own state_b8 microcode
 * completes the PER (B8 -> alpha) on the next cycle.
 */
static int printer_on_clock(struct ge *ge, void *opaque)
{
    struct printer_ctx *ctx = (struct printer_ctx *)opaque;
    struct ge_integrated_printer *p = &ge->integrated_printer;

    if (!p->present)
        return 0;

    /* Channel-2 OUTPUT transfer engine: drive one character per cycle. While
     * bytes remain, hold the channel-2 request (RC02) and the rSI output state
     * so NA_knot routes this cycle to state_02 (mem[V4] -> RO -> CE16 -> sink).
     * RC02 is asserted at TO00, before pulse() latches RIA2 = RC02. When the
     * count is exhausted, drop the request and end the line. */
    if (p->out_active) {
        if (p->out_remaining > 0) {
            ge->RC02 = 1;
            ge->rSI  = 0x02;
            p->out_remaining--;
        } else {
            ge->RC02 = 0;
            if (p->out_len < (int)sizeof(p->out) - 1) {
                p->out[p->out_len++] = '\n';   /* end-of-print (models 0A/0B/FIRU) */
                p->out[p->out_len] = '\0';
            }
            p->out_active = 0;
        }
        return 0;
    }

    if (!in_channel2_print_wait(ge)) {
        ctx->stall = 0;
        return 0;
    }

    if (++ctx->stall < STALL_THRESHOLD)
        return 0;

    /* Genuine channel-2 print-wait: capture + complete via the native path. */
    capture_line(ge);
    ge->PUC2 = 1;   /* channel-2 unit ready  -> DU97 = 1 (PUC2 ^ L2.3) */
    ge->RC00 = 1;   /* CPU-active request    -> NA_knot routes rSO=b8 into rSA */
    ctx->stall = 0; /* one-shot; rSO leaves b8 next cycle */
    return 0;
}

static int printer_deinit(struct ge *ge, void *opaque)
{
    (void)ge;
    free(opaque);
    return 0;
}

/* Channel-2 OUTPUT sink: the CPU's channel-2 transfer microcode hands one
 * character to the printer here (command CE16 "Load Printer Buffer", rSI state
 * 02/03). The byte is the machine's internal GE graphic code; render it through
 * the GE 100-series glyph table into the paper-feed buffer (same buffer the
 * front-ends drain). See docs/peripherals.md "CAN2 data-transfer phase". */
static void printer_sink(struct ge *ge, struct ge_channel *ch, uint8_t c)
{
    struct ge_integrated_printer *p = &ge->integrated_printer;
    (void)ch;

    if (p->out_len < (int)sizeof(p->out) - 1)
        p->out[p->out_len++] = ge_glyph(c);
    p->out[p->out_len] = '\0';
}

int printer_register(struct ge *ge)
{
    struct printer_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return -1;

    ge->channel2.sink = printer_sink;
    ge->integrated_printer.present = 1;
    ge->integrated_printer.out_len = 0;
    ge->integrated_printer.out[0] = '\0';
    ge->integrated_printer.kbd_head = 0;
    ge->integrated_printer.kbd_tail = 0;

    ctx->stall         = 0;
    ctx->peri.next     = NULL;
    ctx->peri.init     = NULL;
    ctx->peri.on_pulse = NULL;
    ctx->peri.on_clock = printer_on_clock;
    ctx->peri.deinit   = printer_deinit;
    ctx->peri.ctx      = ctx;

    return ge_register_peri(ge, &ctx->peri);
}

void printer_begin_output(struct ge *ge, uint16_t buffer, int length)
{
    ge->rV4 = buffer;
    ge->integrated_printer.out_active    = 1;
    ge->integrated_printer.out_remaining = length;
}

void printer_feed_key(struct ge *ge, uint8_t c)
{
    struct ge_integrated_printer *p = &ge->integrated_printer;
    int next = (p->kbd_tail + 1) % (int)sizeof(p->kbd);
    if (next == p->kbd_head)
        return; /* queue full; drop */
    p->kbd[p->kbd_tail] = c;
    p->kbd_tail = next;
}

int printer_output_len(struct ge *ge)
{
    return ge->integrated_printer.out_len;
}

const char *printer_output(struct ge *ge)
{
    return ge->integrated_printer.out;
}

void printer_output_clear(struct ge *ge)
{
    ge->integrated_printer.out_len = 0;
    ge->integrated_printer.out[0] = '\0';
}
