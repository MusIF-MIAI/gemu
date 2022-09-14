#ifndef MSL_H
#define MSL_H

#include <stdint.h>
#include "ge.h"

struct msl_timing_state;

struct msl_timing_state* msl_get_state(uint8_t);
void msl_run_state(struct ge*, struct msl_timing_state *);

#endif /* MSL_H */
