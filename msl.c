#include <stdint.h>

#include "msl.h"

#define MAX_STATUS 255

static struct msl_cell MSL[MAX_CLOCK][MAX_STATUS];

struct msl_cell *msl_get_cell(enum clock clock, uint8_t status)
{
    //return MSL[clock, status];

    MSL[0][0] = MSL[0][0];
    return 0;
}
