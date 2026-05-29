#include "utest.h"

#include "../ge.h"
#include "../bit.h"
#include "../opcodes.h"

/*
 * Interruption routine (flow chart 14023130C) + the LPSR load (14023130D).
 *
 * When alpha detects INTE = RINT & /MASC it routes to F0; the graph
 *   F0 -> D2 -> D3 -> D0 -> D1 -> C2 -> C3 -> C0 -> C1 -> alpha
 * SAVES the current PSR to 0x0300 (status, 0, PO-hi, PO-lo) and LOADS the new
 * PSR from 0x0304 (status, 0, PO-hi, PO-lo), resuming at the new PO (handler).
 */

static void run_to_halt(struct ge *g, int max)
{
    for (int i = 0; i < max && !g->halted; i++)
        ge_run_cycle(g);
}

/* An interrupt taken at PO=0x0050 saves the PSR to 0x0300 and vectors to the
 * handler whose address is in the new PSR at 0x0306-7 (= 0x0100). */
UTEST(interrupt, save_and_vector)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    /* Program: a harmless NOP2 at the entry so alpha does not halt before the
     * interrupt diverts; the handler at 0x0100 halts. */
    g.mem[0x0050] = NOP2_OPCODE;          /* entry instruction */
    g.mem[0x0100] = HLT_OPCODE;           /* interrupt handler */

    /* New PSR at 0x0304: status=0, 0, PO-hi=0x01, PO-lo=0x00 -> handler 0x0100 */
    g.mem[0x0304] = 0x00;
    g.mem[0x0305] = 0x00;
    g.mem[0x0306] = 0x01;
    g.mem[0x0307] = 0x00;

    ge_enter(&g, 0x0050);
    ge_start(&g);

    /* Raise an unmasked interrupt: RINT=1, MASC (FA06) clear; FI04 set so the
     * saved status byte has bit 5 set (FA04 -> status bit 5). */
    g.ffFI = 0x10;                        /* FI04 = 1, FI06 (MASC) = 0 */
    g.RINT = 1;

    run_to_halt(&g, 80);

    ASSERT_TRUE(g.halted);                /* reached the handler HLT */
    ASSERT_EQ(g.rPO, 0x0100);             /* vectored to the new PSR's PO */

    /* Saved (old) PSR at 0x0300: status, 0, PO-hi, PO-lo (PO was 0x0050). */
    ASSERT_EQ(g.mem[0x0300], 0x20);       /* status: FA04 -> bit 5 */
    ASSERT_EQ(g.mem[0x0301], 0x00);
    ASSERT_EQ(g.mem[0x0302], 0x00);       /* saved PO high */
    ASSERT_EQ(g.mem[0x0303], 0x50);       /* saved PO low  */

    /* The request was acknowledged (cleared) on entry, so it did not re-loop. */
    ASSERT_FALSE(g.RINT);
}

/* A masked interrupt (MASC = FA06 = 1) must NOT divert: the entry HLT runs. */
UTEST(interrupt, masked_does_not_divert)
{
    struct ge g;
    ge_init(&g);
    ge_clear(&g);

    g.mem[0x0050] = HLT_OPCODE;           /* entry halts if not diverted */

    ge_enter(&g, 0x0050);
    ge_start(&g);

    g.ffFI = 0x40;                        /* FI06 (MASC) = 1 */
    g.RINT = 1;

    run_to_halt(&g, 40);

    ASSERT_TRUE(g.halted);
    ASSERT_EQ(g.rPO, 0x0050);             /* halted at entry, no vector */
    ASSERT_EQ(g.mem[0x0300], 0x00);       /* no PSR was saved */
}
