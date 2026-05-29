#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "ge.h"
#include "signals.h"
#include "msl.h"
#include "console_socket.h"
#include "peripherical.h"
#include "log.h"

#define MAX_PROGRAM_STORAGE_WORDS 129

void ge_init(struct ge *ge)
{
    memset(ge, 0, sizeof(*ge));
    ge->halted = 1;
    ge->powered = 1;
    ge->register_selector = RS_NORM;

    ge->ST3.name = "ST3";
    ge->ST4.name = "ST4";
}

void ge_clear(struct ge *ge)
{
    ge->AINI = 0;
    ge->ALAM = 0;
    ge->PODI = 0;
    ge->ADIR = 0;
    ge->ACIC = 1;

    /* After the powering on of the machine the timing starts pressing the
     * "CLEAR" switch (cpu fo. 99). */
    ge->halted = 0;

    /* (One of) the possible set conditions (is): or with
     * "CLEAR" and.. (cpu fo. 98) */
    ge->ALTO = 1;

    /* By pressing "CLEAR" tje FF RC01, RC02, RC03 are reset and the FF
     * RC00 is set. (cpu fo. 115) */
    ge->RC00 = 1;
    ge->RC01 = 0;
    ge->RC02 = 0;
    ge->RC03 = 0;

    ge_seed_segment_bases(ge);
}

/* Seed the eight change / segment-base registers to their identity defaults
 * N<<12: change register N is the 16-bit big-endian word at mem[240+2N], and
 * an instruction address with modifier N (address bits 12-14) resolves to
 * displacement + base[N]. With these defaults a bare 12-bit displacement
 * carrying modifier N addresses segment N (0x1000*N ..), so a program's paged
 * addresses (e.g. JU 0x172a) resolve to their full load addresses; programs
 * may reload a base via LR/LA for paged access.
 *
 * Called by ge_clear (reset) and re-applied after a direct binary image load,
 * because a contiguous image spanning the 0x00F0-0x00FF window would otherwise
 * overwrite the bases (with its own bytes, or zeros in reconstructed gaps). */
void ge_seed_segment_bases(struct ge *ge)
{
    for (int n = 0; n < 8; n++) {
        uint16_t v = (uint16_t)(n << 12);
        ge->mem[240 + 2 * n]     = (uint8_t)(v >> 8);
        ge->mem[240 + 2 * n + 1] = (uint8_t)(v & 0xff);
    }
}

int ge_load_program(struct ge *ge, uint8_t *program, uint8_t size)
{
    if (program == NULL && size != 0)
        return -1;

    if (size > MAX_PROGRAM_STORAGE_WORDS)
        size = MAX_PROGRAM_STORAGE_WORDS;

    /* simulate the loading for now */
    memcpy(ge->mem, program, size);
    return 0;
}

/* odd-parity bit for a byte: 1 if the byte has an even number of set bits
 * (so data+parity is odd). Mirrors odd_parity() in pulse.c. */
static inline uint8_t ge_odd_parity(uint8_t data)
{
    return __builtin_parity(data) ? 0 : 1;
}

/* Load a flat image into memory at `origin` (the unified-format payload).
 * Unlike ge_load_program this is origin-aware and not size-capped; it also
 * primes the parity store and marks the cells written, so reads of the loaded
 * code parity-check cleanly. Returns 0 on success, -1 if it would exceed the
 * installed memory. */
int ge_load_image(struct ge *ge, const uint8_t *image, size_t size,
                  uint16_t origin)
{
    uint32_t max = ge->mem_size ? ge->mem_size : MEM_SIZE;

    if (image == NULL && size != 0)
        return -1;
    if ((uint32_t)origin + (uint32_t)size > max)
        return -1;

    for (size_t i = 0; i < size; i++) {
        uint16_t a = (uint16_t)(origin + i);
        ge->mem[a]         = image[i];
        ge->mem_parity[a]  = ge_odd_parity(image[i]);
        ge->mem_written[a] = 1;
    }
    return 0;
}

/* Enter execution at `entry` without the peripheral LOAD bootstrap: seed the
 * program counter and drop the sequencer straight into the alpha (fetch) phase.
 * Use after ge_clear + ge_load_image for the direct binary-load path; the
 * --deck card-reader path uses the natural state 00 -> 80 -> alpha bootstrap
 * instead (which leaves entry at 0). */
void ge_enter(struct ge *ge, uint16_t entry)
{
    ge->rPO = entry;
    ge->rSO = 0xe2;   /* alpha phase: fetch the instruction at PO */
    ge->rSA = 0xe2;
}

void ge_load(struct ge *ge)
{
    /* When pressing LOAD button, AINI is set. If AINI is set, the state 80
     * (initialitiation) goes to state c8, starting the loading of the program
     * (of max 129 words) from one of the peripherc unit. */

    /* set AINI FF to 1 (pag. 96) */
    ge->AINI = 1;
}

void ge_load_1(struct ge * ge)
{
    /* It is possible to choose one between the two units thus prepared
     * positioning the operating console switch LOAD1/LOAD2 (The possible
     * choices are: Conn.2/Conn.3; Conn.4/Conn.3; Conn.2/Conn.4).
     *
     * (cpu fo. 43) */

    /* from the previous manual excerpt, ,i would have expected ALOI = 0t to
     * be LOAD1 and ALOI = 1 to be LOAD2, but running the initial load tests,
     * ALOI = 1 will result in the machine using the 0x80 unit name, which is
     * connector 2, while ALOI = 0 results in a 0x00 unit name, which is
     * connector 3. */

    ge->ALOI = 1;
}

void ge_load_2(struct ge * ge)
{
    ge->ALOI = 0;
}

void ge_start(struct ge *ge)
{
    /* according to the cpu documents, we should set the flipflop ARES here to
     * implement the initial loading of 80 into SO, however with the current
     * implementation it's not needed */

    ge->ALTO = 0; /* cpu fo. 97 */
}

static void ge_print_well_known_states(uint8_t state) {
    const char *name;
    switch (state) {
        case 0x00:
            name = "- Display sequence";
            break;
        case 0x08:
            name = "- Forcing sequence";
            break;
        case 0x64:
        case 0x65:
            name = "- Beta Phase";
            break;
        case 0x80:
            name = "- Initialitiation";
            break;
        case 0xE2:
        case 0xE3:
            name = "- Alpha Phase";
            break;
        case 0xF0:
            name = "- Interruption";
            break;
        default:
            name = "";
    }

    ge_log(LOG_STATES, "Running state %02x %s\n", state, name);
}

const char *ge_clock_name(enum clock c)
{
    switch (c) {
        #define X(name) \
            case name : \
                return #name ;
        ENUMERATE_CLOCKS
        #undef X
    }

    return "";
}

void ge_print_registers_nonverbose(struct ge *ge)
{
    if (ge_log_enabled(LOG_REGS_V))
        return;
    ge_log(LOG_REGS,
           "SO: %02x SA: %02x PO: %04x RO: %04x BO: %04x FO: %04x -  "
           "V1: %04x  V2: %04x V3: %04x  V4: %04x - "
           "L1: %04x  L2: %04x L3 : %04x\n",
           ge->rSO, ge->rSA, ge->rPO, ge->rRO, ge->rBO, ge->rFO,
           ge->rV1, ge->rV2, ge->rV3, ge->rV4,
           ge->rL1, ge->rL2, ge->rL3);
}

void ge_print_registers_verbose(struct ge *ge)
{
    ge_log(LOG_REGS_V,
           "%s:  "
           "SO: %02x SA: %02x PO: %04x RO: %04x BO: %04x FO: %04x  -  "
           "NO: %02x NI: %02x  -  "
           "FA: %02x FI: %02x - "
           "V1: %04x  V2: %04x V3: %04x  V4: %04x - "
           "L1: %04x  L2: %04x L3 : %04x\n",
           ge_clock_name(ge->current_clock),
           ge->rSO, ge->rSA, ge->rPO, ge->rRO, ge->rBO, ge->rFO,
           NO_knot(ge), NI_knot(ge),
           ge->ffFA, ge->ffFI,
           ge->rV1, ge->rV2, ge->rV3, ge->rV4,
           ge->rL1, ge->rL2, ge->rL3);
}

void ge_clock_increment(struct ge* ge)
{
    ge->current_clock++;
    if (ge->current_clock == END_OF_STATUS)
        ge->current_clock = TO00;
}

uint8_t ge_clock_is_first(struct ge* ge)
{
    return ge->current_clock == TO00;
}

uint8_t ge_clock_is_last(struct ge* ge)
{
    return ge->current_clock == (END_OF_STATUS - 1);
}

int ge_run_pulse(struct ge *ge)
{
    int r;
    struct msl_timing_state *state;

    if (ge_clock_is_first(ge)) {
        r = ge_peri_on_clock(ge);
        if (r != 0)
            return r;

        /* poll the connectors and try to set up the cpu state.
         * should this be here? */
        connectors_first_clock(ge);
    }

    /* Execute common pulse machine logic */
    pulse(ge);

    /* Execute peripherals pulse callbacks */
    r = ge_peri_on_pulses(ge);
    if (r != 0)
        return r;

    /* Execute the commands from the timing charts */
    state =  msl_get_state(ge->rSA);

    /* The state to execute gets loaded in SA at TO10 */
    if (ge->current_clock == TO10)
        ge_print_well_known_states(ge->rSA);

    if (!state) {
        ge_log(LOG_ERR, "no timing charts found for state %02X\n", ge->rSA);
        return 1;
    }

    msl_run_state(ge, state);

    if (ge_clock_is_last(ge)) {
        fsn_last_clock(ge);
        ge_print_registers_nonverbose(ge);
    }

    ge_clock_increment(ge);
    return 0;
}

int ge_run_cycle(struct ge *ge)
{
    do {
        int r = ge_run_pulse(ge);
        if (r)
            return r;
    } while (!ge_clock_is_first(ge));

    return 0;
}

int ge_deinit(struct ge *ge)
{
    ge_peri_deinit(ge);
    return 0;
}

void connectors_first_clock(struct ge *ge)
{
    if (RA101(ge)) {
        ge_log(LOG_READER, "RA101: signaling incoming data\n");
        ge->RC01 = 1;
    }
}

void fsn_last_clock(struct ge *ge)
{
    /* At the end of a CPU cycle the future-status network is stored in SO
     * (cpu fo. 127), advancing the program sequencer one state.
     *
     * In maintenance forcing (rotary off NORM) the program sequencer is frozen:
     * the manual (CPU[4] §4 "Maintenance Panel", dwg 30004122 fo. 35-37) says a
     * forcing cycle writes the *register under exam* (displayed through BO), it
     * does not step the program. So a forcing cycle must NOT advance SO — that
     * is what lets the operator key an instruction across phases (force SO=E2,
     * step to E0, force FO, step to the 0x64 beta, force L1, step to execute).
     * The one exception is rotary position 13 (RS_SO), which forces SO/SI
     * itself — that is how the operator sets the sequencer state.
     *
     * RICI ("disable next status") suppresses the advance in normal operation,
     * letting a status be re-executed. */
    uint8_t sel_norm = ge->register_selector == RS_NORM;
    uint8_t sel_so   = ge->register_selector == RS_SO;
    uint8_t advance_so = sel_norm ? !ge->console_switches.RICI : sel_so;
    if (ge->RIA0 && advance_so) {
        ge_log(LOG_FUTURE, "last clock cpu, %02x in SO\n", ge->future_state);
        ge->rSO = ge->future_state;
    } else {
        ge_log(LOG_FUTURE, "last clock cpu, SO held at %02x (RIA0 %d advance %d)\n",
               ge->rSO, ge->RIA0, advance_so);
    }

    /* after the end of a cpu work cycle, (ALTO / ALS71=1) is set if
     * the PAPA switch is inserted, or if the rotary switch is neither
     * in the normal position, nor in position 8 for recording in
     * memory ALSOA=0) (cpu fo. 98)
     */
    uint8_t is_papa = ge->console_switches.PAPA;
    uint8_t is_norm = ge->register_selector == RS_NORM;
    uint8_t is_scr  = ge->register_selector == RS_V1_SCR;

    /* Step-by-step (PAPA, the ASIN request) can be inhibited by the program:
     * INS sets ADIR=1, ENS / CLEAR clear it (CPU[4] §3.3). The maintenance STOC
     * switch overrides the inhibit. This is the HW gate ALTO <- ... ASIN(ATOC +
     * !ADIR), with ATOC = STOC (msl-states.c fo., cpu fo. 98). */
    uint8_t step_enabled = ge->console_switches.STOC || !ge->ADIR;
    uint8_t papa_stop = is_papa && step_enabled;
    ge_log(LOG_FUTURE, "    papa: %d, step_en: %d, norm: %d, scr: %d ==> %d\n",
           is_papa, step_enabled, is_norm, is_scr,
           ge->RIA0 && (papa_stop || !(is_norm || is_scr)));

    if (ge->RIA0 && (papa_stop || !(is_norm || is_scr)))
        ge->ALTO = 1;

    /* PATE stops the timing after every cycle of the delay line — a finer step
     * than PAPA and, unlike PAPA, it is not gated by the CPU/channel cycle
     * (RIA0/RIA2), so it does interfere with peripheral transfers. One START
     * then runs exactly one delay-line cycle. (CPU[4] §4, fo.35) */
    if (ge->console_switches.PATE)
        ge->ALTO = 1;

    /* after the execution of a channel 2 cycle, load the first
     * 4 bits of the future status network in SI. (cpu fo. 127) */
    if (ge->RIA2) {
        ge_log(LOG_FUTURE, "last clock ch2, %02x in SI\n", ge->future_state);
        ge->rSI = ge->future_state;
    }

}
