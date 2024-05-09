#include <stdint.h>
#include "bit.h"
#include "msl-timings.h"
#include "signals.h"

#define MSL_COMMANDS_INCLUDED_BY_MSL_STATES
#include "msl-commands.c"
#undef MSL_COMMANDS_INCLUDED_BY_MSL_STATES

#ifndef MSL_STATES_INCLUDED_BY_MSL_TIMINGS
#   error This file should be include by msl-timings.c and not compiled directly
#endif

/* Common Conditions */
/* ----------------- */

static uint8_t not_RO00(struct ge *ge) { return !BIT(ge->rRO, 0); }
static uint8_t not_RO01(struct ge *ge) { return !BIT(ge->rRO, 1); }
static uint8_t not_RO02(struct ge *ge) { return !BIT(ge->rRO, 2); }
static uint8_t not_RO03(struct ge *ge) { return !BIT(ge->rRO, 3); }
static uint8_t not_RO04(struct ge *ge) { return !BIT(ge->rRO, 4); }
static uint8_t not_RO05(struct ge *ge) { return !BIT(ge->rRO, 5); }
static uint8_t not_RO06(struct ge *ge) { return !BIT(ge->rRO, 6); }
static uint8_t not_RO07(struct ge *ge) { return !BIT(ge->rRO, 7); }

/* Initialitiation */
/* --------------- */

// to state E2+E3 if !AINI
//          C8    if AINI

static uint8_t AINI(struct ge *ge) { return ge->AINI; }
static uint8_t not_AINI(struct ge *ge) { return !AINI(ge); }

/* TODO: "jumpers" configuration.
 *
 * This is the configuration without any "configuration" jumper connector
 * in backplane position E04.
 * It is possible to change the configuration by plugging either a PONT2N
 * or PONT2P in that slot, for this available configurations:
 *
 *  E04   || FUL2 | FUL3 || Connectors for loading
 * -------++------+------++-------------------------
 * EMPTY  ||  1   |  1   || 2 and 3
 * PONT2N ||  1   |  0   || 2 and 4
 * PONT2P ||  0   |  1   || 4 and 3
 *
 * (detailed ch. 002)
 *
 * NOTE: test for the initial load assume FUL2 = FUL3 = 1.
 */
SIG(FUL2) { return 1; }
SIG(FUL3) { return 1; }

static uint8_t state_80_TO30_CO96(struct ge *ge) {
    return (ge->ALOI && !FUL2(ge)) || (!ge->ALOI && !FUL3(ge));
}

static uint8_t state_80_TO30_CO97(struct ge *ge) {
    return ge->ALOI && FUL2(ge);
}

static const struct msl_timing_chart state_80[] = {
    { TO30, CI19, 0, DI28A0 },
    { TO30, CO96, state_80_TO30_CO96 },
    { TO30, CO97, state_80_TO30_CO97 },
    { TO40, CO00, 0 },
    { TO40, CO02, 0 },
    { TO50, CI32, 0, DI28A0 },
    { TO70, CI62, 0 },
    { TO70, CI67, 0, DI28B0 },
    { TI05, CI05, 0, DI28B0 },
    { TI05, CI08, 0 },
    { TI06, CI76, 0 },
    { TI06, CI80, 0 },
    { TI06, CI81, 0 },
    { TI06, CI82, 0 },
    { TI06, CU01, not_AINI },
    { TI06, CU03, AINI },
    { TI06, CU05, not_AINI },
    { TI06, CU06, 0 },
    { END_OF_STATUS, 0, 0 }
};

// Alpha phase

// (to state F0 if RINT & !FA06
//           E0 if !RINT | FA06)

static uint8_t state_E2_E3_TO80_CI89(struct ge *ge) {
    /* (deltaRO = HLT + ASIN(ATOC+!ADIR)) */
    return ge->rRO == HLT_OPCODE;
}

static uint8_t state_E2_E3_TI06_CI82(struct ge *ge) {
    return ge->rRO == PER_OPCODE;
}

static uint8_t state_E2_E3_TI06_CU04(struct ge *ge) {
    return ge->RINT && !BIT(ge->ffFA, 6);
}

static const struct msl_timing_chart state_E2_E3[] = {
    { TO10, CO10, 0 },
    { TO10, CO41, 0, DI12A0 },
    { TO25, CO30, 0, DI12A0 },
    { TO40, CO02, 0, DI18B0 },
    { TO70, CI67, 0, DI12A0 },
    { TO70, CI62, 0, DI12A0 },
    { TO80, CI89, state_E2_E3_TO80_CI89 },
    { TI05, CI08, 0 },
    { TI06, CI80, 0 },
    { TI06, CI82, state_E2_E3_TI06_CI82 },
    { TI06, CI83, 0 },
    { TI06, CU04, state_E2_E3_TI06_CU04 },
    { TI06, CU10, 0 },
    { TI06, CU11, 0, DI18A0 },
    { END_OF_STATUS, 0, 0 }
};

// to state E4    if FO06 | FO07
//          64+65 if !(FO06 | FO07)

static uint8_t state_E0_TI06_CU17(struct ge *ge) {
    return !(BIT(ge->rFO, 6) || BIT(ge->rFO, 7));
}

static const struct msl_timing_chart state_E0[] = {
    { TO10, CO12, 0, DI17A0 },
    { TO10, CO41, 0, DI12A0 },
    { TO25, CO30, 0, DI12A0 },
    { TO40, CO00, 0, DI17A0 },
    { TO70, CI67, 0, DI12A0 },
    { TO70, CI62, 0, DI12A0 },
    { TO80, CI39, 0 },
    { TI05, CI05, 0, DI17A0 },
    { TI06, CU02, 0 },
    { TI06, CU17, state_E0_TI06_CU17 },
    { END_OF_STATUS, 0, 0 }
};

// to state E6

static uint8_t state_E4_TO70_CI60(struct ge *ge) { return 0; }

static const struct msl_timing_chart state_E4[] = {
    { TO10, CO10, 0, DI60A0 },
    { TO10, CO41, 0, DI60A0 },
    { TO25, CO30, 0, DI12A0 },
    { TO40, CO00, 0, DI60A0 },
    { TO70, CI67, 0, DI12A0 },
    { TO70, CI62, 0, DI12A0 },
    { TO70, CI65, 0, DI19A0 },
    { TO70, CI60, state_E4_TO70_CI60 },
    { TI05, CI02, 0 },
    { TI06, CI06, 0 },
    { TI06, CU01, 0, DI60A0 },
    { END_OF_STATUS, 0, 0 }
};

// to state E5 if !L207 & (FO07 & FO06)
//          ED+EC if L207
//          64+65 if !L207 & (!FO07 | !FO06)

static uint8_t state_E6_TO80_CI38(struct ge *ge) { /* DO01? */ return 0; }
static uint8_t state_E6_TI06_CU03(struct ge *ge) { return BIT(ge->rL2, 7); }

static uint8_t state_E6_TI06_CU17(struct ge *ge) {
    return (!BIT(ge->rL2, 7) &&
            (!BIT(ge->rFO, 7) || !BIT(ge->rFO, 6)));
}

static const struct msl_timing_chart state_E6[] = {
    { TO10, CO10, 0, DI60A0 },
    { TO10, CO41, 0, DI60A0 },
    { TO25, CO30, 0, DI12A0 },
    { TO30, CI12, 0 },
    { TO40, CO00, 0, DI60A0 },
    { TO70, CI67, 0, DI12A0 },
    { TO70, CI62, 0, DI12A0 },
    { TO80, CI38, state_E6_TO80_CI38 },
    { TI05, CI01, 0 },
    { TI05, CI02, 0 },
    { TI06, CU00, 0, DI20A0 },
    { TI06, CU03, state_E6_TI06_CU03, EC56A0 },

    /* in the manual this is CU10, but it maybe a mistake.. there's no way to reach
     * the alpha states if we don't reset this bit 1 instead of bit 0 */
    { TI06, CU11, 0 },

    { TI06, CU17, state_E6_TI06_CU17 },
    { END_OF_STATUS, 0, 0 }
};

// to state E7

static const struct msl_timing_chart state_E5[] = {
    { TO10, CO10, 0, DI60A0 },
    { TO10, CO41, 0, DI60A0 },
    { TO25, CO30, 0, DI12A0 },
    { TO40, CO00, 0, DI60A0 },
    { TO70, CI67, 0, DI12A0 },
    { TO70, CI62, 0, DI12A0 },
    { TO70, CI65, 0, DI19A0 },
    { TO70, CI60, not_RO07 },
    { TI05, CI02, 0 },
    { TI06, CI06, 0 },
    { TI06, CU01, 0, DI60A0 },
    { END_OF_STATUS, 0, 0 }
};

// to state 64+65 if !L207
//          ED+EC if L207

static uint8_t state_E7_TO80_CI38(struct ge *ge) { return 1; /* DO01 ?!? */ }
static uint8_t state_E7_TI06_CU03(struct ge *ge) { return BIT(ge->rL2, 7); }

static uint8_t state_E7_TI06_CU17(struct ge *ge) {
    return BIT(ge->rL2, 7) && (BIT(ge->rFO, 7) || BIT(ge->rFO, 6));
}

static const struct msl_timing_chart state_E7[] = {
    { TO10, CO10, 0, DI60A0 },
    { TO10, CO41, 0, DI60A0 },
    { TO25, CO30, 0, DI12A0 },
    { TO30, CI12, 0 },
    { TO40, CO00, 0, DI60A0 },
    { TO70, CI67, 0, DI12A0 },
    { TO70, CI62, 0 },
    { TO80, CI38, state_E7_TO80_CI38 },
    { TI05, CI02, 0 },
    { TI06, CU00, 0, DI20A0 },
    { TI06, CU03, 0, EC56A0 },
    { TI06, CU10, 0 },
    { TI06, CU17, state_E7_TI06_CU17 },
    { END_OF_STATUS, 0, 0 }
};

/* Beta Phase */
/* ---------- */

static uint8_t jc_js1_js2_jie(struct ge *ge) {
    return ((ge->rFO == JC_OPCODE) ||
            (ge->rFO == JS1_OPCODE && ge->rL1 == JS1_2NDCHAR) ||
            (ge->rFO == JS2_OPCODE && ge->rL1 == JS2_2NDCHAR) ||
            (ge->rFO == JIE_OPCODE && ge->rL1 == JIE_2NDCHAR));
}

static uint8_t lon_loll(struct ge *ge) {
    return ((ge->rFO == LON_OPCODE && ge->rL1 == LON_2NDCHAR) ||
            (ge->rFO == LOLL_OPCODE && ge->rL1 == LOLL_OPCODE));
}

static uint8_t ins(struct ge *ge) {
    return ge->rFO == INS_OPCODE && ge->rL1 == INS_2NDCHAR;
}

static uint8_t jie(struct ge *ge) {
    return ge->rFO == JIE_OPCODE && ge->rL1 == JIE_2NDCHAR;
}

static uint8_t ens(struct ge *ge) {
    return ge->rFO == ENS_OPCODE && ge->rL1 == ENS_2NDCHAR;
}

static uint8_t loff(struct ge *ge) {
    return ge->rFO == LOFF_OPCODE && ge->rL1 == LOFF_2NDCHAR;
}

static uint8_t jc_js1_js2_jie_condition_verified(struct ge *ge) {
    return ge->AVER && jc_js1_js2_jie(ge);
}

static uint8_t nop(struct ge *ge) {
    return ge->rFO == NOP2_OPCODE;
}

static uint8_t jc_js1_js2_jie_lon_loll_loff_ins_ens_nop(struct ge *ge) {
    return jc_js1_js2_jie(ge) || lon_loll(ge) || loff(ge) || ins(ge) || ens(ge) || nop(ge);
}

/*  PER - PERI: conditions from fo. 46 */

static uint8_t per_peri(struct ge *ge) {
    return ((ge->rFO == PER_OPCODE) ||
            (ge->rFO == PERI_OPCODE));
}

static uint8_t per_peri_TO25_CO30(struct ge *ge) {
    return per_peri(ge) && !BIT(ge->rFO, 1);
}

static const struct msl_timing_chart state_64_65[] = {
    { TO10, CO10, jc_js1_js2_jie },
    { TO10, CO18, per_peri },
    { TO10, CO95, per_peri, DE07A0 },
    { TO10, CO96, per_peri, DE07A0 },
    { TO10, CO97, per_peri, DE07A0 },
    { TO20, CI87, lon_loll },
    { TO20, CI77, ins },
    { TO25, CO30, per_peri_TO25_CO30, DE08A0 },
    { TO30, CI12, jc_js1_js2_jie },
    { TO40, CO01, jc_js1_js2_jie },
    { TO60, CO35, jie },
    { TO65, CO49, jc_js1_js2_jie_lon_loll_loff_ins_ens_nop },
    { TO70, CI78, ens },
    { TO70, CI62, per_peri, DE07A0 },
    { TO70, CI67, per_peri, DE07A0 },
    { TO89, CI88, loff },
    { TI05, CI05, per_peri_TO25_CO30, DE08A0 },
    { TI05, CI00, jc_js1_js2_jie_condition_verified },
    { TI06, CU01, jc_js1_js2_jie_lon_loll_loff_ins_ens_nop, DE00A0 },
    { TI06, CU10, 0 },
    { TI06, CU07, DE00A0 },
    { TI06, CU12, 0 },
    { TI06, CU15, per_peri },
    { TI06, CU03, per_peri },
    { END_OF_STATUS, 0, 0 },
};

/* Display */
/* ------- */

static uint8_t state_00_TO10_CO10(struct ge *ge) { return AF32(ge) || AF42(ge); }
static uint8_t state_00_TO10_CO11(struct ge *ge) { return AF31(ge) || AF41(ge) || AF51(ge); }
static uint8_t state_00_TO30_CI15(struct ge *ge) { return !AF20(ge) && !AF40(ge); }
static uint8_t state_00_TO50_CI33(struct ge *ge) { return !AF20(ge) && !AF21(ge) && !AF40(ge); }

static const struct msl_timing_chart state_00[] = {
    { TO10, CO10, state_00_TO10_CO10 }, /* RS_NORM or RS_PO */
    { TO10, CO11, state_00_TO10_CO11 }, /* RS_V1 or RS_V1_SCR or RS_V1_LETT */
    { TO10, CO12, AF50 },               /* RS_V2 */
    { TO10, CO13, AF30 },               /* RS_V3 */
    { TO10, CO14, AF10 },               /* RS_V4 */
    { TO30, CI15, state_00_TO30_CI15 }, /* not RS_L3 and not RS_R1_L2 */
    { TO30, CI17, AF20 },               /* RES_L3 */
    { TO30, CI21, AF40 },               /* RS_R1_R2 */
    { TO30, CI16, AF40 },               /* RS_V1_SCR */
    { TO50, CI33, state_00_TO50_CI33 }, /* not RS_L3 and not RS_L1 and not RS_R1_L2 */
    { TI06, CU07, 0 },
    { END_OF_STATUS, 0, 0 }
};

/* Forcing */
/* ------- */

static uint8_t AF52_not_RO00(struct ge *ge) { return AF52(ge) && not_RO00(ge); }
static uint8_t AF52_not_RO01(struct ge *ge) { return AF52(ge) && not_RO01(ge); }
static uint8_t AF52_not_RO02(struct ge *ge) { return AF52(ge) && not_RO02(ge); }
static uint8_t AF52_not_RO03(struct ge *ge) { return AF52(ge) && not_RO03(ge); }
static uint8_t AF52_not_RO04(struct ge *ge) { return AF52(ge) && not_RO04(ge); }
static uint8_t AF52_not_RO05(struct ge *ge) { return AF52(ge) && not_RO05(ge); }
static uint8_t AF52_not_RO06(struct ge *ge) { return AF52(ge) && not_RO06(ge); }
static uint8_t AF52_not_RO07(struct ge *ge) { return AF52(ge) && not_RO07(ge); }

static const struct msl_timing_chart state_08[] = {
    { TO10, CO11, AF41, EC69A0 }, /* fo. 18 */
    { TO10, CO11, AF51 },
    { TO10, CO41, 0 },
    { TO25, CO30, AF51 },
    { TO25, CO31, AF41 },
    { TO30, CI20, 0 },
    { TO40, CO01, AF41, EC69A0 },
    { TO40, CO01, AF51 },
    { TO50, CO48, AF52 },
    /* NO -> BO */
    { TO50, CI33, AF41 },
    { TO50, CI33, AF43 },
    { TO64, CO49, AF52_not_RO07 },
    { TO70, CI62, AF51, EC70A0 },
    { TO70, CI67, AF51 }, /* fo. 19 */
    { TI05, CI04, AF10 },
    { TI05, CI02, AF50 },
    { TI05, CI05, AF21 },
    { TI05, CI05, AF51 },
    { TI05, CI01, AF31 },
    { TI05, CI00, AF42 },
    { TI05, CI08, AF53 },
    { TI06, CI07, AF20 },
    { TI06, CI03, AF30 },
    { TI06, CI06, AF40 },
    { TI06, CI09, AF40 },
    { TI06, CI70, AF52 },
    { TI06, CI71, AF52 },
    { TI06, CI72, AF52 },
    { TI06, CI73, AF52 }, /* fo. 20 */
    { TI06, CI74, AF52 },
    { TI06, CI75, AF52 },
    { TI06, CI76, AF52 },
    { TI06, CI80, AF52_not_RO00 },
    { TI06, CI81, AF52_not_RO01 },
    { TI06, CI82, AF52_not_RO02 },
    { TI06, CI83, AF52_not_RO03 },
    { TI06, CI84, AF52_not_RO04 },
    { TI06, CI85, AF52_not_RO05 },
    { TI06, CI86, AF52_not_RO06 },
    { TI06, CU00, 0 },
    { TI06, CU01, 0, DI57B0 },
    { TI06, CU02, 0 },
    { TI06, CU03, 0, DI57B0 },
    { TI06, CU04, 0, DI57B0 },
    { TI06, CU05, 0 },
    { TI06, CU06, 0 },
    { TI06, CU07, 0, DI57A0 },
    { TI06, CU10, not_RO00 }, /* fo. 21 */
    { TI06, CU11, not_RO01 },
    { TI06, CU12, not_RO02 },
    { TI06, CU13, not_RO03 },
    { TI06, CU14, not_RO04 },
    { TI06, CU15, not_RO05 },
    { TI06, CU16, not_RO06 },
    { TI06, CU17, not_RO07 },
    { END_OF_STATUS, 0, 0 }
};

/* PER - PERI */
/* ---------- */

static uint8_t state_c8_TI06_CI85(struct ge *ge) {
    /* !(selected_connector_busy || selected_channel_busy) */
    return !(PUB01(ge) || DU92(ge));
}

static const struct msl_timing_chart state_c8[] = {
    { TO10, CO12, 0, DI97A0 },
    { TO10, CO41, 0, DI97A0},
    { TO25, CO30, not_AINI, ED70A0},
    { TO40, CO02, 0, DI97A0 },
    { TO70, CI62, 0, DI25A0 },
    { TO70, CI67, 0, DI25A0 },
    { TI06, CI06, 0 },
    { TI06, CI75, 0, DI25A0 },
    { TI06, CI84, 0, DI25A0 },
    { TI06, CI85, state_c8_TI06_CI85 },
    { TI06, CU04, 0 },
    { END_OF_STATUS, 0, 0 },
};

static uint8_t state_d8_TO19_CE02(struct ge *ge) {
    return !BIT(ge->ffFA, 5) && !BIT(ge->ffFA, 4);
}

static uint8_t state_d8_TO40_CO00(struct ge *ge) {
    return BIT(ge->ffFA, 5) && !DU93(ge);
}

static const struct msl_timing_chart state_d8[] = {
    { TO10, CO10, 0 },
    { TO10, CO40, 0, DI21A0 }, // NOTE: both commands have same conditions ?!
    { TO10, CO41, 0, DI21A0 }, // NOTE: it's like this in timing charts.
    { TO19, CE02, state_d8_TO19_CE02 },
    { TO30, CI15, 0, DI21A0 },
    { TO40, CO00, state_d8_TO40_CO00 },
    { TO50, CI33, 0, DI21A0 },
    { TO50 /* PIPO */, CE01, 0 },
    { TI06, CU00, 0, DI93A0},
    { END_OF_STATUS, 0, 0 },
};

static uint8_t state_d9_TO40_CO00(struct ge *ge) {
    return BIT(ge->ffFA, 5) && !DU93(ge);
}

static const struct msl_timing_chart state_d9[] = {
    { TO10, CO10, 0 },
    { TO10, CO40, 0, DI21A0 },
    { TO10, CO41, 0, DI21A0},
    { TO30, CI15, 0, DI21A0 },
    { TO40, CO00, state_d9_TO40_CO00 },
    { TO50, CI33, 0, DI21A0},
    { TI06, CU00, 0, DI93A0},
    { TI06, CU01, 0, DI94A0},
    { TI06, CU10, 0 },
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_da[] = {
    { TO10, CO10, 0 },
    { TO10, CO40, 0, DI21A0 },
    { TO10, CO41, 0, DI21A0 },
    { TO30, CI15, 0, DI21A0 },
    { TO40, CO00, state_d9_TO40_CO00 },
    { TO50, CI33, 0, DI21A0 },
    { TI06, CU00, 0, DI93A0 },
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_db[] = {
    { TO10, CO10, 0 },
    { TO10, CO40, 0, DI21A0 },
    { TO10, CO41, 0, DI21A0 },
    { TO30, CI15, 0, DI21A0 },
    { TO40, CO00, state_d9_TO40_CO00 },
    { TO50, CI33, 0, DI21A0 },
    { TI06, CI74, 0, DI91A0 },
    { TI06, CU00, 0, DI93A0 },
    { TI06, CU10, 0 },
    { TI06, CU01, 0, DI94A0 },
    { TI06, CU11, 0, DI95A0 },
    { TI06, CU12, 0 },
    { TI06, CU02, state_d8_TO19_CE02 },
    { END_OF_STATUS, 0, 0 },
};

/* needs to be 1 for the per preliminary phase to continue */
SIG(PCOV) { return 1; }

static uint8_t state_dc_TI06_CI70(struct ge *ge) {
    return !PCOV(ge) && !BIT(ge->rL2, 2) && !AITE(ge);
}

static uint8_t state_dc_TI06_CU20(struct ge *ge) {
    return BIT(ge->rL2, 0) && !BIT(ge->ffFA, 5);
}

static const struct msl_timing_chart state_dc[] = {
    { TO10, CO13, 0 },
    { TO30, CI19, 0 },
    { TO30, CO90, 0 },
    { TO40, CO01, 0 },
    { TO50, CI32, 0, DI22A0 },
    { TI06, CI70, state_dc_TI06_CI70 },
    { TI06, CU14, 0, DI22A0 },
    { TI06, CU20, state_dc_TI06_CU20 },
    { END_OF_STATUS, 0, 0 },
};

static uint8_t state_cc_TO50_CE00(struct ge *ge) {
    return !ge->PUC3;
}


static uint8_t state_cc_TI06_CU13(struct ge *ge) {
    return (PCOV(ge) && DU96(ge) && !DU95(ge)) || BIT(ge->ffFA, 0);
}

static uint8_t state_cc_TI06_CU05(struct ge *ge) {
    return BIT(ge->ffFA, 5) || (!BIT(ge->ffFA, 0) && DU96(ge));
}

static uint8_t state_cc_TI06_CU04(struct ge *ge) {
    return !BIT(ge->ffFA, 5) && BIT(ge->ffFA, 0);
}

static uint8_t state_cc_TI06_CI75(struct ge *ge) {
    return (PCOV(ge) && DU96(ge) && !DU95(ge)) || BIT(ge->ffFA, 0);
}

static uint8_t state_cc_TI06_CU01(struct ge *ge) {
    return BIT(ge->ffFA, 5) || !BIT(ge->ffFA, 9) ;
}

static const struct msl_timing_chart state_cc[] = {
    { TO10, CO12, 0, DI97A0 },
    { TO10, CO41, 0, DI97A0 },
    { TO25, CO30, not_AINI, ED70A0 },
    { TO30, CI19, 0, DI24A0 },
    { TO30, CO96, 0 },
    { TO40, CO02, DI97A0 },
    { TO50, CI32, AINI, 0 },
    { TO50, CE01, 0 },
    { TO50, CE00, state_cc_TO50_CE00 },
    /* TODO: CI75 seems conditioned also on the type of peri operation (e.g. TPER/SPER ecc) */
    { TI06, CI75, state_cc_TI06_CI75 },
    { TI06, CU13, state_cc_TI06_CU13 },
    { TI06, CU12, 0 },
    { TI06, CU05, state_cc_TI06_CU05 },
    { TI06, CU04, state_cc_TI06_CU04 },
    { TI06, CU01, state_cc_TI06_CU01 },
    { END_OF_STATUS, 0, 0 },
};

/* TPER - CPER */
/* ----------- */

SIG(L207)     { return BIT(ge->rL2, 7); }
SIG(not_L207) { return !L207(ge); }

static uint8_t state_ca_TO80_CE18(struct ge *ge) {
    return L207(ge) && (!BIT(ge->rL1, 7) || BIT(ge->rL1, 6) || !BIT(ge->rL1, 0));
}

static const struct msl_timing_chart state_ca[] = {
    { TO19, CE08, L207 },
    { TO65, CE03, 0 },
    { TO80, CE18, state_ca_TO80_CE18 },
    { TI06, CU16, not_L207 },
    { TI06, CU05, 0 },
    { TI06, CU13, L207 },
    { TI06, CU11, not_L207 },
    { TI10, CE10, L207 },
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_a8[] = {
    { TO10, CO12, 0, DI97A0 },
    { TO10, CO41, 0, DI97A0 },
    { TO25, CO30, not_AINI, ED70A0 },
    { TO30, CI19, 0, DI29A0 },
    { TO40, CO02, 0, DI97A0 },
    { TO70, CI60, 0, DI86A0 },
    { TO70, CI65, 0, DI86A0 },
    { TI05, CI05, 0, DI87A0 },
    { TI06, CU00, 0, DI93A0 },
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_a9[] = {
    { TO10, CO12, 0, DI97A0 },
    { TO10, CO41, 0, DI97A0 },
    { TO25, CO30, not_AINI, ED70A0 },
    { TO30, CI19, 0, DI29A0 },
    { TO30, CI15, 0, DI84A0 },
    { TO30, CO97, AINI },
    { TO40, CO02, 0, DI97A0 },
    { TO50, CI32, AINI, ED75A0 },
    { TO70, CI62, 0, ED79A0 },
    { TO70, CI67, 0, ED79A0 },
    { TI05, CI05, 0, DI87A0 },
    { TI06, CI07, PC031 },
    { TI06, CU00, 0, DI93A0},
    { TI06, CU10, 0 },
    { TI06, CU01, DI94A0 },
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_aa[] = {
    { TO10, CO12, 0, DI97A0 },
    { TO10, CO41, 0, DI97A0 },
    { TO25, CO30, not_AINI, ED70A0 },
    { TO30, CI19, 0, DI29A0 },
    { TO40, CO02, 0, DI97A0 },
    { TO70, CI60, 0, DI86A0 },
    { TO70, CI65, 0, DI86A0 },
    { TI05, CI01, 0 },
    { TI06, CU00, 0, DI93A0},
    { END_OF_STATUS, 0, 0 },
};

static uint8_t state_ab_TO70_CI62(struct ge *ge) { return !(PC111(ge) && PC211(ge)); }

static uint8_t state_ab_TO80_CE18(struct ge *ge) {
    /* this equation is different in the timing charts documentation (fo. 58),
     * and in the cpu PDS documentation (fo. 218) it seems the PDS is the right
     * one. */
    return !(PC121(ge) || PC111(ge) || PC211(ge));
}

static uint8_t state_ab_TI05_CI04(struct ge *ge) { return BIT(ge->rL2, 0); }
static uint8_t state_ab_TI05_CI03(struct ge *ge) { return PC031(ge); }

static const struct msl_timing_chart state_ab[] = {
    { TO10, CO12, 0, DI97A0 },
    { TO10, CO41, 0, DI97A0 },
    { TO19, CE07, 0 }, /* no clock in documentation! */
    { TO19, CE08, 0 },
    { TO25, CO30, not_AINI, ED70A0 },
    { TO30, CI19, 0, DI29A0 },
    { TO30, CI11, 0 },
    { TO40, CO02, 0, DI97A0 },
    { TO70, CI62, state_ab_TO70_CI62, ED79A0 },
    { TO70, CI67, state_ab_TO70_CI62, ED79A0 },
    { TO80, CE18, state_ab_TO80_CE18 },
    { TI05, CI01, 0 },
    { TI05, CI04, state_ab_TI05_CI04 },
    { TI05, CI03, state_ab_TI05_CI03 },
    { TI06, CU00, 0, DI93A0},
    { TI06, CU10, 0 },
    { TI06, CU01, 0, DI94A0},
    { TI06, CU11, 0, DI95A0},
    { TI06, CU04, 0, DI85A0},
    { TI10, CE10, 0 },
    { END_OF_STATUS, 0, 0 },
};


static uint8_t state_b8_TI06_CI72(struct ge *ge) { return BIT(ge->rL2, 0) && BIT(ge->rL2, 3); }
static uint8_t DU97_or_DU98(struct ge *ge) { return DU97(ge) || DU98(ge); }
static uint8_t state_b8_TI10_CE09(struct ge *ge) { return !BIT(ge->ffFA, 0) && !BIT(ge->rL2, 3) && !ge->RACI; }

static const struct msl_timing_chart state_b8[] = {
    { TI06, CI72, state_b8_TI06_CI72 },
    { TI06, CI70, 0 },
    { TI06, CU01, DU97_or_DU98 },
    { TI06, CU13, DU97 },
    { TI06, CU14, DU97_or_DU98 },
    { TI06, CU06, DU97_or_DU98 },
    { TI10, CE09, state_b8_TI10_CE09 },
    { END_OF_STATUS, 0, 0 },
};

SIG(L204) { return BIT(ge->rL2, 4); }
SIG(L205) { return BIT(ge->rL2, 5); }
SIG(L206) { return BIT(ge->rL2, 6); }


SIG(FA01) { return BIT(ge->ffFA, 1); }
SIG(not_FA01) { return !FA01(ge); }

static const struct msl_timing_chart state_b1[] = {
    { TO10, CO11 },
    { TO10, CO41 },
    { TO10, CO40, L205 },
    { TO25, CO31, FA01 },
    { TO30, CI15, not_FA01 },
    { TO30, CI12, FA01 },
    { TO30, CI41 },
    { TO40, CO01, FA01 },
    { TO50, CI33, FA01 },
    { TO80, CE18 },
    { TI05, CI05, not_FA01 },
    { TI06, CI71 },
    { TI06, CI81, FA01 },
    { TI06, CU03 },
    { TI06, CU10 },
    { END_OF_STATUS },
};


SIG(RIG1) { return ge->RIG1; }
SIG(RIG3) { return ge->RIG3; }

SIG(RENIA) { return 1; } // TODO
SIG(RILIA) { return 1; } // TODO

SIG(RIG1A) { return !ge->RIG1; }
SIG(RIVE1) { return !(RIG1A(ge) && RENIA(ge) && RILIA(ge)); }
/** End of transfer for channel 1 */
SIG(RIVE) { return RIVE1(ge); }

SIG(not_L206) { return !L206(ge); }

static uint8_t state_b9_TO25_CO31(struct ge *ge) { return !BIT(ge->ffFA, 1) && !BIT(ge->rL2, 6); }
static uint8_t state_b9_TO30_CI12(struct ge *ge) { return !L204(ge) && !L206(ge); }

/* the original timingchart and the flow chart disagree, RIG1 is spelt "AIGI" in
 * the timings, but RIG1 in the flow, also timings use L206 and flow use L205... */
static uint8_t state_b9_TO40_CO01(struct ge *ge) { return (L204(ge) || (!BIT(ge->ffFA, 1) && RIG1(ge))) && !L206(ge); }

static uint8_t state_b9_TO70_CI67(struct ge *ge) { return BIT(ge->ffFA, 1) && !L206(ge); }
static uint8_t state_b9_TO70_CI66(struct ge *ge) { return !BIT(ge->ffFA, 1) && !L204(ge) && !L206(ge); }
static uint8_t state_b9_TO80_CE05(struct ge *ge) { return !PC121(ge) && !L206(ge); }
static uint8_t state_b9_TI06_CU13(struct ge *ge) { return !L204(ge) && !L206(ge); }
static uint8_t state_b9_TI10_CE09(struct ge *ge) { return !RIVE(ge) && !PC121(ge) && !L206(ge); }

static const struct msl_timing_chart state_b9[] = {
    { TO10, CO11 },
    { TO10, CO41 },
    { TO10, CO40, L205 },
    { TO25, CO31, state_b9_TO25_CO31 },
    { TO30, CI15, L204 },
    { TO30, CI41, L204 },
    { TO30, CI40, L204 },
    { TO30, CI12, state_b9_TO30_CI12 },
    { TO40, CO01, state_b9_TO40_CO01 },
    { TO50, CI34, not_L206 },
    { TO70, CI67, state_b9_TO70_CI67 },
    { TO70, CI66, state_b9_TO70_CI66 },
    { TO80, CE18, L204 },
    { TO80, CE05, state_b9_TO80_CE05 },
    { TO65, CE11, not_L206 },
    { TI05, CI05, L204 },
    { TI05, CI02, not_L206 },
    { TI06, CU13, state_b9_TI06_CU13 },
    { TI10, CE09, state_b9_TI10_CE09 },
    { END_OF_STATUS },
};

static uint8_t L206_or_PC01(struct ge *ge) { return BIT(ge->rL2, 7) || PC011(ge); }

static const struct msl_timing_chart state_ea[] = {
    { TO10, CO18, 0 },
    { TO10, CO97, 0, DI11A0 },
    { TO10, CO96, 0, DI11A0 },
    { TO10, CO95, 0, DI11A0 },
    { TO10, CO94, 0  },
    { TO10, CO93, 0, DI11A0 },
    { TO10, CO92, 0, DI11A0 },
    { TO10, CO91, 0 },
    { TO10, CO90, 0, DI11A0 },
    { TO10, CO40, 0, DI11A0 },
    { TO10, CO41, 0, DI11A0 },
    { TO25, CO31, L206_or_PC01 },
    { TO30, CI11, 0 },
    { TO40, CO02, 0, DI11A0 },
    { TO50, CI33, 0, DI83A0 },
    { TI06, CU00, 0 },
    { END_OF_STATUS, 0, 0 },
};


static uint8_t state_eb_TI06_CI75(struct ge *ge) {
    return ((RIG3(ge) && BIT(ge->rL2, 7)) ||
            (RIG1(ge) && PC011(ge) && !ge->RACI));
}

static uint8_t state_eb_TI06_CE19(struct ge *ge) { return 0; }

static const struct msl_timing_chart state_eb[] = {
    { TO10, CO12, 0, DA25A0 },
    { TO10, CO97, 0, DI11A0 },
    { TO10, CO96, 0, DI11A0 },
    { TO10, CO95, 0, DI11A0 },
    { TO10, CO94, 0 },
    { TO10, CO93, 0, DI11A0 },
    { TO10, CO92, 0, DI11A0 },
    { TO10, CO91, 0 },
    { TO10, CO90, 0, DI11A0 },
    { TO10, CO04, 0, DI11A0 },
    { TO10, CO41, 0, DI11A0 },
    { TO25, CO31, L206_or_PC01 },
    { TO30, CI11, 0 },
    { TO40, CO02, 0, DI11A0 },
    { TO50, CI32, 0, DI82A0 },
    { TO50, CE06, L207 },
    { TI06, CI75, state_eb_TI06_CI75, ED91A0 },
    { TI06, CE19, state_eb_TI06_CE19 },
    { TI06, CU00, 0 },
    { TI06, CU13, 0, DI82A0 },
    { END_OF_STATUS, 0, 0 },
};
