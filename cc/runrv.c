/*
 * runrv — put a .cap deck in the reader, run the machine to HLT, print __rv.
 * Test harness for the gec compiler: reports main()'s return value.
 *
 *   build: make (links ../libge.a)
 *   use:   runrv prog.cap [nbytes] [--no-printer]
 *          nbytes defaults to 2; prints decimal + hex
 *
 * The deck goes in through the card reader, exactly as it would on the real
 * machine: CLEAR, LOAD1, LOAD, START. The IPL reads one card and that card
 * pulls the rest of the deck. There is no shortcut here because there is no
 * shortcut on the iron.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../ge.h"
#include "../cardreader.h"
#include "../transcode.h"
#include "../log.h"
#include "../printer.h"

#define RV 0x0010

int main(int argc, char **argv) {
    const char *path = NULL;
    int nbytes = 2;
    int want_printer = 1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--no-printer")) want_printer = 0;
        else if (argv[i][0] == '-') {
            fprintf(stderr, "runrv: unknown option '%s'\n", argv[i]);
            return 2;
        }
        else if (!path) path = argv[i];
        else nbytes = atoi(argv[i]);
    }
    if (!path) {
        fprintf(stderr, "usage: runrv prog.cap [nbytes] [--printer]\n");
        return 2;
    }
    ge_log_set_active_types_from_spec("none");

    static struct ge ge;   /* zero-initialised, like main.c's global */
    ge_init(&ge);
    ge_clear(&ge);
    ge_load_1(&ge);        /* select connector 2 */
    ge_load(&ge);          /* arm AINI; START is what reads the card */
    if (cardreader_register(&ge, path, TC_NORMAL) != 0) {
        fprintf(stderr, "runrv: cannot read deck '%s'\n", path);
        return 2;
    }

    /* The integrated printer is part of the machine, so it is on by default;
     * --no-printer takes it off the channel. It keeps out of the reader's own
     * orders (printer.c, PC121 + LESAB), so it no longer disturbs the load. */
    if (want_printer) {
        const char *in = getenv("GEMU_STDIN");
        printer_register(&ge);
        if (in) {
            for (const unsigned char *p = (const unsigned char *)in; *p; p++)
                printer_feed_key(&ge, *p);
        }
    }

    ge_start(&ge);

    long max = 5000000, i;
    for (i = 0; i < max && !ge_halted(&ge); i++) ge_run_cycle(&ge);

    long val = 0;
    for (int b = 0; b < nbytes; b++) val = (val << 8) | ge.mem[(RV + b) & 0xffff];
    /* sign-extend for signed display */
    long sval = val;
    if (nbytes == 2 && (val & 0x8000)) sval = val - 0x10000;
    if (nbytes == 1 && (val & 0x80))   sval = val - 0x100;

    if (printer_output_len(&ge) > 0)
        printf("__prn = %s\n", printer_output(&ge));
    printf("__rv = %ld (0x%0*lX)%s cycles=%ld\n",
           sval, nbytes * 2, val,
           ge_halted(&ge) ? "" : " [DID NOT HALT]", i);
    ge_deinit(&ge);
    return ge_halted(&ge) ? 0 : 1;
}
