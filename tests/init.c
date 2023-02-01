#include "utest.h"

#include "../ge.h"

UTEST(initialitiation, initialitiation_state)
{
    struct ge g;
    int r;

    r = ge_init(&g);
    ASSERT_EQ(r, 0);

    ge_clear(&g);
    ASSERT_TRUE(g.AINI == 0);
    r = ge_load(&g);
    ASSERT_EQ(r, 0);
    ASSERT_TRUE(g.AINI == 1);
    r = ge_start(&g);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(g.rSO, 0x80);
    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xc8);
}
