#include <stdlib.h>
#include <string.h>

#include "ge.h"
#include "peripherical.h"

int ge_peri_on_pulses(struct ge *ge)
{
    struct ge_peri *p;
    int r;

    for (p = ge->peri; p != NULL; p = p->next) {
        if (p->on_pulse == NULL)
            continue;
        r = p->on_pulse(ge, p->ctx);
        if (r != 0)
            return r;
    }
    return 0;
}

int ge_peri_on_clock(struct ge *ge)
{
    int r;
    struct ge_peri *p;

    for (p = ge->peri; p != NULL; p = p->next) {
        if (p->on_clock == NULL)
            continue;
        r = p->on_clock(ge, p->ctx);
        if (r != 0)
            return r;
    }
    return 0;
}

int ge_peri_deinit(struct ge *ge)
{
    struct ge_peri *p, *p2;
    int r;

    for (p = ge->peri; p != NULL;) {
        p2 = p;
        p = p->next;
        if (p2->deinit != NULL) {
            r = p2->deinit(ge, p2->ctx);
            if (r != 0)
                return r;
        }
    }

    return 0;
}

int ge_register_peri(struct ge *ge, struct ge_peri *p)
{
    struct ge_peri **prec_next = &ge->peri;
    struct ge_peri *pp = ge->peri;

    while (pp != NULL) {
        prec_next = &pp->next;
        pp = pp->next;
    }
    *prec_next = p;
    p->next = NULL;

    if (p->init != NULL)
        return p->init(ge, p->ctx);

    return 0;
}
