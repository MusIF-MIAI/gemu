#include "utest.h"

#include "../ge.h"
#include "../console.h"

UTEST(initialitiation, pressing_clear)
{
    struct ge g;

    ge_init(&g);
    ASSERT_TRUE(g.halted);
    ASSERT_TRUE(g.powered);

    ge_clear(&g);
    ASSERT_EQ(g.rSO, 0);
    ASSERT_FALSE(g.AINI);
    ASSERT_FALSE(g.ALAM);
}

UTEST(initialitiation, pressing_clear_start)
{
    struct ge g;

    ge_init(&g);

    ge_clear(&g);
    ASSERT_FALSE(g.AINI);
    ASSERT_FALSE(g.ALAM);
    ASSERT_TRUE(g.ALTO);

    ge_start(&g);
    ASSERT_FALSE(g.AINI);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0x80);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xe2);
}

UTEST(initialitiation, pressing_clear_load_start)
{
    struct ge g;

    ge_init(&g);

    ge_clear(&g);
    ge_load(&g);
    ge_start(&g);

    ge_run_cycle(&g);
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
    ASSERT_FALSE(c.lamps.HALT);
    ASSERT_FALSE(c.lamps.OPERATOR_CALL);

    ge_load(&g);
    ge_start(&g);
    ge_fill_console_data(&g, &c);
    ASSERT_FALSE(c.lamps.HALT);
}
