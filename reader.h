#ifndef READER
#define READER

#include <stdint.h>

struct ge;

struct ge_integrated_reader {
    uint8_t sent_tu101:1;
    uint8_t sent_tu201:1;

    uint8_t lu08:1;
    uint8_t fini:1;

    uint8_t data;
};

void reader_setup_to_send(struct ge *ge, uint8_t data, uint8_t end);
void reader_clear_sending(struct ge *ge);

void reader_send_tu00(struct ge *);
void reader_send_tu10(struct ge *);

uint8_t reader_get_LU08(struct ge *);
uint8_t reader_get_LUPO1(struct ge *);
uint8_t reader_get_FINI1(struct ge *);

#endif

