#ifndef CONNECTOR34_H
#define CONNECTOR34_H

#include <stdint.h>

/*
 * Shared core for Standard-GE-100 controllers on the CPU's standard connectors
 * 3 and 4 — the attach point for Disk (DSS 156-157) and Tape (MTC/MTH) units.
 *
 * The CPU side (connector selection PC13A/PC14A from the latched PB06/PB07, the
 * channel-1 read datapath through NE_knot, and the PER/CPER/TPER micro-sequence)
 * is already in the emulator; what was missing is a device that listens on the
 * ST3/ST4 connector line bundle and reacts. This core registers a single ge_peri
 * whose on_clock drives that reaction, and device modules (disk.c, tape.c)
 * register against it through the ge_std_device vtable.
 *
 * Fidelity: functional / channel-API level (NOT the pin-level Standard-GE-100
 * handshake — spec 300740110 is not yet extracted). Data moves at the channel
 * level: input bytes are presented on the connector lines so the CPU's own
 * channel-1 loop writes them to memory.
 *
 * Scope: direct attach only (no MPA-130 fan-out). The 8-bit unit name decodes
 * as: top two bits = connector (00->3, 01->4, 10->2, 11->1); low six bits =
 * unit number (0..63).
 */

struct ge;
struct ge_connector;
struct ge_std_device;

/* The reaction the CPU reads back (FA04/FA05/PECO) after a peripheral op. */
typedef enum {
    STD_ACCEPTED_NO_END = 0, /* command accepted, op performed, no "end"      */
    STD_ACCEPTED_END    = 1, /* command accepted, op ended with "end" from PU */
    STD_NOT_ACCEPTED    = 2, /* command not accepted (rejected)               */
    STD_NOT_POSSIBLE    = 3, /* operation not possible (bad unit / parameter) */
    STD_BUSY            = 4, /* unit/controller busy -> PECO, recycle and wait */
} std_reaction;

/* Decoded unit name targeted by the current PER. */
struct std_unitname {
    uint8_t connector;       /* 3 or 4 (also 2/1 for completeness) */
    uint8_t unit;            /* 0..63  (low six bits of the name byte) */
};

/*
 * A device module (disk.c, tape.c) implements this vtable and attaches to a
 * connector with connector34_attach().
 */
struct ge_std_device {
    const char *name;
    void       *ctx;                 /* device-private state */

    /* Does this device own un.unit on its connector? (A multiple controller —
     * e.g. tape — claims a whole range of unit numbers.) */
    int (*claims)(void *ctx, struct std_unitname un);

    /* CPER: a command/order byte has been delivered to the unit. Returns the
     * reaction. May be NULL (then the core assumes STD_ACCEPTED_END). */
    std_reaction (*command)(struct ge *, void *ctx, struct std_unitname un,
                            uint8_t order);

    /* TPER data transfer at the channel-API level.
     *   dir 0 = input  (device -> CPU): fill buf[0..*len-1], set *len to the
     *                   number of bytes available (clamped to cap).
     *   dir 1 = output (CPU -> device): consume buf[0..*len-1].
     * Returns the reaction. May be NULL (no transfer). */
    std_reaction (*transfer)(struct ge *, void *ctx, struct std_unitname un,
                             int dir, uint8_t *buf, uint16_t *len, uint16_t cap);

    /* Optional per-cycle housekeeping (seek/motion timers). May be NULL. */
    void (*tick)(struct ge *, void *ctx);

    struct ge_std_device *next;      /* core-managed list link */
};

/* Create the core and register its peri. Idempotent. */
int connector34_init(struct ge *ge);

/* Attach a device to connector 3 or 4. */
int connector34_attach(struct ge *ge, struct ge_std_device *dev, uint8_t connector);

/* Decode the unit name captured for the current PER (top bits -> connector,
 * low six bits -> unit). */
struct std_unitname connector34_decode(struct ge *ge);

/* Forwarded by reader.c's connector_send_tu00() when a CPER/order byte is
 * clocked to the selected connector. Internal seam — not for device modules. */
void connector34_deliver_order(struct ge *ge, struct ge_connector *conn);

#endif /* CONNECTOR34_H */
