#include <stdint.h>
#include <stdio.h>

#include "msl.h"
#include "msl-timings.h"
#include "log.h"
#include "ge.h"

const struct msl_timing_state* msl_get_state(uint8_t SO)
{
    const struct msl_timing_state *state = &msl_timings[SO];

    return state->chart ? state : NULL;
}

static void run_rows(struct ge *ge, const struct msl_timing_chart *rows)
{
    const struct msl_timing_chart *chart;
    uint32_t i = 0;

    do {
        const char *clock_name = ge_clock_name(ge->current_clock);
        chart = &rows[i++];

        if (chart->clock != ge->current_clock)
            continue;

        ge_print_registers_verbose(ge);

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


        ge_log(LOG_CMDS, "    %s\n", msl_comment_for_command(chart->command));
        chart->command(ge);
    } while (chart->clock < END_OF_STATUS);
}

void msl_run_state(struct ge* ge, const struct msl_timing_state *state)
{
    if (!state->chart)
        return;

    if (state->chart_ref && ge->current_clock == TO00)
        ge_log(LOG_CONDS, "  chart (%s)\n", state->chart_ref);

    run_rows(ge, state->chart);
}
