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

void ge_load(struct ge *ge)
{
    /* When pressing LOAD button, AINI is set. If AINI is set, the state 80
     * (initialitiation) goes to state c8, starting the loading of the program
     * (of max 129 words) from one of the peripherc unit. */

    /* set AINI FF to 1 (pag. 96) */
    ge->AINI = 1;
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
        case 0x00: name = "- Display sequence"; break;
        case 0x08: name = "- Forcing sequence"; break;
        case 0x64:
        case 0x65: name = "- Beta Phase"; break;
        case 0x80: name = "- Initialitiation"; break;
        case 0xE2:
        case 0xE3: name = "- Alpha Phase"; break;
        case 0xF0: name = "- Interruption"; break;
        default:   name = "";
    }

    ge_log(LOG_STATES, "Running state %02x %s\n", state, name);
}

const char *ge_clock_name(enum clock c)
{
    switch (c) {
        #define X(name) case name : return #name ;
        ENUMERATE_CLOCKS
        #undef X
    }

    return "";
}

void ge_print_registers_nonverbose(struct ge *ge)
{
    if (ge_log_enabled(LOG_REGS_V)) return;
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
           "FA: %02x - "
           "V1: %04x  V2: %04x V3: %04x  V4: %04x - "
           "L1: %04x  L2: %04x L3 : %04x\n",
           ge_clock_name(ge->current_clock),
           ge->rSO, ge->rSA, ge->rPO, ge->rRO, ge->rBO, ge->rFO,
           NO_knot(ge), NI_knot(ge),
           ge->ffFA,
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

void fsn_last_clock(struct ge *ge)
{
    /* at the end of a cycle attributed to the CPU, provided RICI
     * is not active and the rotary switch is in normal position,
     * the future status network is stored in SO. (cpu fo. 127) */
    if (ge->RIA0 && !ge->console_switches.RICI) {
        ge_log(LOG_FUTURE, "last clock cpu, %02x in SO\n", ge->future_state);
        ge->rSO = ge->future_state;
    } else {
        ge_log(LOG_FUTURE, "last clock cpu, not setting future state %02x in SO becuse RIA0 %d and !RICI %d\n", ge->future_state, ge->RIA0, !ge->console_switches.RICI);
    }

    /* after the end of a cpu work cycle, (ALTO / ALS71=1) is set if
     * the PAPA switch is inserted, or if the rotary switch is neither
     * in the normal position, nor in position 8 for recording in
     * memory ALSOA=0) (cpu fo. 98)
     */
    uint8_t is_papa = ge->console_switches.PAPA;
    uint8_t is_norm = ge->register_selector == RS_NORM;
    uint8_t is_scr  = ge->register_selector == RS_V1_SCR;
    ge_log(LOG_FUTURE, "    papa: %d, norm: %d, scr: %d ==> %d\n", is_papa, is_norm, is_scr, ge->RIA0 && (is_papa || !(is_norm || is_scr)));

    if (ge->RIA0 && (is_papa || !(is_norm || is_scr)))
        ge->ALTO = 1;

    /* after the execution of a channel 2 cycle, load the first
     * 4 bits of the future status network in SI. (cpu fo. 127) */
    if (ge->RIA2) {
        ge_log(LOG_FUTURE, "last clock ch2, %02x in SI\n", ge->future_state);
        ge->rSI = ge->future_state;
    }

}
