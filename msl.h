#ifndef MSL_H
#define MSL_H

#include <stdint.h>
#include "ge.h"
#include "msl.h"

struct msl_cell {
    struct msl_cell *next;
    int (*condition)(struct ge*);
    struct command *cmd;
};

struct command {
    const char *name;
    int (*fn)(struct ge*);
};

struct msl_cell *msl_get_cell(enum clock clock, uint8_t status);

#endif /* MSL_H */
