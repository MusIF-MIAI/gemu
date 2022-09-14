#include <stdint.h>

#include "msl.h"
#include "msl-timings.h"

struct msl_timing_state* msl_get_state(uint8_t SO)
{
    struct msl_timing_state *state = &msl_timings[SO];

    return (!state->count || !state->chart)
        ? NULL
        : state;
}

void msl_run_state(struct ge* ge, struct msl_timing_state *state)
{
    struct msl_timing_chart *chart;

    for (int i = 0; i < state->count; i++) {
        chart = &state->chart[i];

        if (chart->clock == ge->current_clock) {
            if (chart->condition && !chart->condition(ge))
                continue;

            chart->command(ge);
        }
    }
}
