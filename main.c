#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "ge.h"
#include "console_socket.h"
#include "cardreader.h"
#include "printer.h"
#include "transcode.h"
#include "binimage.h"
#include "log.h"
#include <fcntl.h>

/*
 * Forward declaration for ge_log_set_active_types_from_spec, which is being
 * added concurrently in log.c/log.h by another agent.  Once that lands the
 * prototype in log.h this extern becomes redundant but harmless.
 */
extern void ge_log_set_active_types_from_spec(const char *spec);

/*
 * Launch the ncurses console client (console/curses/console.py) as a child
 * process for --tui. The client connects to the /tmp/gemu.console socket that
 * --console registers and draws the operator/diagnostic panel; the emulator
 * keeps running in this (parent) process. Returns the child pid, or -1 if the
 * client could not be found / launched.
 *
 * The script is looked for next to the ge executable first (so it works from
 * any cwd), then relative to the current directory.
 */
/* Interactive console switches driven by signals: SIGUSR1 toggles SWITCH 1
 * (JS1), SIGUSR2 toggles SWITCH 2 (JS2). The handler only sets a flag; the
 * run loop applies it between cycles (so we never touch ge state from a
 * handler). Lets a human (or an automated harness) flip the diagnostic
 * switches mid-run: e.g. start the funktionalcpu test with SWITCH 2 on, then
 * `kill -USR2 <pid>` to release it and watch where the deck goes. */
static volatile sig_atomic_t g_toggle_js1 = 0;
static volatile sig_atomic_t g_toggle_js2 = 0;
static void on_sigusr1(int sig) { (void)sig; g_toggle_js1 = 1; }
static void on_sigusr2(int sig) { (void)sig; g_toggle_js2 = 1; }

static pid_t spawn_tui(const char *argv0)
{
    char path[4096];
    const char *slash = strrchr(argv0, '/');

    if (slash) {
        int dlen = (int)(slash - argv0);
        snprintf(path, sizeof(path), "%.*s/console/curses/console.py", dlen, argv0);
    } else {
        snprintf(path, sizeof(path), "console/curses/console.py");
    }
    if (access(path, R_OK) != 0)
        snprintf(path, sizeof(path), "console/curses/console.py");
    if (access(path, R_OK) != 0) {
        fprintf(stderr, "error: --tui: cannot find console/curses/console.py\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execlp("python3", "python3", path, (char *)NULL);
        perror("error: --tui: cannot exec python3");
        _exit(127);
    }
    return pid;
}

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS] [prog.bin]\n"
        "\n"
        "  prog.bin             Unified-format image (gasm output); loaded directly\n"
        "                       at its origin and entered at its entry point.\n"
        "\n"
        "Options:\n"
        "  --raw                Treat the positional file as a headerless flat image\n"
        "  --org <0xNNNN>       Load origin for --raw (default 0x0000; entry = origin)\n"
        "  --poke <A=V>         Write byte V to address A after load (repeatable),\n"
        "                       e.g. --poke 0x0E00=0x80 to set a diagnostic option\n"
        "  --deck <path>        Path to a .cap card deck; loaded via the reader (connector 2)\n"
        "  --trace <spec>       Enable log types from spec string\n"
        "  --max-cycles <N>     Maximum CPU cycles before forced exit (default: 100000)\n"
        "  --console            Enable the console socket /tmp/gemu.console (no UI attached)\n"
        "  --tui                Implies --console and starts the ncurses console client\n"
        "  --interactive, -i    Run until killed; SIGUSR1/SIGUSR2 toggle SWITCH 1/2 at\n"
        "                       runtime (prints the pid + step/halt progress)\n"
        "  --switch1            Start with SWITCH 1 (JS1) on\n"
        "  --switch2            Start with SWITCH 2 (JS2) on\n"
        "                       (console/curses/console.py); runs until you quit the TUI\n"
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
    int use_tui = 0;
    int trace_set = 0;
    const char *deck_path = NULL;
    const char *image_path = NULL;
    int raw = 0;
    long load_org = 0x0000;
    uint16_t poke_addr[32]; uint8_t poke_val[32]; int npoke = 0;
    int interactive = 0;   /* --interactive: run until killed, switches via signals */
    int sw1_init = 0;      /* --switch1: start with SWITCH 1 (JS1) on */
    int sw2_init = 0;      /* --switch2: start with SWITCH 2 (JS2) on */

    /* --- argument parsing: --opt value style --- */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--console") == 0) {
            use_console = 1;
        } else if (strcmp(argv[i], "--tui") == 0) {
            use_tui = 1;
            use_console = 1;   /* --tui implies --console */
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
            trace_set = 1;
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
        } else if (strcmp(argv[i], "--interactive") == 0 || strcmp(argv[i], "-i") == 0) {
            interactive = 1;
        } else if (strcmp(argv[i], "--switch1") == 0) {
            sw1_init = 1;
        } else if (strcmp(argv[i], "--switch2") == 0) {
            sw2_init = 1;
        } else if (strcmp(argv[i], "--bin") == 0) {
            /* Force direct binary (unified-format) load — debugging path. */
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --bin requires an argument\n");
                return 1;
            }
            image_path = argv[++i];
        } else if (strcmp(argv[i], "--raw") == 0) {
            raw = 1;
        } else if (strcmp(argv[i], "--org") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --org requires an argument\n");
                return 1;
            }
            load_org = strtol(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--poke") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --poke requires ADDR=VAL\n");
                return 1;
            }
            char *eq = strchr(argv[i + 1], '=');
            if (!eq || npoke >= 32) {
                fprintf(stderr, "error: bad --poke '%s' (want 0xADDR=0xVAL)\n", argv[i + 1]);
                return 1;
            }
            poke_addr[npoke] = (uint16_t)strtoul(argv[i + 1], NULL, 0);
            poke_val[npoke]  = (uint8_t)strtoul(eq + 1, NULL, 0);
            npoke++;
            i++;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else if (!image_path && !deck_path) {
            /* Positional input. The DEFAULT is the authentic card-reader flow:
             * a `.cap` deck is fed through the reader (CLEAR/LOAD/START
             * bootstrap). Any other file (e.g. a `.bin`) is a direct
             * unified-format binary load — the debugging path. `--deck` / `--bin`
             * force the respective path explicitly. */
            const char *p = argv[i];
            size_t n = strlen(p);
            if (n >= 4 && strcmp(p + n - 4, ".cap") == 0)
                deck_path = p;
            else
                image_path = p;
        } else {
            fprintf(stderr, "error: unexpected argument '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (deck_path && image_path) {
        fprintf(stderr, "error: give either a binary image or --deck, not both\n");
        return 1;
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
    int image_loaded = 0;
    uint16_t image_entry = 0;
    if (deck_path) {
        ge_load_1(&ge);   /* select connector 2 (LOAD1) */
        ge_load(&ge);     /* set AINI: state 80 -> c8 starts the load sequence */
        ret = cardreader_register(&ge, deck_path, TC_NORMAL);
        if (ret != 0) {
            fprintf(stderr, "error: failed to load deck '%s'\n", deck_path);
            ge_deinit(&ge);
            return ret;
        }
    } else if (image_path) {
        /* Direct binary load: read the unified-format image (gasm output) and
         * place it at its origin; entry comes from the header. With --raw the
         * file is a headerless flat blob loaded at --org (entry = origin). */
        static uint8_t buf[MEM_SIZE];
        uint16_t origin, entry, len;
        FILE *f = fopen(image_path, "rb");
        if (!f) {
            fprintf(stderr, "error: cannot open image '%s'\n", image_path);
            ge_deinit(&ge);
            return 1;
        }
        if (raw) {
            size_t n = fread(buf, 1, sizeof buf, f);
            origin = (uint16_t)load_org;
            entry  = origin;
            len    = (uint16_t)n;
        } else {
            int rc = binimage_read(f, &origin, &entry, buf, sizeof buf, &len);
            if (rc != BINIMAGE_OK) {
                fprintf(stderr, "error: %s: %s\n", image_path, binimage_strerror(rc));
                fclose(f);
                ge_deinit(&ge);
                return 1;
            }
        }
        fclose(f);
        if (ge_load_image(&ge, buf, len, origin) != 0) {
            fprintf(stderr, "error: image does not fit in memory\n");
            ge_deinit(&ge);
            return 1;
        }
        /* A contiguous image spanning 0x00F0-0x00FF overwrites the segment-base
         * registers ge_clear set up; re-establish the identity bases so paged
         * addressing works (programs may still reload bases at runtime). */
        ge_seed_segment_bases(&ge);
        image_loaded = 1;
        image_entry  = entry;
        ge_log(LOG_DEBUG, "loaded %u bytes at 0x%04x, entry 0x%04x\n",
               (unsigned)len, origin, entry);
    }

    /* Apply --poke writes after the image + base seeding (e.g. the diagnostic
     * console test-selection byte at 0x0E00) so they override loaded values. */
    for (int p = 0; p < npoke; p++) {
        ge.mem[poke_addr[p]] = poke_val[p];
        ge.mem_parity[poke_addr[p]]  = __builtin_parity(poke_val[p]) ? 0 : 1;
        ge.mem_written[poke_addr[p]] = 1;
        ge_log(LOG_DEBUG, "poke mem[0x%04x] = 0x%02x\n", poke_addr[p], poke_val[p]);
    }

    ge_start(&ge);

    /* Direct binary load enters at the header's entry point without the
     * peripheral LOAD bootstrap. */
    if (image_loaded)
        ge_enter(&ge, image_entry);

    /* Console switch initial state (after ge_start, which clears them). */
    ge.JS1 = sw1_init;
    ge.JS2 = sw2_init;

    if (interactive) {
        /* Signal-driven interactive run: flip the diagnostic switches with
         * `kill -USR1/-USR2 <pid>` and watch the deck. Run until killed.
         * Freeze PC on HLT (the GE-120 sequencer is frozen by ALTO when
         * halted) so the stop address stays readable; signals are still
         * serviced so you can record a switch change before restarting. */
        signal(SIGUSR1, on_sigusr1);
        signal(SIGUSR2, on_sigusr2);
        if (!trace_set)
            ge_log_set_active_types_from_spec("none");
        /* Integrated printer/typewriter on channel 2: completes print PERs (so
         * the machine does not hang waiting for a device gemu does not drive at
         * signal level) and captures output. Two-way: bytes typed on stdin are
         * fed to the operator keyboard queue (non-blocking). */
        printer_register(&ge);
        int kbd_fl = fcntl(0, F_GETFL, 0);
        if (kbd_fl != -1)
            fcntl(0, F_SETFL, kbd_fl | O_NONBLOCK);
        int printed = 0;   /* bytes of printer output already echoed to stdout */
        long pid = (long)getpid();
        printf("interactive: pid=%ld  SWITCH1=%d SWITCH2=%d\n", pid, ge.JS1, ge.JS2);
        printf("  kill -USR1 %ld   # toggle SWITCH 1 (JS1)\n", pid);
        printf("  kill -USR2 %ld   # toggle SWITCH 2 (JS2)\n", pid);
        printf("  type to feed the operator keyboard; printer output appears as 'PRN> ...'\n");
        fflush(stdout);
        uint8_t last_step = ge.mem[0x0010];
        int was_halted = -1;
        for (;;) {
            /* Drain newly-printed characters to the terminal. */
            int olen = printer_output_len(&ge);
            if (olen > printed) {
                const char *o = printer_output(&ge);
                printf("PRN> %.*s", olen - printed, o + printed);
                printed = olen;
                fflush(stdout);
            }
            /* Feed any typed bytes to the operator keyboard queue. */
            {
                unsigned char kb[64];
                ssize_t r = read(0, kb, sizeof kb);
                for (ssize_t k = 0; k < r; k++)
                    printer_feed_key(&ge, kb[k]);
            }
            if (g_toggle_js1) {
                g_toggle_js1 = 0; ge.JS1 = !ge.JS1;
                printf("[cyc %ld] SWITCH 1 -> %d   PO=%04x step=0x%02x%s\n",
                       cycles, ge.JS1, ge.rPO, ge.mem[0x0010],
                       ge.halted ? " (halted)" : "");
                fflush(stdout);
            }
            if (g_toggle_js2) {
                g_toggle_js2 = 0; ge.JS2 = !ge.JS2;
                printf("[cyc %ld] SWITCH 2 -> %d   PO=%04x step=0x%02x%s\n",
                       cycles, ge.JS2, ge.rPO, ge.mem[0x0010],
                       ge.halted ? " (halted)" : "");
                fflush(stdout);
            }
            if (ge.halted) {
                if (was_halted != 1) {
                    printf("[cyc %ld] HALT  PO=%04x step=0x%02x\n",
                           cycles, ge.rPO, ge.mem[0x0010]);
                    fflush(stdout);
                    was_halted = 1;
                }
                usleep(5000);   /* frozen; still responsive to signals */
                continue;
            }
            was_halted = 0;
            ret = ge_run_cycle(&ge);
            cycles++;
            if (ret != 0)
                break;
            uint8_t st = ge.mem[0x0010];
            if (st != last_step) {
                printf("[cyc %ld] step -> 0x%02x   PO=%04x\n", cycles, st, ge.rPO);
                fflush(stdout);
                last_step = st;
            }
        }
    } else if (use_tui) {
        /* Interactive session: launch the ncurses client and run the emulator
         * until the user quits the TUI. Ignore max-cycles, and keep cycling
         * even after a HLT so the console socket stays serviced and the panel
         * stays live (a halted GE-120 just spins on HLT;JU self). Throttle when
         * halted so an idle session doesn't peg a core. */
        /* The TUI owns the terminal; silence the (all-on by default) log so it
         * doesn't scribble over the panel — unless the user explicitly asked
         * for a --trace. */
        if (!trace_set)
            ge_log_set_active_types_from_spec("none");
        pid_t tui_pid = spawn_tui(argv[0]);
        if (tui_pid < 0) {
            ge_deinit(&ge);
            return 1;
        }
        while (waitpid(tui_pid, NULL, WNOHANG) == 0) {
            ret = ge_run_cycle(&ge);
            cycles++;
            if (ret != 0)
                break;
            if (ge.halted)
                usleep(2000);
        }
        /* The TUI restores the terminal (curses.endwin) on quit; make sure the
         * child is gone before we print and exit. */
        kill(tui_pid, SIGTERM);
        waitpid(tui_pid, NULL, 0);
    } else {
        while (!ge.halted && cycles < max_cycles) {
            ret = ge_run_cycle(&ge);
            cycles++;
            if (ret != 0)
                break;
        }
    }

    printf("exit: halted=%d cycles=%ld max=%ld error=%d state=%02x PO=%04x\n",
           ge.halted, cycles, max_cycles, ret, ge.rSO, ge.rPO);

    ge_deinit(&ge);
    return ret;
}
