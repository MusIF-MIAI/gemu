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
    /* just for testing purposes */
    ge->integrated_reader.sent_tu201 = 1;

    ge_log(LOG_PERI, "EMIT TU201 (CE10)\n");

    uint8_t command = ge->rRE;

    switch (command) {
#define X(cmd, name, desc) case cmd: ge_log(LOG_PERI, "    Command: %02x - %s\n", cmd, desc ); break;
            ENUMERATE_READER_COMMANDS
#undef X
    }
}

void reader_send_tu10(struct ge *ge)
{
    /* just for testing purposes */
    ge->integrated_reader.sent_tu101 = 1;

    ge_log(LOG_PERI, "EMIT TU101 (CE09)\n");
    ge_log(LOG_PERI, "    Card feed\n");

    /* send back fake data */
    ge->integrated_reader.sending = 1;
    ge->integrated_reader.data = 0x69;

    /* UNIV 1.2µs --> RC01 */
    /* signal cpu of incoming data */

    /* todo: RA101 seems to be incorrect here: check the PBxx decoding */
    ge->RC01 = 1;
    ge_log(LOG_PERI, "    RA101 = %d\n", RA101(ge));
    ge_log(LOG_PERI, "    Signaling incoming data\n");
}

uint8_t reader_get_lu08(struct ge *ge)
{
    ge_log(LOG_PERI, "reading LU081\n");

    if (ge->integrated_reader.sending) {
        ge_log(LOG_PERI,
               "    wanting to send char: %02x\n", 
               ge->integrated_reader.data);
    }

    return ge->integrated_reader.sending;
}
