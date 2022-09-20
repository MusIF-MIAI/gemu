#include <stdio.h>
#include "ge.h"

#ifndef MSL_COMMANDS_INCLUDED_BY_MSL_STATES
#   error This file should be include by msl-states.c and not compiled directly
#endif

#define CC { printf("implement command %s\n", __FUNCTION__); }

#define BIT(X) (1 << X)
#define SET_BIT(R, X) (R = (R | BIT(X)))
#define RESET_BIT(R, X) (R = (R & ~BIT(X)))

/* uses manual notation */
/* [ 4 | 3 | 2 | 1 ] for 16 bits and [ 2 | 1 ] for 8 bits*/
#define NIBBLE_MASK(X) (0xffff << ((X-1)*4))

/* Commands To Load The Registers */
/* ------------------------------ */

void CO00(struct ge* ge) { ge->rPO = ge->kNI; }
void CO02(struct ge* ge) { ge->rV2 = ge->kNI; }

void CI01(struct ge* ge) CC
void CI02(struct ge* ge) CC
void CI05(struct ge* ge) { ge->rL1 = ge->kNI; }
void CI06(struct ge* ge) CC
void CI08(struct ge* ge) { ge->rFO = (uint8_t)ge->kNI; }

/* NO Knot Selection Commands */
/* -------------------------- */

void CO10(struct ge* ge) CC
void CO12(struct ge* ge) CC

void CI12(struct ge* ge) CC

void CI19(struct ge* ge)
{
    /* TODO: not sure if this is correct */
    if (ge->console.rotary != RS_NORM) {
        printf("Forcing not yet impelemented\n");
        ge->halted = 1;
    }
}

/* VO, BO, RO Loading Commands */
/* --------------------------- */

void CO30(struct ge* ge) CC

void CI32(struct ge* ge)
{
    /* NO43 -> RO */
    ge->rRO = ge->kNO >> 8;
}

void CI38(struct ge *ge) CC
void CI39(struct ge *ge) CC

/* Count And Arithmetical Unit Commands */
/* ------------------------------------ */

void CO41(struct ge* ge) CC


/* NI Knot Selection Commands */
/* -------------------------- */

void CI60(struct ge *ge) CC
void CI61(struct ge *ge) CC

void CI62(struct ge *ge)
{
    ge->kNI &= ~NIBBLE_MASK(2);
    ge->kNI |= (ge->rRO & NIBBLE_MASK(2));
}

void CI63(struct ge *ge) CC
void CI64(struct ge *ge) CC
void CI65(struct ge *ge) CC
void CI66(struct ge *ge) CC
void CI67(struct ge *ge) CC
void CI68(struct ge *ge) CC
void CI69(struct ge *ge) CC

/* Commands To Set And Reset FF Of Condition */
/* ----------------------------------------- */

void CI76(struct ge* ge) { SET_BIT(ge->ffFI, 6); }
void CI80(struct ge* ge) { RESET_BIT(ge->ffFI, 0); }
void CI81(struct ge* ge) { RESET_BIT(ge->ffFI, 1); }
void CI82(struct ge* ge) { RESET_BIT(ge->ffFI, 2); }
void CI83(struct ge* ge) CC
void CI89(struct ge* ge) CC

/* Commands To Force In NO Knot */
/* ---------------------------- */

void CO90(struct ge *ge) CC
void CO91(struct ge *ge) CC
void CO92(struct ge *ge) CC
void CO93(struct ge *ge) CC
void CO94(struct ge *ge) CC
void CO95(struct ge *ge) CC
void CO96(struct ge *ge) { SET_BIT(ge->kNO, 6); }
void CO97(struct ge *ge) { SET_BIT(ge->kNO, 7); }

/* Commands For External Operations */
/* -------------------------------- */

/* void CEXX(struct ge* ge) CC */

/* Future States Commands */
/* ---------------------- */

/* Set S0 */
void CU00(struct ge* ge) { SET_BIT(ge->rSO, 0); }
void CU01(struct ge* ge) { SET_BIT(ge->rSO, 1); }
void CU02(struct ge* ge) { SET_BIT(ge->rSO, 2); }
void CU03(struct ge* ge) { SET_BIT(ge->rSO, 3); }
void CU04(struct ge* ge) { SET_BIT(ge->rSO, 4); }
void CU05(struct ge* ge) { SET_BIT(ge->rSO, 5); }
void CU06(struct ge* ge) { SET_BIT(ge->rSO, 6); }
void CU07(struct ge* ge) { SET_BIT(ge->rSO, 7); }

/* Reset S0 */
void CU10(struct ge* ge) { RESET_BIT(ge->rSO, 0); }
void CU11(struct ge* ge) { RESET_BIT(ge->rSO, 1); }
void CU12(struct ge* ge) { RESET_BIT(ge->rSO, 2); }
void CU13(struct ge* ge) { RESET_BIT(ge->rSO, 3); }
void CU14(struct ge* ge) { RESET_BIT(ge->rSO, 4); }
void CU15(struct ge* ge) { RESET_BIT(ge->rSO, 5); }
void CU16(struct ge* ge) { RESET_BIT(ge->rSO, 6); }
void CU17(struct ge* ge) { RESET_BIT(ge->rSO, 7); }
