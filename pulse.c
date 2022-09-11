#include <stddef.h>
#include <stdlib.h>
#include "ge.h"
#include "pulse.h"

int pulse_register_event(struct ge *ge, enum clock clk, on_pulse_cb cb)
{
    struct pulse_event *c_ev, *l;
    l = ge->on_pulse[clk];
    
    c_ev = malloc(sizeof(struct pulse_event));
    if (c_ev == NULL)
        return -1;
    c_ev->cb = cb;
    c_ev->next = NULL;
    if (!l) {
        ge->on_pulse[ge->current_clock] = c_ev;
        return 0;
    }
    while(l->next)
        l = l->next;
    l->next = c_ev;
    return 0;
} 

static int reset_RO(struct ge *ge)
{
    ge->rRO = 0;
    return 0;
}

static int load_BO(struct ge *ge)
{
    /* ... */
    return 0;
}

static int load_VO(struct ge *ge)
{
    /* ... */
    return 0;
}


int pulse_event_init(struct ge *ge)
{
    pulse_register_event(ge, TO20, reset_RO);
    pulse_register_event(ge, TO20, load_BO);
    pulse_register_event(ge, TO20, load_VO);

}
