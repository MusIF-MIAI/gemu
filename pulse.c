#include <stddef.h>
#include <stdlib.h>
#include "ge.h"

static int on_TO00(struct ge *ge) {
	return 0;
}

static int on_TO10(struct ge *ge) {
    /* load FI00-FI06 in FA00-FA06 (cpu pag129) */
    ge->ffFA = ge->ffFI;

    /* load SA register (cpu pag.128) */
    ge->rSA = ge->kNA;
	return 0;
}

static int on_TO11(struct ge *ge) {
	return 0;
}

static int on_TO15(struct ge *ge) {
	return 0;
}

static int on_TO19(struct ge *ge) {
	return 0;
}

static int on_TO20(struct ge *ge) {

    /* pag 142, pag 126 */
    ge->rBO = ge->kNO;
    /* pag. 124, pag 125 */
    ge->rVO = ge->kNO;

    /* TODO: are there any condition? */
    /* pag 142 */
    ge->rRO = 0;

    /* oag 141, pag 125 */
    ge->kNI = ge->counting_network.output;

    return 0;
}

static int on_TO25(struct ge *ge) {
	return 0;
}

static int on_TO30(struct ge *ge) {
	return 0;
}

static int on_TO40(struct ge *ge) {
	return 0;
}

static int on_TO50(struct ge *ge) {
	return 0;
}

static int on_TO64(struct ge *ge) {
	return 0;
}

static int on_TO70(struct ge *ge) {
	return 0;
}

static int on_TO80(struct ge *ge) {
	return 0;
}

static int on_TO89(struct ge *ge) {
	return 0;
}

static int on_TO90(struct ge *ge) {
	return 0;
}

static int on_TI05(struct ge *ge) {
	return 0;
}

static int on_TI06(struct ge *ge) {
	return 0;
}

static int on_TI10(struct ge *ge) {
	return 0;
}


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
    on_TO64,
    on_TO70,
    on_TO80,
    on_TO89,
    on_TO90,
    on_TI05,
    on_TI06,
    on_TI10
};


int pulse(struct ge *ge)
{
    if (pulse_cb[ge->current_clock]) {
        return pulse_cb[ge->current_clock](ge);
    }
    return 0;
}
