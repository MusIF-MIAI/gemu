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
 * straight into the CPU registers from the console, then stepping the beta
 * phase. This is the console-forcing form of the real front-panel procedure.
 *
 * NOTE on order: on the real GE-120 the operator forces SO=0xE2 (alpha) first
 * and single-steps alpha -> E0 -> beta, forcing FO at E0 and L1 at beta. In
 * gemu, forcing a data register runs the display/forcing state machine which
 * resets the sequencer SO, so the forced phase can't be held mid-sequence;
 * the equivalent here forces FO and L1 first, then SO=0x64 (beta) LAST, then
 * runs. (Closing that ordering gap = making register-forcing preserve SO.)
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

    cons_force(&g, &s, RS_FO, 0x02);  /* LON opcode  -> FO */
    cons_force(&g, &s, RS_L1, 0x80);  /* LON 2nd char -> L1 */
    cons_force(&g, &s, RS_SO, 0x64);  /* sequencer -> beta (forced last) */

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

