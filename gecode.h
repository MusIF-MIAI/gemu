/*
 * gecode.h - GE 100-series internal graphic character code.
 *
 * Single source of the machine's internal character set so the disassembler
 * (gdis, DB-comment glyphs) and the runtime (printer/typewriter capture) cannot
 * drift apart. See gecode.c for the table and its provenance.
 */
#ifndef GECODE_H
#define GECODE_H

#include <stdint.h>

/* Return the ASCII rendering of the GE-120 internal graphic byte `b`, or '.'
 * for any non-graphic code (and for graphics with no ASCII equivalent). */
char ge_glyph(uint8_t b);

#endif /* GECODE_H */
