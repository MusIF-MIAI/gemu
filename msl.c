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
    char done_log = 0;

    do {
        const char *clock_name = ge_clock_name(ge->current_clock);
        chart = &state->chart[i++];

        if (chart->clock != ge->current_clock)
            continue;

        if (!done_log) {
            ge_print_registers_verbose(ge);
            done_log = 1;
        }

        if (chart->additional) {
            if (!chart->additional(ge)) {
                ge_log(LOG_CONDS, "  time %-4s - additional false\n", clock_name);
                continue;
            }
            ge_log(LOG_CONDS, "  time %-4s - additional true\n", clock_name);
        }

        if (chart->condition) {
            if (!chart->condition(ge)) {
                ge_log(LOG_CONDS, "  time %-4s - condition false\n", clock_name);
                continue;
            }
            ge_log(LOG_CONDS, "  time %-4s - condition true\n", clock_name);
        }

        chart->command(ge);
    } while (chart->clock < END_OF_STATUS);
}
