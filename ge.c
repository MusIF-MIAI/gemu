#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "ge.h"
#include "msl.h"

#define MAX_PROGRAM_STORAGE_WORDS 129
static int ge_halted(struct ge *ge)
{
    return ge->halted;
}

int ge_init(struct ge *ge)
{
    ge->halted = 1;
    ge->ticks = 0;
    return 0;
}

/// Emulate the press of the "clear" button in the console
void ge_clear(struct ge *ge)
{
    // From 14023130-0, sheet 5:
    // The pressure of the "CLEAR" push button only determines the continuos
    // performance of the "00" status
    ge->rSO = 0;

    /* From 30004122 o/A, sheet 31:
     * The light is switched off by tle LOFF instruction or by the CLEAR key */
    ge->operator_call = 0;

    // Also clear the emulated memory... what else?!
    memset(ge->mem, 0, sizeof(ge->mem));

    ge->AINI = 0;
}

/// Emulate the press of the "load" button in the console
int ge_load(struct ge *ge, uint8_t *program, uint8_t size)
{
    if (program == NULL && size != 0)
        return -1;

    /* When pressing LOAD button, AINI is set. If AINI is set, the state 80
     * (initialitiation) goes to state c8, starting the loading of the program
     * (of max 129 words) from one of the peripherc unit. */

    /* set AINI FF to 1 (pag. 96)*/
    ge->AINI = 1;
    if (program != NULL) {
        if (size > MAX_PROGRAM_STORAGE_WORDS)
            size = MAX_PROGRAM_STORAGE_WORDS;

        /* simulate the loading for now */
        memcpy(ge->mem, program, size);

        /* I'm supposing that the AINI signal is resetted after loading */
        ge->AINI = 0;
    }

    return 0;
}

/// Emulate the press of the "start" button in the console
int ge_start(struct ge *ge)
{
    // From 14023130-0, sheet 5:
    // With the rotating switch in "NORM" position, after the operation
    // "CLEAR-LOAD-START" or "CLEAR-START", the 80 status is performed.
    ge->rSO = 0x80;

    ge->halted = 0;

    return 0;
}

static void ge_print_well_known_states(uint8_t state) {
    switch (state) {
        case 0x00: printf("\nState 00 - Display sequence\n\n"); break;
        case 0x08: printf("\nState 08 - Forcing sequence\n\n"); break;
        case 0x64:
        case 0x65: printf("\nState 64+65 - Beta Phase\n\n"); break;
        case 0x80: printf("\nState 80 - Initialitiation\n\n"); break;
        case 0xE2:
        case 0xE3: printf("\nState E2+E3 - Alpha Phase\n\n"); break;
        case 0xF0: printf("\nState F0 - Interruption\n\n"); break;
    }
}

int ge_run_cycle(struct ge *ge)
{
    struct msl_timing_state *state;

    int old_SO = ge->rSO;

    ge_print_well_known_states(ge->rSO);
    printf("Running state %02X\n", ge->rSO);

    state = msl_get_state(ge->rSO);

    if (!state) {
        printf("no timing charts found for state %02X\n", ge->rSO);
        return 1;
    }

    for(ge->current_clock = TO00; ge->current_clock < MAX_CLOCK; ge->current_clock++) {
        /* Execute machine logic for pulse*/
        pulse(ge);

        /* Execute the commands from the timing charts */
        msl_run_state(ge, state);
    }

    if (ge->rSO == old_SO) {
        printf("State register SO did not change during an entire cycle, stopping emulation\n");
        return 1;
    }

    return 0;
}

int ge_run(struct ge *ge)
{
    int r;

    while (!ge_halted(ge)) {
        r = ge_run_cycle(ge);
        if (r != 0)
            return r;
        ge->ticks++;
    }

    return 0;
}

int ge_struct_sizeof(void)
{
    return sizeof(struct ge);
}
