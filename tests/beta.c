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

    /* Display */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0x80);

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

UTEST(beta_phase, execute_lon_loff) {
    uint8_t mem[] = { LON_OPCODE, LON_2NDCHAR, LOFF_OPCODE, LOFF_2NDCHAR };
    struct ge g;
    struct ge_console c;

    ge_init(&g);
    ge_load_program(&g, mem, sizeof(mem));

    ge_clear(&g);
    ge_start(&g);

    /* Display */
    ge_run_cycle(&g);

    /* Initialisation */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Lamps */
    ge_fill_console_data(&g, &c);
    ASSERT_FALSE(c.lamps.OPERATOR_CALL);

    /* Alpha 1 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe0);

    /* Alpha 2 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0x64);

    /* Beta */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.ALAM);
    ASSERT_TRUE(g.PODI);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Lights */
    ge_fill_console_data(&g, &c);
    ASSERT_TRUE(c.lamps.OPERATOR_CALL);

    /* Alpha 1 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe0);

    /* Alpha 2 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0x64);

    /* Beta */
    ge_run_cycle(&g);
    ASSERT_FALSE(g.ALAM);
    ASSERT_FALSE(g.PODI);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Lights */
    ge_fill_console_data(&g, &c);
    ASSERT_FALSE(c.lamps.OPERATOR_CALL);
}

UTEST(beta_phase, execute_ins_ens) {
    uint8_t mem[] = { INS_OPCODE, INS_2NDCHAR, ENS_OPCODE, ENS_2NDCHAR };
    struct ge g;
    struct ge_console c;

    ge_init(&g);
    ge_load_program(&g, mem, sizeof(mem));
    ge_clear(&g);
    ge_start(&g);

    ge_run_cycle(&g);

    /* Initialisation */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Lamps */
    ge_fill_console_data(&g, &c);
    ASSERT_FALSE(c.lamps.OPERATOR_CALL);

    /* Alpha 1 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe0);

    /* Alpha 2 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0x64);

    /* Beta */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.ADIR);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Alpha 1 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe0);

    /* Alpha 2 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0x64);

    /* Beta */
    ge_run_cycle(&g);
    ASSERT_FALSE(g.ADIR);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);
}
