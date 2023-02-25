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

static void CI12(struct ge* ge) { CO12(ge);                          }
static void CI15(struct ge* ge) { ge->kNO.cmd = KNOT_L1_IN_NO;       }
static void CI16(struct ge* ge) { CO16(ge);                          }
static void CI17(struct ge* ge) { ge->kNO.cmd = KNOT_L3_IN_NO;       }
static void CI19(struct ge* ge) { ge->kNO.cmd = KNOT_FORCE_IN_NO_21; }
static void CI20(struct ge* ge) { ge->kNO.cmd = KNOT_AM_IN_NO;       }
static void CI21(struct ge* ge) { ge->kNO.cmd = KNOT_RI_IN_NO_43;    }


/* VO, BO, RO Loading Commands */
/* --------------------------- */

static void CO30(struct ge* ge) { ge->memory_command = MC_READ;  }
static void CO31(struct ge* ge) { ge->memory_command = MC_WRITE; }
static void CO35(struct ge* ge) { /* "reset int. error"? (cpu fo. 105) */ }

static void CI32(struct ge* ge) { ge->rRO = NO_knot(ge) >> 8; }

static void CI33(struct ge* ge) {
    ge->rRO = NO_knot(ge) & 0x00ff;
    ge->TO50_conditions.did_CI33 = 1;
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
    ge->PUC1 = 0;
}

/* Count And Arithmetical Unit Commands */
/* ------------------------------------ */

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

static void CO90(struct ge *ge) CC
static void CO91(struct ge *ge) CC
static void CO92(struct ge *ge) CC
static void CO93(struct ge *ge) CC
static void CO94(struct ge *ge) CC
static void CO95(struct ge *ge) CC
static void CO96(struct ge *ge) { SET_BIT(ge->kNO.forcings, 6); }
static void CO97(struct ge *ge) { SET_BIT(ge->kNO.forcings, 7); }

/* Commands For External Operations */
/* -------------------------------- */

/* static void CEXX(struct ge* ge) CC */

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
