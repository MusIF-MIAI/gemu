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
 *
 * A variant now holds only the rows that CARRY A FAMILY TERM.  Rows the
 * sheets print identically for every family live in the state's common
 * chart -- see `msl_timing_state::chart`.
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
 *
 * The GE-120 has ONE micro-sequence logic per sequencer state.  The several
 * per-family sheets the manual prints for 64|65 and for the executive states
 * are not separate hardware: they are the same gate network read through a
 * decode filter, printed once per instruction family because the manual is
 * organised by family.  A row that carries no family term is therefore a
 * property of the STATE, and the CPU performs it for every instruction that
 * enters the state.
 *
 * gemu models that directly: `chart` holds the rows common to every family,
 * `variants` holds only what the decode actually multiplexes.
 */
struct msl_timing_state {
    /**
     * Rows the state performs unconditionally, whatever the decode.
     *
     * Run BEFORE the selected variant's rows at each clock, so that a
     * variant row may still refine state set up by a common row within the
     * same clock (the printed sheets order them the same way).
     *
     * For a state with no `variants` this is simply the whole chart, which
     * is the ordinary case.
     */
    const struct msl_timing_chart *chart;

    /** Sparse instruction-family matrix for states with multiple sheets. */
    const struct msl_timing_variant *variants;

    /**
     * Sheets that print the rows in `chart`.
     *
     * Only meaningful when `chart` and `variants` are both present: the
     * common rows were factored out of several per-family sheets, so no
     * single `msl_timing_variant::manual_ref` covers them and every
     * contributing sheet is listed here instead.  This keeps every flow
     * affected by the state traceable back to a physical page.
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
