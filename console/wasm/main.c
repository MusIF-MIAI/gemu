#include <stdio.h>
#include <emscripten.h>

#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "../../log.h"
#include "../../ge.h"
#include "../../console.h"
#include "../../cardreader.h"
#include "../../disasm.h"
#include "../../binimage.h"
#include "../../bit.h"

struct ge ge130;
struct ge* ge = &ge130;

int running_loop = 0;

/* Run-speed pacing. The GE-120 elementary (memory) cycle is a nominal 4 us
 * (CPU[4]: "memory cycle of nominal 2/4/6 us for 130/120/115/3"), and one
 * ge_run_cycle is one such cycle — so nominal real time is 250 cycles/ms.
 * run_speed scales that: 1.0 = nominal wall-clock real time. */
#define GE120_CYCLE_US 4.0
static double run_speed    = 1.0;
static double last_now_ms  = 0.0;
static double cycle_budget = 0.0;

EM_JS(void, set_lamp, (const char *lamp, int val), {
    document.set_lamp(UTF8ToString(lamp), val);
});

EM_JS(void, set_disasm, (const char *text), {
    document.set_disasm(UTF8ToString(text));
});

void send_console() {
    struct ge_console console = { 0 };
    int r = running_loop;

    ge_fill_console_data(ge, &console);

    set_lamp("RO_0", BIT(console.lamps.RO, 0));
    set_lamp("RO_1", BIT(console.lamps.RO, 1));
    set_lamp("RO_2", BIT(console.lamps.RO, 2));
    set_lamp("RO_3", BIT(console.lamps.RO, 3));
    set_lamp("RO_4", BIT(console.lamps.RO, 4));
    set_lamp("RO_5", BIT(console.lamps.RO, 5));
    set_lamp("RO_6", BIT(console.lamps.RO, 6));
    set_lamp("RO_7", BIT(console.lamps.RO, 7));
    set_lamp("RO_8", BIT(console.lamps.RO, 8));

    set_lamp("SO_0", BIT(console.lamps.SO, 0));
    set_lamp("SO_1", BIT(console.lamps.SO, 1));
    set_lamp("SO_2", BIT(console.lamps.SO, 2));
    set_lamp("SO_3", BIT(console.lamps.SO, 3));
    set_lamp("SO_4", BIT(console.lamps.SO, 4));
    set_lamp("SO_5", BIT(console.lamps.SO, 5));
    set_lamp("SO_6", BIT(console.lamps.SO, 6));
    set_lamp("SO_7", BIT(console.lamps.SO, 7));

    set_lamp("FA_0", BIT(console.lamps.FA, 0));
    set_lamp("FA_1", BIT(console.lamps.FA, 1));
    set_lamp("FA_2", BIT(console.lamps.FA, 2));
    set_lamp("FA_3", BIT(console.lamps.FA, 3));

    set_lamp("SA_0", BIT(console.lamps.SA, 0));
    set_lamp("SA_1", BIT(console.lamps.SA, 1));
    set_lamp("SA_2", BIT(console.lamps.SA, 2));
    set_lamp("SA_3", BIT(console.lamps.SA, 3));
    set_lamp("SA_4", BIT(console.lamps.SA, 4));
    set_lamp("SA_5", BIT(console.lamps.SA, 5));
    set_lamp("SA_6", BIT(console.lamps.SA, 6));
    set_lamp("SA_7", BIT(console.lamps.SA, 7));

    set_lamp("B_0", BIT(console.lamps.B, 0));
    set_lamp("B_1", BIT(console.lamps.B, 1));
    set_lamp("B_2", BIT(console.lamps.B, 2));
    set_lamp("B_3", BIT(console.lamps.B, 3));

    set_lamp("ADD_0", BIT(console.lamps.ADD_reg,  0));
    set_lamp("ADD_1", BIT(console.lamps.ADD_reg,  1));
    set_lamp("ADD_2", BIT(console.lamps.ADD_reg,  2));
    set_lamp("ADD_3", BIT(console.lamps.ADD_reg,  3));
    set_lamp("ADD_4", BIT(console.lamps.ADD_reg,  4));
    set_lamp("ADD_5", BIT(console.lamps.ADD_reg,  5));
    set_lamp("ADD_6", BIT(console.lamps.ADD_reg,  6));
    set_lamp("ADD_7", BIT(console.lamps.ADD_reg,  7));
    set_lamp("ADD_8", BIT(console.lamps.ADD_reg,  8));
    set_lamp("ADD_9", BIT(console.lamps.ADD_reg,  9));
    set_lamp("ADD_A", BIT(console.lamps.ADD_reg, 10));
    set_lamp("ADD_B", BIT(console.lamps.ADD_reg, 11));
    set_lamp("ADD_C", BIT(console.lamps.ADD_reg, 12));
    set_lamp("ADD_D", BIT(console.lamps.ADD_reg, 13));
    set_lamp("ADD_E", BIT(console.lamps.ADD_reg, 14));
    set_lamp("ADD_F", BIT(console.lamps.ADD_reg, 15));

    set_lamp("OP_0", BIT(console.lamps.OP_reg, 0));
    set_lamp("OP_1", BIT(console.lamps.OP_reg, 1));
    set_lamp("OP_2", BIT(console.lamps.OP_reg, 2));
    set_lamp("OP_3", BIT(console.lamps.OP_reg, 3));
    set_lamp("OP_4", BIT(console.lamps.OP_reg, 4));
    set_lamp("OP_5", BIT(console.lamps.OP_reg, 5));
    set_lamp("OP_6", BIT(console.lamps.OP_reg, 6));
    set_lamp("OP_7", BIT(console.lamps.OP_reg, 7));

    set_lamp("UR",  console.lamps.UR);
    set_lamp("C3",  console.lamps.C3);
    set_lamp("C2",  console.lamps.C2);
    set_lamp("C1",  console.lamps.C1);
    set_lamp("I",   console.lamps.I );
    set_lamp("JE",  console.lamps.JE);
    set_lamp("IM",  console.lamps.IM);
    set_lamp("NZ",  console.lamps.NZ);
    set_lamp("OF",  console.lamps.OF);

    set_lamp("DC_ALERT",       console.lamps.DC_ALERT      );
    set_lamp("POWER_OFF",      !r                          );
    set_lamp("STAND_BY",       !r                          );
    set_lamp("POWER_ON",       r                           );
    set_lamp("MAINTENANCE_ON", console.lamps.MAINTENANCE_ON);
    set_lamp("MEM_CHECK",      console.lamps.MEM_CHECK     );
    set_lamp("INV_ADD",        console.lamps.INV_ADD       );
    set_lamp("SWITCH_1",       console.lamps.SWITCH_1      );
    set_lamp("SWITCH_2",       console.lamps.SWITCH_2      );
    set_lamp("STEP_BY_STEP",   console.lamps.STEP_BY_STEP  );
    set_lamp("HALT",           console.lamps.HALT          );
    set_lamp("LOAD_1",         console.lamps.LOAD_1        );
    set_lamp("LOAD_2",         console.lamps.LOAD_2        );
    set_lamp("OPERATOR_CALL",  console.lamps.OPERATOR_CALL );

    /* gdb-style disassembly window centred on the program counter (PO). */
    {
        static char dis[1536];
        ge_disasm_window(ge->mem, ge->rPO, 5, 6, dis, sizeof dis);
        set_disasm(dis);
    }
}


void EMSCRIPTEN_KEEPALIVE press_on()        { running_loop = 1; send_console(); }
void EMSCRIPTEN_KEEPALIVE press_off()       { running_loop = 0; send_console(); }
void EMSCRIPTEN_KEEPALIVE press_power_off() { running_loop = 0; send_console(); }

/* Push the real lamp states again (used to restore the panel after a momentary
 * LAMPS CHECK bulb-test, which forces every lamp on from the JS side). */
void EMSCRIPTEN_KEEPALIVE refresh_lamps()   { send_console(); }
void EMSCRIPTEN_KEEPALIVE press_clear() { ge_clear(ge); send_console(); }
void EMSCRIPTEN_KEEPALIVE press_load()  { ge_load(ge);  send_console(); }

/* Simplified loader (temporary): a unified-format image staged by the page is
 * dropped straight into memory on the CLEAR-LOAD-START sequence, bypassing the
 * (not-yet-faithful) card-reader bootstrap. We detect the sequence by AINI,
 * which LOAD sets: a START with AINI set + an image staged => magic-load it and
 * enter at its entry point. A bare START (resume, AINI=0) just runs. */
static uint8_t  staged_img[MEM_SIZE];
static uint16_t staged_origin, staged_entry, staged_len;
static int      staged = 0;

int EMSCRIPTEN_KEEPALIVE stage_image(void) {
    FILE *f = fopen("/image.bin", "rb");
    if (!f)
        return -1;
    int rc = binimage_read(f, &staged_origin, &staged_entry,
                           staged_img, sizeof staged_img, &staged_len);
    fclose(f);
    staged = (rc == BINIMAGE_OK);
    return rc;
}

void EMSCRIPTEN_KEEPALIVE press_start() {
    if (staged && ge->AINI) {              /* CLEAR-LOAD-START: magic-load */
        ge_load_image(ge, staged_img, staged_len, staged_origin);
        ge_seed_segment_bases(ge);
        ge_enter(ge, staged_entry);
        ge->AINI = 0;                      /* consume the load request */
        running_loop = 1;                  /* power on + run */
    }
    ge_start(ge);
    send_console();
}

/*
 * mount_deck - simulator-only "insert cards in the reader hopper" action.
 *
 * There is no file dialog on a real GE-120: a program enters through a deck of
 * cards physically loaded into the reader on one of the load connectors. This
 * reproduces exactly that — the JS side writes the chosen .cap deck into the
 * emscripten in-memory filesystem at /deck.cap, then we attach it to the card
 * reader on connector 2 and select LOAD1, just like the --deck CLI path.
 *
 * The operator then runs the authentic bootstrap on the real console buttons:
 *   CLEAR -> LOAD -> START
 * which drives the documented 80 -> c8 load sequence (CPU[4] §5.3, fo.43).
 *
 * @binary:     0 = Hollerith transcoding (TC_NORMAL), 1 = raw passthrough.
 * @first_card: index of the first card to feed (skip title/loader cards;
 *              0 for a plain deck).
 * Returns 0 on success, -1 if the deck cannot be opened/parsed.
 */
int EMSCRIPTEN_KEEPALIVE mount_deck(int binary, int first_card) {
    int rc;

    ge_load_1(ge);   /* select connector 2 (LOAD1), matching the reader */
    rc = cardreader_register_from(ge, "/deck.cap",
                                  binary ? TC_BINARY : TC_NORMAL, first_card);
    send_console();
    return rc;
}

void EMSCRIPTEN_KEEPALIVE set_switches(int flags, int am) {
    struct ge_console_switches switches;
    switches.SITE = BIT(flags, 0);
    switches.INCE = BIT(flags, 1);
    switches.INAR = BIT(flags, 2);
    switches.STOC = BIT(flags, 3);
    switches.ACON = BIT(flags, 4);
    switches.ACOV = BIT(flags, 5);
    switches.RICI = BIT(flags, 6);
    switches.PATE = BIT(flags, 7);
    switches.PAPA = BIT(flags, 8);
    switches.AM = am;

    ge_set_console_switches(ge, &switches);
    send_console();
}

void EMSCRIPTEN_KEEPALIVE set_register_selector(int s) {
    ge_set_console_rotary(ge, s);
    send_console();
}

/* SWITCH 1 / SWITCH 2 are operator-panel toggle switches the program reads via
 * the JS1 / JS2 instructions; their lamps follow these inputs. */
void EMSCRIPTEN_KEEPALIVE set_switch_1_2(int s1, int s2) {
    ge->JS1 = !!s1;
    ge->JS2 = !!s2;
    send_console();
}

/* LOAD1/LOAD2 selector: choose which install-time load connector the bootstrap
 * reads from (load1 != 0 -> LOAD1/connector 2, else LOAD2). */
void EMSCRIPTEN_KEEPALIVE set_load_unit(int load1) {
    if (load1)
        ge_load_1(ge);
    else
        ge_load_2(ge);
    send_console();
}

/* Run speed multiplier: 1.0 = nominal real time, <1 slow-mo, >1 fast-forward. */
void EMSCRIPTEN_KEEPALIVE set_speed(double mult) {
    run_speed = mult < 0.0 ? 0.0 : mult;
}

void em_main_loop() {
    double now = emscripten_get_now();        /* high-res wall clock, ms */
    double elapsed = now - last_now_ms;
    last_now_ms = now;

    /* Idle (powered off) or halted: don't run cycles and don't build a backlog
     * of "owed" time, so resuming starts fresh instead of fast-forwarding. */
    if (!running_loop || ge->halted) {
        cycle_budget = 0.0;
        return;
    }

    /* Cap catch-up so a backgrounded tab doesn't burst a huge batch on return. */
    if (elapsed > 100.0)
        elapsed = 100.0;

    /* Nominal real time: 1000/4 = 250 cycles per ms, scaled by run_speed. The
     * fractional remainder is carried in cycle_budget so timing doesn't drift. */
    cycle_budget += elapsed * (1000.0 / GE120_CYCLE_US) * run_speed;

    long n = (long)cycle_budget;
    cycle_budget -= (double)n;

    for (long i = 0; i < n; i++) {
        if (ge_run_cycle(ge) != 0)            /* timing-chart error: stop */
            break;
        if (ge->halted)                       /* HLT: nothing more until reset */
            break;
    }

    send_console();                           /* refresh the panel once per frame */
}

int main() {
    /* No log pane in the browser panel, and at real-time speed the per-cycle
     * log stream would flood stdout and stall the page — suppress all logging
     * (ge_log early-returns on a type miss, so this is also the fast path). */
    ge_log_set_active_types(0);
    ge_init(ge);

    send_console();

    /* requestAnimationFrame-driven loop (fps arg 0); em_main_loop paces the
     * cycle count to nominal GE-120 wall-clock time itself. */
    last_now_ms = emscripten_get_now();
    emscripten_set_main_loop(em_main_loop, 0, 0);
    return 0;
}

