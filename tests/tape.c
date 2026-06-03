/*
 * tape — functional MTC/MTH read through the connector-3/4 core.
 *
 * Write a reel image with one data record followed by a tape mark, attach it on
 * connector 4 (also exercising the ST4 datapath / the PIB41 fix), run a read
 * TPER, and check the record bytes land in memory.
 */
#include <stdio.h>
#include <string.h>

#include "utest.h"
#include "../ge.h"
#include "../bit.h"
#include "../connector34.h"
#include "../tape.h"

UTEST(tape, read_record0)
{
    const char  *path = "/tmp/gemu_tape_test.mt";
    uint8_t      rec[4] = { 0x11, 0x22, 0x33, 0x44 };
    const uint16_t dst  = 0x0040;

    /* Reel: one 4-byte record (uint16 BE length prefix) then a tape mark. */
    FILE *f = fopen(path, "wb");
    ASSERT_TRUE(f != NULL);
    {
        uint8_t hdr[2]  = { 0x00, 0x04 };
        uint8_t mark[2] = { 0x00, 0x00 };
        fwrite(hdr, 1, 2, f);
        fwrite(rec, 1, 4, f);
        fwrite(mark, 1, 2, f);
    }
    fclose(f);

    struct ge g;
    ge_init(&g);

    /* PER naming connector-4 unit 0 (up_name 0x40); order block: z=0,
     * order=READ(0x40), length 0x0010, destination 0x0040. */
    g.mem[0x00] = PER_OPCODE;
    g.mem[0x01] = 0x40;
    g.mem[0x02] = 0x00;
    g.mem[0x03] = 0x10;
    g.mem[0x10] = 0x00;
    g.mem[0x11] = 0x40;
    g.mem[0x12] = 0x00;
    g.mem[0x13] = 0x10;
    g.mem[0x14] = (dst >> 8);
    g.mem[0x15] = (dst & 0xff);

    ge_clear(&g);
    ASSERT_EQ(tape_register(&g, path, 4, 0), 0);
    ge_start(&g);

    for (int i = 0; i < 200; i++)
        ge_run_cycle(&g);

    for (int i = 0; i < 4; i++)
        ASSERT_EQ(g.mem[dst + i], rec[i]);
}
