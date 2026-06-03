/*
 * disk — functional DSS 156-157 read through the connector-3/4 core.
 *
 * Write a pack image whose sector 0 holds a known pattern, attach it on
 * connector 3, run a read TPER, and check the sector bytes land in memory
 * (the device nibble-splits each byte; the binary-mode channel packs them
 * back, so memory reconstructs the original bytes).
 */
#include <stdio.h>
#include <string.h>

#include "utest.h"
#include "../ge.h"
#include "../bit.h"
#include "../connector34.h"
#include "../disk.h"

UTEST(disk, read_sector0)
{
    const char  *path = "/tmp/gemu_disk_test.dsk";
    uint8_t      want[8] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 };
    const uint16_t dst   = 0x0040;

    /* Pack image: sector 0 = want, padded to one 64-byte sector. */
    FILE *f = fopen(path, "wb");
    ASSERT_TRUE(f != NULL);
    fwrite(want, 1, sizeof want, f);
    {
        uint8_t pad[64 - 8] = { 0 };
        fwrite(pad, 1, sizeof pad, f);
    }
    fclose(f);

    struct ge g;
    ge_init(&g);

    /* PER naming connector-3 unit 0; order block: z=0, order=READ(0x40),
     * length 0x0040, destination 0x0040. */
    g.mem[0x00] = PER_OPCODE;
    g.mem[0x01] = 0x00;
    g.mem[0x02] = 0x00;
    g.mem[0x03] = 0x10;
    g.mem[0x10] = 0x00;
    g.mem[0x11] = 0x40;
    g.mem[0x12] = 0x00;
    g.mem[0x13] = 0x40;
    g.mem[0x14] = (dst >> 8);
    g.mem[0x15] = (dst & 0xff);

    ge_clear(&g);
    ASSERT_EQ(disk_register(&g, path, 3, 0), 0);
    ge_start(&g);

    for (int i = 0; i < 500; i++)
        ge_run_cycle(&g);

    for (int i = 0; i < 8; i++)
        ASSERT_EQ(g.mem[dst + i], want[i]);
}
