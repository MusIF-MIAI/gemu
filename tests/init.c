#include "utest.h"

#include "../ge.h"
#include "../console.h"

UTEST(initialitiation, initialitiation_state)
{
    struct ge g;

    ge_init(&g);
    ge_clear(&g);
    ASSERT_EQ(g.rSO, 0);
    ASSERT_FALSE(g.AINI);
    ASSERT_FALSE(g.operator_call);

    ge_load(&g);
    ASSERT_TRUE(g.AINI);

    ge_start(&g);
    ASSERT_EQ(g.rSO, 0x80);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xc8);
}

UTEST(initialitiation, initialitiation_lamps)
{
    struct ge g;
    struct ge_console c;

    ge_init(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_TRUE(c.lamps.HALT);
    ASSERT_TRUE(c.lamps.POWER_ON);

    ge_clear(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_FALSE(c.lamps.OPERATOR_CALL);

    ge_load(&g);
    ge_start(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_FALSE(c.lamps.HALT);
}
