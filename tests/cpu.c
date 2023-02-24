#include "utest.h"
#include "../ge.h"
#include "../log.h"

/** diag fo. 82 */
UTEST(cpu_isolation, test_k)
{
    struct ge g;
    struct ge_console c;
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

