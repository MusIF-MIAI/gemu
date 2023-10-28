#include "utest.h"
#include "../ge.h"
#include "../bit.h"

#define ASSERT_CYCLE(x, name) \
    do {                      \
        printf("\n" name "\n\n"); \
        ASSERT_EQ(g.rSO, x);  \
        ge_run_cycle(&g);     \
    } while (0)

UTEST(load, initial_load) {
    struct ge g;
    struct ge_console c;
    
    ge_init(&g);
    ge_clear(&g);
    ge_load(&g);
    ge_start(&g);

    ASSERT_CYCLE(0x00, "Display");
    ASSERT_CYCLE(0x80, "Initialisation");

    ASSERT_CYCLE(0xc8, "PER-PERI 1");
    ASSERT_EQ(g.rL2, 0x00);
    ASSERT_FALSE(BIT(g.ffFI, 4)); /* always false */
    ASSERT_FALSE(BIT(g.ffFI, 5)); /* connector and channel not busy */

    ASSERT_CYCLE(0xd8, "PER-PERI 2");
    ASSERT_EQ(g.rRE, 0xc0); /* peripheral command(?) for "load 1" initial load */

    ASSERT_CYCLE(0xd9, "PER-PERI 3");
    ASSERT_CYCLE(0xda, "PER-PERI 4");
    ASSERT_CYCLE(0xdb, "PER-PERI 5");

    ASSERT_CYCLE(0xdc, "PER-PERI 6");
    ASSERT_EQ(g.rV1, g.rV3);

    ASSERT_CYCLE(0xcc, "PER-PERI 7");
    ASSERT_EQ(g.rRE, 0x40);
    ASSERT_EQ(g.rRA, 0x40);

    ASSERT_CYCLE(0xca, "TPER-CPER 1");

    ASSERT_CYCLE(0xa8, "TPER-CPER 2");
    ASSERT_EQ((g.rL1 & 0xff00) >> 8, 0);

    ASSERT_CYCLE(0xa9, "TPER-CPER 3");
    ASSERT_EQ(g.rL1, 0);

    ASSERT_CYCLE(0xaa, "TPER-CPER 4");
    ASSERT_EQ((g.rV1 & 0xff00) >> 8, 0);

    ASSERT_CYCLE(0xab, "TPER-CPER 5");
    ASSERT_EQ(g.rV1, 0);

    ASSERT_TRUE(g.RIA0);
    ASSERT_FALSE(g.RESI);
    ASSERT_FALSE(g.RIA2);
    ASSERT_FALSE(g.RIA3);


    ASSERT_CYCLE(0xb8, "TPER-CPER 6");

    /*
    ASSERT_CYCLE(0xea, "TPER-CPER 7");
    ASSERT_CYCLE(0xeb, "TPER-CPER 8");
    ASSERT_CYCLE(0xe3, "Alpha 1");
     */
}
