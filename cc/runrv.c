/*
 * runrv — load a unified-format image, run it to HLT, print __rv (mem[0x10..]).
 * Test harness for the gec compiler: prints main()'s return value.
 *
 *   build: make (links ../libge.a + ../binimage.c)
 *   use:   runrv prog.bin [bytes]   (bytes default 2; prints decimal + hex)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../ge.h"
#include "../binimage.h"
#include "../log.h"
#include "../printer.h"

#define RV 0x0010

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: runrv prog.bin [nbytes]\n"); return 2; }
    int nbytes = (argc > 2) ? atoi(argv[2]) : 2;
    ge_log_set_active_types_from_spec("none");

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    static uint8_t buf[MEM_SIZE];
    uint16_t origin, entry, len;
    int rc = binimage_read(f, &origin, &entry, buf, sizeof buf, &len);
    fclose(f);
    if (rc != BINIMAGE_OK) { fprintf(stderr, "%s: %s\n", argv[1], binimage_strerror(rc)); return 2; }

    static struct ge ge;   /* zero-initialised, like main.c's global */
    ge_init(&ge);
    ge_clear(&ge);         /* clears halted, seeds identity segment bases */
    if (ge_load_image(&ge, buf, len, origin) != 0) { fprintf(stderr, "load failed\n"); return 2; }
    ge_seed_segment_bases(&ge);
    printer_register(&ge);
    {
        const char *in = getenv("GEMU_STDIN");
        if (in) {
            for (const unsigned char *p = (const unsigned char *)in; *p; p++)
                printer_feed_key(&ge, *p);
        }
    }
    ge_start(&ge);
    ge_enter(&ge, entry);

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
    printf("__rv = %ld (0x%0*lX)%s cycles=%ld%s\n",
           sval, nbytes * 2, val,
           ge_halted(&ge) ? "" : " [DID NOT HALT]", i,
           ge_halted(&ge) ? "" : "");
    ge_deinit(&ge);
    return ge_halted(&ge) ? 0 : 1;
}
