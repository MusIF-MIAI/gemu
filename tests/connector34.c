/*
 * connector34 — Standard-GE-100 controller core on connectors 3/4 (disk/tape).
 *
 * These tests prove the ST3/ST4 reactivation: a PER naming a connector-3 unit
 * drives the connector selector (PC131), a registered device claims the unit
 * and supplies data through the channel-API transfer hook, and the bytes land
 * in memory via the CPU's own channel-1 input loop — exactly the path the card
 * reader uses on connector 2 (cf. tests/perperi.c). A second test guards that a
 * device on ST3 does NOT perturb a connector-2 reader transfer.
 */
#include <string.h>

#include "utest.h"
#include "../ge.h"
#include "../bit.h"
#include "../reader.h"
#include "../signals.h"
#include "../connector34.h"

/* ---- a trivial stub device on connector 3 ------------------------------- */

static uint8_t stub_pattern[4] = { 6, 9, 6, 9 };  /* packs -> 0x69, 0x69 */
static int     stub_claimed;
static int     stub_cmd_order;

static int stub_claims(void *ctx, struct std_unitname un)
{
    (void)ctx;
    stub_claimed = 1;
    return un.connector == 3;
}

static std_reaction stub_command(struct ge *ge, void *ctx,
                                 struct std_unitname un, uint8_t order)
{
    (void)ge; (void)ctx; (void)un;
    stub_cmd_order = order;
    return STD_ACCEPTED_NO_END;
}

static std_reaction stub_transfer(struct ge *ge, void *ctx,
                                  struct std_unitname un, int dir,
                                  uint8_t *buf, uint16_t *len, uint16_t cap)
{
    (void)ge; (void)ctx; (void)un; (void)dir;
    uint16_t n = sizeof stub_pattern;
    if (n > cap)
        n = cap;
    memcpy(buf, stub_pattern, n);
    *len = n;
    return STD_ACCEPTED_END;
}

static void stub_init(struct ge_std_device *dev)
{
    memset(dev, 0, sizeof *dev);
    dev->name     = "stub";
    dev->claims   = stub_claims;
    dev->command  = stub_command;
    dev->transfer = stub_transfer;
    stub_claimed   = 0;
    stub_cmd_order = -1;
}

/* Build the PER + order block for a connector-3 input transfer. */
static void build_per(struct ge *g, uint8_t up_name, uint16_t dst)
{
    g->mem[0x00] = PER_OPCODE;
    g->mem[0x01] = up_name;
    g->mem[0x02] = 0x00;   /* order-block addr hi */
    g->mem[0x03] = 0x10;   /* order-block addr lo -> base 0x0010 */

    g->mem[0x10] = 0x00;            /* z   : mode / direction */
    g->mem[0x11] = 0x40;            /* x   : order/command byte */
    g->mem[0x12] = 0x00;            /* l1  : length (fini-terminated here) */
    g->mem[0x13] = 0x10;            /* l2  */
    g->mem[0x14] = (dst >> 8);      /* i1  : destination addr hi */
    g->mem[0x15] = (dst & 0xff);    /* i2  : destination addr lo */
}

/* ---- test 1: input TPER on connector 3 ---------------------------------- */

UTEST(connector34, input_tper_conn3)
{
    const uint16_t dst = 0x0030;
    struct ge g;
    struct ge_std_device dev;

    ge_init(&g);
    build_per(&g, 0x00 /* connector 3, unit 0 */, dst);

    ge_clear(&g);
    stub_init(&dev);
    connector34_init(&g);
    connector34_attach(&g, &dev, 3);
    ge_start(&g);

    int saw_pc131 = 0;
    for (int i = 0; i < 200; i++) {
        if (PC131(&g))
            saw_pc131 = 1;
        ge_run_cycle(&g);
    }

    ASSERT_TRUE(stub_claimed);             /* device was asked to claim the unit */
    ASSERT_TRUE(saw_pc131);                /* connector 3 was actually selected  */
    ASSERT_EQ(stub_cmd_order, 0x40);       /* CPER order byte was forwarded       */
    ASSERT_EQ(g.mem[dst], 0x69);           /* first packed byte landed in memory  */
    ASSERT_EQ(g.mem[dst + 1], 0x69);       /* second packed byte                  */
}

/* ---- test 2: a device on ST3 must not perturb a connector-2 reader ------ */

UTEST(connector34, no_interference_reader)
{
    const uint16_t dst = 0x0030;
    struct ge g;
    struct ge_std_device dev;

    ge_init(&g);
    build_per(&g, 0x80 /* connector 2 (integrated reader) */, dst);

    ge_clear(&g);
    stub_init(&dev);
    connector34_init(&g);
    connector34_attach(&g, &dev, 3);       /* present but should stay idle */
    ge_start(&g);

    /* Run the connector-2 reader PER. The connector-3/4 core must never select
     * or drive ST3, and a connector-2 unit name must never resolve to the
     * connector-3 device. The reader op must still proceed to its channel-1
     * transfer wait (RASI/b8) — proving the core did not hijack it. */
    int st3_touched = 0;
    int reached_transfer = 0;
    for (int i = 0; i < 60; i++) {
        if (g.ST3.te10 || g.ST3.te20 || g.ST3.data || g.ST3.fine || g.ST3.mare)
            st3_touched = 1;
        if (g.rSO == 0xb8 && g.RASI)
            reached_transfer = 1;
        ge_run_cycle(&g);
    }

    ASSERT_FALSE(st3_touched);             /* the connector-3 device stayed inert */
    ASSERT_FALSE(stub_claimed);            /* a conn-2 name never resolves to it  */
    ASSERT_TRUE(reached_transfer);         /* the reader PER proceeded normally   */
}

/* ---- test 3: access latency (busy) delays but completes the transfer ----- */

static std_reaction busy_transfer(struct ge *ge, void *ctx,
                                  struct std_unitname un, int dir,
                                  uint8_t *buf, uint16_t *len, uint16_t cap)
{
    (void)ctx; (void)un; (void)dir;
    connector34_set_busy(ge, 60);          /* model a 60-cycle seek/motion */
    uint16_t n = sizeof stub_pattern;
    if (n > cap)
        n = cap;
    memcpy(buf, stub_pattern, n);
    *len = n;
    return STD_ACCEPTED_END;
}

UTEST(connector34, busy_latency)
{
    const uint16_t dst = 0x0030;
    struct ge g;
    struct ge_std_device dev;

    ge_init(&g);
    build_per(&g, 0x00, dst);

    ge_clear(&g);
    stub_init(&dev);
    dev.transfer = busy_transfer;          /* declares latency before transfer */
    connector34_init(&g);
    connector34_attach(&g, &dev, 3);
    ge_start(&g);

    int mem_at_40 = -1;
    for (int i = 0; i < 160; i++) {
        if (i == 40)
            mem_at_40 = g.mem[dst];
        ge_run_cycle(&g);
    }

    ASSERT_EQ(mem_at_40, 0x00);            /* still busy at cycle 40: no data yet */
    ASSERT_EQ(g.mem[dst], 0x69);           /* the transfer completes once ready   */
    ASSERT_EQ(g.mem[dst + 1], 0x69);
}

/* ---- test 4: a rejected command reports an abnormal examine status -------- */

static std_reaction reject_command(struct ge *ge, void *ctx,
                                   struct std_unitname un, uint8_t order)
{
    (void)ge; (void)ctx; (void)un; (void)order;
    return STD_NOT_POSSIBLE;
}
static std_reaction reject_transfer(struct ge *ge, void *ctx,
                                    struct std_unitname un, int dir,
                                    uint8_t *buf, uint16_t *len, uint16_t cap)
{
    (void)ge; (void)ctx; (void)un; (void)dir; (void)buf; (void)cap;
    *len = 0;
    return STD_NOT_POSSIBLE;
}

UTEST(connector34, reject_sets_error_status)
{
    struct ge g;
    struct ge_std_device dev;

    ge_init(&g);
    build_per(&g, 0x00, 0x0030);

    ge_clear(&g);
    stub_init(&dev);
    dev.command  = reject_command;
    dev.transfer = reject_transfer;
    connector34_init(&g);
    connector34_attach(&g, &dev, 3);
    ge_start(&g);

    for (int i = 0; i < 30; i++)
        ge_run_cycle(&g);

    /* The rejected reaction is mapped onto the channel-1 examine status byte
     * (0x42 sets RO1 so an EPER's DU95 "no-error" decode reads error). */
    ASSERT_EQ(g.inject_chan1_status, 0x42);
}

/* ---- test 5: a device can raise an end-of-operation interrupt ------------ */

static std_reaction irq_transfer(struct ge *ge, void *ctx,
                                 struct std_unitname un, int dir,
                                 uint8_t *buf, uint16_t *len, uint16_t cap)
{
    (void)ctx; (void)un; (void)dir; (void)cap;
    connector34_raise_interrupt(ge);       /* signal end-of-operation */
    buf[0] = 0x01;                          /* low nibbles 1,2 -> mem 0x12 */
    buf[1] = 0x02;
    *len = 2;
    return STD_ACCEPTED_END;
}

UTEST(connector34, end_of_op_interrupt)
{
    const uint16_t dst = 0x0040;
    struct ge g;
    struct ge_std_device dev;

    ge_init(&g);
    build_per(&g, 0x00, dst);              /* connector-3 read */

    ge_clear(&g);
    stub_init(&dev);
    dev.transfer = irq_transfer;
    connector34_init(&g);
    connector34_attach(&g, &dev, 3);
    ge_start(&g);

    for (int i = 0; i < 80; i++)
        ge_run_cycle(&g);

    /* The transfer completed and the device's end-of-operation interrupt is
     * pending (RINT). Servicing is the machine's own interrupt vector through
     * 0x0300/0x0304 (decimal 768-775) once the program has interrupts enabled
     * (MASC=0) — that path is exercised by tests/interrupt.c. */
    ASSERT_EQ(g.mem[dst], 0x12);           /* transfer completed */
    ASSERT_TRUE(g.RINT);                   /* end-of-op interrupt request pending */
}
