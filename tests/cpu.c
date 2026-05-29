#include "utest.h"
#include "../ge.h"
#include "../log.h"

/** diag fo. 82 */
UTEST(cpu_isolation, test_k)
{
    struct ge g;
    struct ge_console c = { 0 };
    struct ge_console_switches s = { 0 };

    ge_log(LOG_CONSOLE, "CLEAR\n");
    ge_init(&g);
    ge_clear(&g);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "SET V1 SCR, AM = ff, INAR\n");
    ge_set_console_rotary(&g, RS_V1_SCR);
    s.AM = 0x00ff;
    s.INAR = 1;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "START\n");
    ge_start(&g);
    ge_run_cycle(&g);

    ASSERT_EQ(g.mem[0], 0xff);

    ge_log(LOG_CONSOLE, "SET PAPA\n");
    s.PAPA = 1;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "RESET PAPA, INAR\n");
    s.PAPA = 0;
    s.INAR = 0;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "CLEAR\n");
    ge_clear(&g);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "SET V1, AM = 00\n");
    ge_set_console_rotary(&g, RS_V1);
    s.AM = 0x0000;
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "START\n");
    ge_start(&g);
    ge_run_cycle(&g);

    ASSERT_TRUE(g.rV1 == 0x00);

    ge_log(LOG_CONSOLE, "SET V1 LETT\n");
    ge_set_console_rotary(&g, RS_V1_LETT);
    ge_run_cycle(&g);

    ge_log(LOG_CONSOLE, "START\n");
    ge_start(&g);
    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);

    ge_log(LOG_CONSOLE, "RO == FF?!?\n");

    ASSERT_TRUE(c.lamps.RO == 0x0ff);
    ASSERT_TRUE(c.lamps.MEM_CHECK == 0);
}

/*
 * Light OPER CALL by keying a LON instruction (opcode 0x02, 2nd char 0x80)
 * into the CPU registers from the maintenance panel and single-stepping it
 * through the instruction phases — the exact real-GE-120 front-panel
 * procedure (CPU[4] §4 "Maintenance Panel", dwg 30004122 fo. 35-37):
 *
 *   CLEAR; insert PAPA (passo-passo / step-by-step)
 *   rotary SO  : force 0xE2  -> sequencer set to the alpha phase
 *   rotary PO  : force 0x00  -> program counter (SO preserved across the force)
 *   NORM, START: step alpha (0xE2) -> operand-fetch phase E0
 *   rotary FO  : force 0x02  -> the LON opcode (SO held at E0)
 *   NORM, START: E0 decodes a 2-byte op -> beta phase 0x64
 *   rotary L1  : force 0x80  -> LON 2nd char (SO held at 0x64)
 *   NORM, START: beta executes LON -> CI87 sets ALAM -> OPER CALL on
 *
 * This works because a forcing cycle preserves the program sequencer SO
 * (fsn_last_clock); only forcing SO itself (rotary pos 13) changes it.
 */
static void cons_force(struct ge *g, struct ge_console_switches *s, int rot, int am)
{
    ge_set_console_rotary(g, rot);
    s->AM = am;
    s->INAR = 1;
    ge_set_console_switches(g, s);
    ge_run_cycle(g);
    ge_start(g);
    ge_run_cycle(g);
}

static void cons_step(struct ge *g, struct ge_console_switches *s)
{
    s->INAR = 0;
    ge_set_console_rotary(g, RS_NORM);
    ge_set_console_switches(g, s);
    ge_start(g);
    ge_run_cycle(g);
}

UTEST(cpu_isolation, oper_call_by_register_forcing)
{
    struct ge g;
    struct ge_console c = { 0 };
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);
    ge_run_cycle(&g);

    s.PAPA = 1;                       /* step-by-step (passo-passo) */
    ge_set_console_switches(&g, &s);

    cons_force(&g, &s, RS_SO, 0xE2);  /* sequencer -> alpha phase */
    cons_force(&g, &s, RS_PO, 0x00);  /* PO <- 0 (SO preserved) */
    ASSERT_EQ((int)g.rSO, 0xE2);      /* the forced alpha state held */

    cons_step(&g, &s);                /* alpha -> E0 (instruction fetch) */
    ASSERT_EQ((int)g.rSO, 0xE0);

    cons_force(&g, &s, RS_FO, 0x02);  /* LON opcode -> FO (SO held at E0) */
    ASSERT_EQ((int)g.rSO, 0xE0);

    cons_step(&g, &s);                /* E0 -> beta 0x64 (2-byte op) */
    ASSERT_EQ((int)g.rSO, 0x64);
    ASSERT_EQ((int)g.rFO, 0x02);

    cons_force(&g, &s, RS_L1, 0x80);  /* LON 2nd char -> L1 (SO held at 0x64) */
    ASSERT_EQ((int)g.rSO, 0x64);

    /* final step: beta executes LON */
    s.INAR = 0;
    ge_set_console_rotary(&g, RS_NORM);
    ge_set_console_switches(&g, &s);
    ge_start(&g);
    int lit = 0;
    for (int i = 0; i < 25; i++) {
        ge_run_cycle(&g);
        if (g.ALAM) {
            lit = 1;
            break;
        }
    }

    ASSERT_EQ(lit, 1);                            /* LON executed -> ALAM set */
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.OPERATOR_CALL, 1);     /* OPER CALL lamp on */
    ASSERT_EQ((int)c.lamps.STEP_BY_STEP, 1);      /* PAPA -> step-by-step lamp */
}

/*
 * SWITCH 1 / SWITCH 2 lamps follow the two program-readable switches: lit when
 * the switch reads logic 1 (the value that makes JS1 / JS2 jump).
 * CPU[4] §3.3, fo.33.
 */
UTEST(console_fidelity, switch_lamps)
{
    struct ge g;
    struct ge_console c = { 0 };

    ge_init(&g);
    ge_clear(&g);

    g.JS1 = 1;
    g.JS2 = 0;
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.SWITCH_1, 1);
    ASSERT_EQ((int)c.lamps.SWITCH_2, 0);

    g.JS1 = 0;
    g.JS2 = 1;
    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.SWITCH_1, 0);
    ASSERT_EQ((int)c.lamps.SWITCH_2, 1);
}

/*
 * Step-by-step (PAPA) can be inhibited by the program: INS sets ADIR, ENS /
 * CLEAR clear it, and the maintenance STOC switch overrides the inhibit.
 * CPU[4] §3.3; HW gate ALTO <- ASIN(ATOC + !ADIR). Drives fsn_last_clock with
 * a CPU cycle pending (RIA0) and the rotary in NORM.
 */
UTEST(console_fidelity, step_by_step_inhibit)
{
    struct ge g;
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);
    g.register_selector = RS_NORM;
    g.RC00 = 1;
    g.RIA0 = 1;
    s.PAPA = 1;

    /* step-by-step enabled (ADIR = 0): the CPU halts after the step */
    g.ADIR = 0;
    g.console_switches = s;
    g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 1);

    /* INS inhibited step-by-step (ADIR = 1), STOC off: PAPA no longer halts */
    g.ADIR = 1;
    s.STOC = 0;
    g.console_switches = s;
    g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 0);

    /* STOC overrides the program inhibit: PAPA halts again */
    s.STOC = 1;
    g.console_switches = s;
    g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 1);
}

/*
 * PATE stops the timing after every delay-line cycle — finer than PAPA, and
 * ungated by the CPU/channel cycle. With PAPA off and the rotary in NORM the
 * machine free-runs (no halt); inserting PATE halts it after a single cycle.
 * CPU[4] §4, fo.35.
 */
UTEST(console_fidelity, pate_single_cycle)
{
    struct ge g;
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);
    g.register_selector = RS_NORM;
    g.RC00 = 1;
    g.RIA0 = 1;

    /* baseline: free-running (no PAPA, NORM rotary) -> no halt */
    g.console_switches = s;
    g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 0);

    /* PATE halts after one delay-line cycle */
    s.PATE = 1;
    g.console_switches = s;
    g.ALTO = 0;
    fsn_last_clock(&g);
    ASSERT_EQ((int)g.ALTO, 1);
}

/*
 * INCE check-bit forcing (CPU[4] §4.2, fo.36-37): with the rotary in V1 SCR
 * (storage forcing) and INCE inserted, AM08 is stored as the memory parity bit
 * instead of generated odd parity — so the operator can key in a wrong check
 * bit to exercise MEM CHECK. AM08=1 is wrong for 0x01 (popcount 1 is odd ->
 * correct parity 0), so reading the byte back raises MEM CHECK.
 */
UTEST(console_fidelity, ince_forces_check_bit)
{
    struct ge g;
    struct ge_console c = { 0 };
    struct ge_console_switches s = { 0 };

    ge_init(&g);
    ge_clear(&g);
    ge_run_cycle(&g);

    /* force 0x01 into mem[0] with a deliberately wrong check bit (AM08 = 1) */
    ge_set_console_rotary(&g, RS_V1_SCR);
    s.AM   = 0x0101;   /* AM07-00 = 0x01 (data); AM08 = 1 (forced parity) */
    s.INCE = 1;
    s.INAR = 1;        /* inhibit the fault stop while keying in */
    ge_set_console_switches(&g, &s);
    ge_run_cycle(&g);
    ge_start(&g);
    ge_run_cycle(&g);

    ASSERT_EQ((int)g.mem[0], 0x01);
    ASSERT_EQ((int)g.mem_parity[0], 1);   /* AM08, not odd_parity(0x01) = 0 */

    /* point V1 at address 0, then read it back through V1 LETT */
    ge_clear(&g);
    ge_run_cycle(&g);
    ge_set_console_rotary(&g, RS_V1);
    s.AM   = 0x0000;
    s.INCE = 0;
    s.INAR = 0;
    ge_set_console_switches(&g, &s);
    ge_start(&g);
    ge_run_cycle(&g);

    ge_set_console_rotary(&g, RS_V1_LETT);
    ge_run_cycle(&g);
    ge_start(&g);
    ge_run_cycle(&g);

    ge_fill_console_data(&g, &c);
    ASSERT_EQ((int)c.lamps.RO, 0x01);
    ASSERT_EQ((int)c.lamps.MEM_CHECK, 1);   /* wrong check bit detected on read */
}

