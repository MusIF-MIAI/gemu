/*
 * ge.h - peripheral access for the GE-120 / GE-130.
 *
 * Wraps the compiler's `_`-prefixed primitives so C code never has to know that
 * the machine reaches a peripheral through a PER instruction pointing at an
 * order block in memory.
 *
 *      #include <ge.h>
 *
 *      char card[80];
 *      int  rdr = _open_reader();      // the unit byte, folded at compile time
 *      _read(rdr, card, 80);
 *      if (GE_STATUS(rdr) != GE_QR_GT)
 *              lon();
 *      _close(rdr);
 *
 * The device argument must be an _open_*() call: the PER unit name is an
 * immediate inside the instruction, so it cannot come from a variable.
 */
#ifndef GE_H
#define GE_H

/* Z operation bits (cp04 5.10.1). Combinable; channel in bits 0-2. */
#define GE_CPER   0x80
#define GE_EPER   0x40
#define GE_SPER   0x20
#define GE_LPER   0x10
#define GE_TPER   0x08
#define GE_CH(n)  ((n) & 7)

/* Qualitative result, as _order() returns it (ISA.md 5.1). */
#define GE_QR_ZERO  0
#define GE_QR_LT    1
#define GE_QR_EQ    2
#define GE_QR_GT    3

/*
 * Cooked orders. Only orders this project has actually established appear
 * here. An order carrying GE_EPER returns a result worth testing; one without
 * it does not.
 *
 * GE_STATUS - ask the unit for its condition. X=0x2E is the status poll the
 *             I5I decks, printer-mechanical-test and control-program-cr all use.
 * GE_AVAIL  - is the channel/connector/unit available? The one LPER-bit order
 *             form seen in the decks.
 * GE_ORDER  - raw escape hatch, for an order this header has no name for yet.
 */
#define GE_STATUS(d)     _order((d), GE_CPER | GE_EPER, 0x2E)
#define GE_AVAIL(d)      _order((d), GE_LPER, 0x40)
#define GE_ORDER(d,z,x)  _order((d), (z), (x))

#endif
