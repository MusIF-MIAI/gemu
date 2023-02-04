#include "utest.h"

#include "../ge.h"

UTEST(alpha_phase, p_instruction_to_alpha)
{
    uint8_t mem[2] = {NOP2_OPCODE, 0xAA};
    struct ge g;
    int r;


    ge_init(&g);
    ge_clear(&g);
    ASSERT_TRUE(g.AINI == 0);

    r = ge_load_program(&g, mem, sizeof(mem));
    ASSERT_EQ(r, 0);
    ASSERT_TRUE(g.AINI == 0);

    ge_start(&g);
    ASSERT_EQ(g.rSO, 0x80);
    ASSERT_EQ(g.rPO, 0x0000);

    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xe0);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rPO, 0x0002);
    ASSERT_EQ(g.rFO, NOP2_OPCODE);
    ASSERT_EQ(g.rL1 & 0xFF, 0xAA);
    ASSERT_TRUE(g.rSO == 0x64 || g.rSO == 0x65);
}

UTEST(alpha_phase, test_int)
{
    uint8_t mem[2] = {HLT_OPCODE, 0xAA};
    struct ge g;
    int r;

    ge_init(&g);
    ge_clear(&g);

    r = ge_load_program(&g, mem, sizeof(mem));
    ASSERT_EQ(r, 0);

    ge_start(&g);
    ASSERT_EQ(g.rSO, 0x80);

    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* not sure about setting FI to 0... it is set by state 80
     * and i didn't notice where it's supposed to be reset */
    g.ffFI = 0;

    /* setting an interruption */
    g.RINT = 1;

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xf0);
}

UTEST(alpha_phase, test_hlt)
{
    uint8_t mem[2] = {HLT_OPCODE, 0xAA};
    struct ge g;
    int r;

    ge_init(&g);
    ge_clear(&g);

    r = ge_load_program(&g, mem, sizeof(mem));
    ASSERT_EQ(r, 0);

    ge_start(&g);
    ASSERT_EQ(g.rSO, 0x80);

    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    ge_run_cycle(&g);
    ASSERT_TRUE(g.ALTO);

    /* how does the machine behave after ALTO = 1? */
    /* how should the emulation do? */
}

UTEST(alpha_phase, pm_instruction_to_alpha)
{
    /* a jmp instruction taken from software/compile-print.txt */
    const uint8_t opcode = 0x47;
    const uint8_t character = 0xf0;
    const uint8_t arg1 = 0x0d;
    const uint8_t arg2 = 0xfc;

    uint8_t mem[] = {opcode, character, arg1, arg2};

    struct ge g;
    int r;

    ge_init(&g);
    ge_clear(&g);

    r = ge_load_program(&g, mem, sizeof(mem));
    ASSERT_EQ(r, 0);

    ge_start(&g);
    ASSERT_EQ(g.rSO, 0x80);

    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xe0);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xe4);

    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0xe6);

    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0x64 || g.rSO == 0x65);

    ASSERT_EQ(g.rPO, 4);
    ASSERT_EQ(g.rRO, arg2);
    ASSERT_EQ(g.rV1, arg2);
    ASSERT_EQ(g.rV2, arg2);
    ASSERT_EQ(g.rL1, character);
    ASSERT_EQ(g.rL2, arg1);
}
