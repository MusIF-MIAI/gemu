#include <stdio.h>

#include "utest.h"
#include "../ge.h"
#include "../bit.h"

#define ASSERT_CYCLE(x, name) \
    do {                      \
        printf("\n" name "\n\n"); \
        ASSERT_EQ(g.rSO, x);  \
        ge_run_cycle(&g);     \
    } while (0)

UTEST(peri, per_peri) {
    /* first instruction of software/loader.txt */

    uint8_t up_name = 0x80; /* connector 2 */
    uint8_t addr_hi = 0x00;
    uint8_t addr_lo = 0x10;
    uint8_t z  = 0x00;
    uint8_t x  = 0xbb; /* 0x40; */
    uint8_t l1 = 0xcc; /* 0x00; */
    uint8_t l2 = 0xdd; /* 0x4f; */
    uint8_t i1 = 0xee; /* 0x00; */
    uint8_t i2 = 0xff; /* 0x28; */

    uint16_t addr  = (addr_hi << 8) | addr_lo;

    struct ge g;
    struct ge_console c;
    
    ge_init(&g);

    g.mem[0x00] = PER_OPCODE;
    g.mem[0x01] = up_name;
    g.mem[0x02] = addr_hi;
    g.mem[0x03] = addr_lo;
    g.mem[0x10] = z;
    g.mem[0x11] = x;
    g.mem[0x12] = l1;
    g.mem[0x13] = l2;
    g.mem[0x14] = i1;
    g.mem[0x15] = i2;

    ge_clear(&g);
    ge_start(&g);
    
    ASSERT_CYCLE(0x00, "Display");
    ASSERT_CYCLE(0x80, "Initialisation");
    ASSERT_CYCLE(0xe2, "Alpha 1");
    ASSERT_CYCLE(0xe0, "Alpha 2");
    ASSERT_CYCLE(0xe4, "Alpha 3");
    ASSERT_CYCLE(0xe6, "Alpha 4");
    
    ASSERT_CYCLE(0x65, "Beta 1");
    ASSERT_EQ(g.rPO, 0x0004);
    ASSERT_EQ(g.rL1, up_name);
    ASSERT_EQ(g.rV1, addr);
    ASSERT_EQ(g.rV2, addr);
    
    ASSERT_CYCLE(0xc8, "PER-PERI 1");
    ASSERT_EQ(g.rV2, addr + 1);
    ASSERT_EQ(g.rL2, z);
    ASSERT_FALSE(BIT(g.ffFI, 4));

    ASSERT_CYCLE(0xd8, "PER-PERI 2");
    ASSERT_EQ(g.rRE, up_name);

    ASSERT_CYCLE(0xd9, "PER-PERI 3");
    ASSERT_CYCLE(0xda, "PER-PERI 4");
    ASSERT_CYCLE(0xdb, "PER-PERI 5");
    ASSERT_CYCLE(0xdc, "PER-PERI 6");
    ASSERT_EQ(g.rV1, g.rV3);

    ASSERT_CYCLE(0xcc, "PER-PERI 7");
    ASSERT_EQ(g.rRE, x);
    ASSERT_EQ(g.rRA, x);

    /* The sequence, from Status CC, can follow 4 different ways:
     *
     *    1) With !FA05 . FAOO the sequence reaches back into
     * Status D8 to do a decount in PO. This is necessary
     * to carry out a wait recycle for unit free.
     *
     *    2) A return to phase ALPHA is done if the unit is busy and
     * if condition FA05 is present the second time that status CC
     * is entered into.
     * For an LPER the instruction is considered. finished,
     * thus the sequence returns to phase ALPHA.
     *
     *    3) If the instruction is a SPER (!FAOO . !FA05 . DU96 . DU95),
     * the sequence goes to Statuses EA and EB, which perform the
     * unloading of channel 3 conditions.
     *
     *    4) If the instruction is either a CPER or a TPER and all the
     * conditions set down necessary to continue the sequence have
     * been satisfied, Status CA is entered. */

    ASSERT_CYCLE(0xca, "TPER-CPER 1");

    ASSERT_CYCLE(0xa8, "TPER-CPER 2");
    ASSERT_EQ((g.rL1 & 0xff00) >> 8, l1);

    ASSERT_CYCLE(0xa9, "TPER-CPER 3");
    ASSERT_EQ((g.rL1 & 0xff00) >> 8, l1);
    ASSERT_EQ((g.rL1 & 0x00ff) >> 0, l2);

    ASSERT_CYCLE(0xaa, "TPER-CPER 4");
    ASSERT_EQ((g.rV1 & 0xff00) >> 8, i1);

    ASSERT_CYCLE(0xab, "TPER-CPER 5");
    ASSERT_EQ((g.rV1 & 0xff00) >> 8, i1);
    ASSERT_EQ((g.rV1 & 0x00ff) >> 0, i2);

    ASSERT_CYCLE(0xb8, "TPER-CPER 6");
    ASSERT_CYCLE(0xea, "TPER-CPER 7");
    ASSERT_CYCLE(0xeb, "TPER-CPER 8");
    ASSERT_CYCLE(0xe3, "Alpha 1");
}
