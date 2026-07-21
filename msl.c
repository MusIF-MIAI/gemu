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

/**
 * Rows the decode multiplexes for the instruction currently in FO.
 *
 * NULL when the state has no variant matrix, or -- the tripwire -- when it
 * has one and nothing matched, which means a family reached an executive
 * state whose sheet has not been transcribed yet.  The state's common rows
 * still run in that case: they carry no family term, so the real MSL would
 * perform them regardless of what the decode matrix says.
 */
static const struct msl_timing_chart *select_variant_chart(
    struct ge *ge, const struct msl_timing_state *state)
{
    const struct msl_timing_variant *variant;

    if (!state->variants)
        return NULL;

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
    const struct msl_timing_chart *variant_rows;

    /* Resolve the decode first so the "chart <name>" trace line is emitted
     * before the rows it explains, but run the common rows first: they are
     * the state's own, and the sheets print them ahead of the family rows
     * they share a clock with. */
    variant_rows = select_variant_chart(ge, state);

    if (state->chart) {
        if (state->variants && state->chart_ref && ge->current_clock == TO00)
            ge_log(LOG_CONDS, "  chart common (%s)\n", state->chart_ref);
        run_rows(ge, state->chart);
    }

    if (variant_rows)
        run_rows(ge, variant_rows);
}
