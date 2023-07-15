#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ge.h"
#include "signals.h"
#include "log.h"

static void on_TO00(struct ge *ge) {
    /* cpu fo. 115 */
    ge->RIA0 = ge->RC00 && !ge->ALTO;
    ge->RESI = ge->RC01;
    ge->RIA2 = ge->RC02;
    ge->RIA3 = ge->RC03;

    /* TODO: a "counter" with RAMO, RAMI should condition RIA0 */

    ge_log(LOG_CYCLE, "  async: RC00: %d RC01: %d RC02: %d RC03: %d ALTO: %d\n",
           ge->RC00, ge->RC01, ge->RC02, ge->RC03, ge->ALTO);

    ge_log(LOG_CYCLE, "  sync:  RIA0: %d RESI: %d RIA2: %d RIA3: %d\n",
           ge->RIA0, ge->RESI, ge->RIA2, ge->RIA3);

    ge_log(LOG_CYCLE, "      -> RIUC: %d RES0: %d RES2: %d RES3: %d\n",
           RIUC(ge), RES0(ge), RES2(ge), RES3(ge));

    /* set NI to output the counting network.
     * ("this occoursr alwas during the 1st phase", cpu fo.125) */

    ge->kNI.ni1 = NS_CN1;
    ge->kNI.ni2 = NS_CN2;
    ge->kNI.ni3 = NS_CN3;
    ge->kNI.ni4 = NS_CN4;

    /* CO41 used to set from_zero is issued in TO10, so this looks
     * like a reasonable place to reset this */
    ge->counting_network.cmds.from_zero = 0;
}

static void on_TO10(struct ge *ge) {
    ge->ffFA = ge->ffFI; /* cpu fo. 129  */
    ge->rSA  = NA_knot(ge); /* cpu fo. 128 */

    /* save SA to emulate the future state network */
    ge->future_state = ge->rSA;

    /* TODO: a "counter" with RAMO, RAMI should count (cpu fo. 115) */
}

static void on_TO11(struct ge *ge) {}
static void on_TO15(struct ge *ge) {}

static void on_TO19(struct ge *ge) {
    /* intermediate fo. 9 B1*/
    ge->RECE = 0;
}

static void on_TO20(struct ge *ge) {
    ge->rBO = NO_knot(ge); /* cpu fo. 142, 126 */
    ge->rVO = NO_knot(ge); /* cpu fo. 124, 125 */

    ge->ACIC = 0; /* cpu fo. 99  */

    /* TODO: are there any condition?   */
    ge->rRO = 0;  /* cpu fo. 142 */
}

static void on_TO25(struct ge *ge) {}
static void on_TO30(struct ge *ge) {}

static void on_TO40(struct ge *ge) {
    /* stub */
    if (ge->counting_network.cmds.from_zero) {
        ge->kNI.ni1 = NS_CN1;
        ge->kNI.ni2 = NS_CN2;
        ge->kNI.ni3 = NS_CN3;
        ge->kNI.ni4 = NS_CN4;
    }
}

static void on_TO50(struct ge *ge) {}

static void on_TO50_1(struct ge *ge) {
    if (!ge->TO50_conditions.did_CI33) {
        /* timing chart js1-js2-jie-ecc, fo. 32,
         * also, display, fo. 17 */
        ge->rBO = NO_knot(ge);
    }

    memset(&ge->TO50_conditions, 0, sizeof(ge->TO50_conditions));
}

static void on_TO60(struct ge *ge) {}
static void on_TO64(struct ge *ge) {}

static void on_TO65(struct ge *ge) {
    /* the only reference i found for this is the timing diagram
     * in cpu fo. 145 */

    if (ge->memory_command == MC_READ) {
        ge->rRO = ge->mem[ge->rVO];
        ge_log(LOG_STATES, "memory read: RO = mem[VO] = mem[%x] = %x\n", ge->rVO, ge->rRO);
    }

    if (ge->memory_command == MC_WRITE) {
        ge->mem[ge->rVO] = ge->rRO;
        ge_log(LOG_STATES, "memory write: mem[VO] = RO = mem[%x] = %x\n", ge->rVO, ge->rRO);
    }

    ge->memory_command = MC_NONE;

    /* "enables the second phase commands for count selection"
     * (cpu fo. 142), not sure this is what it means */
    ge->counting_network.cmds.from_zero = 0;
}

static void on_TO70(struct ge *ge) {}
static void on_TO80(struct ge *ge) {}
static void on_TO89(struct ge *ge) {}

static void on_TO90(struct ge *ge) {
    /* TODO: check if ! is correct: PODIB should be PODI negated */
    if (!ge->PODI)
        ge->ACIC = 1;  /* cpu fo. 99 */
}

static void on_TI05(struct ge *ge) {
    /* TODO: check if ! is correct: PODIB should be PODI negated */
    if (ge->PODI)
        ge->ACIC = 1;  /* cpu fo. 99 */
}

static void on_TI06(struct ge *ge) {}
static void on_TI10(struct ge *ge) {}

static on_pulse_cb pulse_cb[END_OF_STATUS] = {
    on_TO00,
    on_TO10,
    on_TO11,
    on_TO15,
    on_TO19,
    on_TO20,
    on_TO25,
    on_TO30,
    on_TO40,
    on_TO50,
    on_TO50_1,
    on_TO60,
    on_TO64,
    on_TO65,
    on_TO70,
    on_TO80,
    on_TO89,
    on_TO90,
    on_TI05,
    on_TI06,
    on_TI10,
};


void pulse(struct ge *ge)
{
    if (pulse_cb[ge->current_clock]) {
        pulse_cb[ge->current_clock](ge);
    }
}
