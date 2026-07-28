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
 *   State b8 is SHARED with the card reader, and so is the whole org phase that
 *   leads to it (c8 d8 ... ab). The machine's own answer to "whose order is
 *   this?" is the connector decode: PC121 = PUC11 . PB071 . PB06A is connector 2
 *   on channel 1 -- the integrated card reader -- being the selected peripheral.
 *   Measured at the wait itself: the reader's order shows PC121=1, a genuine
 *   channel-2 print PER shows PC121=0. So the printer keeps out of any wait
 *   PC121 claims (see channel1_reader_owns_the_order).
 *
 *   That check has to happen at b8, not at c8 where the order is first noticed:
 *   PB06/PB07 are latched by CE02 later in the org phase, so PC121 is still 0 at
 *   c8. The claim made at c8 is therefore provisional, and is disowned at b8.
 *
 *   Beyond that, a genuine channel-2 print-wait persists where a reader
 *   inter-byte gap does not, so a stall counter debounces the remaining cases:
 *   only after the wait has held unbroken for STALL_THRESHOLD on_clock calls do
 *   we treat it as a printer op and complete it.
 */

#include "printer.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "gecode.h"
#include "signals.h"

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
/* Line printer (LP/PRT, connector 2): the WRITE/print order is the same 6-byte
 * block {z, cmd, len_hi, len_lo, buf_hi, buf_lo} as the console typewriter, but
 * with command byte 0x42 (WRITE) instead of the typewriter's PUT (bit 7 set) —
 * verified from printermechanicaltest.cap (order {51 42 00 9F 02 02} prints 159
 * bytes from 0x0202) and the PRT mechanic-test listing ("X'C0',WRITE"). Unlike
 * the typewriter output it parks the CPU at the b8 external request-wait with
 * RC00 still set, so it is released there (like an input order) and the actual
 * transfer is armed when the PER returns to alpha. */
#define LP_CMD_WRITE 0x42
#define IS_OUTPUT_CMD(cmd) (PRINT_CMD_IS_PUT(cmd) || (cmd) == LP_CMD_WRITE)

#define STDIO_STATUS_ADDR 0x0030
#define STDIO_COUNT_ADDR  0x0032
#define LP_CMD_SINGLE_SPACE 0x2E
#define LP_CMD_TRIPLE_SPACE 0x5A

static void store16(struct ge *ge, uint16_t addr, uint16_t value)
{
    ge_mem_store8(ge, addr, (uint8_t)(value >> 8));
    ge_mem_store8(ge, (uint16_t)(addr + 1), (uint8_t)value);
}

static void printer_capture_char(struct ge_integrated_printer *p, char c)
{
    if (p->out_len >= (int)sizeof(p->out) - 1)
        return;
    p->out[p->out_len++] = c;
    p->out[p->out_len] = '\0';
}

static void printer_capture_breaks(struct ge_integrated_printer *p, int count)
{
    for (int i = 0; i < count; i++)
        printer_capture_char(p, '\n');
}

static void printer_capture_control(struct ge_integrated_printer *p, uint8_t cmd)
{
    switch (cmd) {
        case LP_CMD_SINGLE_SPACE:
            printer_capture_breaks(p, 1);
            break;
        case LP_CMD_TRIPLE_SPACE:
            printer_capture_breaks(p, 3);
            break;
        default:
            break;
    }
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

/*
 * Does the card reader own the order the machine is waiting on?
 *
 * PC121 is the machine's own decode: connector 2 on channel 1 -- the integrated
 * card reader -- is the selected peripheral. State b8 is shared between the
 * channel-1 reader input-wait and the channel-2 print-wait and they are
 * otherwise indistinguishable from here (both show PUC1=1, RASI=1, and the
 * reader's IPL order carries no order block in memory at all, so its "command"
 * byte reads as whatever happens to be at mem[1]).
 *
 * Answering a reader order is not a cosmetic error: PUC2 goes up, DU97 fires,
 * state_b8's microcode completes the PER, and the IPL falls through to alpha at
 * address 0 having read nothing -- the machine stops at cycle 17 with a deck
 * still in the hopper.
 */
static int channel1_reader_owns_the_order(struct ge *ge)
{
    /* LESAB says a card reader is physically on connector 2, so there is a unit
     * of its own to answer this order. Both halves are needed: PC121 alone is
     * just "connector 2 on channel 1", and gemu's channel-2 order blocks are
     * loose enough about the Z byte (CE02 takes the channel from L2 bit 0) that
     * some of them decode that way too. With a reader on the connector there is
     * no ambiguity left -- the order is its. */
    return PC121(ge) && ge->integrated_reader.lesab;
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
            if (p->out_line_mode)
                printer_capture_breaks(p, 1);
            store16(ge, STDIO_COUNT_ADDR, (uint16_t)p->out_total);
            store16(ge, STDIO_STATUS_ADDR, 1);
            p->out_active = 0;
            p->out_line_mode = 0;
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
        if (IS_OUTPUT_CMD(cmd) && len >= 1 && len <= PRINT_LEN_MAX)
            printer_begin_output(ge, buf, len, cmd == LP_CMD_WRITE);
        else if (cmd == KBD_CMD_LINE && len >= 1 && len <= PRINT_LEN_MAX)
            service_input_line(ge, buf, len);
        else if (cmd == KBD_CMD_CHAR && len >= 1 && len <= PRINT_LEN_MAX)
            service_input_char(ge, buf, len);
    }

    /* Whose wait is this? PC121 is only meaningful once the org phase has
     * latched the connector select, which is why the claim at c8 above is
     * provisional: disown it here if the order turns out to be the reader's,
     * and take no further part in this cycle. */
    if (ge->rSO == 0xb8 && channel1_reader_owns_the_order(ge)) {
        ctx->per_pending = 0;
        ctx->stall = 0;
        return 0;
    }

    /* Input orders (read line / read char) park the CPU at the channel-2
     * external request-wait (rSO=b8) with RC00 still asserted: unlike an
     * output/control PER, no transfer cycles ran to reset it via CE18, so the
     * print-wait detector below (which requires !RC00) never fires. Complete
     * the read here, independent of RC00, as soon as the requested input is
     * available (a full line for KBD_CMD_LINE, any key for KBD_CMD_CHAR). */
    if (ctx->per_pending && ge->rSO == 0xb8) {
        uint8_t cmd = ge->mem[(uint16_t)(ctx->per_base + 1)];
        int     len = (ge->mem[(uint16_t)(ctx->per_base + 2)] << 8) |
                       ge->mem[(uint16_t)(ctx->per_base + 3)];
        /* A line-printer WRITE parks here with RC00 set; release it so the PER
         * returns to alpha, where the e2 arm starts the buffer transfer. */
        int write_ready = (cmd == LP_CMD_WRITE && len >= 1 && len <= PRINT_LEN_MAX);
        int input_ready = (cmd == KBD_CMD_LINE || cmd == KBD_CMD_CHAR) &&
                          input_order_ready(ge, cmd);
        /* Any OTHER channel-2 order parked here with RC00 still set is a printer
         * control op (paper spacing, jump-to-channel, drum/ribbon select) that
         * transfers no data: the device accepts it immediately. Complete it here
         * — the RC00=0 path below (stall debounce vs the card reader's b8 gap)
         * cannot, because RC00 never dropped. Gated on RC00 so a reader inter-byte
         * gap (RC00=0) and funktionalcpu's RC00=0 control PER are left to it. */
        int control_ready = ge->RC00 &&
                            cmd != KBD_CMD_LINE &&
                            cmd != KBD_CMD_CHAR &&
                            !write_ready;
        if (write_ready || input_ready || control_ready) {
            if (control_ready)
                printer_capture_control(p, cmd);
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
    if (ctx->per_pending) {
        uint8_t cmd = ge->mem[(uint16_t)(ctx->per_base + 1)];
        printer_capture_control(p, cmd);
    }
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

    printer_capture_char(p, ge_glyph(c));
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

void printer_begin_output(struct ge *ge, uint16_t buffer, int length, int line_mode)
{
    ge->rV4 = buffer;
    ge->integrated_printer.out_active    = 1;
    ge->integrated_printer.out_remaining = length;
    ge->integrated_printer.out_total     = length;
    ge->integrated_printer.out_line_mode = line_mode;
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
