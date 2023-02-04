#include "utest.h"

#include "../ge.h"

UTEST(beta_phase, execute_nop)
{
    uint8_t mem[2] = {NOP2_OPCODE, 0xAA};
    struct ge g;
    int r;

    ge_init(&g);
    ge_clear(&g);

    r = ge_load_program(&g, mem, sizeof(mem));
    ASSERT_EQ(r, 0);

    ge_start(&g);

    /* Initialisation */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Alpha */
    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xe0);

    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0x64 || g.rSO == 0x65);

    /* Beta */
    ASSERT_EQ(g.rFO, NOP2_OPCODE);

    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
    ASSERT_EQ(g.rPO, 0x0002);
}
