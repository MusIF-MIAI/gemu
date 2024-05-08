#ifndef MSL_TIMINGS_H
#define MSL_TIMINGS_H

#include "ge.h"

typedef void (*msl_command_cb)(struct ge*);

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
    uint8_t (*condition)(struct ge*);


    /**
     * Additional condition for the command
     *
     * In the GE timing charts, this is the equation in parens.
     *
     * If NULL, only `condition` is evaluated, otherwise the commmand
     * will be executed if both `condition` and `additional` return
     * true.
     */
    uint8_t (*additional)(struct ge*);
};

/**
 * Timing chart
 *
 * The timing chart for an entire state of the MSL.
 */
struct msl_timing_state {
    const struct msl_timing_chart *chart;
};

/**
 * Timing chart definitions
 *
 * The timing states of the GE-120, recovered from the manuals.
 */
extern struct msl_timing_state msl_timings[0xff];

const char *msl_comment_for_command(msl_command_cb command);

#endif /* MSL_TIMINGS_H */
