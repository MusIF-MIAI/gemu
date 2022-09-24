#include <stdio.h>
#include <unistd.h>
#include "ge.h"
#include "console_socket.h"

int main(int argc, char *argv[])
{
    uint8_t test_program = 0;
    struct ge ge130;
    int ret;

    ret = ge_init(&ge130);
    if (ret != 0)
        return ret;

    ret = console_socket_register(&ge130);
    if (ret != 0)
        return ret;

    /* load with memory / and or setup peripherics */

    while(1) {
        console_socket_check(&ge130);
        ge_clear(&ge130);
        ge_load(&ge130, &test_program, 1);
        ge_start(&ge130);

        if (!ge130.halted)
            ret = ge_run(&ge130);

        ge_halt(&ge130);
        sleep(1);
        printf(" *** RESTART *** \n");
    }
    ge_deinit(&ge130);
    return ret;
}
