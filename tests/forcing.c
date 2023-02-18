#include "utest.h"
#include "../ge.h"

UTEST(display, show_registers)
{
    struct ge g;
    struct ge_console c;

    ge_init(&g);
    ge_clear(&g);

    g.rPO = 0xaaaa;
    g.rV1 = 0x1010;
    g.rV2 = 0x2020;
    g.rV3 = 0x3030;
    g.rL1 = 0x1111;
    g.rL2 = 0x22;
    g.rL3 = 0x3333;

    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ(c.lamps.ADD_reg, 0xaaaa);

    ge_set_console_rotary(&g, RS_V1);
    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ(c.lamps.ADD_reg, 0x1010);

    ge_set_console_rotary(&g, RS_V2);
    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ(c.lamps.ADD_reg, 0x2020);

    ge_set_console_rotary(&g, RS_V3);
    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ(c.lamps.ADD_reg, 0x3030);

    ge_set_console_rotary(&g, RS_L1);
    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ(c.lamps.ADD_reg, 0x1111);

    ge_set_console_rotary(&g, RS_R1_L2);
    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ(c.lamps.ADD_reg, 0x22);

    ge_set_console_rotary(&g, RS_L3);
    ge_run_cycle(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_EQ(c.lamps.ADD_reg, 0x3333);
}
