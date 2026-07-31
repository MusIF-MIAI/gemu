#include <stdio.h>
#include <string.h>
#include <emscripten.h>

#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "../../log.h"
#include "../../ge.h"
#include "../../console.h"
#include "../../cardreader.h"
#include "../../printer.h"
#include "../../disasm.h"
#include "../../cap.h"
#include "../../peripherical.h"
#include "../../signals.h"
#include "../../transcode.h"
#include "../../bit.h"

struct ge ge130;
struct ge* ge = &ge130;

int running_loop = 0;

/* Run-speed pacing. One ge_run_cycle is one elementary (memory) cycle, and how
 * long that takes is the machine's own business: the E03 strap picks it, 2/4/6
 * us for the UCE 468/467/466 (cp06 ch.002 TAB.1, ge_cycle_period_ns). This
 * machine is a 468, so nominal real time is 500 cycles/ms -- the panel used to
 * assume the 4 us of a 467 and ran the whole machine at half speed, which is
 * exactly the factor of two Dan measured against the iron. run_speed scales
 * it: 1.0 = nominal wall-clock real time. */
#define CYCLES_PER_MS(ge) (1000000.0 / (double)ge_cycle_period_ns(ge))
static double run_speed    = 1.0;
static double last_now_ms  = 0.0;
static double cycle_budget = 0.0;

EM_JS(void, set_lamp, (const char *lamp, int val), {
    document.set_lamp(UTF8ToString(lamp), val);
});

/* Live Assembly: the full-space listing (rebuilt occasionally — it is large)
 * and the per-frame current-instruction marker (cheap; JS just moves the
 * highlight and, while "following", scrolls it into view). */
EM_JS(void, set_disasm_full, (const char *text), {
    if (document.set_disasm_full) document.set_disasm_full(UTF8ToString(text));
});
EM_JS(void, disasm_set_pc, (int pc), {
    if (document.disasm_set_pc) document.disasm_set_pc(pc);
});

/* Disassemble the populated address space (0x0000 .. highest non-zero byte +
 * a margin) into a static buffer for the scrollable Live Assembly panel.
 * Called occasionally from send_console(), not every frame. */
static const char *disasm_all(void) {
    static char buf[2621440];   /* 2.5 MiB; worst case ~64K one-byte lines */
    unsigned hi = 0;
    for (unsigned a = 0; a <= 0xFFFF; a++)
        if (ge->mem[a]) hi = a;
    unsigned end = (hi + 0x100u > 0xFFFFu) ? 0xFFFFu : hi + 0x100u;
    size_t used = 0;
    buf[0] = '\0';
    for (unsigned a = 0; a <= end && used < sizeof buf - 96; ) {
        char text[64];
        int l = ge_disasm_one(ge->mem, (uint16_t)a, text, sizeof text);
        if (l <= 0) l = 1;
        char hex[24];
        int hp = 0;
        for (int k = 0; k < l && hp < 20; k++)
            hp += snprintf(hex + hp, sizeof hex - (size_t)hp, "%02X ",
                           ge->mem[(uint16_t)(a + k)]);
        used += (size_t)snprintf(buf + used, sizeof buf - used,
                                 "%04X: %-14s %s\n", a, hex, text);
        a += (unsigned)l;
    }
    return buf;
}

/* Integrated printer/typewriter (channel 2) -> the chat transcript. */
EM_JS(void, printer_emit, (const char *text), {
    if (document.gemu_printer_emit) document.gemu_printer_emit(UTF8ToString(text));
});

/* Bytes the machine has already printed and echoed to the chat panel. */
static int printer_echoed = 0;

/* Push any newly-captured printer output to the JS chat transcript, then
 * RECLAIM the capture buffer. The JS side keeps its own scrolling transcript,
 * so the emulator-side out[] is only a per-frame staging buffer: if we merely
 * advanced a cursor (the old behaviour) out[] would accumulate across frames
 * and cap at sizeof(out)-1 within ~50 ms of real-time printing, after which
 * printer_capture_char drops everything and the panel freezes mid-test (the
 * line-printer mechanical test alone prints millions of characters). Emitting
 * the new tail and clearing each frame keeps the stream flowing without bound. */
static void drain_printer(void) {
    int olen = printer_output_len(ge);
    if (olen > printer_echoed)
        printer_emit(printer_output(ge) + printer_echoed);
    if (olen > 0) {
        printer_output_clear(ge);
        printer_echoed = 0;
    }
}

/* Feed one operator-keyboard byte (two-way chat input). Exposed to JS. */
void EMSCRIPTEN_KEEPALIVE printer_key(int c) {
    printer_feed_key(ge, (uint8_t)c);
}

/* Only the lamps that CHANGED cross into JS.
 *
 * The panel has about 150 of them and send_console runs every frame; pushing
 * all of them meant 150 EM_JS calls, 150 string decodes and 150 DOM lookups
 * per frame whatever the machine was doing, which is the sort of overhead that
 * decides whether the page keeps up with a 2 us machine. Steady state is now
 * nearly silent: a lamp is written when it moves. The order of the LAMP() calls
 * below is the index, so it must not be reordered without invalidating the
 * cache, which `lamps_dirty` does anyway whenever the page asks for a repaint. */
static uint8_t lamp_prev[192];
static int lamps_dirty = 1;

void EMSCRIPTEN_KEEPALIVE refresh_lamps(void);

void send_console() {
    struct ge_console console = { 0 };
    int powered = ge->powered != 0;
    int running = powered && running_loop;
    int lamp_i = 0;

    ge_fill_console_data(ge, &console);

#define LAMP(name, val) do {                                       \
        uint8_t v_ = (val) ? 1 : 0;                                \
        if (lamps_dirty || lamp_prev[lamp_i] != v_) {              \
            set_lamp(name, v_);                                    \
            lamp_prev[lamp_i] = v_;                                \
        }                                                          \
        lamp_i++;                                                  \
    } while (0)

    LAMP("RO_0", BIT(console.lamps.RO, 0));
    LAMP("RO_1", BIT(console.lamps.RO, 1));
    LAMP("RO_2", BIT(console.lamps.RO, 2));
    LAMP("RO_3", BIT(console.lamps.RO, 3));
    LAMP("RO_4", BIT(console.lamps.RO, 4));
    LAMP("RO_5", BIT(console.lamps.RO, 5));
    LAMP("RO_6", BIT(console.lamps.RO, 6));
    LAMP("RO_7", BIT(console.lamps.RO, 7));
    LAMP("RO_8", BIT(console.lamps.RO, 8));

    LAMP("SO_0", BIT(console.lamps.SO, 0));
    LAMP("SO_1", BIT(console.lamps.SO, 1));
    LAMP("SO_2", BIT(console.lamps.SO, 2));
    LAMP("SO_3", BIT(console.lamps.SO, 3));
    LAMP("SO_4", BIT(console.lamps.SO, 4));
    LAMP("SO_5", BIT(console.lamps.SO, 5));
    LAMP("SO_6", BIT(console.lamps.SO, 6));
    LAMP("SO_7", BIT(console.lamps.SO, 7));

    LAMP("FA_0", BIT(console.lamps.FA, 0));
    LAMP("FA_1", BIT(console.lamps.FA, 1));
    LAMP("FA_2", BIT(console.lamps.FA, 2));
    LAMP("FA_3", BIT(console.lamps.FA, 3));

    LAMP("SA_0", BIT(console.lamps.SA, 0));
    LAMP("SA_1", BIT(console.lamps.SA, 1));
    LAMP("SA_2", BIT(console.lamps.SA, 2));
    LAMP("SA_3", BIT(console.lamps.SA, 3));
    LAMP("SA_4", BIT(console.lamps.SA, 4));
    LAMP("SA_5", BIT(console.lamps.SA, 5));
    LAMP("SA_6", BIT(console.lamps.SA, 6));
    LAMP("SA_7", BIT(console.lamps.SA, 7));

    LAMP("B_0", BIT(console.lamps.B, 0));
    LAMP("B_1", BIT(console.lamps.B, 1));
    LAMP("B_2", BIT(console.lamps.B, 2));
    LAMP("B_3", BIT(console.lamps.B, 3));

    LAMP("ADD_0", BIT(console.lamps.ADD_reg,  0));
    LAMP("ADD_1", BIT(console.lamps.ADD_reg,  1));
    LAMP("ADD_2", BIT(console.lamps.ADD_reg,  2));
    LAMP("ADD_3", BIT(console.lamps.ADD_reg,  3));
    LAMP("ADD_4", BIT(console.lamps.ADD_reg,  4));
    LAMP("ADD_5", BIT(console.lamps.ADD_reg,  5));
    LAMP("ADD_6", BIT(console.lamps.ADD_reg,  6));
    LAMP("ADD_7", BIT(console.lamps.ADD_reg,  7));
    LAMP("ADD_8", BIT(console.lamps.ADD_reg,  8));
    LAMP("ADD_9", BIT(console.lamps.ADD_reg,  9));
    LAMP("ADD_A", BIT(console.lamps.ADD_reg, 10));
    LAMP("ADD_B", BIT(console.lamps.ADD_reg, 11));
    LAMP("ADD_C", BIT(console.lamps.ADD_reg, 12));
    LAMP("ADD_D", BIT(console.lamps.ADD_reg, 13));
    LAMP("ADD_E", BIT(console.lamps.ADD_reg, 14));
    LAMP("ADD_F", BIT(console.lamps.ADD_reg, 15));

    LAMP("OP_0", BIT(console.lamps.OP_reg, 0));
    LAMP("OP_1", BIT(console.lamps.OP_reg, 1));
    LAMP("OP_2", BIT(console.lamps.OP_reg, 2));
    LAMP("OP_3", BIT(console.lamps.OP_reg, 3));
    LAMP("OP_4", BIT(console.lamps.OP_reg, 4));
    LAMP("OP_5", BIT(console.lamps.OP_reg, 5));
    LAMP("OP_6", BIT(console.lamps.OP_reg, 6));
    LAMP("OP_7", BIT(console.lamps.OP_reg, 7));

    LAMP("UR",  console.lamps.UR);
    LAMP("C3",  console.lamps.C3);
    LAMP("C2",  console.lamps.C2);
    LAMP("C1",  console.lamps.C1);
    LAMP("I",   console.lamps.I );
    LAMP("JE",  console.lamps.JE);
    LAMP("IM",  console.lamps.IM);
    LAMP("NZ",  console.lamps.NZ);
    LAMP("OF",  console.lamps.OF);

    LAMP("DC_ALERT",       console.lamps.DC_ALERT      );
    LAMP("POWER_OFF",      !powered                    );
    LAMP("STAND_BY",       powered && !running         );
    LAMP("POWER_ON",       powered                     );
    LAMP("MAINTENANCE_ON", console.lamps.MAINTENANCE_ON);
    LAMP("MEM_CHECK",      console.lamps.MEM_CHECK     );
    LAMP("INV_ADD",        console.lamps.INV_ADD       );
    LAMP("SWITCH_1",       console.lamps.SWITCH_1      );
    LAMP("SWITCH_2",       console.lamps.SWITCH_2      );
    LAMP("STEP_BY_STEP",   console.lamps.STEP_BY_STEP  );
    LAMP("HALT",           console.lamps.HALT          );
    LAMP("LOAD_1",         console.lamps.LOAD_1        );
    LAMP("LOAD_2",         console.lamps.LOAD_2        );
    LAMP("OPERATOR_CALL",  console.lamps.OPERATOR_CALL );

    /* gdb-style disassembly window centred on the instruction-start PC
     * (latched in the alpha fetch), so the highlight stays on the instruction
     * being executed instead of drifting onto operand bytes / the next line as
     * the live PO advances mid-instruction (e.g. while computing a jump). */
    /* Mark the current instruction every frame (cheap: JS moves the highlight
     * and, while following, scrolls it into view). Rebuild the full-space
     * listing only occasionally (~2 Hz) since it covers the whole program and
     * is large; the JS skips the DOM rebuild when the text is unchanged. */
    lamps_dirty = 0;
#undef LAMP

    disasm_set_pc(ge->instr_pc);
    {
        static unsigned dcount = 0;
        if ((dcount++ % 30u) == 0u)
            set_disasm_full(disasm_all());
    }
}


static void wasm_set_power(int on)
{
    ge->powered = !!on;
    if (!ge->powered)
        running_loop = 0;
    cycle_budget = 0.0;
    last_now_ms = emscripten_get_now();
    send_console();
}

void EMSCRIPTEN_KEEPALIVE press_power_on()  { wasm_set_power(1); }
void EMSCRIPTEN_KEEPALIVE press_on()        { press_power_on(); }
void EMSCRIPTEN_KEEPALIVE press_off()       { wasm_set_power(0); }
void EMSCRIPTEN_KEEPALIVE press_power_off() { wasm_set_power(0); }

/* LAMPS CHECK, the momentary bulb test sharing the MAINT ON button: held = 1
 * lights every lamp, held = 0 restores the real states. The model owns it
 * (ge->lamps_test, see console.c) so the browser panel and the ncurses panel
 * test the same lamps the same way. */
void EMSCRIPTEN_KEEPALIVE set_lamps_check(int held) {
    ge->lamps_test = held ? 1 : 0;
    lamps_dirty = 1;            /* every lamp changes, both ways */
    send_console();
}

/* Push the real lamp states again -- every one of them, whether it moved or
 * not: the caller is asking because something outside the model touched the
 * glass (the LAMPS CHECK bulb test paints the whole panel from JS). */
void EMSCRIPTEN_KEEPALIVE refresh_lamps()   { lamps_dirty = 1; send_console(); }
/* The page's only way in is the reader hopper: it writes the chosen deck to
 * /deck.cap and calls mount_deck(). There is no path from a file straight into
 * memory, because the machine has no such door. */

static void reset_sim_bindings(void)
{
    ge_peri_deinit(ge);
    ge->peri = NULL;
    printer_register(ge);
}

void EMSCRIPTEN_KEEPALIVE press_clear() { ge_clear(ge); send_console(); }

/* LOAD.
 *
 * The key does one thing and does it immediately: it sets AINI. Nothing is read
 * yet. The next START releases the machine, and the 80 -> c8 IPL sequence pulls
 * ONE card from the reader, nibble-packs its 80 columns into 40 bytes at
 * address 0, and executes them. Whatever that card's code does next -- and on
 * every real deck it reads the rest of the deck -- is the program's business,
 * not the machine's. */
void EMSCRIPTEN_KEEPALIVE press_load()  {
    if (!ge->powered) {
        send_console();
        return;
    }
    ge_load(ge);
    send_console();
}

/* STEP BY STEP: the operator panel's own switch (ASIN). A toggle, and a
 * different circuit from the maintenance PAPA -- it stops the machine at each
 * instruction and the program can inhibit it with INS. See ge.h ASIN. */
void EMSCRIPTEN_KEEPALIVE press_step_by_step() {
    ge->ASIN = !ge->ASIN;
    send_console();
}

void EMSCRIPTEN_KEEPALIVE press_start() {
    if (!ge->powered) {
        send_console();
        return;
    }

    /* At NORM, START runs the machine: the deck is already resident and entered
     * by LOAD, so this just releases the CPU, and clearing ALTO lets a START
     * after a HLT resume. Off NORM it is one maintenance cycle and the machine
     * stays stopped -- ge_console_start does it and says which happened, so the
     * animation loop is only released for a real run (see console.c: releasing
     * it for a storage key-in filled core with the keyed byte). */
    if (ge_console_start(ge))
        running_loop = 1;
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

    reset_sim_bindings();
    ge_load_1(ge);   /* select connector 2 (LOAD1), matching the reader */
    rc = cardreader_register_from(ge, "/deck.cap",
                                  binary ? TC_BINARY : TC_NORMAL, first_card);
    send_console();
    return rc;
}

/* What the operator can see by looking at the hopper: the size of the deck
 * that was put in it, and how many cards are still waiting at the throat. -1
 * means there is no reader on the connector. */
int EMSCRIPTEN_KEEPALIVE deck_cards()      { return cardreader_deck_cards(ge); }
int EMSCRIPTEN_KEEPALIVE deck_cards_left() { return cardreader_cards_left(ge); }

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

    /* Powered off: don't run cycles and don't build a backlog of "owed" time,
     * so resuming starts fresh instead of fast-forwarding.
     *
     * POWERED is the only condition. We do NOT stop on ge_halted(ge), and we do
     * not wait for a program to be running: a powered GE-120's delay line turns
     * whatever the CPU is doing, and the Logic Sequence Matrix it clocks goes on
     * running the DISPLAY sequence -- which is what puts the rotary-selected
     * register on the lamps and what makes the console work on a machine with
     * nothing in the reader at all. Gating this on `running_loop` left the panel
     * frozen until a deck was started: register forcing "worked" in the model
     * and nothing moved on the glass, because the emulator was not turning. */
    if (!ge->powered) {
        cycle_budget = 0.0;
        return;
    }

    /* Cap catch-up so a backgrounded tab doesn't burst a huge batch on return. */
    if (elapsed > 100.0)
        elapsed = 100.0;

    /* Nominal real time for the strapped cycle period (500 cycles/ms on this
     * 2 us machine), scaled by run_speed. The fractional remainder is carried
     * in cycle_budget so the timing does not drift. */
    cycle_budget += elapsed * CYCLES_PER_MS(ge) * run_speed;

    long n = (long)cycle_budget;
    cycle_budget -= (double)n;

    for (long i = 0; i < n; i++) {
        if (ge_run_cycle(ge) != 0)            /* timing-chart error: stop */
            break;
        /* keep cycling when halted: ALTO freezes the CPU at the HLT (PO stays
         * put, HALT lamp lit), but the delay line still turns so the panel and
         * console forcing/display stay live. */
    }

    drain_printer();                          /* push any printed output to the chat */
    send_console();                           /* refresh the panel once per frame */
}

int main() {
    /* No log pane in the browser panel, and at real-time speed the per-cycle
     * log stream would flood stdout and stall the page — suppress all logging
     * (ge_log early-returns on a type miss, so this is also the fast path). */
    ge_log_set_active_types(0);
    ge_init(ge);

    /* Integrated printer/typewriter on channel 2: completes print PERs so the
     * machine does not hang on output, and captures characters into the chat. */
    printer_register(ge);

    send_console();

    /* requestAnimationFrame-driven loop (fps arg 0); em_main_loop paces the
     * cycle count to nominal GE-120 wall-clock time itself. */
    last_now_ms = emscripten_get_now();
    emscripten_set_main_loop(em_main_loop, 0, 0);
    return 0;
}
