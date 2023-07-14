#include <stdio.h>

#include "utest.h"
#include "../ge.h"

#define ASSERT_CYCLE(x, name) \
    do {                      \
        printf("\n" name "\n\n"); \
        ASSERT_EQ(g.rSO, x);  \
        ge_run_cycle(&g);     \
    } while (0)

UTEST(peri, per_peri) {
    /* first instruction of software/loader.txt */
    uint8_t up_name = 0x80; // connector 2 - 
    uint8_t addr_hi = 0x00;
    uint8_t addr_lo = 0x10;

    uint16_t addr  = (addr_hi << 8) | addr_lo;
    uint8_t  mem[] = { PER_OPCODE, up_name, addr_hi, addr_lo };

    struct ge g;
    struct ge_console c;

    ge_init(&g);
    ge_load_program(&g, mem, sizeof(mem));

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

    ASSERT_CYCLE(0xd8, "PER-PERI 2");
    ASSERT_CYCLE(0xd9, "PER-PERI 3");
    ASSERT_CYCLE(0xda, "PER-PERI 4");
    printf("FI: %08x  FA: %08x\n", g.ffFI, g.ffFA);

    ASSERT_CYCLE(0xdb, "PER-PERI 5");
    printf("FI: %08x  FA: %08x\n", g.ffFI, g.ffFA);

//    ASSERT_CYCLE(0xdc, "PER");

}
