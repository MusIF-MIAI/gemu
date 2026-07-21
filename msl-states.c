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

/* The states below transcribe the micro-sequencer flow-chart foldouts
 * (drawing 14023130, CPU[7] = Volume 7 schematics). docs/flowchart-sheets.md
 * maps each sheet -> the state(s) here and records the per-state fidelity audit
 * (which states are faithful per-clock transcriptions vs. functionally-correct
 * hybrids that call the alu_* helpers). Sheet citations are noted on each chart. */

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

/* Address modify flag (bit 15 of the operand address = L2 bit 7 once the
 * addr-hi byte has been read into L2). An absolute address exits operand
 * fetch to beta; a modified one detours through the indexing micro-cycle.
 * (SIG(L207)/SIG(not_L207) are defined later in this file for the TPER/CPER
 * path; this mirrors them but is visible to the alpha-phase states below.) */
static uint8_t addr_absolute(struct ge *ge) { return !BIT(ge->rL2, 7); }

/* Change-register modifier bits of the operand field (L2 bits 4-6 = address
 * bits 12-14). Gate the NO-knot forcings CO91/CO92/CO93 that build the
 * change-register byte address in the indexing micro-cycle (CPU[7] p64,
 * equations EG63A0/EG62A0/EG61A0). Also used by the TPER/CPER cluster. */
SIG(L204) { return BIT(ge->rL2, 4); }
SIG(L205) { return BIT(ge->rL2, 5); }
SIG(L206) { return BIT(ge->rL2, 6); }

/* Register-number bits of the register-instruction format (L1 bits 4-6),
 * gating the change-register address build CO91/CO92/CO93 in the LA and
 * LR-family beta sheets (cp07 fo.36/37, equations EG60A0/EG59A0/EG58A0). */
static uint8_t LI04(struct ge *ge) { return BIT(ge->rL1, 4); }
static uint8_t LI05(struct ge *ge) { return BIT(ge->rL1, 5); }
static uint8_t LI06(struct ge *ge) { return BIT(ge->rL1, 6); }

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
    /* Reset FI01. Present in the per-clock timing table (14024137 fo.10/11,
     * CPU[7] p61: "TI06 CI81 CI81A0 = EC73A0") but was missing here. FI01 is set
     * only by forcing (state 08) / the b1 peripheral path, so the instruction
     * fetch clearing it is a no-op for normal CPU flow (verified: deck + tests
     * stay green). EC73A0 isn't transcribed; like the sibling CI80/CI83 resets
     * (table cond DI18B0 = the in-state decode) it is treated as unconditional. */
    { TI06, CI81, 0 },
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

// SS (Storage-to-Storage) data ops: opcode list from opcodes.h.
// These are 6-byte instructions (opcode, LL, A1hi, A1lo, A2hi, A2lo).
// Operands are loaded by the E4->E6->E5->E7 micro-loop:
//   E4 -> E6 (loads V1 from A1), E5 -> E7 (loads V2 from A2); a modified
//   address detours through the indexing micro-cycle ED|EC -> EF|EE. E7 then
//   exits to beta (64|65 family) via its documented CU rows, where the op
//   executes (EXEC_SS at TO65, like the other EXEC_* hybrids).
// V1 = destination address, V2 = source address, L1 = length byte.
static uint8_t is_ss_data_op(struct ge *ge) {
    switch (ge->rFO) {
        case MVC_OPCODE:
        case NC_OPCODE:
        case CMC_OPCODE:
        case OC_OPCODE:
        case XC_OPCODE:
        case UPK_OPCODE:
        case PK_OPCODE:
        case TL_OPCODE:
        case EDT_OPCODE:
        case MVP_OPCODE:
        case CMP_OPCODE:
        case AP_OPCODE:
        case SP_OPCODE:
        case MP_OPCODE:
        case DP_OPCODE:
        case PKS_OPCODE:
        case UPKS_OPCODE:
        case AB_OPCODE:
        case SB_OPCODE:
        case AD_OPCODE:
        case SD_OPCODE:
        case MVQ_OPCODE:
        case CMQ_OPCODE:
        case SR_OPCODE:
        case SL_OPCODE:
            return 1;
        default:
            return 0;
    }
}

// to state E6

/* E4 reads the FIRST operand's high byte (A1hi), exactly as E5 reads A2hi; CI60
 * (ni4 = top quartet = modifier+bit15) fires on /R007 (= not_RO07), matching the
 * E5 box (CPU[7] flow chart 14023130). The original `return 0` stub never loaded
 * it, so for absolute addresses the SS destination lost its high quartet and
 * writes fell into segment 0; restoring the /R007 gating fixes that. (For the
 * modified case the hardware zeroes V4 and indexes in ED|EC|EF|EE — transcribed
 * in reference_operand_fetch_flowchart, to be implemented cycle-accurately.) */
static uint8_t state_E4_TO70_CI60(struct ge *ge) { return not_RO07(ge); }
static uint8_t RO07(struct ge *ge) { return BIT(ge->rRO, 7); }

static const struct msl_timing_chart state_E4[] = {
    { TO10, CO10, 0, DI60A0 },
    { TO10, CO41, 0, DI60A0 },
    { TO25, CO30, 0, DI12A0 },
    { TO40, CO00, 0, DI60A0 },
    { TO70, CI67, 0, DI12A0 },
    { TO70, CI62, 0, DI12A0 },
    { TO70, CI65, 0, DI19A0 },
    { TO70, CI60, state_E4_TO70_CI60 },
    { TO70, NI4_ZERO, RO07 },            /* 0->V_4 [R007]: strip modifier+flag */
    { TI05, CI02, 0 },
    { TI06, CI06, 0 },
    { TI06, CU01, 0, DI60A0 },
    { END_OF_STATUS, 0, 0 }
};

// to state E5    if !L207 & (FO07 & FO06)
//          ED    if L207   (modified-address indexing cycle; the unconditional
//                           CU00 leaves bit 0 SET = first operand <SA00>)
//          64+65 if !L207 & (!FO07 | !FO06)

/* CI38 "set AVER auto" in E6/E7: gate DE51A0 = DO011 & DI201 (cp06 ch.261
 * gate 8, read on the sheet: DE51A = NAND(DO011, DI201)) — fires only for the
 * jump-class function codes (DO011 = FO06 & !FO03 & !FO07). Previously E6
 * returned 0 and E7 returned 1, both guesses ("DO01?"). */
static uint8_t state_E6_TO80_CI38(struct ge *ge) { return DO011(ge); }

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
    /* EC56A0 = DI201 & L207: enter the indexing micro-cycle on a modified
     * address (timing table CPU[7] p63 prints the same gate for E7). */
    { TI06, CU03, 0, EC56A0 },

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
    { TO70, CI60, not_RO07 },            /* ni4 [/R007]: top quartet from RO only
                                          * for absolute (CPU[7] E5 box) */
    { TO70, NI4_ZERO, RO07 },            /* 0->V_4 [R007]: strip modifier+flag */
    { TI05, CI02, 0 },
    { TI06, CI06, 0 },
    { TI06, CU01, 0, DI60A0 },
    { END_OF_STATUS, 0, 0 }
};

// to state 64+65 if !L207   (beta: the SS op executes there)
//          ED+EC if L207    (the CU10 = DI64A0 reset of bit 0 lands on EC =
//                            second operand, the <SA00> diamond cleared)

static uint8_t state_E7_TO80_CI38(struct ge *ge) { return DO011(ge); /* DE51A0, see E6 */ }

/* Timing table CPU[7] p63 (state 1110 0111, DA-FROM E5), verified row-by-row:
 *   TO10 CO10 = CB19A0 (DI60A0)   PO->NO
 *   TO10 CO41 = CB14A0 (DI60A0)   count from 00
 *   TO25 CO30 = CB07A0 (DI12A0)   MEM->RO
 *   TO30 CI12 = DI20A0            V2->NO
 *   TO40 CO00 = CB00A0 (DI60A0)   NI->PO
 *   TO50 (hardware)               NO->BO
 *   TO70 CI67 = CD14A0 (DI12A0)   RO1->NI1
 *   TO70 CI62 = CD14A0 (DI12A0)   RO2->NI2
 *   TO80 CI38 = DE51A0 {DO01}     set AVER auto
 *   TI05 CI02 = DI60A0            NI->V2
 *   TI06 CU00 = CM00A0 (DI20A0)   Set S000
 *   TI06 CU03 = CM05A0 (EC56A0)   Set S003   (modified address)
 *   TI06 CU10 = DI64A0            Reset S000 (E7-only: operand 2 -> EC)
 *   TI06 CU17 = EC57A0 {/L207}    Reset S007 (absolute -> beta)
 * Exit box: 64+65 (0110 0100) {/L207} | ED+EC (1110 1100) {L207}.
 * Reaching those exact exit codes needs a bit-1 reset the printed rows lack;
 * a CU11 is added below — the same manual CU10/CU11 ambiguity already noted
 * in state_E6. The exit-box state codes are unambiguous. */
static const struct msl_timing_chart state_E7[] = {
    { TO10, CO10, 0, DI60A0 },
    { TO10, CO41, 0, DI60A0 },
    { TO25, CO30, 0, DI12A0 },
    { TO30, CI12, 0, DI20A0 },
    { TO40, CO00, 0, DI60A0 },
    { TO70, CI67, 0, DI12A0 },
    { TO70, CI62, 0, DI12A0 },
    { TO80, CI38, state_E7_TO80_CI38 },
    { TI05, CI02, 0, DI60A0 },
    { TI06, CU00, 0, DI20A0 },
    { TI06, CU03, 0, EC56A0 },
    { TI06, CU10, 0, DI64A0 },
    { TI06, CU11, 0, DI64A0 },
    { TI06, CU17, addr_absolute },   /* CU17A0 = EC57A0 {/L207} */
    { END_OF_STATUS, 0, 0 }
};

/* Modified-Address Indexing Micro-Cycle */
/* ------------------------------------- */

/* Entered from E6 (operand 1 -> ED, SA bit 0 set) or E7 (operand 2 -> EC,
 * SA bit 0 cleared by CU10 = DI64A0); SA bit 0 IS the flow chart's <SA00>
 * first-vs-second operand diamond. Per-clock transcription of the timing
 * tables CPU[7] p64 ("FASE ALFA ED-EC / EF-EE", dwg 14024137₀, fo.15-16):
 *
 *   ED|EC — CO18 + CO97..CO90 force the NO knot to the change-register LOW
 *           byte address 1111 nnn 1 = 241+2N (N = modifier, L2 bits 4-6 via
 *           the {L206}/{L205}/{L204} gates; CO90's DI65A0 gate sets the odd
 *           byte only in this state); MEM->RO reads it; V2->NO / NO->BO put
 *           the displacement on the UA's other input; CO49 resets the carry
 *           FFs; CI69 latches UA = BO.low + RO into NI21; CI02 stores NI
 *           (V2 high byte unchanged through the counting network) to V2;
 *           CU01 -> EF|EE.
 *   EF|EE — same address build WITHOUT CO90 (= 240+2N, the HIGH byte);
 *           CI68 latches UA = BO.high + RO + carry into NI43; CI02 stores
 *           the resolved EA to V2; CI01 (DI67A0 {SA00}) copies it to V1 for
 *           the first operand. Routing: CU01/CU11 (net Reset S001) + CU13
 *           always; CU17 = DE52A0 {/FO07+/FO06+/SA00} exits to beta except
 *           for the first operand of a two-address op, which goes to E5 to
 *           fetch operand 2. Exit box: 64+65 {/(SA00·FO07·FO06)} |
 *           E5 {SA00·FO07·FO06}.
 */
static uint8_t state_EF_EE_TI06_CU17(struct ge *ge) {
    return !(BIT(ge->rFO, 7) && BIT(ge->rFO, 6) && BIT(ge->rSA, 0));
}

static const struct msl_timing_chart state_ED_EC[] = {
    { TO10, CO18, 0, DI13A0 },          /* forcing in NO21 */
    { TO10, CO97, 0, DI13A0 },          /* 1->NO07 */
    { TO10, CO96, 0, DI13A0 },          /* 1->NO06 */
    { TO10, CO95, 0, DI13A0 },          /* 1->NO05 */
    { TO10, CO94, 0, DI13A0 },          /* 1->NO04 */
    { TO10, CO93, L206, DI13A0 },       /* EG61A0 {L206}: N2->NO03 */
    { TO10, CO92, L205, DI13A0 },       /* EG62A0 {L205}: N1->NO02 */
    { TO10, CO91, L204, DI13A0 },       /* EG63A0 {L204}: N0->NO01 */
    { TO10, CO90, 0, DI65A0 },          /* 1->NO00: low (odd) cr byte */
    { TO25, CO30, 0, DI13A0 },          /* MEM->RO */
    { TO30, CI12, 0, DI13A0 },          /* V2->NO */
    { TO65, CO49, 0, DI65A0 },          /* reset URPE/URPU (carry) */
    { TO70, CI69, 0, DI65A0 },          /* UA->NI21: BO.low + RO */
    { TI05, CI02, 0, DI13A0 },          /* NI->V2 */
    { TI06, CU01, 0, DI13A0 },          /* Set S001 -> EF|EE */
    { END_OF_STATUS, 0, 0 }
};

static const struct msl_timing_chart state_EF_EE[] = {
    { TO10, CO18, 0, DI13A0 },          /* forcing in NO21 */
    { TO10, CO97, 0, DI13A0 },          /* 1->NO07 */
    { TO10, CO96, 0, DI13A0 },          /* 1->NO06 */
    { TO10, CO95, 0, DI13A0 },          /* 1->NO05 */
    { TO10, CO94, 0, DI13A0 },          /* 1->NO04 */
    { TO10, CO93, L206, DI13A0 },       /* EG61A0 {L206}: N2->NO03 */
    { TO10, CO92, L205, DI13A0 },       /* EG62A0 {L205}: N1->NO02 */
    { TO10, CO91, L204, DI13A0 },       /* EG63A0 {L204}: N0->NO01 */
    { TO25, CO30, 0, DI13A0 },          /* MEM->RO (high/even cr byte) */
    { TO30, CI12, 0, DI13A0 },          /* V2->NO */
    { TO70, CI68, 0, DI66A0 },          /* UA->NI43: BO.high + RO + carry */
    { TI05, CI02, 0, DI13A0 },          /* NI->V2: resolved EA */
    { TI05, CI01, 0, DI67A0 },          /* NI->V1 {SA00}: first operand only */
    { TI06, CU01, 0, DI13A0 },          /* Set S001 ... */
    { TI06, CU11, 0, DI66A0 },          /* ... net Reset S001 in EF|EE */
    { TI06, CU13, 0, DI66A0 },          /* Reset S003 */
    { TI06, CU17, state_EF_EE_TI06_CU17 }, /* DE52A0 {/FO07+/FO06+/SA00} */
    { END_OF_STATUS, 0, 0 }
};

/* Interruption + LPSR */
/* ------------------- */

/* Interruption + LPSR chain, per-clock (cp07 timing charts fo.22-31, dwg
 * 14024137, read from source at 600dpi):
 *
 *   E2/E3 --INTE--> F0 -> D2 -> D3 -> D0 -> D1 -> C2 -> C3 -> C0 -> C1 -> E2/E3
 *   LPSR (beta 64|65, fo.27) ------------------------^ (C2 header: "DA-FROM 64+65 D1")
 *
 * F0 forces 0x0300 into V1 (CI19 + C091/C090 -> NO43, latched into BO at the
 * TO50 relatch, stored by CI01). The D-states WRITE the old PSR at V1++:
 * D2 = status byte from the FA-gated forcings via NO43 (FA06->b0, FA05->b4,
 * FA04->b5), D3 = zero (its forcing rows have no mode command, and the NO
 * selection pulse has decayed by TO50 -> NO_UNDRIVEN), D0/D1 = PO high/low
 * (CI10 PO->NO at TO30, CI32/CI33 at TO50). The C-states READ the new PSR at
 * V1++ (0x0304 onward for the interrupt path; the LPSR operand address for
 * LPSR): C2 = status byte -> FI04/05/06 (set, then conditional reset on the
 * R0 bits — reset listed after set, as the hardware resolves it), C3 = skip
 * byte, C0/C1 = PO high/low assembled through NI (RO halves + the BO
 * passthrough of the counting network, CI00s at TI05).
 *
 * TI06 CU rows are ordered sets-before-resets so gemu's sequential dispatch
 * preserves the hardware "reset prevails" rule; the printed row order on the
 * sheets is data-equivalent. All exits are unconditional per the sheets. */

static uint8_t FA04(struct ge *ge) { return BIT(ge->ffFA, 4); }
static uint8_t FA05(struct ge *ge) { return BIT(ge->ffFA, 5); }
static uint8_t FA06(struct ge *ge) { return BIT(ge->ffFA, 6); }

static const struct msl_timing_chart state_F0[] = {          /* fo.22 */
    { TO30, CI19, 0 },              /* forcing in NO43 */
    { TO30, CO91, 0 },              /* 1 -> NO09 */
    { TO30, CO90, 0 },              /* 1 -> NO08: NO = 0x0300 */
    { TO50, NO_UNDRIVEN, 0 },       /* selection decayed: BO relatch reads
                                     * the pure forced 0x0300 */
    { TI05, CI01, 0 },              /* NI -> V1 (save address) */
    { TI06, INT_ACK, 0 },           /* gemu request handshake (see command) */
    { TI06, CU01, 0 },              /* Set S001 */
    { TI06, CU15, 0 },              /* Reset S005 -> D2 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_D2[] = {          /* fo.23 */
    { TO10, CO11, 0 },              /* V1 -> NO (address latch at TO20) */
    { TO10, CO41, 0 },              /* count from 00 */
    { TO25, CO31, 0 },              /* RO -> MEM (fires at TO65 on rRO) */
    { TO30, CI19, 0 },              /* forcing in NO43 */
    { TO30, CO90, FA06 },           /* status bit 0 */
    { TO30, CO94, FA05 },           /* status bit 4 */
    { TO30, CO95, FA04 },           /* status bit 5 */
    { TO40, CO01, 0 },              /* NI -> V1 : V1+1 */
    { TO50, NO_UNDRIVEN, 0 },       /* selection decayed by TO50 */
    { TO50, CI32, 0 },              /* NO43 -> RO: the status byte */
    { TI06, CU00, 0 },              /* -> D3 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_D3[] = {          /* fo.24 */
    { TO10, CO11, 0 },
    { TO10, CO41, 0 },
    { TO25, CO31, 0 },
    { TO30, CO90, FA06 },           /* forcings fire but no mode command: */
    { TO30, CO94, FA05 },           /*  they reach nothing (cf. D1, where   */
    { TO30, CO95, FA04 },           /*  they must not corrupt PO-low)      */
    { TO40, CO01, 0 },
    { TO50, NO_UNDRIVEN, 0 },       /* selection pulse decayed by TO50 */
    { TO50, CI33, 0 },              /* NO21 -> RO: zero byte */
    { TI06, CU00, 0 },              /* sets first... */
    { TI06, CU01, 0 },
    { TI06, CU10, 0 },              /* ...resets prevail -> D0 */
    { TI06, CU11, 0 },
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_D0[] = {          /* fo.25 */
    { TO10, CO11, 0 },
    { TO10, CO41, 0 },
    { TO25, CO31, 0 },
    { TO30, CO90, FA06 },           /* inert: no mode command */
    { TO30, CO94, FA05 },
    { TO30, CO95, FA04 },
    { TO30, CI10, 0 },              /* PO -> NO */
    { TO40, CO01, 0 },
    { TO50, CI32, 0 },              /* NO43 -> RO: PO high */
    { TI06, CU00, 0 },              /* -> D1 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_D1[] = {          /* fo.26 */
    { TO10, CO11, 0 },
    { TO10, CO41, 0 },
    { TO25, CO31, 0 },
    { TO30, CO90, FA06 },           /* inert */
    { TO30, CO94, FA05 },
    { TO30, CO95, FA04 },
    { TO30, CI10, 0 },
    { TO40, CO01, 0 },
    { TO50, CI33, 0 },              /* NO21 -> RO: PO low */
    { TI06, CU00, 0 },              /* sets first */
    { TI06, CU01, 0 },
    { TI06, CU10, 0 },
    { TI06, CU14, 0 },              /* -> C2 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_C2[] = {          /* fo.28 */
    { TO10, CO11, 0 },
    { TO10, CO41, 0 },
    { TO25, CO30, 0 },              /* MEM -> RO: new status byte */
    { TO30, CI19, 0 },              /* forcing in NO43 (as printed; harmless) */
    { TO30, CO90, FA06 },
    { TO30, CO94, FA05 },
    { TO30, CO95, FA04 },
    { TO40, CO01, 0 },
    { TO70, CI60, 0 },              /* RO2 -> NI4 */
    { TO70, CI65, 0 },              /* RO1 -> NI3 */
    { TI06, CI74, 0 },              /* set FI04... */
    { TI06, CI75, 0 },              /* set FI05... */
    { TI06, CI76, 0 },              /* set FI06... */
    { TI06, CI84, not_RO05 },       /* ...reset FI04 {/R005} */
    { TI06, CI85, not_RO04 },       /* ...reset FI05 {/R004} */
    { TI06, CI86, not_RO00 },       /* ...reset FI06 {/R000} */
    { TI06, CU00, 0 },              /* -> C3 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_C3[] = {          /* fo.29 */
    { TO10, CO11, 0 },
    { TO10, CO41, 0 },
    { TO25, CO30, 0 },
    { TO30, CO90, FA06 },           /* inert */
    { TO30, CO94, FA05 },
    { TO30, CO95, FA04 },
    { TO40, CO01, 0 },
    { TO70, CI62, 0 },              /* RO2 -> NI2 */
    { TO70, CI67, 0 },              /* RO1 -> NI1 */
    { TI06, CU00, 0 },              /* sets first */
    { TI06, CU01, 0 },
    { TI06, CU10, 0 },
    { TI06, CU11, 0 },              /* -> C0 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_C0[] = {          /* fo.30 */
    { TO10, CO11, 0 },
    { TO10, CO41, 0 },
    { TO25, CO30, 0 },              /* MEM -> RO: new PO high */
    { TO30, CO90, FA06 },           /* inert */
    { TO30, CO94, FA05 },
    { TO30, CO95, FA04 },
    { TO30, CI10, 0 },              /* PO -> NO: low half passthrough */
    { TO40, CO01, 0 },
    { TO70, CI60, 0 },              /* RO2 -> NI4 */
    { TO70, CI65, 0 },              /* RO1 -> NI3 */
    { TI05, CI00s, 0 },             /* NI -> PO */
    { TI06, CU00, 0 },              /* -> C1 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_C1[] = {          /* fo.31 */
    { TO10, CO11, 0 },
    { TO10, CO41, 0 },
    { TO25, CO30, 0 },              /* MEM -> RO: new PO low */
    { TO30, CO90, FA06 },           /* inert */
    { TO30, CO94, FA05 },
    { TO30, CO95, FA04 },
    { TO30, CI10, 0 },              /* PO -> NO: high half passthrough */
    { TO40, CO01, 0 },
    { TO70, CI62, 0 },              /* RO2 -> NI2 */
    { TO70, CI67, 0 },              /* RO1 -> NI1 */
    { TI05, CI00s, 0 },             /* NI -> PO */
    { TI06, CU00, 0 },              /* sets first */
    { TI06, CU01, 0 },
    { TI06, CU05, 0 },
    { TI06, CU10, 0 },
    { TI06, CU14, 0 },              /* -> E2/E3 */
    { END_OF_STATUS, 0, 0 },
};

/* Beta Phase */
/* ---------- */

static uint8_t jc_js1_js2_jie(struct ge *ge) {
    return ((ge->rFO == JC_OPCODE) ||
            (ge->rFO == JU_OPCODE) ||
            (ge->rFO == JCC_OPCODE) ||
            (ge->rFO == JRT_OPCODE) ||
            (ge->rFO == JS1_OPCODE && (ge->rL1 & 0xFF) ==JS1_2NDCHAR) ||
            (ge->rFO == JS2_OPCODE && (ge->rL1 & 0xFF) ==JS2_2NDCHAR) ||
            (ge->rFO == JIE_OPCODE && (ge->rL1 & 0xFF) ==JIE_2NDCHAR));
}

static uint8_t lon_loll(struct ge *ge) {
    /* The 2nd char is an 8-bit field; mask L1 to its low byte (in real execution
     * L1's high byte carries leftover bits, unlike the console-forced case). */
    return ((ge->rFO == LON_OPCODE  && (ge->rL1 & 0xFF) == LON_2NDCHAR) ||
            (ge->rFO == LOLL_OPCODE && (ge->rL1 & 0xFF) == LOLL_OPCODE));
}

static uint8_t ins(struct ge *ge) {
    return ge->rFO == INS_OPCODE && (ge->rL1 & 0xFF) ==INS_2NDCHAR;
}

static uint8_t jie(struct ge *ge) {
    return ge->rFO == JIE_OPCODE && (ge->rL1 & 0xFF) ==JIE_2NDCHAR;
}

static uint8_t ens(struct ge *ge) {
    return ge->rFO == ENS_OPCODE && (ge->rL1 & 0xFF) ==ENS_2NDCHAR;
}

static uint8_t loff(struct ge *ge) {
    return ge->rFO == LOFF_OPCODE && (ge->rL1 & 0xFF) ==LOFF_2NDCHAR;
}

static uint8_t jc_js1_js2_jie_condition_verified(struct ge *ge) {
    return ge->AVER && jc_js1_js2_jie(ge);
}

static uint8_t nop(struct ge *ge) {
    return ge->rFO == NOP2_OPCODE;
}

static uint8_t is_jrt(struct ge *ge) {
    return ge->rFO == JRT_OPCODE;
}

/* PM/SI immediate-format data ops executed in beta via the ALU helpers.
 * After operand fetch these arrive in beta with V1=address, L1=immediate. */
static uint8_t is_mvi(struct ge *ge) { return ge->rFO == MVI_OPCODE; }
static uint8_t is_ni (struct ge *ge) { return ge->rFO == NI_OPCODE;  }
static uint8_t is_ci (struct ge *ge) { return ge->rFO == CI_OPCODE;  }
static uint8_t is_cmi(struct ge *ge) { return ge->rFO == CMI_OPCODE; }
static uint8_t is_xi (struct ge *ge) { return ge->rFO == XI_OPCODE;  }
static uint8_t is_tm (struct ge *ge) { return ge->rFO == TM_OPCODE;  }
static uint8_t pm_imm_exec(struct ge *ge) {
    return is_mvi(ge) || is_ni(ge) || is_ci(ge) || is_cmi(ge) || is_xi(ge) || is_tm(ge);
}

/* PM register ops (change registers, memory-mapped at 240+N*2): arrive in
 * beta with V1=I1 address, L1=register-code aux char. */
static uint8_t is_lr (struct ge *ge) { return ge->rFO == LR_OPCODE;  }
static uint8_t is_mvc(struct ge *ge) { return ge->rFO == MVC_OPCODE; }
static uint8_t is_cmc(struct ge *ge) { return ge->rFO == CMC_OPCODE; }
static uint8_t is_xc (struct ge *ge) { return ge->rFO == XC_OPCODE;  }
static uint8_t is_xoc_nc(struct ge *ge) {
    return ge->rFO == XC_OPCODE || ge->rFO == OC_OPCODE ||
           ge->rFO == NC_OPCODE;
}
static uint8_t is_oc_or_nc(struct ge *ge) {
    return ge->rFO == OC_OPCODE || ge->rFO == NC_OPCODE;
}
static uint8_t is_xc_or_oc(struct ge *ge) {
    return ge->rFO == XC_OPCODE || ge->rFO == OC_OPCODE;
}
static uint8_t not_FA03(struct ge *ge) { return !BIT(ge->ffFA, 3); }
/* SS byte-loop terminal count: L1 low byte underflowed to all ones — the
 * same convention as the channel length (RL1U1): the loop runs L1+1 times. */
static uint8_t L1_21_ones(struct ge *ge) { return (ge->rL1 & 0xff) == 0xff; }
static uint8_t not_str(struct ge *ge) { return ge->rFO != STR_OPCODE; }
static uint8_t not_cmr(struct ge *ge) { return ge->rFO != CMR_OPCODE; }
static uint8_t is_smr_or_cmr(struct ge *ge) {
    return ge->rFO == SMR_OPCODE || ge->rFO == CMR_OPCODE;
}
/* Pass 2 of the executive byte loop = the state's own X bit (62/52/42). */
static uint8_t SA01_pass2(struct ge *ge) { return BIT(ge->rSA, 1); }
static uint8_t SA01_pass1(struct ge *ge) { return !BIT(ge->rSA, 1); }
static uint8_t is_str(struct ge *ge) { return ge->rFO == STR_OPCODE; }
static uint8_t is_cmr(struct ge *ge) { return ge->rFO == CMR_OPCODE; }
static uint8_t is_amr(struct ge *ge) { return ge->rFO == AMR_OPCODE; }
static uint8_t is_smr(struct ge *ge) { return ge->rFO == SMR_OPCODE; }
static uint8_t is_la (struct ge *ge) { return ge->rFO == LA_OPCODE;  }
static uint8_t is_lpsr(struct ge *ge) { return ge->rFO == LPSR_OPCODE; }
static uint8_t pm_reg_exec(struct ge *ge) {
    return is_lr(ge) || is_str(ge) || is_cmr(ge) || is_amr(ge) || is_smr(ge) || is_la(ge);
}

static uint8_t beta_jump_control(struct ge *ge) {
    return (!is_jrt(ge) && jc_js1_js2_jie(ge)) || lon_loll(ge) ||
           loff(ge) || ins(ge) || ens(ge) || nop(ge) ||
           ge->rFO == HLT_OPCODE;
}

static uint8_t beta_register(struct ge *ge) {
    return is_lr(ge) || is_str(ge) || is_cmr(ge) || is_amr(ge) || is_smr(ge);
}

static uint8_t beta_register_arithmetic(struct ge *ge) {
    return is_cmr(ge) || is_amr(ge) || is_smr(ge);
}

static uint8_t beta_immediate_logic(struct ge *ge) {
    return is_ni(ge) || is_ci(ge) || is_xi(ge) || is_tm(ge);
}

static uint8_t beta_immediate_shift(struct ge *ge) {
    return is_mvi(ge) || beta_immediate_logic(ge) || is_cmi(ge);
}

static uint8_t immediate_and_mode(struct ge *ge) {
    return is_ni(ge) || is_ci(ge) || is_tm(ge);
}

static uint8_t immediate_xor_or_mode(struct ge *ge) {
    return is_xi(ge) || is_ci(ge);
}

static uint8_t immediate_writes_memory(struct ge *ge) { return !is_tm(ge); }
static uint8_t immediate_sets_cc(struct ge *ge) {
    return is_ci(ge) || is_xi(ge) || is_tm(ge);
}
static uint8_t immediate_nonzero_cc(struct ge *ge) {
    return immediate_sets_cc(ge) && (ge->rRO & 0xff) != 0;
}
static uint8_t cmi_result_nonzero(struct ge *ge) { return (ge->rRO & 0xff) != 0; }
static uint8_t cmi_borrow(struct ge *ge) { return !ge->URPE; }

static uint8_t beta_always(struct ge *ge) { (void)ge; return 1; }

static uint8_t jc_js1_js2_jie_lon_loll_loff_ins_ens_nop(struct ge *ge) {
    return jc_js1_js2_jie(ge) || lon_loll(ge) || loff(ge) || ins(ge) || ens(ge) || nop(ge)
           || pm_imm_exec(ge) || pm_reg_exec(ge) || is_ss_data_op(ge);
}

/*  PER - PERI: conditions from fo. 46 */

static uint8_t per_peri(struct ge *ge) {
    return ((ge->rFO == PER_OPCODE) ||
            (ge->rFO == PERI_OPCODE) ||
            (ge->rFO == RDC_OPCODE));
}

static uint8_t per_peri_TO25_CO30(struct ge *ge) {
    return per_peri(ge) && !BIT(ge->rFO, 1);
}

/* Beta exit, return-to-alpha gate (the CU01 "Set S001" row): every operation
 * except PER/PERI/RDC leaves 64|65 for E2 (CU01+CU07+CU10+CU12 -> 1110 0010);
 * the external instructions instead route to CC via CU15/CU03 and must NOT
 * get bit 1.
 *
 * This was previously gated on the explicit list of implemented op groups
 * (jc/js/lon/ins/nop/pm/ss), which wedged the sequencer on any OTHER function
 * code: with no CU01, beta exited to E0 (1110 0000), whose decode sent a
 * 0-address-class FO straight back to beta -- an infinite e0<->64 ping-pong no
 * real MSL can produce (the decode gates always yield a defined route, and the
 * GE-120 has no invalid-operation trap: an unknown code performs no datapath
 * commands and falls through to the next fetch). Observed in the field with
 * the funktionalcpu deck, whose continuous-restart path jumps into swept core
 * and executes FO=0x00.
 *
 * The real row condition is now transcribed and lives in signals.h as CM01A0
 * (cp06 ch.252-7), so the documented opcodes no longer need this gate: it
 * survives only on the two sheets that are not real MSL transcriptions --
 * beta_64_undocumented, whose whole purpose is to keep swept core from
 * wedging the emulator, and beta_64_ss below.  Both go away with the last
 * hybrid; nothing else should acquire a use of it. */
static uint8_t not_per_peri(struct ge *ge) {
    return !per_peri(ge);
}

/* Artificial beta exit for the SS one-shot.
 *
 * The real machine runs the SS data operations through the executive band
 * (64 -> 60|62 -> 50|52 -> 40|42 -> E2), and CM01A0 correctly withholds CU01
 * from every SS opcode in beta because the loop, not the beta phase, is what
 * eventually returns to alpha.  gemu's EXEC_SS one-shot performs the whole
 * instruction inside 64|65, so it has to synthesise the return that the
 * executive states would otherwise have made.  Delete this the moment the
 * family is converted -- it is the marker for where the hybrid still is. */
static uint8_t ss_hybrid_exit(struct ge *ge) {
    return is_ss_data_op(ge) && !per_peri(ge);
}

/* EPER "examine" operation: Z character (in L2) = 0xC0 (bits 7,6 set).
 * (TPER read Z=0x00 -> bit7=0; "set by-pass" Z=0x80 -> bit6=0.) */
static uint8_t is_eper_examine(struct ge *ge) {
    return BIT(ge->rL2, 7) && BIT(ge->rL2, 6);
}

/* Beta phase instruction sheets.
 *
 * CPU[7] prints multiple 64|65 timing sheets, selected by the instruction
 * decode matrix.  They are one MSL, not several: the sheets differ only where
 * a row carries a family term, and every sheet reprints the rows that do not.
 * Those go in beta_64_common below and run for every instruction entering the
 * state; each array that follows carries only what the decode multiplexes, so
 * it reads as the DELTA against the physical sheet rather than the whole page.
 *
 * Some rows remain to be transcribed; the EXEC rows are temporary markers
 * pending the downstream datapath commands described below and in
 * docs/flowchart-sheets.md. */

/* Rows every 64|65|66 sheet prints, with no family term on any of them: the
 * beta phase always clears the two future-state bits that route it out of
 * 0110 01XX.  CU10 (reset S000) retires the X bit -- beta is entered as 64 or
 * 65 depending on the alpha exit, and neither successor keeps bit 0 -- and
 * CU12 (reset S002) drops the 0x04 that distinguishes beta from the executive
 * and alpha bands.  Whichever CU0x SETS follow from the variant then name the
 * successor: +CU01+CU07 -> E2 (return to alpha), +CU03 -> EA (link/register
 * write), +CU15+CU03 -> CC (external), none -> 60|62 (executive loop).
 *
 * Safe to hoist ahead of the variant rows: no 64|65 sheet issues CU00 or CU02,
 * so no variant row contends with these two bits. */
static const struct msl_timing_chart beta_64_common[] = {
    { TI06, CU10, 0 },               /* reset S000: retire the state X bit */
    { TI06, CU12, 0 },               /* reset S002: leave the beta band */
    { END_OF_STATUS, 0, 0 },
};

/* CPU[7] fo.9 + fo.10: JS1/JS2/JIE/JC/NOP2/HLT/INS/ENS/LON/LOFF/LOLL. */
static const struct msl_timing_chart beta_64_control[] = {
    /* One sheet, ten opcodes: every datapath row here is gated on the
     * sub-group that actually uses it, and the groups are disjoint.
     *   {JC+JS1+JS2+JIE} take the address path (CO10 PO->NO, CI12 V2->NO,
     *     CO01 NI->V1) -- only these opcodes compute a branch target;
     *   {JIE} alone adds CO35 (TO60), the interrupt-enable side effect;
     *   {LON+LOLL} / {INS} / {ENS} / {LOFF} each drive exactly one console
     *     or interrupt flip-flop and touch no address register at all. */
    { TO10, CO10, jc_js1_js2_jie },
    { TO20, CI87, lon_loll },
    { TO20, CI77, ins },
    { TO30, CI12, jc_js1_js2_jie },
    { TO40, CO01, jc_js1_js2_jie },
    { TO60, CO35, jie },
    { TO65, CO49, jc_js1_js2_jie_lon_loll_loff_ins_ens_nop },
    { TO70, CI78, ens },
    { TO89, CI88, loff },
    /* {AVER.JC+JS1+JS2+JIE}: the branch is taken only when the condition
     * mask in the operand matches the current FA -- an unmatched jump falls
     * through with V1/PO untouched. */
    { TI05, CI00s, jc_js1_js2_jie_condition_verified },
    { TI06, CU01, CM01A0 },          /* CM01 = DE00A+DE06A+DE11A+DE13A */
    { TI06, CU07, CM01A0 },
    { END_OF_STATUS, 0, 0 },
};

/* cp07 fo.33: JRT beta sheet, verified row-by-row. V1 ends up holding the
 * RETURN address (CO10 PO->NO at TO10 -> BO at TO20 -> CO01 at TO40) and PO
 * the jump target (CI12 V2->NO at TO30 -> BO relatch at TO50 -> CI00 {AVER
 * JRT} at TI05). The link write itself happens in EA/EB (fo.34/35), reached
 * via CU03: the forced address 0xFF/0xFE = change register 7. */
static const struct msl_timing_chart beta_64_jrt[] = {
    { TO10, CO10, 0 },               /* PO -> NO (return address) */
    { TO30, CI12, 0 },               /* V2 -> NO (jump target) */
    { TO40, CO01, 0 },               /* NI -> V1: V1 = return address */
    { TO65, CO49, 0 },
    { TI05, CI00s, jc_js1_js2_jie_condition_verified },  /* {AVER.JRT} */
    { TI06, CU01, CM01A0 },          /* {DO01 class}: JRT is 0x41 */
    { TI06, CU07, CM01A0 },
    { TI06, CU03, 0 },               /* {JRT}: -> EA */
    { END_OF_STATUS, 0, 0 },
};

/* cp07 fo.36: LA beta sheet. The forcings build the change-register LOW-byte
 * address 1111 nnn 1 (= 241+2N, N = L1 bits 6-4 via {LI06}/{LI05}/{LI04})
 * onto NO21; CO02 stores it to V2 for the EA/EB write walk. V1 keeps the
 * operand EA from alpha — it is the DATUM the EA/EB states write into the
 * register cell. (CI89 SET ALTO {FUL4} not modeled. The printed CI41/CI42
 * "CONTA DA 00/04" rows are NOT carried: they configure the CI-side counting
 * network gemu does not model yet, and routing them through the single CN
 * would corrupt the forced address; the net V2 result — the register-cell
 * low-byte address — is produced by the passthrough.) */
static const struct msl_timing_chart beta_64_la[] = {
    /* CO90-97 are not an opcode multiplex but an OPERAND-FIELD one: the three
     * gated forcings select change register N from the instruction's register
     * field, L1 bits 6-4, one bit per NO position. {LI06}/{LI05}/{LI04} are
     * that field read bit-by-bit, so the eight registers share one row set
     * instead of eight sheets. CO90 is ungated: bit 0 of the address is always
     * 1 because a change register's LOW byte lives at the odd address. */
    { TO10, CO18, 0 },               /* forcing in NO21 */
    { TO10, CO97, 0 },
    { TO10, CO96, 0 },
    { TO10, CO95, 0 },
    { TO10, CO94, 0 },
    { TO10, CO93, LI06 },            /* N2 -> NO03 */
    { TO10, CO92, LI05 },            /* N1 -> NO02 */
    { TO10, CO91, LI04 },            /* N0 -> NO01 */
    { TO10, CO90, 0 },               /* 1 -> NO00: odd (low) byte */
    { TO40, CO02, 0 },               /* NI -> V2: register-cell address */
    { TO65, CO49, 0 },
    { TI05, CI05, 0 },               /* NI -> L1 */
    { TI06, CU01, CM01A0 },          /* {DO02 class}: LA is 0x68 */
    { TI06, CU07, CM01A0 },
    { TI06, CU03, 0 },               /* -> EA */
    { END_OF_STATUS, 0, 0 },
};

/* cp07 fo.27: LPSR beta sheet (CI89 SET ALTO {FUL4} not modeled: FUL4
 * strapping unimplemented). */
static const struct msl_timing_chart beta_64_lpsr[] = {
    { TO65, CO49, 0 },               /* reset URPE/URPU */
    { TI06, CU01, CM01A0 },          /* {DO06 class}: LPSR is 0x9d */
    { TI06, CU07, CM01A0 },          /* 64|65 -> C2 with the CU15 below */
    { TI06, CU15, 0 },
    { END_OF_STATUS, 0, 0 },
};

/* cp07 fo.37: LR-AMR-SMR-CMR-STR beta sheet. The forcings build the
 * change-register low-byte address 1111 nnn 1 (N = L1 bits 6-4); CO01
 * {/STR} = DE04A0 (family & FO03: STR is 0xb4, the others 0xbd-0xbf, so
 * bit 3 alone splits them) loads it into V1 for the non-STR ops — their
 * WRITE/target side — while V2 keeps the operand EA from alpha as the
 * SOURCE side; CO02 {STR} = DE05A0 does the reverse for STR. CI05's
 * loop-counter init and the CI41/CI42 rows are not carried: the pass
 * counter is architecturally visible as the state's own X bit (60/62,
 * 50/52, 40/42 -- pass 1 vs pass 2), which the 40|42 exit rows drive
 * (CU01 sets it looping back, CU07 {pass 2} leaves). CI89 {FUL4} not
 * modeled. */
static const struct msl_timing_chart beta_64_register[] = {
    { TO10, CO18, 0 },               /* forcing in NO21 */
    { TO10, CO97, 0 },
    { TO10, CO96, 0 },
    { TO10, CO95, 0 },
    { TO10, CO94, 0 },
    { TO10, CO93, LI06 },            /* N2 -> NO03 */
    { TO10, CO92, LI05 },            /* N1 -> NO02 */
    { TO10, CO91, LI04 },            /* N0 -> NO01 */
    { TO10, CO90, 0 },               /* 1 -> NO00: odd (low) byte */
    /* The one place this family's five opcodes diverge in beta, and it is a
     * single FO bit that does it: STR is 0xb4, LR/AMR/SMR/CMR are 0xbd-0xbf,
     * so FO bit 3 alone separates "store to memory" from "load/operate into
     * the register". Same forcings, same address on NI -- only its DESTINATION
     * register swaps, which is what makes V1 the write side for four opcodes
     * and the read side for the fifth. */
    { TO40, CO01, not_str },         /* {/STR}: V1 = register-cell address */
    { TO40, CO02, is_str },          /* {STR}:  V2 = register-cell address */
    { TO65, CO49, 0 },               /* reset URPE/URPU */
    { END_OF_STATUS, 0, 0 },         /* common CU10+CU12 -> 60|62 (pass 1) */
};

/* CPU[7] fo.12 plus CMI/CHI sheet: immediate logical operations. */
static const struct msl_timing_chart beta_64_immediate_shift[] = {
    { TO30, CI15, 0 },             /* L1 -> NO */
    { TO50, CI33, 0 },             /* NO21 -> RO */
    { TO65, CO49, 0 },             /* reset URPE/URPU */
    { TO70, CI60, 0 },             /* RO2 -> NI4 */
    { TO70, CI65, 0 },             /* RO1 -> NI3 */
    { TI05, CI05, 0 },             /* immediate byte -> L1 high byte */
    { END_OF_STATUS, 0, 0 },       /* common CU10+CU12 -> 60|62 */
};

/* CPU[7] fo.44-45: currently implemented executive data operations. */
static const struct msl_timing_chart beta_64_ss[] = {
    { TO65, CO49, jc_js1_js2_jie_lon_loll_loff_ins_ens_nop },
    { TO65, EXEC_SS, is_ss_data_op },
    { TI06, CU01, ss_hybrid_exit },
    { TI06, CU07, ss_hybrid_exit },
    { END_OF_STATUS, 0, 0 },
};

/* CPU[7] fo.13: PER/PERI preliminary beta sheet. */
static const struct msl_timing_chart beta_64_per[] = {
    { TO10, CO18, per_peri },
    { TO10, CO95, per_peri, DE07A0 },
    { TO10, CO96, per_peri, DE07A0 },
    { TO10, CO97, per_peri, DE07A0 },
    /* PER vs PERI vs RDC: FO bit 1 is the discriminator (per_peri_TO25_CO30).
     * The two rows it gates are the ones that FETCH a control byte from
     * memory -- PERI carries its order inline and needs the read; the other
     * form takes it from the channel and must not disturb RO/L1. Every other
     * row here is common to all three external opcodes. */
    { TO25, CO30, per_peri_TO25_CO30, DE08A0 },
    { TO70, CI62, per_peri, DE07A0 },
    { TO70, CI67, per_peri, DE07A0 },
    { TI05, CI05, per_peri_TO25_CO30, DE08A0 },
    { TI06, CU07, per_peri, DE07A0 },
    { TI06, CU15, per_peri },
    { TI06, CU03, per_peri },        /* +common CU10+CU12 -> CC, not E2 */
    { END_OF_STATUS, 0, 0 },
};

/* Compatibility route for undocumented function codes.  This is intentionally
 * explicit: the cp06 DE00A transcription is incomplete and aa7ed63 established
 * that swept-core bytes must not wedge the emulator. */
static const struct msl_timing_chart beta_64_undocumented[] = {
    { TI06, CU01, not_per_peri },
    { TI06, CU07, not_per_peri },
    { END_OF_STATUS, 0, 0 },
};

static uint8_t xc_first_pass(struct ge *ge) {
    return is_xc(ge) && SA01_pass1(ge);
}
static uint8_t xc_byte_nonzero(struct ge *ge) {
    return is_xc(ge) && ge->rUA != 0;
}
static uint8_t cmc_byte_differs(struct ge *ge) { return ge->rUA != 0; }
static uint8_t cmc_borrow(struct ge *ge) { return !ge->URPE; }
static uint8_t cmc_done(struct ge *ge) {
    return L1_21_ones(ge) || ge->rUA != 0;
}

/* XC-OC-NC (cp07 fo.144-147) and CMC (fo.76-79), verified row-by-row.
 * Both share the MVC-style byte loop (source byte staged at V2++ in 60|62
 * with the CI-phase length count, result/write at V1++ in 40|42); the logic
 * family adds the 50|52 UA pass with the mode selectors exactly as gemu's
 * CI68 table expects (CI45 logic + CI46 {OC+NC} + CI47 {XC+OC}: XC=xor,
 * OC=or, NC=and), and CMC's 50|52 is a per-byte compare: CO48 presets the
 * borrow EVERY pass, CI68 subtracts (op1 byte - op2 byte), and 40|42 exits
 * early on the first difference {(L1_2,1=1i) + /(dRO=0i)} with the flags of
 * that pass: FI04 armed on the first byte (CI74 {/SA01}) and reset on
 * borrow {/URPE}, FI05 set on a differing byte {/(dRO=0)} -- the FA04/FA05
 * pair lands on the manual's compare table (equal=2, low=1, high=3). */

/* MVC (cp07 fo.73/74/75, verified row-by-row): the SS byte-copy loop.
 * beta contributes only CO49 + the exit to 60|62 (all datapath rows on the
 * shared sheet are {MVI}-gated); 60|62 reads the source byte at V2++ and
 * stages it in L1's high byte while the CI-phase count decrements the
 * length in L1's low byte (CI40+CI44+CI41: -1, byte-local); 40|42 writes
 * the staged byte at V1++ and loops {/(L1_2,1=1i)} back to 60+62 or exits
 * to E2/E3 on the terminal count {L1_2,1=1i} (overbar placement verified
 * at high zoom: the loop condition carries the full-expression overbar). */
static const struct msl_timing_chart beta_64_mvc[] = {
    { TO65, CO49, 0 },
    { END_OF_STATUS, 0, 0 },         /* common CU10+CU12 -> 60|62 */
};

static const struct msl_timing_variant beta_64_variants[] = {
    { beta_jump_control, beta_64_control,  "beta-control",  "CPU[7] fo.9" },
    { is_jrt,            beta_64_jrt,     "beta-jrt",      "CPU[7] fo.10" },
    { is_la,             beta_64_la,      "beta-la",       "CPU[7] fo.10" },
    { is_lpsr,           beta_64_lpsr,    "beta-lpsr",     "CPU[7] fo.8" },
    { beta_register,     beta_64_register,"beta-register", "CPU[7] fo.11" },
    { beta_immediate_shift, beta_64_immediate_shift,
                                      "beta-immediate", "CPU[7] fo.41/73/76" },
    { per_peri,          beta_64_per,     "beta-per",      "CPU[7] fo.13" },
    { is_mvc,            beta_64_mvc,     "beta-mvc",      "cp07 fo.73" },
    { is_cmc,            beta_64_mvc,     "beta-cmc",      "cp07 fo.76" },
    { is_xoc_nc,         beta_64_mvc,     "beta-xoc-nc",   "cp07 fo.144" },
    { is_ss_data_op,     beta_64_ss,      "beta-ss",       "CPU[7] fo.44-45" },
    { beta_always,       beta_64_undocumented,
                                           "beta-undocumented", "compat aa7ed63" },
    { 0, 0, 0, 0 },
};

/* Register-family executive states, cp07 fo.38/39/40, verified row-by-row.
 * Two passes over the 16-bit quantities, low byte then high byte, encoded in
 * the state X bit: 60 -> (50) -> 40 -> 62 -> (52) -> 42 -> E2/E3, with
 * LR/STR skipping 50|52 (CU04 set + CU14 {LR+STR} reset). Per pass:
 * 60|62 reads the SOURCE byte at V2-- (memory operand for the non-STR ops,
 * the register cell for STR) and stages it in L1's high byte (CI60/CI65 +
 * CI05); 50|52 (arithmetic only) reads the register byte at V1, runs the UA
 * (CI47 subtract for SMR/CMR, CO48 {/SA01} presets the borrow on pass 1,
 * URPE carries between passes) and restages the result; 40|42 writes the
 * staged byte to V1-- ({LR+AMR+SMR+STR} -- CMR writes nothing) and sets the
 * qualitative flags: FI04 = pass carry (CI84 re-arms it each pass in 60|62),
 * FI05 = result zero accumulated across passes (CI85 arms on pass 1 only).
 * The printed 40|42 exit gate is {(L1_2=1i)}, the loop counter the CI41/42
 * init rows feed; the state X bit is the architecturally equivalent pass
 * encoding gemu uses while that counter init remains undecoded. */

static uint8_t reg_arith_pass1(struct ge *ge) {
    return beta_register_arithmetic(ge) && SA01_pass1(ge);
}

static uint8_t beta_register_lr_str(struct ge *ge) {
    return is_lr(ge) || is_str(ge);
}

static uint8_t reg_arith50_pass1_sub(struct ge *ge) {
    return is_smr_or_cmr(ge) && SA01_pass1(ge);
}

static uint8_t reg_result_nonzero(struct ge *ge) {
    /* {/(dRO=0i).(AMR+SMR+CMR)} -- the sheet's condition is OVERBARRED:
     * set FI05 when the pass result byte is NONZERO. CI85 arms (resets)
     * FI05 on pass 1, so the two passes OR into it: FA05 = result != 0,
     * exactly the manual's CC tables (cp04 sec.5.6.4.2-4), where the
     * carry/nonzero pair encodes compare results for CMR (unsigned) and
     * SMR (signed, two's complement) alike. */
    return beta_register_arithmetic(ge) && ge->rUA != 0;
}

static uint8_t reg_carry(struct ge *ge) {
    return beta_register_arithmetic(ge) && ge->URPE;
}

/* Executive-state common rows.
 * ---------------------------
 * The three executive states are a fetch/operate/store pipeline that every
 * family walks the same way, and the sheets show it: the rows below are
 * printed with no family term on any of the sheets listed in chart_ref, so
 * they belong to the state rather than to the instruction.  What the decode
 * multiplexes is only WHERE the byte comes from, WHAT the arithmetic unit
 * does to it, and WHEN the loop stops -- which is exactly what is left in
 * each variant array.
 *
 * 60|62 is the source-fetch state.  Only the exit is universal: CU15 (reset
 * S005) drops the 0x20 that holds the sequencer in the 6x band, and whether
 * the successor is 50|52 or 40|42 is then decided by the CU04/CU14 pair the
 * variants issue -- LR/STR and MVC/MVI have nothing for the UA to do and
 * reset S004 straight back out, skipping the operate state entirely. */
static const struct msl_timing_chart exec_60_common[] = {
    { TI06, CU15, 0 },               /* reset S005: leave the 6x band */
    { END_OF_STATUS, 0, 0 },
};

/* 50|52 is the OPERATE state, and it is the same six rows for every family
 * that enters it: read the operand-1 byte at V1 (CO11 + CO30), put the byte
 * staged by 60|62 on NO so it reaches BO (CI15), run the arithmetic unit
 * (CI68), restage the result in L1's high byte (CI05), and hand on to 40|42
 * (CU14).  No family alters that skeleton -- what each one contributes is
 * only the UA MODE SELECTORS (CI45/CI46/CI47 and the CO48 carry-in preset),
 * which is why the variant arrays below are two to four rows long.  The
 * families that need no operate step never enter the state at all. */
/* State 50|52, the OPERATE state, as a single chart.
 *
 * Notes that used to live on the per-family sheets, kept because they explain
 * the mode rows below: {SMR+CMR} select subtract while {AMR} leaves the UA in
 * its default add mode and so appears in no row at all; the register family's
 * CO48 carries an extra /SA01 because its borrow-in preset belongs to pass 1
 * only, pass 2 inheriting URPE so the 16-bit borrow chain works, whereas CMC
 * compares byte by byte and reissues the preset every pass. */
static const struct msl_timing_chart exec_50[] = {
    /* Shared skeleton -- every family that enters 50|52 walks it identically:
     * read the operand-1 byte at V1, put the byte 60|62 staged onto NO so it
     * reaches BO, run the arithmetic unit, restage the result, hand on. */
    { TO10, CO11, 0 },               /* V1 -> NO: operand-1 address */
    { TO25, CO30, 0 },               /* MEM -> RO: operand-1 byte */
    { TO30, CI15, 0 },               /* L1 -> NO: staged byte reaches BO */

    /* UA mode. This is the ONLY thing the decode multiplexes in this state,
     * and each row carries the gate its own sheet prints -- no dispatch. */
    { TO30, CI45, beta_immediate_logic },   /* fo.43  logic unit          */
    { TO30, CI46, immediate_and_mode },     /* fo.43  {NI+CI+TM}          */
    { TO30, CI47, immediate_xor_or_mode },  /* fo.43  {XI+CI}             */
    { TO30, CI45, is_xoc_nc },              /* fo.146 logic unit          */
    { TO30, CI46, is_oc_or_nc },            /* fo.146 {OC+NC}             */
    { TO30, CI47, is_xc_or_oc },            /* fo.146 {XC+OC}             */
    { TO30, CI47, is_cmi },                 /* fo.78  subtract            */
    { TO30, CI47, is_cmc },                 /* fo.78  subtract            */
    { TO50, CO48, immediate_xor_or_mode },  /* fo.43  carry-in            */
    { TO50, CO48, is_xc_or_oc },            /* fo.146 as printed          */
    { TO50, CO48, is_cmi },                 /* fo.78  complement add      */
    { TO50, CO48, is_cmc },                 /* fo.78  borrow EVERY byte   */
    { TO50, CO48, reg_arith50_pass1_sub },  /* fo.39  {(SMR+CMR)./SA01}   */
    { TO50, CI47, is_smr_or_cmr },          /* fo.39  subtract            */

    { TO70, CI68, 0 },               /* UA -> NI43, in the mode set above */
    { TI05, CI05, 0 },               /* restage result in L1 high byte */
    { TI06, CU14, 0 },               /* reset S004 -> 40|42 */
    { END_OF_STATUS, 0, 0 },
};

/* 40|42 is the STORE-and-advance state.  Common: address operand 1 (CO11),
 * put the staged byte back on NO (CI15), step V1 through the counting
 * network (CO01), and latch the byte into RO for the write (CI32).  The two
 * unconditional CU sets are the loop arc -- CU01+CU05 turn 40|42 back into
 * 60|62 -- so a family LEAVES the loop by adding CU07 under its own terminal
 * condition, never by withholding these.
 *
 * Multiplexed per family, and deliberately not hoisted:
 *   TO10 CO41/CO40  -- direction. Ascending for the SS byte loops, absent
 *     for the single-byte immediates, descending (CO40) for the register
 *     family, which walks 16-bit quantities LSB-first for the carry chain.
 *   TO25 CO31       -- whether the byte is WRITTEN at all: CMR and CMC
 *     compare without storing, TM tests without storing.
 *   TI06 CU07       -- the terminal condition, different for every family
 *     (state X bit for the register pair, L1 all-ones for MVC/XC, either
 *     that or first-difference for CMC). */
/* The three SS byte loops step their destination pointer upward; the register
 * family walks 16-bit quantities downward, LSB first, for the carry chain. */
static uint8_t ss_byte_loop(struct ge *ge) {
    return is_mvc(ge) || is_cmc(ge) || is_xoc_nc(ge);
}

static const struct msl_timing_chart exec_40[] = {
    /* Shared skeleton: address operand 1, put the staged byte back on NO,
     * step V1 through the counting network, latch the byte into RO. */
    { TO10, CO11, 0 },               /* V1 -> NO: operand-1 address */
    { TO30, CI15, 0 },               /* L1 -> NO: the staged result byte */
    { TO40, CO01, 0 },               /* NI -> V1: stepped address */
    { TO50, CI32, 0 },               /* NO43 -> RO: byte to write */

    /* Direction of the step. */
    { TO10, CO41, beta_register },   /* V1 - 1 ... */
    { TO10, CO40, beta_register },   /* ...DESCENDING: LSB-first, for carry */
    { TO10, CO41, ss_byte_loop },    /* V1 + 1: ascending */

    /* Whether the byte is written at all: the compares never store, and TM
     * tests without storing. */
    { TO25, CO31, beta_register, not_cmr },   /* {LR+AMR+SMR+STR}        */
    { TO25, CO31, beta_immediate_logic, immediate_writes_memory }, /* {/TM} */
    { TO25, CO31, is_mvi },
    { TO25, CO31, is_mvc },
    { TO25, CO31, is_xoc_nc },
                                     /* CMI and CMC issue no CO31 at all */

    /* Condition-code flags. CI85 (reset FI05) is kept ahead of CI75 (set
     * FI05) because the immediate sheet prints them in that order and the
     * pair shares a flip-flop. */
    { TI06, CI85, immediate_sets_cc },        /* {CI+XI+TM}: NI sets no CC */
    { TI06, CI74, reg_carry },                /* {URPE.(A/S/CMR)}         */
    { TI06, CI74, immediate_sets_cc },
    { TI06, CI75, reg_result_nonzero },       /* {/(dRO=0)}               */
    { TI06, CI75, immediate_nonzero_cc },
    { TI06, CI75, is_cmi, cmi_result_nonzero },
    { TI06, CI75, is_cmc, cmc_byte_differs }, /* {/(dRO=0i)}              */
    { TI06, CI75, xc_byte_nonzero },          /* {XC./(dRO=0i)}           */
    { TI06, CI84, is_cmi, cmi_borrow },
    { TI06, CI84, is_cmc, cmc_borrow },       /* {/URPE}                  */

    /* The loop arc: CU01+CU05 turn 40|42 back into 60|62, so a family LEAVES
     * by ADDING CU07 under its own terminal condition, never by withholding
     * these. Every terminal gate below is a different one. */
    { TI06, CU01, 0 },
    { TI06, CU05, 0 },
    { TI06, CU07, beta_register, SA01_pass2 },   /* pass 2 done           */
    { TI06, CU07, is_mvc, L1_21_ones },          /* {L1_2,1=1i} terminal  */
    { TI06, CU07, is_xoc_nc, L1_21_ones },
    { TI06, CU07, is_cmc, cmc_done },            /* {(L1=1i)+/(dRO=0i)}   */
    { TI06, CU07, beta_immediate_shift },        /* single byte: always   */
    { TI06, CU10, beta_immediate_shift },
    { TI06, CU12, beta_immediate_shift },
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart exec_60_register[] = {
    { TO10, CO12, 0 },               /* V2 -> NO: source address */
    { TO10, CO41, 0 },               /* V2 - 1 ... */
    { TO10, CO40, 0 },
    { TO25, CO30, 0 },               /* MEM -> RO: source byte */
    { TO30, CI15, 0 },               /* L1 -> NO (counter path) */
    { TO30, CI41, 0 },               /* CI-phase count rows as printed */
    { TO30, CI42, 0 },
    { TO30, CI40, 0 },
    { TO30, CI44, 0 },               /* byte-local */
    { TO40, CO02, 0 },               /* V2 = V2 - 1 */
    { TO70, CI65, 0 },               /* RO1 -> NI3 */
    { TO70, CI60, 0 },               /* RO2 -> NI4: stage source byte */
    { TI05, CI05, 0 },               /* L1 = [source byte][counted low] */
    { TI06, CI85, reg_arith_pass1 }, /* arm FI05 {/SA01.(AMR+SMR+CMR)} */
    { TI06, CI84, beta_register_arithmetic }, /* re-arm FI04 each pass */
    /* {LR+STR} vs {AMR+SMR+CMR}: the same FO bit-3 split as the beta sheet,
     * read here as "is there anything for the UA to do". CU04 sets S004 and
     * the two pure MOVE opcodes immediately reset it, routing 60|62 -> 40|42
     * and skipping the operate state; the three arithmetic opcodes leave it
     * set and go through 50|52. */
    { TI06, CU04, 0 },               /* set S004... */
    { TI06, CU14, beta_register_lr_str },     /* ...{LR+STR}: skip 50|52 */
    { END_OF_STATUS, 0, 0 },         /* common CU15 -> 50|52 or 40|42 */
};

static const struct msl_timing_chart exec_60_immediate[] = {
    { TI06, CU04, 0 },                       /* +common CU15: 60 -> 50 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart exec_60_mvi[] = {
    /* MVI has no operate step -- the byte to store is already in L1 from
     * beta -- so it withholds CU04 and the common CU15 alone routes 60 -> 40
     * (fo.74). It is the only immediate that skips 50|52. */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart exec_60_cmi[] = {
    { TI06, CI74, 0 },                       /* initial compare CC = equal */
    { TI06, CI85, 0 },
    { TI06, CU04, 0 },                       /* +common CU15: 60 -> 50 */
    { END_OF_STATUS, 0, 0 },
};




static const struct msl_timing_chart exec_60_mvc[] = {
    { TO10, CO12, 0 },               /* V2 -> NO: source address */
    { TO10, CO41, 0 },               /* V2 + 1 */
    { TO25, CO30, 0 },               /* {MVC} MEM -> RO: source byte */
    { TO30, CI15, 0 },               /* L1 -> NO (count path) */
    { TO30, CI40, 0 },               /* CI-phase: L1 low byte - 1 ... */
    { TO30, CI44, 0 },               /* ...byte-local */
    { TO30, CI41, 0 },
    { TO40, CO02, 0 },               /* V2 = V2 + 1 */
    { TO70, CI60, 0 },               /* {MVC} RO2 -> NI4 */
    { TO70, CI65, 0 },               /* {MVC} RO1 -> NI3: stage byte */
    { TI05, CI05, 0 },               /* L1 = [byte][count-1] */
    { TI06, CU04, 0 },               /* set S004... */
    { TI06, CU14, 0 },               /* ...reset: -> 40|42 (no 50|52) */
    { END_OF_STATUS, 0, 0 },         /* common CU15 leaves the 6x band */
};

static const struct msl_timing_chart exec_60_xoc[] = {   /* fo.145 */
    { TO10, CO12, 0 },               /* V2 -> NO */
    { TO10, CO41, 0 },               /* V2 + 1 */
    { TO25, CO30, not_FA03 },        /* {/FA03} MEM -> RO */
    { TO30, CI15, 0 },               /* L1 -> NO */
    { TO30, CI40, 0 },
    { TO30, CI44, 0 },
    { TO30, CI41, 0 },               /* CI-phase: length - 1 */
    { TO40, CO02, 0 },               /* V2 = V2 + 1 */
    { TO70, CI60, 0 },
    { TO70, CI65, 0 },               /* stage source byte */
    { TI05, CI05, 0 },
    /* {/SA01.XC}: only XC reports a condition code, and only on the first
     * byte does it arm the accumulating flags -- OC and NC leave FA alone
     * entirely, which is the sole flag difference between the three. */
    { TI06, CI74, xc_first_pass },   /* SET FI04 {/SA01.XC} */
    { TI06, CI85, xc_first_pass },   /* RESET FI05 {/SA01.XC} */
    { TI06, CU04, 0 },               /* +common CU15 -> 50|52 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart exec_60_cmc[] = {   /* fo.77 */
    { TO10, CO12, 0 },
    { TO10, CO41, 0 },
    { TO25, CO30, 0 },               /* MEM -> RO: operand-2 byte */
    { TO30, CI15, 0 },
    { TO30, CI40, 0 },
    { TO30, CI44, 0 },
    { TO30, CI41, 0 },
    { TO40, CO02, 0 },
    { TO70, CI60, 0 },               /* {CMC} */
    { TO70, CI65, 0 },
    { TI05, CI05, 0 },
    { TI06, CI74, SA01_pass1 },      /* SET FI04 {/SA01} */
    { TI06, CI85, SA01_pass1 },      /* RESET FI05 {/SA01} */
    { TI06, CU04, 0 },               /* +common CU15 -> 50|52 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_variant exec_60_variants[] = {
    { beta_register, exec_60_register, "exec-register-60|62", "CPU[7] fo.38" },
    { is_mvi,        exec_60_mvi,      "exec-mvi-60|62",      "CPU[7] fo.74" },
    { beta_immediate_logic, exec_60_immediate,
                                      "exec-immediate-60|62", "CPU[7] fo.42" },
    { is_cmi,        exec_60_cmi,      "exec-cmi-60|62",      "CPU[7] fo.77" },
    { is_mvc,        exec_60_mvc,      "exec-mvc-60|62",      "cp07 fo.74" },
    { is_cmc,        exec_60_cmc,      "exec-cmc-60|62",      "cp07 fo.77" },
    { is_xoc_nc,     exec_60_xoc,      "exec-xoc-60|62",      "cp07 fo.145" },
    { 0, 0, 0, 0 },
};

/* Display */
/* ------- */

static uint8_t state_00_TO10_CO10(struct ge *ge) { return AF32(ge) || AF42(ge); }
static uint8_t state_00_TO10_CO11(struct ge *ge) { return AF31(ge) || AF41(ge) || AF51(ge); }
static uint8_t state_00_TO30_CI15(struct ge *ge) { return !AF20(ge) && !AF40(ge); }
static uint8_t state_00_TO50_CI33(struct ge *ge) { return !AF20(ge) && !AF21(ge) && !AF40(ge); }

/* Flow chart 14023130A "DISPLAY SEQUENCE" (CPU[7] render-pg 24). Verified
 * row-by-row; the chart's `V3->BO [AF36]` is a scan artifact for `[AF30]`. */
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

/* Flow chart 14023130B "FORCING SEQUENCE" (CPU[7] render-pg 25). States match
 * + tests/forcing.c passes; a few forcing-read brackets (CO30/CO31/CI20/CI33)
 * need a higher-DPI/physical recheck (docs/flowchart-sheets.md). */
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

/* PER-PERI preliminary phase, flow chart 14023130F (CPU[7] render-pg 32).
 * State graph 64/65->c8->d8/d9/da/db->dc->cc and 80->(AINI)->c8|alpha verified
 * via the CUxx future-state arithmetic; tests/initial-load.c locks the per-state
 * register values. Peripheral-status decode (DU95/DU96/PCOV) is partial — PCOV
 * is stubbed to 1. See docs/flowchart-sheets.md. */
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
    /* CU13 resets future_state bit 3. The original `|| FA00` term cleared bit 3
     * on a unit-busy (FA00) exit, which sent CC -> 0xd2 (an interrupt-save
     * state) instead of recycling to D8 to wait for the unit. Per the PER-PERI
     * preliminary-phase flow chart (14023130F, CPU[7] render-pg 32): when FA05
     * is clear and FA00 is set ("UNITA' OCCUPATA / UNIT BUSY") the sequence goes
     * "again back to D8". Dropping FA00 here keeps bit 3 set so CC -> 0xd8 (with
     * the CU11/CU04 below). The FA00=0 bootstrap path is unchanged. */
    return (PCOV(ge) && DU96(ge) && !DU95(ge));
}

/* Unit-busy recycle: FA05 clear, FA00 set -> reset future_state bit 1 so the
 * CC exit lands on 0xd8 (D8) rather than 0xda. (CU04 below sets bit 4.) */
static uint8_t state_cc_TI06_CU11_busy(struct ge *ge) {
    return !BIT(ge->ffFA, 5) && BIT(ge->ffFA, 0);
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
    /* For an EPER examine, load the real channel-1 status into RO (after the
     * memory read at TO50) so the DU95 no-error decode at TI06 is meaningful. */
    { TO50, CE_chan1_status, is_eper_examine },
    /* TODO: CI75 seems conditioned also on the type of peri operation (e.g. TPER/SPER ecc) */
    { TI06, CI75, state_cc_TI06_CI75 },
    { TI06, CU13, state_cc_TI06_CU13 },
    { TI06, CU12, 0 },
    { TI06, CU05, state_cc_TI06_CU05 },
    { TI06, CU04, state_cc_TI06_CU04 },
    { TI06, CU01, state_cc_TI06_CU01 },
    { TI06, CU11, state_cc_TI06_CU11_busy },  /* FA00 unit-busy -> recycle to D8 */
    { END_OF_STATUS, 0, 0 },
};

/* Channel-2 OUTPUT data-transfer (rSI state 02/03; flow chart 14023130₁, CPU[7]
 * render-pg 36 "CHANNEL 2 DATA TRANSFER PHASE"). One character per RES2 cycle:
 *   NO <- V4 (CO14); memory read RO <- mem[VO=V4] (CO30); V4 <- V4+1 (CO41/CO04);
 *   "Load Printer Buffer" (CE16) hands RO to the integrated printer.
 * Reached via NA_knot (RES2 -> rSA = rSI & 0x0f = 0x02) while the printer holds
 * the channel-2 request; the per-character loop persists because the cycle leaves
 * future_state = 0x02 and the channel-2 length terminates the request. */
static const struct msl_timing_chart state_02[] = {
    { TO10, CO14, 0 },   /* NO <- V4 (channel-2 operand addresser) */
    { TO10, CO41, 0 },   /* counting network: V4 + 1 on NI */
    { TO25, CO30, 0 },   /* memory read: RO <- mem[VO = V4] */
    { TO40, CO04, 0 },   /* V4 <- NI (advance to next byte) */
    { TI06, CE16, 0 },   /* Load Printer Buffer: emit RO to channel 2 */
    { END_OF_STATUS, 0, 0 },
};

/* Channel-2 INPUT data-transfer (rSI state 0C|0E; CPU[7] sheet 36 "CHANNEL 2
 * DATA TRANSFER PHASE"). One byte per RES2 cycle from the integrated reader:
 *   VO <- V4 (CO14); V4+1 -> V4 (CO41/CO04 — card/photo reader; a magnetic
 *   reader [PELM] would decrement); NE -> RO (CI34, the channel-2 input byte via
 *   NE_knot when the reader-input select PIB21 is asserted); RO -> mem[VO=V4]
 *   (CO31 WRITE, commits at TO65). Reached via NA_knot (RES2 -> rSA = rSI&0x0f =
 *   0x0c) while the reader holds the channel-2 request RC02; per the sheet-36
 *   diamond a reader (PC22) byte returns to B8 to await the next request.
 * The page-36 RO->RI and the [~PC22] external-error arming are printer/compare
 * concerns (states 04|06); the bare reader read is the five commands below. */
static const struct msl_timing_chart state_0c[] = {
    { TO10, CO14, 0 },   /* VO <- V4 (channel-2 operand addresser) */
    { TO10, CO41, 0 },   /* counting network: NI = V4 + 1 */
    { TO25, CI34, 0 },   /* NE -> RO: latch the channel-2 input byte */
    { TO25, CO31, 0 },   /* arm memory WRITE (commits TO65: mem[VO=V4] <- RO) */
    { TO40, CO04, 0 },   /* V4 <- NI (advance to next byte) */
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

/* State b8 is the org-phase external request-wait for a channel-2 transfer.
 * The natural exit to alpha is gated on DU97 (= PUC2 ^ L2.3): when the channel-2
 * unit signals "ready/done" (PUC2), CU01/CU13/CU14/CU06 build the PER-completion
 * future_state and the sequencer returns to alpha with the CPU context intact.
 * gemu does not drive channel-2 timing at signal level, so for an integrated
 * printer/typewriter the printer peripheral (printer.c) asserts PUC2 (and the
 * CPU-active request RC00) at this wait; the completion is then performed by the
 * machine's own microcode here, NOT by forcing the state from outside. The
 * bootstrap/reader tests register no printer and never assert PUC2, so they are
 * unaffected. See the LPSR/TPER channel-2 flow charts (B8 -> E2|E3 via DU97). */
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

/* RENIA/RILIA: channel-1 read length-count "not exhausted". The length is in L1,
 * decremented per character (CI15->count->CI05; CPU[7] B9 timing), and a
 * length-counted transfer ends at L1+1 chars (CPU[4] §5.8.4.3a) — i.e. when L1
 * underflows to all ones (RL1U1, ch.128).
 *
 * RENIA is the faithful terminal-count equation, gated by L204 (the order-block
 * "length-counted transfer" bit, rL2.4): it drops to 0 only when an actively
 * length-counted transfer has reached terminal. This is INERT for every read
 * gemu currently performs: the bootstrap/initial-load reads keep L204=0 and L1
 * constant at the order length (the per-character L1 decrement is not yet wired
 * into the b1/b9 read datapath), so they continue to end on FININ (RIG1)
 * byte-identically. Enabling true length termination needs that L1 decrement
 * wired first — tracked as the remaining datapath gap. (Equation/decode covered
 * by reader_signals.rl1u1_terminal_decode; inertness by the bootstrap reads.) */
SIG(RENIA) { return !(RL1U1(ge) && L204(ge)); }
SIG(RILIA) { return 1; } // 2nd-length count (decimal SS transfers) — not exercised

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

/* Write-back condition for states ea/eb.
 *
 * The original condition read  BIT(rL2,7) || PC011,  but that causes a
 * spurious mem[V2]=0 write when the machine reaches state_ea via the
 * peripheral-load path (b8-WAIT → ea).  During a channel-1 INPUT
 * (bootstrap/load) operation PC011=1 and rL2[7]=0, so the old condition
 * fired unconditionally and clobbered the just-loaded data.
 *
 * The write-back is only meaningful for OUTPUT transfers (rL2 bit 7 = L207
 * set), where the CPU had previously read memory destructively and now needs
 * to restore it.  For INPUT transfers (peripheral → memory, L207=0) no
 * destructive read occurred, so no write-back is needed.
 *
 * Rename: the function used BIT(rL2,7) which is L207 (output-transfer flag),
 * not L206 (bit 6).  Correct the name and drop the spurious PC011 term.
 */
static uint8_t L207_output_writeback(struct ge *ge) { return BIT(ge->rL2, 7); }

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
    { TO25, CO31, L207_output_writeback },
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
    { TO25, CO31, L207_output_writeback },
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

/* Instruction-side EA/EB (cp07 fo.34/35, "JRT - SR - SL - LA", DA-FROM
 * 64+65 / 40+42): the two-byte register-cell write walk. EA writes the LOW
 * byte, EB the HIGH byte, V1 holds the datum (JRT: the return address; LA:
 * the operand EA), and the cell address comes from the forced 0xFF (= change
 * register 7, {SR+SL+JRT}) or from V2 built in the LA beta sheet; CO40/CO41
 * walk it down for the second byte. SR/SL will route here once their
 * executive states (cp07 fo.152-155) are transcribed. NOTE: the write goes
 * through the ordinary memory path, so cr_cache (a debug aid) is not synced
 * by JRT/LA anymore -- live addressing reads memory, as on the machine. */
static uint8_t is_jrt_or_la(struct ge *ge) { return is_jrt(ge) || is_la(ge); }

static const struct msl_timing_chart state_ea_instr[] = {
    { TO10, CO18, is_jrt },          /* {SR+SL+JRT}: forced address... */
    { TO10, CO97, 0 },               /* ...1111 1111 = 0xFF = cr7 low byte */
    { TO10, CO96, 0 },
    { TO10, CO95, 0 },
    { TO10, CO94, 0 },
    { TO10, CO93, 0 },
    { TO10, CO92, 0 },
    { TO10, CO91, 0 },
    { TO10, CO90, 0 },
    { TO10, CO12, is_la },           /* {LA}: address from V2 (beta build) */
    { TO10, CO40, 0 },               /* decreasing... */
    { TO10, CO41, 0 },               /* ...count: next byte address */
    { TO25, CO31, 0 },               /* RO -> MEM (ED92A0) */
    { TO30, CI11, 0 },               /* V1 -> NO: the datum */
    { TO40, CO02, 0 },               /* NI -> V2: address-1 */
    { TO50, CI33, 0 },               /* NO21 -> RO: datum low */
    { TI06, CU00, 0 },               /* -> EB */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_chart state_eb_instr[] = {
    { TO10, CO12, 0 },               /* V2 -> NO: the walked-down address */
    { TO10, CO97, 0 },               /* forcings as printed (no CO18: inert) */
    { TO10, CO96, 0 },
    { TO10, CO95, 0 },
    { TO10, CO94, 0 },
    { TO10, CO93, 0 },
    { TO10, CO92, 0 },
    { TO10, CO91, 0 },
    { TO10, CO90, 0 },
    { TO10, CO40, 0 },
    { TO10, CO41, 0 },
    { TO25, CO31, 0 },               /* RO -> MEM */
    { TO30, CI11, 0 },
    { TO40, CO02, 0 },
    { TO50, CI32, 0 },               /* NO43 -> RO: datum high */
    { TI06, CU00, 0 },               /* sets first */
    { TI06, CU13, 0 },               /* -> E2/E3 */
    { END_OF_STATUS, 0, 0 },
};

static const struct msl_timing_variant ea_variants[] = {
    { is_jrt_or_la, state_ea_instr, "ea-jrt-la",     "cp07 fo.34" },
    { beta_always,  state_ea,       "ea-peripheral", "cp07 fo.60" },
    { 0, 0, 0, 0 },
};

static const struct msl_timing_variant eb_variants[] = {
    { is_jrt_or_la, state_eb_instr, "eb-jrt-la",     "cp07 fo.35" },
    { beta_always,  state_eb,       "eb-peripheral", "cp07 fo.61" },
    { 0, 0, 0, 0 },
};
