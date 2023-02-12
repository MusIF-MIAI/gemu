#include <stdio.h>
#include <unistd.h>
#include "ge.h"
#include "console_socket.h"
#include "log.h"

int main(int argc, char *argv[])
{
    uint8_t test_program = 0;
    struct ge ge130;
    int ret;

    ge_init(&ge130);

    ret = console_socket_register(&ge130);
    if (ret != 0)
        return ret;

    while(1) {
        /* load with memory / and or setup peripherics */
        ge_clear(&ge130);
        ge_start(&ge130);

        while (!ge130.halted || ret != 0) {
            /* Delay */
            usleep(CLOCK_PERIOD);
            ret = ge_run_pulse(&ge130);
        }

        printf(" *** RESTART *** ");
        sleep(1);
    }

    ge_deinit(&ge130);
    return ret;
}
