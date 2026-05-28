#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ge.h"
#include "console_socket.h"
#include "cardreader.h"
#include "transcode.h"
#include "log.h"

/*
 * Forward declaration for ge_log_set_active_types_from_spec, which is being
 * added concurrently in log.c/log.h by another agent.  Once that lands the
 * prototype in log.h this extern becomes redundant but harmless.
 */
extern void ge_log_set_active_types_from_spec(const char *spec);

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  --deck <path>        Path to a .cap card deck; loaded via the reader (connector 2)\n"
        "  --trace <spec>       Enable log types from spec string\n"
        "  --max-cycles <N>     Maximum CPU cycles before forced exit (default: 100000)\n"
        "  --console            Enable interactive console socket (/tmp/gemu.console)\n"
        "  --help, -h           Print this help and exit\n",
        argv0);
}

int main(int argc, char *argv[])
{
    struct ge ge;
    int ret = 0;
    long max_cycles = 100000;
    long cycles = 0;
    int use_console = 0;
    const char *deck_path = NULL;

    /* --- argument parsing: --opt value style --- */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--console") == 0) {
            use_console = 1;
        } else if (strcmp(argv[i], "--deck") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --deck requires an argument\n");
                return 1;
            }
            deck_path = argv[++i];
        } else if (strcmp(argv[i], "--trace") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --trace requires an argument\n");
                return 1;
            }
            ge_log_set_active_types_from_spec(argv[++i]);
        } else if (strcmp(argv[i], "--max-cycles") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --max-cycles requires an argument\n");
                return 1;
            }
            max_cycles = atol(argv[++i]);
            if (max_cycles <= 0) {
                fprintf(stderr, "error: --max-cycles must be a positive integer\n");
                return 1;
            }
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    ge_init(&ge);

    if (use_console) {
        ret = console_socket_register(&ge);
        if (ret != 0) {
            ge_deinit(&ge);
            return ret;
        }
    }

    ge_clear(&ge);

    /* Faithful load: drive the LOAD button so the bootstrap pulls the deck in
     * through the card reader on connector 2 (channel 1), exactly as the
     * bootstrap.c test does — no direct mem[] writes. */
    if (deck_path) {
        ge_load_1(&ge);   /* select connector 2 (LOAD1) */
        ge_load(&ge);     /* set AINI: state 80 -> c8 starts the load sequence */
        ret = cardreader_register(&ge, deck_path, TC_NORMAL);
        if (ret != 0) {
            fprintf(stderr, "error: failed to load deck '%s'\n", deck_path);
            ge_deinit(&ge);
            return ret;
        }
    }

    ge_start(&ge);

    while (!ge.halted && cycles < max_cycles) {
        ret = ge_run_cycle(&ge);
        cycles++;
        if (ret != 0)
            break;
    }

    printf("exit: halted=%d cycles=%ld max=%ld error=%d state=%02x PO=%04x\n",
           ge.halted, cycles, max_cycles, ret, ge.rSO, ge.rPO);

    ge_deinit(&ge);
    return ret;
}
