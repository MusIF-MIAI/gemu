#ifndef READER
#define READER

#include <stdint.h>

#include "transcode.h"   /* enum transcode_mode (the CPU-selected read mode) */

struct ge;

/*
 * The integrated card reader on connector 2 / channel 1, modelled signal-by-
 * signal against the COCA connector (see docs/signals.md). The data path
 * (lu08/fini/data) is the working base; the remaining 1-bit fields are the
 * other COCA pins, each a discrete modelled wire (reader->CPU status and
 * CPU->reader command/mode). They are added inert and wired up phase by phase.
 */
struct ge_integrated_reader {
    /* --- data path (base) --- */
    uint8_t lu08:1;     /* LU08N : character-ready strobe                  */
    uint8_t fini:1;     /* FININ : end-of-read, raised with the last char  */
    uint8_t data;       /* LU00N-LU07N : the transcoded character byte     */

    /* --- reader -> CPU status lines --- */
    uint8_t fiden:1;    /* FIDEN : end-of-sequence (record fully read)     */
    uint8_t lupor:1;    /* LUPOR : reader free / ready (= signal LUPO1)     */
    uint8_t luren:1;    /* LUREN : error (transcoder / jam)                */
    uint8_t lusen:1;    /* LUSEN : out-of-service                          */
    uint8_t lenon:1;    /* LENON : "not operable" condition (LENOB/LENO)   */
    uint8_t bi20:1;     /* BI20  : binary-read 2nd-nibble aux clock        */
    uint8_t pom01:1;    /* POM01 : binary-mode (by-pass) indicator         */
    uint8_t picon:1;    /* PICON : first-column check                      */

    /* --- CPU -> reader command / mode lines (latched on this side) --- */
    uint8_t tu00:1;     /* TU00N : read-strobe clock for RE data           */
    uint8_t tu03:1;     /* TU03N : card-feed / advance clock               */
    uint8_t rifan:1;    /* RIFAN : card reject / eject                     */
    uint8_t regen:1;    /* REGEN : general clear / reset                   */
    uint8_t sesen:1;    /* SESEN : put-in-manual                           */
    uint8_t cocon:1;    /* COCON : mode-select clock (latches the decode)  */
    uint8_t mode_n001:1;/* N001  : normal-mode decode                      */
    uint8_t mode_n002:1;/* N002                                            */
    uint8_t mode_debi:1;/* DEBI  : binary-read (by-pass) decode            */
    uint8_t mode_mi01:1;/* MI01  : mixed-mode decode                       */
    uint8_t mode_mi02:1;/* MI02                                            */

    /* Read mode the CPU has selected, latched by COCON. While active_valid==0
     * (the CPU has not driven a mode) the cardreader peripheral falls back to
     * its harness-registered transcode mode; once the CPU issues a COCON latch
     * active_mode takes over. */
    enum transcode_mode active_mode;
    uint8_t active_valid:1;
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

