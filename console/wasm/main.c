#include <stdio.h>
#include <emscripten.h>

#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "../../log.h"
#include "../../ge.h"
#include "../../console.h"
#include "../../cardreader.h"
#include "../../bit.h"

struct ge ge130;
struct ge* ge = &ge130;

int running_loop = 0;

EM_JS(void, set_lamp, (const char *lamp, int val), {
    document.set_lamp(UTF8ToString(lamp), val);
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
}


void EMSCRIPTEN_KEEPALIVE press_on()        { running_loop = 1; send_console(); }
void EMSCRIPTEN_KEEPALIVE press_off()       { running_loop = 0; send_console(); }
void EMSCRIPTEN_KEEPALIVE press_power_off() { running_loop = 0; send_console(); }

/* Push the real lamp states again (used to restore the panel after a momentary
 * LAMPS CHECK bulb-test, which forces every lamp on from the JS side). */
void EMSCRIPTEN_KEEPALIVE refresh_lamps()   { send_console(); }
void EMSCRIPTEN_KEEPALIVE press_clear() { ge_clear(ge); send_console(); }
void EMSCRIPTEN_KEEPALIVE press_load()  { ge_load(ge);  send_console(); }
void EMSCRIPTEN_KEEPALIVE press_start() { ge_start(ge); send_console(); }

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

void em_main_loop() {
    if (!running_loop) 
        return;
    
    ge_run_cycle(ge);
    send_console();
}

int main() {
    ge_log_set_active_types(~(LOG_REGS_V | LOG_CONDS));
    ge_init(ge);

    send_console();

    /* run the main loop at 1Hz, just for show */
    emscripten_set_main_loop(em_main_loop, 1, 0);
    return 0;
}

