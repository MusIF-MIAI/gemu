#include "utest.h"
#include "../ge.h"
#include "../opcodes.h"

/*
 * Test that executing an HLT instruction sets ge->ALTO = 1.
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

    /* CLEAR stops the CPU: it is START (cpu fo. 97) that releases it.  This
     * used to assert the opposite, because a separate `halted` field was
     * cleared here while ALTO -- the hardware stop flip-flop the assertion
     * now reads -- was being set a few lines later in ge_clear(). */
    ASSERT_EQ(ge_halted(&g), 1);

    r = ge_load_program(&g, program, sizeof(program));
    ASSERT_EQ(r, 0);

    ge_start(&g);
    ASSERT_EQ(ge_halted(&g), 0);     /* START releases the CPU */

    /* Cycle 1: Display state (00) -> sets SO = 0x80 */
    ge_run_cycle(&g);
    ASSERT_EQ(g.rSO, 0x80);

    /* Cycle 2: Initialisation state (80) -> sets SO = E2 or E3 */
    ge_run_cycle(&g);
    ASSERT_TRUE(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Cycle 3: Alpha state (E2/E3): HLT opcode is in RO, CI89 fires at TO80
     * setting ALTO=1, which IS the halted condition. */
    ge_run_cycle(&g);
    ASSERT_EQ(g.ALTO, 1);
    ASSERT_EQ(ge_halted(&g), 1);
}

/*
 * Unknown function codes must not wedge the sequencer.
 *
 * The GE-120 has no invalid-operation trap: a function code that matches no
 * decode performs no datapath commands in beta and execution falls through to
 * the next fetch. Before the not_per_peri fix on the beta CU01 row, any FO
 * outside the implemented op groups looped e0<->64 forever (seen with the
 * funktionalcpu deck's continuous-restart jump into swept core, FO=0x00).
 * Both a 0-address-class (0x00) and an addressed-class (0x80) unknown code
 * must march through to the following HLT.
 */
UTEST(halt, unknown_opcode_0x00_reaches_hlt)
{
    uint8_t program[4] = { 0x00, 0x00, HLT_OPCODE, 0x00 };
    struct ge g;

    ge_init(&g);
    ge_clear(&g);
    ASSERT_EQ(ge_load_program(&g, program, sizeof(program)), 0);
    ge_start(&g);

    for (int i = 0; i < 40 && !ge_halted(&g); i++)
        ge_run_cycle(&g);

    ASSERT_EQ(ge_halted(&g), 1);
    ASSERT_EQ(g.rPO, 2);   /* stopped at the HLT, not wandering */
}

UTEST(halt, unknown_opcode_0x80_reaches_hlt)
{
    uint8_t program[6] = { 0x80, 0x00, 0x00, 0x00, HLT_OPCODE, 0x00 };
    struct ge g;

    ge_init(&g);
    ge_clear(&g);
    ASSERT_EQ(ge_load_program(&g, program, sizeof(program)), 0);
    ge_start(&g);

    for (int i = 0; i < 40 && !ge_halted(&g); i++)
        ge_run_cycle(&g);

    ASSERT_EQ(ge_halted(&g), 1);
}
