/*
 * channel.h - generic CPU<->peripheral line bundle for an integrated channel.
 *
 * The GE-120 integrated peripherals hang off the CPU's organisation phase over a
 * small set of hardware lines: a cycle-request, a data bus + "char ready" strobe,
 * an end-of-transfer line, and a command/order byte. Channel 2 (CAN2) carries the
 * integrated reader (input) AND the printer/typewriter (output); per dwg
 * 14023130, the channel-2 transfer micro-states (rSI low nibble 0C/0E input,
 * 02/03 printer-output, 0A/0B end-print) read/drive exactly these lines.
 *
 * `struct ge_channel` generalises `ge_integrated_reader` (lu08/data/fini) and
 * `ge_connector` (te10/te20/data/fine) into one line bundle so the reader,
 * keyboard, and .cap feeder (input) and the printer (output) all attach the same
 * way: a peripheral registers via ge_register_peri and, in its callbacks, either
 * offers input characters on the lines (input) or is handed output characters by
 * the CPU microcode through `sink` (output). The machine's own microcode moves
 * the data — the peripheral only presents/consumes characters on the lines.
 *
 * This header introduces the abstraction; wiring the microcode (Phase 3) and
 * migrating the reader onto it (Phase 4) follow. See docs/peripherals.md.
 */
#ifndef CHANNEL_H
#define CHANNEL_H

#include <stdint.h>

struct ge;
struct ge_channel;

/* Output: the CPU hands a character to the peripheral (printer "Load Printer
 * Buffer", command CE16 in transfer state 02|03). */
typedef void (*ge_channel_sink)(struct ge *, struct ge_channel *, uint8_t ch);
/* Input: the peripheral offers the next character on the bus; returns 1 if a
 * character is available (and sets *ch, *end), 0 if not ready this cycle. */
typedef int  (*ge_channel_source)(struct ge *, struct ge_channel *,
                                   uint8_t *ch, uint8_t *end);

struct ge_channel {
    const char *name;

    /* peripheral -> CPU cycle request (generalises LU08; printer OR'd via RIMZA) */
    uint8_t req:1;
    /* data bus + "character ready" strobe (generalises lu08 / te10|te20) */
    uint8_t data;
    uint8_t data_valid:1;
    /* peripheral end-of-transfer (drives the RF10x chain -> RIG1) */
    uint8_t fini:1;
    /* last order/command byte delivered to the peripheral (from rRE) */
    uint8_t cmd;
    /* transfer direction for the in-flight operation: 1 = OUTPUT (CPU->peri,
     * L207), 0 = INPUT (peri->CPU). */
    uint8_t out:1;

    ge_channel_sink   sink;     /* CPU->peri character (output) */
    ge_channel_source source;   /* peri->CPU character (input)  */
    void             *ctx;      /* peripheral private state      */
};

/* Input line ops (generalise reader_setup_to_send / reader_clear_sending). */
void    channel_offer_input(struct ge *, struct ge_channel *, uint8_t data, uint8_t end);
void    channel_clear_input(struct ge *, struct ge_channel *);
/* Output line op: deliver one character from the CPU to the peripheral sink. */
void    channel_accept_output(struct ge *, struct ge_channel *, uint8_t ch);

/* Line reads (feed the request/data/end signals). */
uint8_t channel_get_req(struct ge_channel *);
uint8_t channel_get_data(struct ge_channel *);
uint8_t channel_get_fini(struct ge_channel *);

#endif /* CHANNEL_H */
