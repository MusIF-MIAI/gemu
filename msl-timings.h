#ifndef MSL_TIMINGS_H
#define MSL_TIMINGS_H

#include "ge.h"

typedef void (*msl_command_cb)(struct ge*);
typedef uint8_t (*msl_condition_cb)(struct ge*);

/**
 * Timing chart row
 *
 * A row of a timing chart, as described in the manual. Defines
 * what command should be performed at a given clock cycle, and
 * under which conditions.
 */
struct msl_timing_chart {
    /** Clock at which the command should be perfomed */
    enum clock clock;

    /** Pointer to the command function */
    msl_command_cb command;

    /**
     * Condition for the command
     *
     * In the GE timing charts, this is the equation in curly
     * brackets.
     *
     * If NULL, the command will always be executed, otherwise
     * the command will get executed only if the condition
     * returns true.
     */
    msl_condition_cb condition;


    /**
     * Additional condition for the command
     *
     * In the GE timing charts, this is the equation in parens.
     *
     * If NULL, only `condition` is evaluated, otherwise the commmand
     * will be executed if both `condition` and `additional` return
     * true.
     */
    msl_condition_cb additional;
};

/**
 * Timing chart
 *
 * The timing chart for an entire state of the MSL.
 *
 * The GE-120 has ONE micro-sequence logic per sequencer state.  The several
 * per-family sheets the manual prints for 64|65 and for the executive states
 * are not separate hardware: they are the same gate network read through a
 * decode filter, printed once per instruction family because the manual is
 * organised by family.  A row that carries no family term is therefore a
 * property of the STATE, and the CPU performs it for every instruction that
 * enters the state.
 *
 * gemu models that directly: one chart per state, every row carrying its own
 * gate.  There is no dispatch -- nothing selects a sheet, the gates select
 * themselves, exactly as the hardware does.
 */
struct msl_timing_state {
    /** The state's timing chart.  NULL for an unimplemented state. */
    const struct msl_timing_chart *chart;

    /**
     * Sheets this chart was transcribed from.
     *
     * Several states are printed as more than one sheet -- the manual repeats
     * a state once per instruction family because it is organised by family --
     * so a single reference is not always enough and every contributing sheet
     * is listed.  Emitted once per state entry under LOG_CONDS.
     */
    const char *chart_ref;
};
/**
 * Timing chart definitions
 *
 * The timing states of the GE-120, recovered from the manuals.
 */
extern struct msl_timing_state msl_timings[0xff];

const char *msl_comment_for_command(msl_command_cb command);

#endif /* MSL_TIMINGS_H */
