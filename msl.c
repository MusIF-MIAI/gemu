#include <stdint.h>
#include <stdio.h>

#include "msl.h"
#include "msl-timings.h"
#include "log.h"
#include "ge.h"

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
            if (chart->condition) {
                if (!chart->condition(ge)) {
                    ge_log(LOG_CONDS, "  time %-4s - condition false\n", ge_clock_name(ge->current_clock));
                    continue;
                }
                ge_log(LOG_CONDS, "  time %-4s - condition true\n", ge_clock_name(ge->current_clock));
            }
            chart->command(ge);
        }
    } while (chart->clock < END_OF_STATUS);
}
