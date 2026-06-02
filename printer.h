/*
 * printer.h - Integrated printer / console-typewriter peripheral (channel 2).
 *
 * Pragmatic model: gemu does not drive the channel-2 (rSI) per-character
 * transfer states at signal level, so a print/typewriter PER leaves the CPU
 * suspended in the org-phase external request-wait (state b8, rSA idle). This
 * peripheral detects that stall and completes the operation the way the real
 * channel-2 unit would — by asserting the unit-ready condition (PUC2, which
 * makes DU97 true) and the CPU-active request (RC00) so the machine's OWN
 * state_b8 microcode (CU01/CU13/CU14/CU06) routes B8 -> alpha with the CPU
 * context intact. No state is forced from outside.
 *
 * Output is a real per-character transfer: an output PER's order block (entry at
 * state c8, base = rV1; {z, cmd, len_hi, len_lo, buf_hi, buf_lo}) arms the
 * transfer engine when the command is a put (bit 7) with a plausible length, and
 * the rSI output microcode (state 02, CE16) drains mem[buf..] over channel 2 to
 * the sink, each byte rendered through the GE 100-series graphic set (gecode.c) —
 * the machine's own code, not ASCII. A channel-2 PER that parks at the b8 wait
 * but is NOT an armed data transfer (a control/order PER, e.g. funktionalcpu's
 * banner/report) is completed but prints nothing.
 *
 * Two-way: kbd[] is the operator-keyboard input queue (printer_feed_key); the
 * wasm/interactive front-end pushes typed characters here.
 */
#ifndef PRINTER_H
#define PRINTER_H

#include "ge.h"

/* Register the integrated printer/typewriter on channel 2. Sets
 * ge->integrated_printer.present = 1 so the channel-2 print-wait is completed
 * (interactive/wasm runs). Bootstrap/reader tests that do not call this leave
 * present == 0 and are unaffected. Returns 0 on success, -1 on error. */
int printer_register(struct ge *ge);

/* Begin a channel-2 OUTPUT transfer: print `length` characters starting at
 * memory address `buffer`. The printer then requests a channel-2 cycle per
 * character (RC02 + rSI state 0x02) so the machine's microcode drains the buffer
 * to the printer. `line_mode` appends a paper break when the transfer ends;
 * this is used by the line printer's WRITE order but not by the console
 * typewriter PUT path. (Seam for the org-phase PER hook; also used directly by
 * tests.) */
void printer_begin_output(struct ge *ge, uint16_t buffer, int length, int line_mode);

/* Push one operator-keyboard byte into the input queue (two-way chat). */
void printer_feed_key(struct ge *ge, uint8_t c);

/* Number of captured printed characters currently in the paper-feed buffer. */
int printer_output_len(struct ge *ge);

/* Pointer to the captured printed characters (NUL-terminated view via len). */
const char *printer_output(struct ge *ge);

/* Clear the captured paper-feed buffer (e.g. after the front-end drained it). */
void printer_output_clear(struct ge *ge);

#endif /* PRINTER_H */
