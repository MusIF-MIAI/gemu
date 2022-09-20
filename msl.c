#include <stdint.h>

#include "msl.h"
#include "msl-timings.h"

struct msl_timing_state* msl_get_state(uint8_t SO)
{
    struct msl_timing_state *state = &msl_timings[SO];

    return (!state->chart)
        ? NULL
        : state;
}

void msl_run_state(struct ge* ge, struct msl_timing_state *state)
{
    const struct msl_timing_chart *chart;
    uint32_t i = 0;

    do {
        chart = &state->chart[i++];
        if (chart->clock == ge->current_clock) {
            if (chart->condition && !chart->condition(ge))
                continue;
            chart->command(ge);
        }
    } while (chart->clock < MAX_CLOCK);
}
