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
 * One instruction-family variant of a sequencer state.
 *
 * The GE-120 manual prints several timing charts for the same numerical
 * state (notably 64|65 and the executive-state pairs), selected by the
 * instruction decode matrix.  Keeping each sheet in a separate chart makes
 * the transcribed rows directly comparable with the corresponding manual page.
 */
struct msl_timing_variant {
    /** Instruction-family decode.  A NULL match terminates the variant list. */
    msl_condition_cb match;

    /** Timing rows currently transcribed for this family and state. */
    const struct msl_timing_chart *chart;

    /** Stable names used in diagnostics and timing-trace tests. */
    const char *name;
    const char *manual_ref;
};

/**
 * Timing chart
 *
 * The timing chart for an entire state of the MSL.
 */
struct msl_timing_state {
    /** Ordinary single-chart state, mutually exclusive with variants. */
    const struct msl_timing_chart *chart;

    /** Sparse instruction-family matrix for states with multiple sheets. */
    const struct msl_timing_variant *variants;
};

/**
 * Timing chart definitions
 *
 * The timing states of the GE-120, recovered from the manuals.
 */
extern struct msl_timing_state msl_timings[0xff];

const char *msl_comment_for_command(msl_command_cb command);

#endif /* MSL_TIMINGS_H */
