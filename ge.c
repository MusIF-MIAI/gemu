#include <stdint.h>
#include <stddef.h>
#include "ge.h"
#include "msl.h"


static int ge_halted(struct ge *ge)
{
    return 1;
}

int ge_init(struct ge *ge)
{
    ge->ticks = 0; 
    return 0;
}

int ge_run_cycle(struct ge *ge)
{
    struct msl_cell *c;
    int r;

    for(;ge->current_clock < MAX_CLOCK; ge->current_clock++) {
        struct pulse_event *c_ev = ge->on_pulse[ge->current_clock];
        while (c_ev) {
            if (c_ev->cb) {
                r = c_ev->cb(ge);
                if (r != 0)
                    return r;
            }
            c_ev = c_ev->next;
        }

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
