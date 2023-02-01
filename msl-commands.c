#include "ge.h"
#include "log.h"

#ifndef MSL_COMMANDS_INCLUDED_BY_MSL_STATES
#   error This file should be include by msl-states.c and not compiled directly
#endif

#define CC { ge_log(LOG_ERR, "implement command %s\n", __FUNCTION__); }

#define BIT(X) (1 << X)
#define SET_BIT(R, X) (R = (R | BIT(X)))
#define RESET_BIT(R, X) (R = (R & ~BIT(X)))

/* uses manual notation */
/* [ 4 | 3 | 2 | 1 ] for 16 bits and [ 2 | 1 ] for 8 bits*/
#define NIBBLE_MASK(X) (0xf << ((X-1)*4))

/* Commands To Load The Registers */
/* ------------------------------ */

static void CO00(struct ge* ge) { ge->rPO = ge->kNI; }
static void CO01(struct ge* ge) { ge->rV1 = ge->kNI; }
static void CO02(struct ge* ge) { ge->rV2 = ge->kNI; }
static void CO03(struct ge* ge) { ge->rV3 = ge->kNI; }
static void CO04(struct ge* ge) { ge->rV4 = ge->kNI; }

static void CI00(struct ge* ge) { CO00(ge); }
static void CI01(struct ge* ge) { CO01(ge); }
static void CI02(struct ge* ge) { CO02(ge); }
static void CI03(struct ge* ge) { CO03(ge); }
static void CI04(struct ge* ge) { CO04(ge); }
static void CI05(struct ge* ge) { ge->rL1 = ge->kNI; }
static void CI06(struct ge* ge) { ge->rL2 = ge->kNI & 0x00ff; }

static void CI08(struct ge* ge) { ge->rFO = (uint8_t)ge->kNI; }

/* NO Knot Selection Commands */
/* -------------------------- */

static void CO10(struct ge* ge) { ge->kNO = ge->rPO;}
static void CO12(struct ge* ge) { ge->kNO = ge->rV2;}

static void CI12(struct ge* ge) { CO12(ge); }

static void CI19(struct ge* ge)
{
    /* TODO: not sure if this is correct */
    enum ge_console_register_selector reg = ge_console_get_register_selector(ge);

    if (reg != RS_NORM) {
        ge_log(LOG_ERR, "Forcing not yet impelemented\n");
        ge->halted = 1;
    }
}

/* VO, BO, RO Loading Commands */
/* --------------------------- */

static void CO30(struct ge* ge) { ge->rRO = ge->mem[ge->rVO]; }
static void CO35(struct ge* ge) { /* "reset int. error"? (cpu fo. 105) */ }

static void CI32(struct ge* ge)
{
    /* NO43 -> RO */
    ge->rRO = ge->kNO >> 8;
}

static void CI38(struct ge *ge) { ge->AVER = ge->ALTO; }
static void CI39(struct ge *ge) { ge->AVER = 0; }

/* Count And Arithmetical Unit Commands */
/* ------------------------------------ */

static void CO41(struct ge* ge) { ge->counting_network.cmds.from_zero = 1; };
static void CO49(struct ge* ge) {
    /* most probably incorrect, "reset urpe/urpu" (cpu fo. 106) */
    ge->URPE = 0;
    ge->URPU = 0;
};


/* NI Knot Selection Commands */
/* -------------------------- */

static void CI60(struct ge *ge)
{
    ge->kNI &= ~NIBBLE_MASK(4);
    ge->kNI |= (ge->rRO & NIBBLE_MASK(2));
}

static void CI61(struct ge *ge) CC

static void CI62(struct ge *ge)
{
    ge->kNI &= ~NIBBLE_MASK(2);
    ge->kNI |= (ge->rRO & NIBBLE_MASK(2));
}

static void CI63(struct ge *ge) CC
static void CI64(struct ge *ge) CC

static void CI65(struct ge *ge)
{
    ge->kNI &= ~NIBBLE_MASK(3);
    ge->kNI |= (ge->rRO & NIBBLE_MASK(1));
}

static void CI66(struct ge *ge) CC
static void CI67(struct ge *ge)
{
    /* this should inhibit couting network to drive NI */
    ge->kNI &= ~NIBBLE_MASK(1);
    ge->kNI |= (ge->rRO & NIBBLE_MASK(1));
}

static void CI68(struct ge *ge) CC
static void CI69(struct ge *ge) { ge->ALTO = 1; }

/* Commands To Set And Reset FF Of Condition */
/* ----------------------------------------- */

static void CI76(struct ge* ge) { SET_BIT(ge->ffFI, 6); }
static void CI77(struct ge* ge) { ge->ADIR = 1; }
static void CI78(struct ge* ge) { ge->ADIR = 0; }
static void CI80(struct ge* ge) { RESET_BIT(ge->ffFI, 0); }
static void CI81(struct ge* ge) { RESET_BIT(ge->ffFI, 1); }
static void CI82(struct ge* ge) { RESET_BIT(ge->ffFI, 2); }
static void CI83(struct ge* ge) { RESET_BIT(ge->ffFI, 3); }
static void CI87(struct ge* ge) { ge->ALAM = 1; }
static void CI88(struct ge* ge) { ge->ALAM = 0; }
static void CI89(struct ge* ge) { ge->ALTO = 1; }

/* Commands To Force In NO Knot */
/* ---------------------------- */

static void CO90(struct ge *ge) CC
static void CO91(struct ge *ge) CC
static void CO92(struct ge *ge) CC
static void CO93(struct ge *ge) CC
static void CO94(struct ge *ge) CC
static void CO95(struct ge *ge) CC
static void CO96(struct ge *ge) { SET_BIT(ge->kNO, 6); }
static void CO97(struct ge *ge) { SET_BIT(ge->kNO, 7); }

/* Commands For External Operations */
/* -------------------------------- */

/* static void CEXX(struct ge* ge) CC */

/* Future States Commands */
/* ---------------------- */

/* Set S0 */
static void CU00(struct ge* ge) { SET_BIT(ge->rSO, 0); }
static void CU01(struct ge* ge) { SET_BIT(ge->rSO, 1); }
static void CU02(struct ge* ge) { SET_BIT(ge->rSO, 2); }
static void CU03(struct ge* ge) { SET_BIT(ge->rSO, 3); }
static void CU04(struct ge* ge) { SET_BIT(ge->rSO, 4); }
static void CU05(struct ge* ge) { SET_BIT(ge->rSO, 5); }
static void CU06(struct ge* ge) { SET_BIT(ge->rSO, 6); }
static void CU07(struct ge* ge) { SET_BIT(ge->rSO, 7); }

/* Reset S0 */
static void CU10(struct ge* ge) { RESET_BIT(ge->rSO, 0); }
static void CU11(struct ge* ge) { RESET_BIT(ge->rSO, 1); }
static void CU12(struct ge* ge) { RESET_BIT(ge->rSO, 2); }
static void CU13(struct ge* ge) { RESET_BIT(ge->rSO, 3); }
static void CU14(struct ge* ge) { RESET_BIT(ge->rSO, 4); }
static void CU15(struct ge* ge) { RESET_BIT(ge->rSO, 5); }
static void CU16(struct ge* ge) { RESET_BIT(ge->rSO, 6); }
static void CU17(struct ge* ge) { RESET_BIT(ge->rSO, 7); }
