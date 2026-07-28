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
#include "disk.h"
#include "tape.h"
#include "transcode.h"
#include "log.h"
#include "sat_batches.h"
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
        "Usage: %s [OPTIONS] [deck.cap]\n"
        "\n"
        "  deck.cap             A card deck. It is placed in the reader's hopper and\n"
        "                       pulled in by the machine's own bootstrap: CLEAR, LOAD1,\n"
        "                       LOAD, START. The IPL reads exactly ONE card (80 columns\n"
        "                       nibble-packed to 40 bytes at 0x0000) and executes it;\n"
        "                       that card's code pulls the rest of the deck.\n"
        "\n"
        "  A deck is the only way to get a program into the machine, here as on the\n"
        "  iron. Build one with `gasm -o prog.cap prog.s` or `gec -o prog.cap prog.c`.\n"
        "\n"
        "Options:\n"
        "  --deck <path>        Explicit alias for the positional .cap argument\n"
        "  --sat <id>           Use a built-in Site Acceptance Test batch\n"
        "  --list-sat           List the built-in SAT batches and exit\n"
        "  --trace <spec>       Enable log types from spec string\n"
        "  --max-cycles <N>     Maximum CPU cycles before forced exit (default: 100000,\n"
        "                       or 500000 for --deck unless overridden)\n"
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
    int max_cycles_set = 0;
    long cycles = 0;
    int use_console = 0;
    int use_tui = 0;
    int trace_set = 0;
    const char *deck_path = NULL;   /* --deck: cycle-faithful card-reader bootstrap */
    const char *disk_path = NULL;   /* --disk: DSS pack image on connector 3 unit 0 */
    const char *tape_path = NULL;   /* --tape: MTC reel image on connector 4 unit 0 */
    const char *sat_batch = NULL;   /* --sat: built-in SAT batch */
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
        } else if (strcmp(argv[i], "--disk") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --disk requires an argument\n");
                return 1;
            }
            disk_path = argv[++i];   /* DSS pack image; connector 3, unit 0 */
        } else if (strcmp(argv[i], "--tape") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --tape requires an argument\n");
                return 1;
            }
            tape_path = argv[++i];   /* MTC reel image; connector 4, unit 0 */
        } else if (strcmp(argv[i], "--sat") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --sat requires an argument\n");
                return 1;
            }
            sat_batch = argv[++i];
        } else if (strcmp(argv[i], "--list-sat") == 0) {
            for (int j = 0; j < sat_batch_count(); j++) {
                const struct sat_batch_info *info = sat_batch_info_at(j);
                printf("%-20s  %s\n", info->id, info->title);
                printf("  %s\n", info->summary);
            }
            return 0;
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
            max_cycles_set = 1;
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
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else if (!deck_path && !sat_batch) {
            /* Positional input: a card deck, and nothing else. There is no
             * direct-to-memory load — the machine has no such door. */
            const char *p = argv[i];
            size_t n = strlen(p);
            if (n < 4 || strcmp(p + n - 4, ".cap") != 0) {
                fprintf(stderr,
                        "error: '%s' is not a .cap card deck.\n"
                        "       A program reaches the machine only on cards; build a deck with\n"
                        "       `gasm -o prog.cap prog.s` or `gec -o prog.cap prog.c`.\n", p);
                return 1;
            }
            deck_path = p;
        } else {
            fprintf(stderr, "error: unexpected argument '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (deck_path && sat_batch) {
        fprintf(stderr, "error: give only one of a .cap deck, --deck, or --sat\n");
        return 1;
    }

    /* Reading a deck through the reader costs real machine cycles. Give a deck
     * run a roomier default so the load does not time out unless the user
     * explicitly requested a tighter budget. */
    if ((deck_path || sat_batch) && !max_cycles_set)
        max_cycles = 500000;

    ge_init(&ge);

    if (use_console) {
        ret = console_socket_register(&ge);
        if (ret != 0) {
            ge_deinit(&ge);
            return ret;
        }
    }

    ge_clear(&ge);

    /* The only load path there is: put the deck in the hopper, select the load
     * unit, arm the bootstrap, then START. LOAD itself does nothing but set
     * AINI — the read happens when the machine is released. */
    if (sat_batch) {
        char note[256];
        static const char sat_cap_path[] = "/tmp/gemu_sat_batch.cap";
        const struct sat_batch_info *info = sat_batch_find(sat_batch);
        if (!info) {
            fprintf(stderr, "error: unknown SAT batch '%s' (use --list-sat)\n", sat_batch);
            ge_deinit(&ge);
            return 1;
        }

        if (sat_batch_prepare_deck("Site_Acceptance_Test", sat_batch,
                                   sat_cap_path, note, sizeof(note)) != 0) {
            fprintf(stderr, "error: failed to compose SAT batch '%s'\n", sat_batch);
            ge_deinit(&ge);
            return 1;
        }
        ge_load_1(&ge);
        ge_load(&ge);
        ret = cardreader_register(&ge, sat_cap_path, TC_NORMAL);
        if (ret != 0) {
            fprintf(stderr, "error: failed to load SAT batch '%s'\n", sat_batch);
            ge_deinit(&ge);
            return ret;
        }
        fprintf(stderr, "SAT batch %s: %s\n", sat_batch, note);
    } else if (deck_path) {
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

    /* Console switch initial state (after ge_start, which clears them). */
    ge.JS1 = sw1_init;
    ge.JS2 = sw2_init;

    /* Attach a DSS disk pack on connector 3 (standard GE-100), if requested. */
    if (disk_path) {
        if (disk_register(&ge, disk_path, 3, 0) != 0)
            fprintf(stderr, "warning: failed to attach disk '%s'\n", disk_path);
        else
            fprintf(stderr, "disk: attached '%s' on connector 3 unit 0\n", disk_path);
    }

    /* Attach an MTC tape reel on connector 4 (standard GE-100), if requested. */
    if (tape_path) {
        if (tape_register(&ge, tape_path, 4, 0) != 0)
            fprintf(stderr, "warning: failed to attach tape '%s'\n", tape_path);
        else
            fprintf(stderr, "tape: attached '%s' on connector 4 unit 0\n", tape_path);
    }

    int printer_enabled = 0;
    int printed = 0;
    int kbd_fl = -1;
    /* The integrated printer/typewriter on channel 2 is part of the machine,
     * not an option: attach it for every non-TUI run, so a deck that prints is
     * not left parked on an unanswered PER.
     *
     * It used to swallow the card load instead. State b8 is shared between the
     * channel-1 reader input-wait and the channel-2 print-wait, and printer.c
     * answered both, so the IPL fell through to alpha at address 0 having read
     * nothing. It now checks PC121 -- the machine's own decode of "connector 2
     * on channel 1", the card reader -- and keeps out of an order that is not
     * its own. See printer.c. */
    if (!use_tui) {
        printer_register(&ge);
        printer_enabled = 1;
        kbd_fl = fcntl(0, F_GETFL, 0);
        if (kbd_fl != -1)
            fcntl(0, F_SETFL, kbd_fl | O_NONBLOCK);
    }

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
        if (!printer_enabled) {
            printer_register(&ge);
            printer_enabled = 1;
        }
        kbd_fl = fcntl(0, F_GETFL, 0);
        if (kbd_fl != -1)
            fcntl(0, F_SETFL, kbd_fl | O_NONBLOCK);
        printed = 0;   /* bytes of printer output already echoed to stdout */
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
                       ge_halted(&ge) ? " (halted)" : "");
                fflush(stdout);
            }
            if (g_toggle_js2) {
                g_toggle_js2 = 0; ge.JS2 = !ge.JS2;
                printf("[cyc %ld] SWITCH 2 -> %d   PO=%04x step=0x%02x%s\n",
                       cycles, ge.JS2, ge.rPO, ge.mem[0x0010],
                       ge_halted(&ge) ? " (halted)" : "");
                fflush(stdout);
            }
            if (ge_halted(&ge)) {
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
            if (ge_halted(&ge))
                usleep(2000);
        }
        /* The TUI restores the terminal (curses.endwin) on quit; make sure the
         * child is gone before we print and exit. */
        kill(tui_pid, SIGTERM);
        waitpid(tui_pid, NULL, 0);
    } else {
        while (!ge_halted(&ge) && cycles < max_cycles) {
            if (printer_enabled) {
                int olen = printer_output_len(&ge);
                if (olen > printed) {
                    const char *o = printer_output(&ge);
                    fwrite(o + printed, 1, (size_t)(olen - printed), stdout);
                    fflush(stdout);
                    printed = olen;
                }
                unsigned char kb[64];
                ssize_t r = read(0, kb, sizeof kb);
                for (ssize_t k = 0; k < r; k++)
                    printer_feed_key(&ge, kb[k]);
            }
            ret = ge_run_cycle(&ge);
            cycles++;
            if (ret != 0)
                break;
        }
    }

    printf("exit: halted=%d cycles=%ld max=%ld error=%d state=%02x PO=%04x\n",
           ge_halted(&ge), cycles, max_cycles, ret, ge.rSO, ge.rPO);

    ge_deinit(&ge);
    return ret;
}
