#include "utest.h"

#include "../ge.h"

UTEST(initialitiation, initialitiation_state)
{
    struct ge g;
    int r;

    r = ge_init(&g);
    ASSERT_EQ(r, 0);

    ge_clear(&g);
    ASSERT_EQ(g.rSO, 0);
    ASSERT_FALSE(g.operator_call);
    ASSERT_FALSE(g.AINI);

    ge_load(&g);
    ASSERT_TRUE(g.AINI);

    ge_start(&g);
    ASSERT_EQ(g.rSO, 0x80);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xc8);
}
