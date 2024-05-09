#ifndef READER
#define READER

#include <stdint.h>

struct ge;

struct ge_integrated_reader {
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

struct ge_connector {
    const char *name;

    uint8_t data;

    uint8_t mare:1;
    uint8_t te10:1;
    uint8_t te20:1;
    uint8_t te30:1;
    uint8_t fine:1;
};

void connector_setup_to_send(struct ge *, struct ge_connector *, uint8_t, uint8_t);
void connector_send_tu00(struct ge *, struct ge_connector *);
void connector_clear_sending(struct ge_connector *);

uint8_t connector_get_MARE(struct ge_connector *);
uint8_t connector_get_TE10(struct ge_connector *);
uint8_t connector_get_TE20(struct ge_connector *);
uint8_t connector_get_TE30(struct ge_connector *);
uint8_t connector_get_FINE(struct ge_connector *);

#endif

