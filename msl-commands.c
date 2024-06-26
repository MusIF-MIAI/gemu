#include "ge.h"
#include "log.h"
#include "bit.h"
#include "signals.h"

#ifndef MSL_COMMANDS_INCLUDED_BY_MSL_STATES
#   error This file should be include by msl-states.c and not compiled directly
#endif

#define CC { ge_log(LOG_ERR, "implement command %s\n", __FUNCTION__); }

/* Commands To Load The Registers */
/* ------------------------------ */

static void CO00(struct ge* ge) { ge->rPO = NI_knot(ge); }
static void CO01(struct ge* ge) { ge->rV1 = NI_knot(ge); }
static void CO02(struct ge* ge) { ge->rV2 = NI_knot(ge); }
static void CO03(struct ge* ge) { ge->rV3 = NI_knot(ge); }
static void CO04(struct ge* ge) { ge->rV4 = NI_knot(ge); }

static void CI00(struct ge* ge) { CO00(ge); }
static void CI01(struct ge* ge) { CO01(ge); }
static void CI02(struct ge* ge) { CO02(ge); }
static void CI03(struct ge* ge) { CO03(ge); }
static void CI04(struct ge* ge) { CO04(ge); }
static void CI05(struct ge* ge) { ge->rL1 = NI_knot(ge); }
static void CI06(struct ge* ge) { ge->rL2 = NI_knot(ge) & 0x00ff; }
static void CI07(struct ge* ge) { ge->rL3 = NI_knot(ge); }
static void CI08(struct ge* ge) { ge->rFO = (NI_knot(ge) & 0x00ff); }
static void CI09(struct ge* ge) { ge->rRI = (NI_knot(ge) & 0xff00) >> 8; }

/* NO Knot Selection Commands */
/* -------------------------- */

static void CO10(struct ge* ge) { ge->kNO.cmd = KNOT_PO_IN_NO; }
static void CO11(struct ge* ge) { ge->kNO.cmd = KNOT_V1_IN_NO; }
static void CO12(struct ge* ge) { ge->kNO.cmd = KNOT_V2_IN_NO; }
static void CO13(struct ge* ge) { ge->kNO.cmd = KNOT_V3_IN_NO; }
static void CO14(struct ge* ge) { ge->kNO.cmd = KNOT_V4_IN_NO; }
static void CO16(struct ge* ge) { ge->kNO.cmd = KNOT_L2_IN_NO; }
static void CO18(struct ge *ge) { ge->kNO.forcings = KNOT_FORCING_NO_21; }

static void CI11(struct ge* ge) { CO11(ge);                          }
static void CI12(struct ge* ge) { CO12(ge);                          }
static void CI15(struct ge* ge) { ge->kNO.cmd = KNOT_L1_IN_NO;       }
static void CI16(struct ge* ge) { CO16(ge);                          }
static void CI17(struct ge* ge) { ge->kNO.cmd = KNOT_L3_IN_NO;       }
static void CI19(struct ge* ge) { ge->kNO.force_mode = KNOT_FORCING_NO_43; }
static void CI20(struct ge* ge) { ge->kNO.cmd = KNOT_AM_IN_NO;       }
static void CI21(struct ge* ge) { ge->kNO.cmd = KNOT_RI_IN_NO_43;    }


/* VO, BO, RO Loading Commands */
/* --------------------------- */

static void CO30(struct ge* ge) { ge->memory_command = MC_READ;  }
static void CO31(struct ge* ge) { ge->memory_command = MC_WRITE; }
static void CO35(struct ge* ge) { /* "reset int. error"? (cpu fo. 105) */ }

static void CI32(struct ge* ge) {
    ge->rRO = NO_knot(ge) >> 8;
    ge->TO50_did_CI32_or_CI33 = 1;
}

static void CI33(struct ge* ge) {
    ge->rRO = NO_knot(ge) & 0x00ff;
    ge->TO50_did_CI32_or_CI33 = 1;
}

static void CI34(struct ge* ge) {
    ge->rRO = NE_knot(ge);
}

static void CI38(struct ge *ge)
{
    /* Enable set of aver & alto (cpu fo. 105) */
    ge->AVER = verified_condition(ge);

    /* (One) possible (ALTO) set condition (is): the ACOV or ACON
     * switches are insterted, an the related condition is verified
     * (cpu fo. 98) */
    if (ge->AVER && ge->console_switches.ACOV)
        ge->ALTO = 1;

    if (!ge->AVER && ge->console_switches.ACON)
        ge->ALTO = 1;
}

static void CI39(struct ge *ge)
{
    /* Reset AVER, it also resets the FF AINI and PUC1 (cpu fo. 105) */
    ge->AVER = 0;
    ge->AINI = 0;
    ge->PIC1 = 0;

    /* intermediate fo. 10 B6 */
    ge->RASI = 0;

    if (!PUC1(ge))
        ge->RC00 = 1;
}

/* Count And Arithmetical Unit Commands */
/* ------------------------------------ */

static void CO40(struct ge* ge) { ge->counting_network.cmds.decresing = 1; };
static void CO41(struct ge* ge) { ge->counting_network.cmds.from_zero = 1; };
static void CO48(struct ge* ge)
{
    /* most probably incorrect, "set urpe/urpu" (cpu fo. 106) */
    ge->URPE = 1;
    ge->URPU = 1;
}

static void CO49(struct ge* ge) {
    /* most probably incorrect, "reset urpe/urpu" (cpu fo. 106) */
    ge->URPE = 0;
    ge->URPU = 0;
};

static void CI40(struct ge *ge) { CO40(ge); }
static void CI41(struct ge *ge) { CO41(ge); }


/* NI Knot Selection Commands */
/* -------------------------- */

static void CI60(struct ge *ge) { ge->kNI.ni4 = NS_RO2; }
static void CI61(struct ge *ge) { ge->kNI.ni3 = NS_RO2; }
static void CI62(struct ge *ge) { ge->kNI.ni2 = NS_RO2; }
static void CI63(struct ge *ge) { ge->kNI.ni1 = NS_RO2; }
static void CI64(struct ge *ge) { ge->kNI.ni4 = NS_RO1; }
static void CI65(struct ge *ge) { ge->kNI.ni3 = NS_RO1; }
static void CI66(struct ge *ge) { ge->kNI.ni2 = NS_RO1; }
static void CI67(struct ge *ge) { ge->kNI.ni1 = NS_RO1; }
static void CI68(struct ge *ge) { ge->kNI.ni4 = NS_UA2; ge->kNI.ni3 = NS_UA1; }
static void CI69(struct ge *ge) { ge->kNI.ni2 = NS_UA2; ge->kNI.ni1 = NS_UA1; }

/* Commands To Set And Reset FF Of Condition */
/* ----------------------------------------- */

static void CI70(struct ge* ge) { SET_BIT(ge->ffFI, 0); }
static void CI71(struct ge* ge) { SET_BIT(ge->ffFI, 1); }
static void CI72(struct ge* ge) { SET_BIT(ge->ffFI, 2); }
static void CI73(struct ge* ge) { SET_BIT(ge->ffFI, 3); }
static void CI74(struct ge* ge) { SET_BIT(ge->ffFI, 4); }
static void CI75(struct ge* ge) { SET_BIT(ge->ffFI, 5); }
static void CI76(struct ge* ge) { SET_BIT(ge->ffFI, 6); }
static void CI77(struct ge* ge) { ge->ADIR = 1; }
static void CI78(struct ge* ge) { ge->ADIR = 0; }
static void CI80(struct ge* ge) { RESET_BIT(ge->ffFI, 0); }
static void CI81(struct ge* ge) { RESET_BIT(ge->ffFI, 1); }
static void CI82(struct ge* ge) { RESET_BIT(ge->ffFI, 2); }
static void CI83(struct ge* ge) { RESET_BIT(ge->ffFI, 3); }
static void CI84(struct ge* ge) { RESET_BIT(ge->ffFI, 4); }
static void CI85(struct ge* ge) { RESET_BIT(ge->ffFI, 5); }
static void CI86(struct ge* ge) { RESET_BIT(ge->ffFI, 6); }

static void CI87(struct ge* ge) {
    ge->ALAM = 1;
    ge->PODI = 1; /* should PODI be set here? */
}

static void CI88(struct ge* ge) {
    ge->ALAM = 0;
    ge->PODI = 0; /* should PODI be set here? */
}

static void CI89(struct ge* ge) { ge->ALTO = 1; }

/* Commands To Force In NO Knot */
/* ---------------------------- */

static void CO90(struct ge *ge) { SET_BIT(ge->kNO.forcings, 0); }
static void CO91(struct ge *ge) { SET_BIT(ge->kNO.forcings, 1); }
static void CO92(struct ge *ge) { SET_BIT(ge->kNO.forcings, 2); }
static void CO93(struct ge *ge) { SET_BIT(ge->kNO.forcings, 3); }
static void CO94(struct ge *ge) { SET_BIT(ge->kNO.forcings, 4); }
static void CO95(struct ge *ge) { SET_BIT(ge->kNO.forcings, 5); }
static void CO96(struct ge *ge) { SET_BIT(ge->kNO.forcings, 6); }
static void CO97(struct ge *ge) { SET_BIT(ge->kNO.forcings, 7); }

/* Commands For External Operations */
/* -------------------------------- */

static void CE00(struct ge* ge) {
    /* is PIPO needed?! (intermediate fo. 10 A1*/
    ge_log(LOG_PERI, "RA <- %x\n", ge->rRO);
    ge->rRA = ge->rRO;
}

static void CE01(struct ge* ge) {
    /* is PIPO needed?! (intermediate fo. 10 A1*/
    ge_log(LOG_PERI, "RE <- %x\n", ge->rRO);
    ge->rRE = ge->rRO;
}

static void CE02(struct ge* ge) {
    /* admits AEBE: */
    /*     UNIV 1.2µs --> RATE1 nand PC131 --> AEBE */
    /*     AEBE is a control signal sent to ST3 and ST4 */

    /* Unconditionally set by command CE02 (cpu fo. 235) */
    ge->PIC1 = 1;

    /* Latch PB flip flops (intermediate diagram fo. 9  D 1,2,3) */
    ge->PB06 = BIT(ge->rL1, 6);
    ge->PB07 = BIT(ge->rL1, 7);

    /* In case of initial load / TPER, L2 here should be the Z character of
     * the instructions. It encodes the channel to be used for the transfer.
     *
     * If bit 0 of L2 is 0, channel is either 1 or 3,
     * if bit 0 of L2 is 1, channel is 2.
     *
     * (cpu fo. 73)
     */
    if (BIT(ge->rL2, 0))
        ge->PB26 = BIT(ge->rL1, 6);

    if (PC031(ge)) {
        ge->PB36 = BIT(ge->rL1, 6);
        ge->PB37 = BIT(ge->rL1, 7);
    }

    /* parity check ignored */
}

static void CE03(struct ge *ge) {
    /* reset IO */
    uint8_t CE031 = 1;
    uint8_t TO191 = ge->current_clock == TO19;
    uint8_t TO651 = ge->current_clock == TO65;
    uint8_t RECIA = !(CE031 && PC011(ge));
    uint8_t RECI1 = !RECIA;

    ge_log(LOG_PERI, "RESET I/U (CE03)\n");

    if (TO651 && RECI1) {
        ge->RIG1 = 0;
        ge_log(LOG_PERI, "RESETTING RIG1 (CE03)\n", ge->RIG1);
    } else {
        ge_log(LOG_PERI, "NOT RESETTING RIG1 (CE03)\n");
    }

    if (TO191) {
        ge->RACI = 0;
        ge_log(LOG_PERI, "RESETTING RACI (CE03)\n");
    } else {
        ge_log(LOG_PERI, "NOT RESETTING RACI (CE03)\n");
    }

    /* maybe more? */
}

static void CE05(struct ge* ge) {
    ge_log(LOG_PERI, "TODO: enable selection external error\n");
}

static void CE06(struct ge *ge) {
    /* enable set error 1 */
}

static void CE07(struct ge *ge) {
    /* set io for can 1, 2 or 3 */
    uint8_t TO191 = ge->current_clock == TO19;

    if (TO191 && PC011(ge)) {
        ge->RASI = 1;
        ge_log(LOG_PERI, "SET RASI (CE07)\n");
    } else {
        ge_log(LOG_PERI, "NOT SETTING RASI (CE07)\n");
    }

    if (TU00A(ge)) {
        ge_log(LOG_PERI, "TU00A! %d\n", TU00A(ge));
    }
}

static void CE08(struct ge *ge) {
    /* set VICU */
    uint8_t TO191 = ge->current_clock == TO19;

    ge_log(LOG_PERI, "SET VICU (CE08)\n");

    if (TO191 && ge->RETO) {
        ge->RAVI = 1;
        ge_log(LOG_PERI, "SET RAVI (CE08)\n");
    } else {
        ge_log(LOG_PERI, "NOT SETTING RAVI (CE08)\n");
    }

    if (ge->RAVI && RB111(ge)) {
        ge->RACI = 1;
        ge_log(LOG_PERI, "SET RACI (CE08)\n");
    } else {
        ge_log(LOG_PERI, "NOT SETTING RACI (CE08)\n");
    }
}

static void CE09(struct ge *ge) {
    /* character request */

    /* emits TU101: */
    /* UNIV 1.2µs --> RT111 */

    uint8_t RT111 = 1;

    /* intermediate fo 14 D3 */
    uint8_t TU03A = !(RT111 && PC121(ge));
    uint8_t TU03 = !TU03A;

    if (TU03)
        reader_send_tu10(ge);
}

static void CE10(struct ge *ge) {
    /* send command */

    /* emits TU201: UNIV 1.2µs --> RT121 */
    ge->RT121 = 1;

    /* UNIV seems a delay line to synchronise the hardware, let's
     * ignore the exact timings. */
    reader_send_tu00(ge);
}

static void CE11(struct ge* ge) {
    ge->RT131 = 1;

    /* UNIV seems a delay line to synchronise the hardware, let's
     * ignore the exact timings. */

    /* ad hoc logic, this should be conditioned by TU30C and TU30D
     * (intermediate fo 14, B4 B5) to send the command only to the
     * specified units, but the signals don't fully work here */

    if (PC131(ge))
        connector_send_tu00(ge, &ge->ST3);

    if (PC141(ge))
        connector_send_tu00(ge, &ge->ST4);
}

static void CE18(struct ge *ge) {
    /* enable reset RIAP */
    ge_log(LOG_PERI, "RESET RIAP\n");

    uint8_t TO801 = ge->current_clock == TO80;
    
    if (TO801 && RIUC(ge)) {
        ge->RC00 = 0;
        ge_log(LOG_PERI, "RESETING RC00 (CE18)\n");
    } else {
        ge_log(LOG_PERI, "NOT RESETING RC00 (CE18)\n");
    }

    if (TO801 && RESI(ge)) {
        ge->RC01 = 0;
        ge_log(LOG_PERI, "RESETING RC01 (CE18)\n");
    } else {
        ge_log(LOG_PERI, "NOT RESETING RC01 (CE18)\n");
    }

    if (TO801 && RES2(ge)) {
        ge->RC02 = 0;
        ge_log(LOG_PERI, "RESETING RC02 (CE18)\n");
    } else {
        ge_log(LOG_PERI, "NOT RESETING RC02 (CE18)\n");
    }

    if (TO801 && RES3(ge)) {
        ge->RC03 = 0;
        ge_log(LOG_PERI, "RESETING RC03 (CE18)\n");
    } else {
        ge_log(LOG_PERI, "NOT RESETING RC03 (CE18)\n");
    }
}

static void CE19(struct ge *ge) {
    /* reset selection can 3 */
}


/* Future States Commands */
/* ---------------------- */

/* Set S0 */
static void CU00(struct ge* ge) { SET_BIT(ge->future_state, 0); }
static void CU01(struct ge* ge) { SET_BIT(ge->future_state, 1); }
static void CU02(struct ge* ge) { SET_BIT(ge->future_state, 2); }
static void CU03(struct ge* ge) { SET_BIT(ge->future_state, 3); }
static void CU04(struct ge* ge) { SET_BIT(ge->future_state, 4); }
static void CU05(struct ge* ge) { SET_BIT(ge->future_state, 5); }
static void CU06(struct ge* ge) { SET_BIT(ge->future_state, 6); }
static void CU07(struct ge* ge) { SET_BIT(ge->future_state, 7); }

/* Reset S0 */
static void CU10(struct ge* ge) { RESET_BIT(ge->future_state, 0); }
static void CU11(struct ge* ge) { RESET_BIT(ge->future_state, 1); }
static void CU12(struct ge* ge) { RESET_BIT(ge->future_state, 2); }
static void CU13(struct ge* ge) { RESET_BIT(ge->future_state, 3); }
static void CU14(struct ge* ge) { RESET_BIT(ge->future_state, 4); }
static void CU15(struct ge* ge) { RESET_BIT(ge->future_state, 5); }
static void CU16(struct ge* ge) { RESET_BIT(ge->future_state, 6); }
static void CU17(struct ge* ge) { RESET_BIT(ge->future_state, 7); }

static void CU20(struct ge *ge) {
    ge->rSO = ge->rSI = ge->future_state;
    ge_log(LOG_FUTURE, "forcing state with CU20: %2x\n", ge->future_state);
}
