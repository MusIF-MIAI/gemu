#include <stddef.h>
#include <stdlib.h>
#include "ge.h"
#include "signals.h"

static void on_TO00(struct ge *ge) {
    /* cpu fo. 115 */
    ge->RIA0 = ge->RC00 && !ge->ALTO;
    ge->RESI = ge->RC01;
    ge->RIA2 = ge->RC02;
    ge->RIA3 = ge->RC03;

    /* TODO: a "counter" with RAMO, RAMI should condition RIA0 */


    ge_log(LOG_STATES, "TO00 cycle attribution - ALTO: %d RC00: %d RC01: %d RC02: %d RC03: %d\n", ge->ALTO, ge->RC00, ge->RC01, ge->RC02, ge->RC03);
    ge_log(LOG_STATES, "TO00 cycle attribution - RIA0: %d RESI: %d RIA2: %d RIA3: %d\n", ge->RIA0, ge->RESI, ge->RIA2, ge->RIA3);
    ge_log(LOG_STATES, "TO00 cycle attribution - RIUC: %d RES0: %d RES2: %d RES3: %d\n", RIUC(ge), RES0(ge), RES2(ge), RES3(ge));
}

static void on_TO10(struct ge *ge) {
    ge->ffFA = ge->ffFI; /* cpu fo. 129  */
    ge->rSA  = NA(ge);  /* cpu fo. 128 */

    /* save SA to emulate the future state network */
    ge->future_state = ge->rSA;

    /* TODO: a "counter" with RAMO, RAMI should count (cpu fo. 115) */
}

static void on_TO11(struct ge *ge) {}
static void on_TO15(struct ge *ge) {}
static void on_TO19(struct ge *ge) {}

static void on_TO20(struct ge *ge) {
    ge->rBO = NO_knot(ge); /* cpu fo. 142, 126 */
    ge->rVO = NO_knot(ge); /* cpu fo. 124, 125 */

    ge->ACIC = 0;        /* cpu fo. 99  */

    /* TODO: are there any condition?   */
    ge->rRO = 0;         /* cpu fo. 142 */
}

static void on_TO25(struct ge *ge) {}
static void on_TO30(struct ge *ge) {}

static void on_TO40(struct ge *ge) {
    /* stub */
    if (ge->counting_network.cmds.from_zero) {
        ge->kNI = ge->rBO + 1;
    }
}

static void on_TO50(struct ge *ge) {
    /* timing chart js1-js2-jie-ecc, fo. 32,
     * also, display, fo. 17 */
    ge->rBO = NO_knot(ge);
}

static void on_TO60(struct ge *ge) {}
static void on_TO64(struct ge *ge) {}
static void on_TO65(struct ge *ge) {}
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
