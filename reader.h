#ifndef READER
#define READER

#include <stdint.h>

struct ge;

struct ge_integrated_reader {
    uint8_t sent_tu101:1;
    uint8_t sent_tu201:1;

    uint8_t sending;
    uint8_t data;
};

void reader_send_tu00(struct ge *);
void reader_send_tu10(struct ge *);

uint8_t reader_get_lu08(struct ge *);

#endif

