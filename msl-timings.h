#ifndef MSL_TIMINGS_H
#define MSL_TIMINGS_H

#include <stdio.h>
#include "ge.h"

struct msl_timing_chart {
    enum clock clock;
    void (*command)(struct ge*);
    uint8_t (*condition)(struct ge*);
};

struct msl_timing_state {
    const struct msl_timing_chart *chart;
};

extern struct msl_timing_state msl_timings[0xff];

#endif /* MSL_TIMINGS_H */
