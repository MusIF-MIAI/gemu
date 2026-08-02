#include "utest.h"

#include "../ge.h"
#include "../signals.h"
#include "../opcodes.h"

/*
 * The change-register-7 write walk (cp07 fo.34/35, states EA and EB).
 *
 * JRT and LA both end in the same two states: EA writes the LOW byte of a
 * two-byte register cell, EB the HIGH byte, with V1 carrying the datum.  What
 * distinguishes them is where the ADDRESS comes from.  JRT (and SR/SL) force
 * it: CO18 opens the NO21 forcing and CO90-CO97 raise all eight bits, so the
 * knot reads 0xFF -- change register 7, mem[254]/mem[255].  LA does not get
 * CO18; its address was built into V2 by the LA beta sheet.
 *
 * These tests pin that path at the micro level, one clock phase at a time,
 * because everything above it (tests/exec.c) only sees the finished link and
 * would still pass if the address arrived by some other route.
 */

/* Step pulses until `upto` has run -- the machine is left poised on the clock
 * after it, since ge_run_pulse increments only once the state's rows are out. */
static void run_to(struct ge *g, enum clock upto)
{
    int i;
    for (i = 0; i < 64; i++) {
        ge_run_pulse(g);
        if (g->current_clock == upto + 1)
            return;
    }
}

/* Drop the machine into one executive state with a decoded opcode in FO. The
 * alpha/beta phases that would normally lead here are not run: these states
 * are being examined on their own, exactly as the sheet prints them. */
static void enter(struct ge *g, uint8_t state, uint8_t opcode)
{
    ge_init(g);
    ge_clear(g);
    g->rFO = opcode;
    g->rSO = state;
    ge_start(g);
    ge_run_pulse(g);            /* TO00: SA <- NA knot, i.e. SO */
}

/* ------------------------------------------------------------------ *
 * The forced address
 * ------------------------------------------------------------------ */

/* EA at TO10 raises CO18 plus all eight CO9x, and TO20 latches the knot into
 * BO/VO. The cell addressed is 255 -- change register 7, low byte. */
UTEST(register07, jrt_forces_the_change_register_address)
{
    struct ge g;
    enter(&g, 0xea, JRT_OPCODE);
    g.rV1 = 0x1234;                       /* the return address */

    run_to(&g, TO10);
    ASSERT_EQ(g.kNO.force_mode, KNOT_FORCING_NO_21);
    ASSERT_EQ(g.kNO.forcings, 0xff);
    ASSERT_EQ(NO_knot(&g), 0x00ff);       /* forced byte alone, quartets 4,3 = 0 */

    run_to(&g, TO20);
    ASSERT_EQ(g.rVO, 0x00ff);             /* mem[255] = register 7 low */
    ASSERT_EQ(g.rBO, 0x00ff);
}

/* Gap that the fidelity assessment flagged as "correct but fragile": the
 * forcings are cleared the moment BO/VO have latched, so nothing downstream in
 * the same cycle can read them.  CI11 at TO30 and CI33 at TO50 depend on that
 * -- if the forcings outlived TO20 the datum path would carry 0xFF instead of
 * V1, and the link would be deposited as 0xFFFF. */
UTEST(register07, forcings_do_not_outlive_the_vo_latch)
{
    struct ge g;
    enter(&g, 0xea, JRT_OPCODE);
    g.rV1 = 0x1234;

    run_to(&g, TO10);
    ASSERT_EQ(g.kNO.forcings, 0xff);

    run_to(&g, TO20);
    ASSERT_EQ(g.kNO.forcings, 0x00);
    ASSERT_EQ(g.kNO.force_mode, KNOT_FORCING_NONE);

    run_to(&g, TO30);                     /* CI11: V1 -> NO, unforced */
    ASSERT_EQ(g.kNO.cmd, KNOT_V1_IN_NO);
    ASSERT_EQ(NO_knot(&g), 0x1234);
}

/* LA is the one caller that must NOT be forced: ea_co18 = !is_la, and its
 * address comes in on V2 through CO12.  Getting this wrong would send every
 * LA's operand address to register 7 instead. */
UTEST(register07, la_addresses_through_v2_not_the_forcing)
{
    struct ge g;
    enter(&g, 0xea, LA_OPCODE);
    g.rV1 = 0x1234;
    g.rV2 = 0x0500;                       /* address built by the LA beta sheet */

    run_to(&g, TO10);
    ASSERT_EQ(g.kNO.force_mode, KNOT_FORCING_NONE);
    ASSERT_EQ(g.kNO.cmd, KNOT_V2_IN_NO);

    run_to(&g, TO20);
    ASSERT_EQ(g.rVO, 0x0500);
}

/* ------------------------------------------------------------------ *
 * The datum, and the byte that reaches core
 * ------------------------------------------------------------------ */

/* EA moves the datum's LOW byte: CI11 selects V1 at TO30, CI33 gates quartets
 * 2,1 into RO at TO50, and the CO31 write commits it at the forced address. */
UTEST(register07, ea_writes_the_low_byte_of_the_link)
{
    struct ge g;
    enter(&g, 0xea, JRT_OPCODE);
    g.rV1 = 0x1234;

    run_to(&g, TO50);
    ASSERT_EQ(g.rRO, 0x34);

    ge_run_cycle(&g);                     /* finish the cycle: the write commits */
    ASSERT_EQ(g.mem[255], 0x34);
}

/* EB is the second half of the same walk: V2 now holds the address walked down
 * by CO40/CO41, and CI32 gates quartets 4,3 -- the datum's HIGH byte. */
UTEST(register07, eb_writes_the_high_byte_of_the_link)
{
    struct ge g;
    enter(&g, 0xeb, JRT_OPCODE);
    g.rV1 = 0x1234;
    g.rV2 = 0x00fe;                       /* what EA's CO40/CO41 leave behind */

    run_to(&g, TO20);
    ASSERT_EQ(g.rVO, 0x00fe);

    run_to(&g, TO50);
    ASSERT_EQ(g.rRO, 0x12);

    ge_run_cycle(&g);
    ASSERT_EQ(g.mem[254], 0x12);
}

/* ------------------------------------------------------------------ *
 * The gate the sheet prints
 * ------------------------------------------------------------------ */

/*
 * DI11A0 gates most of the CO9x rows in EA/EB.  It is a pure STATE decode --
 * true for SA in e8..eb and nowhere else -- so inside these two states it is
 * always satisfied and discriminates nothing; it is transcribed because the
 * sheet prints it, not because it selects.
 *
 * Worth pinning, because a plausible-sounding "correction" is to re-gate CO90
 * on DI65A0 (ED|EC).  DI65A0 is FALSE across the whole EA/EB band, so that
 * change would silently drop bit 0 of the forcing: the link would go to
 * mem[254]/mem[253] instead of mem[255]/mem[254], and every JRT return would
 * land somewhere else.
 */
UTEST(register07, the_ea_eb_forcing_gate_is_a_state_decode)
{
    struct ge g;
    unsigned sa;

    ge_init(&g);
    for (sa = 0; sa < 256; sa++) {
        g.rSA = (uint8_t)sa;
        ASSERT_EQ(DI11A0(&g), (sa >= 0xe8 && sa <= 0xeb) ? 1 : 0);
        if (sa >= 0xe8 && sa <= 0xeb)
            ASSERT_EQ(DI65A0(&g), 0);     /* ED|EC decode: never true here */
    }
}
