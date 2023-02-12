#include <stdio.h>

#include "ge.h"

int main() {
    struct ge ge130;
    struct ge* ge = &ge130;

    ge_clear(ge);
    ge_start(ge);

    ge_run_pulse(ge);

    return 0;
}

