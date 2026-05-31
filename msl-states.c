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
 * addr-hi byte has been read into L2). Used to route operand fetch into the
 * modified-address indexing micro-cycle. (SIG(L207)/SIG(not_L207) are defined
 * later in this file for the TPER/CPER path; these mirror them but are visible
 * to the alpha-phase states below.) */
static uint8_t addr_modified(struct ge *ge) { return BIT(ge->rL2, 7); }
static uint8_t addr_absolute(struct ge *ge) { return !BIT(ge->rL2, 7); }

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
//   E4 -> E6 (first pass, loads V1/V2 base), E5 (loads V1 from A1),
//   E7 (CI02 at TI05 loads V2 from A2), then EXEC_SS + SS_TO_ALPHA at TI06.
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
//          ED+EC if L207   (modified-address indexing cycle, not yet implemented)
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

    /* Modified first operand (L207 = address bit 15): enter the indexing
     * micro-cycle (ED|EC -> EF|EE). INDEX_OP1 sets SA00 (first operand) and
     * forces the next state to 0xec, overriding the CU bits above. */
    { TI06, INDEX_OP1, addr_modified },
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
    /* Mechanism B (hybrid one-shot): execute SS op here at TI06, after CI02 at
     * TI05 has loaded V2 (source address) and V1 (destination address) was loaded
     * during the preceding E5 pass.  SS_TO_ALPHA overrides the future state to
     * e2 (alpha), breaking out of the operand-fetch micro-loop.
     *
     * Only when the second operand is ABSOLUTE (not_L207). A modified second
     * operand instead enters the indexing micro-cycle via INDEX_OP2 (SA00 = 0,
     * no V1 copy); the SS op is then executed by INDEX_DONE in state EF|EE once
     * V2 holds the resolved source address. */
    { TI06, EXEC_SS,     is_ss_data_op, addr_absolute },
    { TI06, SS_TO_ALPHA, is_ss_data_op, addr_absolute },
    { TI06, INDEX_OP2,   addr_modified },
    { END_OF_STATUS, 0, 0 }
};

/* Modified-Address Indexing Micro-Cycle */
/* ------------------------------------- */

/* Entered from E6 (operand 1) or E7 (operand 2) when the address is modified
 * (L207 = bit 15 of the address field). Faithful to flow chart dwg 14023130:
 *
 *   ED|EC  — compute EA = change_register[N] + displacement into V2, and copy
 *            it to V1 for the first operand (SA00). (EXEC_INDEX fuses the
 *            flow chart's low-byte add ED|EC and high-byte/carry add EF|EE into
 *            one hybrid step; the two sheets survive as two states so the
 *            control flow still mirrors the chart.)
 *   EF|EE  — route onward (INDEX_DONE):
 *              first operand of a two-address (PMM) op -> fetch/index op 2 (E5)
 *              SS data op, both operands resolved       -> EXEC_SS, then alpha
 *              single-address op (data or jump)         -> beta (0x64) to run it
 */
static uint8_t is_pmm(struct ge *ge) { return BIT(ge->rFO, 6) && BIT(ge->rFO, 7); }

static void INDEX_DONE(struct ge *ge) {
    if (ge->SA00 && is_pmm(ge)) {
        /* first operand of a two-address op resolved -> go fetch operand 2 */
        ge->SA00 = 0;
        ge->future_state = 0xe5;
    } else if (is_ss_data_op(ge)) {
        /* both SS operands resolved -> execute and return to alpha */
        EXEC_SS(ge);
        ge->future_state = 0xe2;
    } else {
        /* single-address op (data op or jump) -> execute in beta */
        ge->future_state = 0x64;
    }
}

static const struct msl_timing_chart state_ED_EC[] = {
    { TI06, EXEC_INDEX, 0 },
    { TI06, INDEX_NEXT, 0 },   /* -> EF|EE (0xee) */
    { END_OF_STATUS, 0, 0 }
};

static const struct msl_timing_chart state_EF_EE[] = {
    { TI06, INDEX_DONE, 0 },
    { END_OF_STATUS, 0, 0 }
};

/* Interruption + LPSR */
/* ------------------- */

/* Flow charts 14023130C "INTERRUPTION" (CPU[7] render-pg 26) and 14023130D
 * "LPSR SEQUENCE" (render-pg 27). Entered from alpha (e2/e3) when INTE =
 * RINT & /MASC routes there (the existing CU04 path: 0xE2 -> 0xF0). The graph
 * F0 -> D2 -> D3 -> D0 -> D1 -> C2 -> C3 -> C0 -> C1 -> alpha saves the current
 * PSR to 0x0300 and loads the new PSR (handler) from 0x0304. Each box is one
 * hybrid command (INT_*, msl-commands.c) issued at TI06 that performs its byte
 * transfer + V1 walk + routing; the states are otherwise inert. The earlier
 * `INTE` diamond and CU04 routing already existed — only these target states
 * were missing (empty slots). RINT is never asserted by the deck/tests, so the
 * path is dormant there; tests/exec.c drives it explicitly. */
static const struct msl_timing_chart state_F0[] = { { TI06, INT_F0, 0 }, { END_OF_STATUS, 0, 0 } };
static const struct msl_timing_chart state_D2[] = { { TI06, INT_D2, 0 }, { END_OF_STATUS, 0, 0 } };
static const struct msl_timing_chart state_D3[] = { { TI06, INT_D3, 0 }, { END_OF_STATUS, 0, 0 } };
static const struct msl_timing_chart state_D0[] = { { TI06, INT_D0, 0 }, { END_OF_STATUS, 0, 0 } };
static const struct msl_timing_chart state_D1[] = { { TI06, INT_D1, 0 }, { END_OF_STATUS, 0, 0 } };
static const struct msl_timing_chart state_C2[] = { { TI06, INT_C2, 0 }, { END_OF_STATUS, 0, 0 } };
static const struct msl_timing_chart state_C3[] = { { TI06, INT_C3, 0 }, { END_OF_STATUS, 0, 0 } };
static const struct msl_timing_chart state_C0[] = { { TI06, INT_C0, 0 }, { END_OF_STATUS, 0, 0 } };
static const struct msl_timing_chart state_C1[] = { { TI06, INT_C1, 0 }, { END_OF_STATUS, 0, 0 } };

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
static uint8_t is_str(struct ge *ge) { return ge->rFO == STR_OPCODE; }
static uint8_t is_cmr(struct ge *ge) { return ge->rFO == CMR_OPCODE; }
static uint8_t is_amr(struct ge *ge) { return ge->rFO == AMR_OPCODE; }
static uint8_t is_smr(struct ge *ge) { return ge->rFO == SMR_OPCODE; }
static uint8_t is_la (struct ge *ge) { return ge->rFO == LA_OPCODE;  }
static uint8_t is_lpsr(struct ge *ge) { return ge->rFO == LPSR_OPCODE; }
static uint8_t pm_reg_exec(struct ge *ge) {
    return is_lr(ge) || is_str(ge) || is_cmr(ge) || is_amr(ge) || is_smr(ge) || is_la(ge);
}

static uint8_t jc_js1_js2_jie_lon_loll_loff_ins_ens_nop(struct ge *ge) {
    return jc_js1_js2_jie(ge) || lon_loll(ge) || loff(ge) || ins(ge) || ens(ge) || nop(ge)
           || pm_imm_exec(ge) || pm_reg_exec(ge);
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

/* EPER "examine" operation: Z character (in L2) = 0xC0 (bits 7,6 set).
 * (TPER read Z=0x00 -> bit7=0; "set by-pass" Z=0x80 -> bit6=0.) */
static uint8_t is_eper_examine(struct ge *ge) {
    return BIT(ge->rL2, 7) && BIT(ge->rL2, 6);
}

/* Beta phase. The per-opcode beta recipes are flow charts 14023130E (JU/JC/JRT/
 * JS/JE), the JS/JC/NOP/HLT/INS/ENS/LON/LOFF/LOLL sheet (render-pg 28), the
 * LR/AMR/CMR/SMR/STR sheet (render-pg 30), 14023130O (NI/XI/OI/TM, render-pg 31),
 * the CMI/CHI sheet (render-pg 38) and the EXECUTIVE-PHASE data-op sheets
 * (render-pg 44-45). gemu implements these HYBRIDLY: the MSL drives routing here
 * while the operation is performed once by an alu_* helper (EXEC_*), rather than
 * transcribing each sheet's per-clock datapath. Functionally validated by
 * tests/exec.c + the deck + cc; not cycle-accurate. See docs/flowchart-sheets.md. */
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
    { TO60, CI38, jc_js1_js2_jie },   /* set AVER = verified_condition before the TI05 jump */
    { TO65, CO49, jc_js1_js2_jie_lon_loll_loff_ins_ens_nop },
    { TO65, EXEC_MVI, is_mvi },
    { TO65, EXEC_NI,  is_ni  },
    { TO65, EXEC_CI,  is_ci  },
    { TO65, EXEC_CMI, is_cmi },
    { TO65, EXEC_XI,  is_xi  },
    { TO65, EXEC_TM,  is_tm  },
    { TO65, EXEC_LR,  is_lr  },
    { TO65, EXEC_STR, is_str },
    { TO65, EXEC_CMR, is_cmr },
    { TO65, EXEC_AMR, is_amr },
    { TO65, EXEC_SMR, is_smr },
    { TO65, EXEC_LA,  is_la  },
    { TO65, EXEC_LPSR, is_lpsr },
    { TO65, JRT_LINK, is_jrt },
    { TO70, CI78, ens },
    { TO70, CI62, per_peri, DE07A0 },
    { TO70, CI67, per_peri, DE07A0 },
    { TO89, CI88, loff },
    { TI05, CI05, per_peri_TO25_CO30, DE08A0 },
    { TI05, CI00s, jc_js1_js2_jie_condition_verified },
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

/* RENIA/RILIA: channel-1 read length-count "not exhausted". The length is in L1,
 * decremented per character (CI15->count->CI05, gated L204; CPU[7] B9 timing),
 * and a length-counted transfer ends at L1+1 chars (CPU[4] §5.8.4.3a). Left as 1
 * for now: the funktionalcpu boot does NOT stall on this — it stalls earlier,
 * executing loaded data as code at PO=0x000e (the IPL load/entry is wrong), so
 * wiring the count terminal here was verified inert. Needs the RENIA/RILIA
 * combinational equation (terminal-count -> RIVE) before enabling. */
SIG(RENIA) { return 1; } // TODO: L1 terminal count (see note above)
SIG(RILIA) { return 1; } // TODO: 2nd-length count (decimal SS transfers)

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
