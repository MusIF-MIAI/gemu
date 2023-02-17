#ifndef MSL_H
#define MSL_H

#include <stdint.h>
#include "ge.h"

struct msl_timing_state;

/**
 * Gets timing state
 *
 * Returns the timing definitions for a given state
 *
 * @param  state the state as used by the SO cpu sequencer
 * @return a pointer to a static definition of the given state
 */
struct msl_timing_state* msl_get_state(uint8_t state);

/**
 * Runs a machine state
 *
 * Modifies the machine state by running the given state according
 * to the current emualted clock time.
 *
 * @param ge    the emulator state
 * @param state the state to run
 */
void msl_run_state(struct ge* ge, struct msl_timing_state *state);

#endif /* MSL_H */
