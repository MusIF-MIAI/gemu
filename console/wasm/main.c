#include <stdio.h>

#include "ge.h"

int main() {
    struct ge ge130;
    struct ge* ge = &ge130;

    ge_clear(ge);
    ge_start(ge);

    for (int i = 0; i < 10000; i++) {
        ge_run_pulse(ge);
    }

    return 0;
}

