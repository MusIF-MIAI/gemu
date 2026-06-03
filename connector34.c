/*
 * connector34.c — shared core for Standard-GE-100 controllers on connectors
 * 3 and 4 (disk / tape). See connector34.h for the architecture overview.
 *
 * Functional / channel-API model. The core registers one ge_peri; its on_clock
 * runs the reaction state machine, mirroring the integrated reader/printer
 * pattern: it watches the PER micro-sequence (rSO states), resolves the target
 * device from the latched connector + the unit-name byte, and during the
 * channel-1 transfer phase presents input bytes on the selected connector's
 * line bundle (ST3/ST4) so the CPU's own state_b9/state_b1 loop writes them to
 * memory — exactly as the card reader does on connector 2.
 *
 * Non-interference: the core only acts while a connector-3/4 PER is in flight
 * (per_pending) and that connector is actually selected (PC131/PC141). For any
 * reader (connector 2) or channel-2 printer op those are false, so the core is
 * inert and never touches ST3/ST4.
 */

#include "connector34.h"
#include "ge.h"
#include "reader.h"
#include "signals.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

/* Largest single channel-API input transfer the core buffers. */
#define C34_XFER_MAX 4096

/* Per-byte presentation cadence (mirrors the card reader): present at IDLE,
 * retire (clear the lines) at PRESENT, then back to IDLE for the next byte. */
enum c34_state {
    C34_IDLE,
    C34_PRESENT,
    C34_DONE,
};

struct connector34_core {
    struct ge_std_device *dev3;     /* devices on connector 3 */
    struct ge_std_device *dev4;     /* devices on connector 4 */

    /* current PER context */
    int                   per_pending;
    uint16_t              per_base;
    uint8_t               name_byte;
    int                   name_captured;
    struct std_unitname   un;
    struct ge_std_device *cur;      /* device claiming this PER, or NULL */
    uint8_t               order;
    int                   order_seen;

    /* input transfer buffer (device -> CPU) */
    uint8_t               buf[C34_XFER_MAX];
    uint16_t              len;
    uint16_t              pos;
    int                   filled;

    /* functional access-latency model: a free-running on_clock counter (no real
     * clock is available) and the tick at which the current op may transfer. */
    unsigned long         ticks;
    unsigned long         busy_until;

    enum c34_state        state;

    struct ge_peri        peri;
};

/* Map a device reaction onto the channel-1 examine status byte (read back by an
 * EPER via CE_chan1_status): 0x40 = RO6 set, no error; 0x42 also sets RO1 so the
 * DU95 "no-error" decode reads error. */
static void apply_reaction(struct ge *ge, std_reaction r)
{
    switch (r) {
    case STD_NOT_ACCEPTED:
    case STD_NOT_POSSIBLE:
        ge->inject_chan1_status = 0x42;   /* abnormal: EPER examine reads error */
        break;
    default:
        ge->inject_chan1_status = 0x00;   /* clean: CE_chan1_status uses 0x40   */
        break;
    }
}

static struct std_unitname decode_name(uint8_t nb)
{
    struct std_unitname un;
    uint8_t top = (nb >> 6) & 3;
    un.connector = (top == 0) ? 3 : (top == 1) ? 4 : (top == 2) ? 2 : 1;
    un.unit      = nb & 0x3f;
    return un;
}

struct std_unitname connector34_decode(struct ge *ge)
{
    struct connector34_core *c = (struct connector34_core *)ge->std_core;
    return decode_name(c ? c->name_byte : 0);
}

/* The connector the CPU has selected this cycle, or NULL. */
static struct ge_connector *selected_connector(struct ge *ge)
{
    if (PC131(ge)) return &ge->ST3;
    if (PC141(ge)) return &ge->ST4;
    return NULL;
}

static struct ge_std_device *find_device(struct connector34_core *c,
                                          struct std_unitname un)
{
    struct ge_std_device *d = (un.connector == 4) ? c->dev4
                            : (un.connector == 3) ? c->dev3 : NULL;
    for (; d; d = d->next)
        if (d->claims && d->claims(d->ctx, un))
            return d;
    return NULL;
}

/*
 * Forwarded from reader.c connector_send_tu00() at CE11 (state b9 TO65), when
 * the order byte (rRE) is clocked to the selected connector. Capture the order
 * and deliver it to the device's command() hook. Timing/side-effects are left
 * to on_clock (printer precedent).
 */
void connector34_deliver_order(struct ge *ge, struct ge_connector *conn)
{
    struct connector34_core *c = (struct connector34_core *)ge->std_core;
    (void)conn;
    if (!c || !c->per_pending)
        return;
    c->order      = ge->rRE;
    c->order_seen = 1;
    if (c->cur && c->cur->command) {
        std_reaction r = c->cur->command(ge, c->cur->ctx, c->un, c->order);
        apply_reaction(ge, r);
    }
}

void connector34_set_busy(struct ge *ge, unsigned ticks)
{
    struct connector34_core *c = (struct connector34_core *)ge->std_core;
    if (c)
        c->busy_until = c->ticks + ticks;
}

static int connector34_on_clock(struct ge *ge, void *opaque)
{
    struct connector34_core *c = (struct connector34_core *)opaque;
    struct ge_connector *conn;

    c->ticks++;

    /* Per-cycle device housekeeping (seek / motion timers). */
    for (struct ge_std_device *d = c->dev3; d; d = d->next)
        if (d->tick) d->tick(ge, d->ctx);
    for (struct ge_std_device *d = c->dev4; d; d = d->next)
        if (d->tick) d->tick(ge, d->ctx);

    /* Org phase of a PER begins: rV1 is the order-block base. Reset context and
     * drop any stale unit-present assertion from a previous op. */
    if (ge->rSO == 0xc8) {
        c->per_pending   = 1;
        c->per_base      = ge->rV1;
        c->cur           = NULL;
        c->name_captured = 0;
        c->order_seen    = 0;
        c->filled        = 0;
        c->len = c->pos  = 0;
        c->busy_until    = 0;
        c->state         = C34_IDLE;
        ge->ST3.mare     = 0;
        ge->ST4.mare     = 0;
        ge->inject_chan1_status = 0;   /* clean unless a device reports error */
        return 0;
    }

    if (!c->per_pending)
        return 0;

    /* The unit-name byte sits in rRE during state d8 (it is overwritten by the
     * order byte later). Capture it and resolve the target device. d8 can be
     * revisited by the unit-busy recycle, so re-capture is harmless. */
    if (ge->rSO == 0xd9 && !c->name_captured) {
        /* The unit-name byte lands in rRE during d9 (at the d8 cycle's TO00,
         * when on_clock runs, rRE is still stale). Capture exactly once: later
         * d-states overwrite rRE with the order byte, and the unit-busy CC->D8
         * recycle revisits with other values. */
        c->name_captured = 1;
        c->name_byte = ge->rRE;
        c->un        = decode_name(c->name_byte);
        c->cur       = find_device(c, c->un);
        /* Assert the addressed unit "present/ready" (MARE3/MARE4) so the
         * channel-1 selection chain (PM13A/PM14A -> RM101 -> PUC1) completes
         * and the PER proceeds to transfer, instead of taking the
         * unit-not-available branch. */
        if (c->cur) {
            if (c->un.connector == 4)
                ge->ST4.mare = 1;
            else
                ge->ST3.mare = 1;
        }
        return 0;
    }

    /* Transfer phase. Act only while this connector is selected and a device
     * owns the addressed unit. */
    conn = selected_connector(ge);
    if (!c->cur || !conn)
        return 0;

    /* Retire a byte the CPU has consumed. This runs regardless of RASI so the
     * FINAL byte (which carries fine=1 -> RIG1) is always cleared; otherwise a
     * stuck te10 would keep the read loop re-reading stale data past the count
     * (the card reader clears the same way at CR_PRESENTED). */
    if (c->state == C34_PRESENT) {
        connector_clear_sending(conn);
        c->state = (c->pos >= c->len) ? C34_DONE : C34_IDLE;
        return 0;
    }

    /* Whole record presented: the last byte set RIG1 (RIVE), so the machine's
     * own end-of-transfer microcode winds the PER back to alpha. Keep holding
     * RC00 until it leaves the channel-1 transfer states so the sequencer can
     * run that wind-down. */
    if (c->state == C34_DONE) {
        if (ge->rSO == 0xb8 || ge->rSO == 0xb1 || ge->rSO == 0xb9)
            ge->RC00 = 1;
        return 0;
    }

    /* Feed only once the CPU has entered the channel-1 transfer phase (RASI). */
    if (!ge->RASI)
        return 0;

    /* Hold the CPU-active request (RC00) for the duration of the transfer. The
     * channel-1 service loop only advances while RIA0 (= RC00 & !ALTO) is set;
     * the integrated reader keeps RC00 asserted through its own RACI/RIUC path
     * (CE03/CE08), but the connector path runs state_b9's !PC121 branch whose
     * CE05 is a stub, so RC00 would drop and freeze the sequencer at b8. Assert
     * it directly, mirroring how printer.c drives RC00 to complete a channel-2
     * PER. Gated on connector selection, so reader/channel-2 ops are unaffected. */
    ge->RC00 = 1;

    /* Lazily pull the input record from the device on first entry. The transfer
     * hook may declare access latency here (connector34_set_busy) before any
     * byte is presented, so the whole transfer is delayed from the start. */
    if (!c->filled) {
        uint16_t n = C34_XFER_MAX;
        std_reaction r;
        c->len = 0;
        if (c->cur->transfer) {
            r = c->cur->transfer(ge, c->cur->ctx, c->un, 0 /*input*/,
                                 c->buf, &n, C34_XFER_MAX);
            c->len = (n > C34_XFER_MAX) ? C34_XFER_MAX : n;
            apply_reaction(ge, r);
        }
        c->pos    = 0;
        c->filled = 1;
        c->state  = C34_IDLE;
    }

    /* Access latency: while the unit is still "busy" (seek/motion), present no
     * data — the PER simply waits here (RC00 held) like any slow peripheral. */
    if (c->ticks < c->busy_until)
        return 0;

    /* C34_IDLE: present the next byte (the last one carries end=1 -> RIG1). */
    if (conn->te10)                 /* previous byte not yet consumed */
        return 0;
    if (c->pos >= c->len) {         /* nothing left to present */
        c->state = C34_DONE;
        return 0;
    }
    {
        uint8_t b   = c->buf[c->pos++];
        int     end = (c->pos >= c->len);
        connector_setup_to_send(ge, conn, b, end ? 1 : 0);
        c->state = C34_PRESENT;
    }
    return 0;
}

static int connector34_deinit(struct ge *ge, void *opaque)
{
    if (ge->std_core == opaque)
        ge->std_core = NULL;
    free(opaque);
    return 0;
}

int connector34_init(struct ge *ge)
{
    struct connector34_core *c = (struct connector34_core *)ge->std_core;
    if (c)
        return 0;                       /* already initialised */
    c = calloc(1, sizeof(*c));
    if (!c)
        return -1;
    c->peri.on_clock = connector34_on_clock;
    c->peri.deinit   = connector34_deinit;
    c->peri.ctx      = c;
    ge->std_core     = c;
    return ge_register_peri(ge, &c->peri);
}

int connector34_attach(struct ge *ge, struct ge_std_device *dev, uint8_t connector)
{
    struct connector34_core *c = (struct connector34_core *)ge->std_core;
    if (!c || !dev || (connector != 3 && connector != 4))
        return -1;
    if (connector == 4) {
        dev->next = c->dev4;
        c->dev4   = dev;
    } else {
        dev->next = c->dev3;
        c->dev3   = dev;
    }
    ge_log(LOG_PERI, "connector34: attached '%s' on connector %u\n",
           dev->name ? dev->name : "?", connector);
    return 0;
}
