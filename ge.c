#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "ge.h"
#include "msl.h"


static int ge_halted(struct ge *ge)
{
    return ge->halted;
}

int ge_init(struct ge *ge)
{
    ge->halted = 1;
    ge->ticks = 0; 
    return 0;
}

/// Emulate the press of the "clear" button in the console
void ge_clear(struct ge * ge)
{
    // From 14023130-0, sheet 5:
    // The pressure of the "CLEAR" push button only determines the continuos
    // performance of the "00" status
    ge->rSO = 0;

    // Also clear the emulated memory... what else?!
    memset(ge->mem, 0, sizeof(ge->mem));
}

/// Emulate the press of the "load" button in the console
void ge_load(struct ge * ge)
{
    // not emulated
}

/// Emulate the press of the "start" button in the console
void ge_start(struct ge * ge)
{
    // From 14023130-0, sheet 5:
    // With the rotating switch in "NORM" position, after the operation
    // "CLEAR-LOAD-START" or "CLEAR-START", the 80 status is performed.
    ge->rSO = 0x80;

    ge->halted = 0;
}

int ge_run_cycle(struct ge *ge)
{
    struct msl_cell *c;
    int r;

    for(;ge->current_clock < MAX_CLOCK; ge->current_clock++) {

        /* Execute machine logic for pulse*/
        pulse(ge);

        c = msl_get_cell(ge->current_clock, ge->rSO);
        for (;c != NULL; c = c->next) {
            if (c->condition && !c->condition(ge))
                continue;
            if (c->cmd == NULL || c->cmd->fn == NULL)
                continue;
            r = c->cmd->fn(ge);
            if (r != 0)
                return r;
        }

    }
    return 0;
}

int ge_run(struct ge *ge)
{
    int r;

    while (!ge_halted(ge)) {
        r = ge_run_cycle(ge);
        if (r != 0)
            return r;
        ge->ticks++;
    }

    return 0;
}
