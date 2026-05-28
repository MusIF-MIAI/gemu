#include "utest.h"
#include "../ge.h"
#include "../opcodes.h"

/*
 * Test that executing an HLT instruction sets ge->halted = 1.
 *
 * The HLT opcode (0x0A) is fetched through the normal alpha-phase
 * sequence (states 00 -> 80 -> E2/E3).  In state E2_E3 at clock TO80,
 * command CI89 fires when rRO == HLT_OPCODE.  CI89 sets both ALTO and
 * halted.  We run enough cycles to reach the E2/E3 state and verify
 * halted is set.
 */
UTEST(halt, hlt_sets_halted)
{
    uint8_t program[2] = { HLT_OPCODE, 0x00 };
    struct ge g;
    int r;

    ge_init(&g);
    ge_clear(&g);

    /* halted is 0 after ge_clear */
    ASSERT_EQ(g.halted, 0);

    r = ge_load_program(&g, program, sizeof(program));
    ASSERT_EQ(r, 0);

    ge_start(&g);

    /* Cycle 1: Display state (00) -> sets SO = 0x80 */
    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0x80);

    /* Cycle 2: Initialisation state (80) -> sets SO = E2 or E3 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Cycle 3: Alpha state (E2/E3): HLT opcode is in RO, CI89 fires at TO80
     * setting ALTO=1 and halted=1. */
    ge_run_cycle(&g);
    ASSERT_EQ(g.ALTO, 1);
    ASSERT_EQ(g.halted, 1);
}
