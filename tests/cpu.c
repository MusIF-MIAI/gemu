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

