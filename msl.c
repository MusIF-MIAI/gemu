#include <stdint.h>
#include <stdio.h>

#include "msl.h"
#include "msl-timings.h"
#include "log.h"
#include "ge.h"

const struct msl_timing_state* msl_get_state(uint8_t SO)
{
    const struct msl_timing_state *state = &msl_timings[SO];

    return (!state->chart && !state->variants)
        ? NULL
        : state;
}

const struct msl_timing_variant *msl_select_variant(
    struct ge *ge, const struct msl_timing_state *state)
{
    const struct msl_timing_variant *variant;

    if (!state || !state->variants)
        return NULL;

    for (variant = state->variants; variant && variant->match; variant++) {
        if (variant->match(ge))
            return variant;
    }

    return NULL;
}

static const struct msl_timing_chart *select_chart(
    struct ge *ge, const struct msl_timing_state *state)
{
    const struct msl_timing_variant *variant;

    if (state->chart)
        return state->chart;

    variant = msl_select_variant(ge, state);
    if (variant) {
        if (ge->current_clock == TO00)
            ge_log(LOG_CONDS, "  chart %s (%s)\n", variant->name,
                   variant->manual_ref);
        return variant->chart;
    }

    ge_log(LOG_ERR, "no instruction timing chart for state %02x FO=%02x L1=%02x\n",
           ge->rSA, ge->rFO, ge->rL1 & 0xff);
    return NULL;
}

void msl_run_state(struct ge* ge, const struct msl_timing_state *state)
{
    const struct msl_timing_chart *rows, *chart;
    uint32_t i = 0;

    rows = select_chart(ge, state);
    if (!rows)
        return;

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
