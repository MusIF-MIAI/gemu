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

    ge_log_set_active_types(~(LOG_CONDS | LOG_STATES));

    ret = ge_init(&ge130);
    if (ret != 0)
        return ret;

    ret = console_socket_register(&ge130);
    if (ret != 0)
        return ret;

    /* load with memory / and or setup peripherics */

    while(1) {
        ge_clear(&ge130);
        ge_load(&ge130, &test_program, 1);
        ge_start(&ge130);

        ret = ge_run(&ge130);

        sleep(1);
        printf(" *** RESTART *** ");
    }
    ge_deinit(&ge130);
    return ret;
}
