#include <stdint.h>
#include <stddef.h>
#include "ge.h"
#include "msl.h"

typedef int (*on_clock_cb)(struct ge *);

static on_clock_cb on_clock[MAX_CLOCK];

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

        if (on_clock[ge->current_clock] != 0) {
            r = on_clock[ge->current_clock](ge);
            if (r != 0)
                return r;
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
