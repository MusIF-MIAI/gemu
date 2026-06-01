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

struct printer_ctx {
    int            stall;       /* consecutive idle channel-2 wait cycles */
    int            per_pending; /* a channel-2 PER org-phase is in progress */
    uint16_t       per_base;    /* its order-block base (rV1 at state c8) */
    struct ge_peri peri;
};

/* A channel-2 PER order block is {z, cmd, len_hi, len_lo, buf_hi, buf_lo}
 * (perperi-validated). A put/print command has bit 7 set; a length beyond a
 * print line is taken as a control PER (e.g. funktionalcpu's banner, whose
 * 2-byte order overlaps the following code) and ignored. */
#define PRINT_CMD_IS_PUT(cmd) ((cmd) & 0x80)
#define PRINT_LEN_MAX 256
#define KBD_CMD_LINE 0x40
#define KBD_CMD_CHAR 0x41

#define STDIO_STATUS_ADDR 0x0030
#define STDIO_COUNT_ADDR  0x0032

static void store16(struct ge *ge, uint16_t addr, uint16_t value)
{
    ge_mem_store8(ge, addr, (uint8_t)(value >> 8));
    ge_mem_store8(ge, (uint16_t)(addr + 1), (uint8_t)value);
}

static int kbd_ready(const struct ge_integrated_printer *p)
{
    return p->kbd_head != p->kbd_tail;
}

static int kbd_has_complete_line(const struct ge_integrated_printer *p)
{
    for (int i = p->kbd_head; i != p->kbd_tail; i = (i + 1) % (int)sizeof(p->kbd)) {
        if (p->kbd[i] == '\n' || p->kbd[i] == '\r')
            return 1;
    }
    return 0;
}

static uint8_t kbd_pop(struct ge_integrated_printer *p)
{
    uint8_t c = p->kbd[p->kbd_head];
    p->kbd_head = (p->kbd_head + 1) % (int)sizeof(p->kbd);
    return c;
}

static int service_input_line(struct ge *ge, uint16_t buf, int len)
{
    struct ge_integrated_printer *p = &ge->integrated_printer;
    int max = (len > 0) ? len - 1 : 0;
    int n = 0;

    while (kbd_ready(p)) {
        uint8_t c = kbd_pop(p);
        if (c == '\r')
            continue;
        if (c == '\n')
            break;
        if (n < max)
            ge_mem_store8(ge, (uint16_t)(buf + n++), ge_code(c));
    }
    if (len > 0)
        ge_mem_store8(ge, (uint16_t)(buf + n), 0x00);
    store16(ge, STDIO_COUNT_ADDR, (uint16_t)n);
    store16(ge, STDIO_STATUS_ADDR, 1);
    return n;
}

static int service_input_char(struct ge *ge, uint16_t buf, int len)
{
    struct ge_integrated_printer *p = &ge->integrated_printer;
    uint8_t c;

    if (!kbd_ready(p))
        return 0;
    c = kbd_pop(p);
    if (c == '\r')
        c = '\n';
    if (len > 0)
        ge_mem_store8(ge, buf, ge_code(c));
    store16(ge, STDIO_COUNT_ADDR, len > 0 ? 1 : 0);
    store16(ge, STDIO_STATUS_ADDR, len > 0 ? 1 : 0);
    return len > 0 ? 1 : 0;
}

static int input_order_ready(struct ge *ge, uint8_t cmd)
{
    if (cmd == KBD_CMD_CHAR)
        return kbd_ready(&ge->integrated_printer);
    if (cmd == KBD_CMD_LINE)
        return kbd_has_complete_line(&ge->integrated_printer);
    return 0;
}

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
            /* Transfer done: drop the request (so this cycle is no longer a
             * channel-2 cycle and the CPU resumes), restore the CPU sequencer
             * the stolen cycles clobbered, and end the line. */
            ge->RC02 = 0;
            ge->rSO  = p->out_saved_so;
            store16(ge, STDIO_COUNT_ADDR, (uint16_t)p->out_total);
            store16(ge, STDIO_STATUS_ADDR, 1);
            p->out_active = 0;
        }
        return 0;
    }

    /* Arm an output transfer from a channel-2 print PER. At the org-phase entry
     * (SO=0xc8) rV1 is the order-block base; when the PER returns to alpha (e2),
     * decode the order and, for a put command with a plausible length, start the
     * transfer engine (which then drains the buffer over channel 2). */
    if (ge->rSO == 0xc8) {
        ctx->per_pending = 1;
        ctx->per_base = ge->rV1;
        /* Start each PER's org phase with a clean channel-2 handshake. PUC2 is
         * a flip-flop the real machine clears via the L2.3 toggle; the model
         * never toggles L2.3, so an input completion that asserted PUC2 would
         * otherwise leave it stuck and make DU97 fire prematurely in the next
         * PER (jamming its org phase). Clear it here. */
        ge->PUC2 = 0;
    } else if (ctx->per_pending && ge->rSO == 0xe2) {
        uint16_t base = ctx->per_base;
        uint8_t  cmd  = ge->mem[(uint16_t)(base + 1)];
        int      len  = (ge->mem[(uint16_t)(base + 2)] << 8) |
                         ge->mem[(uint16_t)(base + 3)];
        uint16_t buf  = (ge->mem[(uint16_t)(base + 4)] << 8) |
                         ge->mem[(uint16_t)(base + 5)];
        ctx->per_pending = 0;
        if (PRINT_CMD_IS_PUT(cmd) && len >= 1 && len <= PRINT_LEN_MAX)
            printer_begin_output(ge, buf, len);
        else if (cmd == KBD_CMD_LINE && len >= 1 && len <= PRINT_LEN_MAX)
            service_input_line(ge, buf, len);
        else if (cmd == KBD_CMD_CHAR && len >= 1 && len <= PRINT_LEN_MAX)
            service_input_char(ge, buf, len);
    }

    /* Input orders (read line / read char) park the CPU at the channel-2
     * external request-wait (rSO=b8) with RC00 still asserted: unlike an
     * output/control PER, no transfer cycles ran to reset it via CE18, so the
     * print-wait detector below (which requires !RC00) never fires. Complete
     * the read here, independent of RC00, as soon as the requested input is
     * available (a full line for KBD_CMD_LINE, any key for KBD_CMD_CHAR). */
    if (ctx->per_pending && ge->rSO == 0xb8) {
        uint8_t cmd = ge->mem[(uint16_t)(ctx->per_base + 1)];
        if ((cmd == KBD_CMD_LINE || cmd == KBD_CMD_CHAR) &&
            input_order_ready(ge, cmd)) {
            ge->PUC2 = 1;   /* channel-2 unit ready -> DU97 completes the PER */
            ge->RC00 = 1;   /* CPU-active request   -> rSO=b8 routed into rSA */
            ctx->stall = 0;
            return 0;
        }
    }

    if (!in_channel2_print_wait(ge)) {
        ctx->stall = 0;
        return 0;
    }

    if (ctx->per_pending) {
        uint8_t cmd = ge->mem[(uint16_t)(ctx->per_base + 1)];
        if ((cmd == KBD_CMD_LINE || cmd == KBD_CMD_CHAR) &&
            input_order_ready(ge, cmd)) {
            ge->PUC2 = 1;
            ge->RC00 = 1;
            ctx->stall = 0;
            return 0;
        }
    }

    if (++ctx->stall < STALL_THRESHOLD)
        return 0;

    /* A channel-2 PER parked at the b8 external request-wait that is NOT a data
     * print (no order-block transfer was armed above) — e.g. a control/order PER
     * such as funktionalcpu's banner/report. Complete it via the native path so
     * the CPU resumes, but emit nothing: real printed text comes only from an
     * armed output transfer, not from heuristically scraping the order block. */
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
    ge->integrated_printer.out_total     = length;
    ge->integrated_printer.out_saved_so  = ge->rSO;  /* resume here when done */
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
