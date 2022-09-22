#ifndef PERI_H
#define PERI_H

#include "ge.h"

int ge_peri_on_clock(struct ge *ge);
int ge_peri_on_pulses(struct ge *ge);
int ge_peri_deinit(struct ge *ge);

#endif /* PERI_H */
