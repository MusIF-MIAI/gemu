#include "ge.h"
#include "log.h"
#include "signals.h"

#define ENUMERATE_READER_COMMANDS \
    X(0x40, read,          "Read unchanged") \
    X(0x21, read_normal_1, "Read normal i" ) \
    X(0x01, read_normal_2, "Read normal ii") \
    X(0x24, read_mixed_1,  "Read mixed i"  ) \
    X(0x04, read_mixed_2,  "Read mixed ii" ) \
    X(0x20, read_binary,   "Read binary"   ) \
    X(0xa1, put_normal_1,  "Put normal i"  ) \
    X(0x81, put_normal_2,  "Put normal ii" ) \
    X(0xa4, put_mixed_1,   "Put mixed i"   ) \
    X(0x84, put_mixed_2,   "Put mixed ii"  ) \
    X(0xa0, put_binary,    "Put binary"    ) \
    X(0xac, put_manual,    "Put manual"    ) \
    X(0x48, card_reject,   "Card_reject"   ) \
    X(0x0c, no_function,   "No function"   )

void reader_send_tu00(struct ge *ge)
{
    uint8_t command = ge->rRE;
    ge_log(LOG_READER, "EMIT TU201 (CE10)\n");

    switch (command) {
#define X(cmd, name, desc) case cmd: ge_log(LOG_READER, "    Command: %02x - %s\n", cmd, desc ); break;
            ENUMERATE_READER_COMMANDS
#undef X
    }
}

void reader_setup_to_send(struct ge *ge, uint8_t data, uint8_t end)
{
    ge->integrated_reader.lu08 = 1;
    ge->integrated_reader.data = data;
    ge->integrated_reader.fini = end;

    /* UNIV 1.2µs --> RC01 */
    /* signal cpu of incoming data */
    ge->RC01 = 1;

    ge_log(LOG_READER, "    RA101 = %d\n", RA101(ge));
    ge_log(LOG_READER, "    RF101 = %d\n", RF101(ge));
    ge_log(LOG_READER, "    Signaling incoming data\n");

    /* signal end character */
    /* todo: should use RF101 here? is "if (end)" correct? */
    if (end)
        ge->RIG1 = 1;

    /* todo: should be conditioned by PIM11, but it's false at this point
     * without this, we don't get to state ea after waiting state b8 when
     * reading */
    if (end) {
        ge->PEC1 = 1;
    }
}

void reader_clear_sending(struct ge *ge) 
{
    ge->integrated_reader.lu08 = 0;
    ge->integrated_reader.data = 0;
}

void reader_send_tu10(struct ge *ge)
{
    ge_log(LOG_READER, "EMIT TU101 (CE09)\n");
    ge_log(LOG_READER, "    Card feed\n");
}

uint8_t reader_get_LU08(struct ge *ge)
{
    ge_log(LOG_READER, "reading LU081\n");

    if (ge->integrated_reader.lu08) {
        ge_log(LOG_READER,
               "    wanting to send char: %02x\n",
               ge->integrated_reader.data);
    }

    return ge->integrated_reader.lu08;
}

uint8_t reader_get_LUPO1(struct ge *ge)
{
    ge_log(LOG_READER, "reading LUPO1\n");
    return 0;
}

uint8_t reader_get_FINI1(struct ge *ge)
{
    ge_log(LOG_READER, "**** reading FINI1 %d\n", ge->integrated_reader.fini);
    return ge->integrated_reader.fini;
}
