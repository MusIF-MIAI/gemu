#include "ge.h"
#include "log.h"
#include "bit.h"
#include "signals.h"
#include "alu_logic.h"
#include "alu_bin.h"
#include "alu_dec.h"
#include "alu_reg.h"
#include "alu_cc.h"

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
/* "Forcing in NO21": enable the program forcings (CO90-CO97) onto quartets
 * 2,1 of the NO knot. Previously assigned the enum constant to the forcings
 * VALUE (leaving force_mode off), which made every CO18+CO9x address build
 * inert. */
static void CO18(struct ge *ge) { ge->kNO.force_mode = KNOT_FORCING_NO_21; }

/* Pulse-decay of the NO selection (see ge.h KNOT_NONE_IN_NO). */
static void NO_UNDRIVEN(struct ge *ge) { ge->kNO.cmd = KNOT_NONE_IN_NO; }

static void CI10(struct ge* ge) { CO10(ge);                          }
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

/* Remaining single-address architectural commits.  The immediate family no
 * longer lives here: its CPU[7] CI/CO timing rows drive the normal knots,
 * memory cycle and UA. */
static uint16_t eff_v1_l2(struct ge* ge);

/* Change registers are memory-mapped, 16-bit big-endian, at addresses
 * 240 + N*2 (N = 0..7). The register-op aux char (in L1) carries the 4-bit
 * register code 1000..1111; the register index N = code & 7.
 * NOTE: the code is assumed to sit in the LOW nibble of the aux char —
 * verify against the funktionalcpu oracle. V1 holds the I1 memory address. */
/* Register-op aux char format is 1XXX0000: bit 7 = 1, the register number is
 * bits 4-6, low nibble 0 (confirmed against the funktionalcpu deck, step 0x37
 * LR with aux 0xE0 -> register 6). N = (aux >> 4) & 7. */
static uint16_t reg_addr_of(struct ge* ge) { return (uint16_t)(240 + ((ge->rL1 >> 4) & 0x07) * 2); }
static uint16_t cr_rd16(struct ge* ge, uint16_t a) {
    return (uint16_t)((ge->mem[a] << 8) | ge->mem[(uint16_t)(a + 1)]);
}
static void cr_wr16(struct ge* ge, uint16_t a, uint16_t v) {
    ge_mem_store8(ge, a, (uint8_t)(v >> 8));
    ge_mem_store8(ge, (uint16_t)(a + 1), (uint8_t)(v & 0xff));
    /* cr_wr16 is the register-instruction write path (LR/LA/AMR/SMR/JRT, always
     * to a change-register slot 240+2N). Keep the addressing cache in sync here
     * so a register instruction updates live addressing — but a GENERAL memory
     * write to 0xF0-0xFF (e.g. a destructive memory test) does NOT, leaving
     * addressing intact. See struct ge::cr_cache. */
    if (a >= 240 && a <= 254 && !(a & 1))
        ge->cr_cache[(a - 240) >> 1] = v;
}

/* Register-op MEMORY operand: the instruction address (V1) points to the
 * RIGHTMOST (low) byte; the 16-bit value occupies [addr-1 .. addr] (the
 * LR-AMR-SMR-CMR-STR microcode reads V2 DOWNWARD, V2-1->V2). Confirmed by
 * funktionalcpu step 0x37: LR cr6 <- mem[0x0621..0x0622], verified by the
 * CMC against the saved copy. (The change-register storage at 240+2N is the
 * other way -- high byte first -- handled by cr_rd16/cr_wr16.) */
static uint16_t mem_rd16_op(struct ge* ge, uint16_t a) {
    return (uint16_t)((ge->mem[(uint16_t)(a - 1)] << 8) | ge->mem[a]);
}
static void mem_wr16_op(struct ge* ge, uint16_t a, uint16_t v) {
    ge_mem_store8(ge, (uint16_t)(a - 1), (uint8_t)(v >> 8));
    ge_mem_store8(ge, a, (uint8_t)(v & 0xff));
}

/* Address modification — bit 15 = absolute/modified flag (CPU[4] §2.5, FO.19-20;
 * flow chart dwg 14023130, transcribed in reference_operand_fetch_flowchart).
 *
 * Every operand address field carries the modify flag in bit 15:
 *   bit 15 = 0  ABSOLUTE: bits 0-14 are the address directly (0..0x7FFF); the
 *               change registers are NOT consulted.
 *   bit 15 = 1  MODIFIED: bits 12-14 select one of the 8 change registers and
 *               EA = change_register[N] + (bits 0-11 displacement), 16-bit wrap.
 *
 * The GE-130 resolves this *during operand fetch*: an absolute field is left in
 * V verbatim (bit 15 = 0, so V < 0x8000); a modified field is reduced to its
 * 12-bit displacement in V and the change register is then added by the
 * indexing micro-cycle ED|EC -> EF|EE (per-clock states in msl-states.c,
 * timing tables CPU[7] p64), which reads the change register from mem[240+2N]
 * byte by byte, leaves the resolved EA in V2 and (for the first operand,
 * SA = ED|EF with bit 0 set) copies it to V1 (CI01).
 * So after fetch the operand registers hold the RESOLVED effective address and
 * execution uses them directly — eff_v1_l2 / CI00s / EXEC_SS just mask to the
 * 15-bit address space. The change registers live at mem[240+2N] and default to
 * base[N] = N<<12 at reset (ge_clear); programs reload a base via LR/LA/AMR. */

/* Effective V1 for single-address PM/SI ops: V1 already holds the resolved EA
 * (absolute field, or change_register+displacement after the indexing cycle). */
static uint16_t eff_v1_l2(struct ge* ge) { return ge->rV1 & 0x7FFFu; }

/* Jump target: NI re-assembles the resolved target field at TI05 (for a modified
 * jump it was resolved into V2 by the indexing cycle). Mask to the address space. */
static void CI00s(struct ge* ge) {
    ge->rPO = NI_knot(ge) & 0x7FFFu;
}

static void EXEC_LR (struct ge* ge) { cr_wr16(ge, reg_addr_of(ge), mem_rd16_op(ge, eff_v1_l2(ge))); }
static void EXEC_STR(struct ge* ge) { mem_wr16_op(ge, eff_v1_l2(ge), cr_rd16(ge, reg_addr_of(ge))); }
/* JRT (Jump Return, op 0x41): deposits the address of the subsequent instruction
 * (rPO before the jump rewrites it at TI05/CI00s) into index register 7
 * (mem 254/255). Unconditional, per CPU[4] sec.5.5.6.2 / 5.6.5.1: the link is
 * reserved even when the conditional form does not take the jump. */
static void EXEC_CMR(struct ge* ge) { alu_cmr(ge, cr_rd16(ge, reg_addr_of(ge)), mem_rd16_op(ge, eff_v1_l2(ge))); }
static void EXEC_AMR(struct ge* ge) { uint16_t r = cr_rd16(ge, reg_addr_of(ge)); alu_amr(ge, &r, mem_rd16_op(ge, eff_v1_l2(ge))); cr_wr16(ge, reg_addr_of(ge), r); }
static void EXEC_SMR(struct ge* ge) { uint16_t r = cr_rd16(ge, reg_addr_of(ge)); alu_smr(ge, &r, mem_rd16_op(ge, eff_v1_l2(ge))); cr_wr16(ge, reg_addr_of(ge), r); }

/* SS (Storage-to-Storage) data-op execution (hybrid one-shot).
 *
 * EXEC_SS fires in the beta phase (state 64|65 family, TO65), once the
 * alpha operand-fetch micro-loop has resolved both
 * operands: E7 routes an SS op to beta via its documented CU rows (absolute
 * second operand), or through the indexing micro-cycle ED|EC -> EF|EE first
 * (modified second operand). The per-clock executive-phase recipes for the
 * data ops (CPU[7] sheets 44+ / timing tables p93-p120) are NOT transcribed —
 * the operation happens once here via the alu_* helpers; see
 * docs/flowchart-sheets.md for the fidelity roadmap.
 *
 * Operand layout on entering beta:
 *   V1 = destination address
 *   V2 = source address
 *   L1 = length byte
 *
 * Single-length ops (MVC/NC/OC/XC/CMC/TL/UPK/PK/EDT):
 *   len = (rL1 & 0xff) + 1
 *
 * Two-length decimal ops (AP/SP/MP/DP/MVP/CMP/PKS/UPKS):
 *   alen = ((rL1 >> 4) & 0xf) + 1    (high nibble + 1)
 *   blen = (rL1 & 0xf) + 1           (low nibble + 1)
 */
static void EXEC_SS(struct ge *ge)
{
    uint16_t len  = (uint16_t)((ge->rL1 & 0xff) + 1);  /* up to 256 (LL=0xff); must not be uint8_t */
    uint8_t  alen = ((ge->rL1 >> 4) & 0xf) + 1;
    uint8_t  blen = (ge->rL1 & 0x0f) + 1;
    /* SS effective addresses: V1 (dest) and V2 (src) already hold the resolved
     * effective address after operand fetch (absolute field verbatim, or
     * change_register+displacement via the indexing micro-cycle). Mask to the
     * 15-bit address space. */
    uint16_t dst  = ge->rV1 & 0x7FFFu;
    uint16_t src  = ge->rV2 & 0x7FFFu;

    switch (ge->rFO) {
        case MVC_OPCODE:
            alu_mvc(ge, dst, src, len);
            break;
        case NC_OPCODE:
            alu_nc(ge, dst, src, len);
            break;
        case OC_OPCODE:
            alu_oc(ge, dst, src, len);
            break;
        case XC_OPCODE:
            alu_xc(ge, dst, src, len);
            break;
        case CMC_OPCODE:
            alu_cmc(ge, dst, src, len);
            break;
        case TL_OPCODE:
            alu_tl(ge, dst, len, src);
            break;
        case UPK_OPCODE:
            alu_upk(ge, dst, len-1, src, len-1);
            break;
        case PK_OPCODE:
            alu_pk(ge, dst, len-1, src, len-1);
            break;
        case EDT_OPCODE:
            alu_edt(ge, dst, len, src);
            break;
        case MVP_OPCODE:
            alu_mvp(ge, dst, alen-1, src, blen-1);
            break;
        case CMP_OPCODE:
            alu_cmp(ge, dst, alen-1, src, blen-1);
            break;
        case AP_OPCODE:
            alu_ap(ge, dst, alen-1, src, blen-1);
            break;
        case SP_OPCODE:
            alu_sp(ge, dst, alen-1, src, blen-1);
            break;
        case MP_OPCODE:
            alu_mp(ge, dst, alen-1, src, blen-1);
            break;
        case DP_OPCODE:
            alu_dp(ge, dst, alen-1, src, blen-1);
            break;
        case PKS_OPCODE:
            alu_pks(ge, dst, alen-1, src, blen-1);
            break;
        case UPKS_OPCODE:
            alu_upks(ge, dst, alen-1, src, blen-1);
            break;
        case AB_OPCODE:
            alu_ab(ge, dst, alen, src, blen);
            break;
        case SB_OPCODE:
            alu_sb(ge, dst, alen, src, blen);
            break;
        case AD_OPCODE:
            alu_ad(ge, dst, alen, src, blen);
            break;
        case SD_OPCODE:
            alu_sd(ge, dst, alen, src, blen);
            break;
        /* MVQ/CMQ are part of the AB-SB-AD-SD-MVQ-CMQ microcode family, whose
         * field length is governed by the L1.2 counter (= the high-nibble
         * length, alen), not the full L1 byte. funktionalcpu step 0x1B
         * (MVQ 2,0x0531,0x0533 with L1=0x01) moves only alen=1 byte. */
        case MVQ_OPCODE:
            alu_mvq(ge, dst, src, alen);
            break;
        case CMQ_OPCODE:
            alu_cmq(ge, dst, src, alen);
            break;
        /* Search Right/Left (SR=0xD9, SL=0xDB), SS1 format "SR len, A1, A2":
         *   A1 (V1=dst) = leftmost byte of the search field
         *   A2 (V2=src) = address of the 1-byte model to search for
         *   result address -> change/segment register 7 (mem[0xFE..0xFF], BE)
         * The result-register location is confirmed by the funktionalcpu deck
         * (the instruction following SR at 0x03dc reads mem[0x00FE]); the
         * model-byte source (mem[A2]) is the best-supported reading of the
         * SS1 two-address form and remains MEDIUM confidence pending the
         * search-instruction page image (docs/ISA.md §6.10). */
        case SR_OPCODE: {
            uint16_t r7 = (uint16_t)((ge->mem[0xFE] << 8) | ge->mem[0xFF]);
            alu_sr(ge, &r7, dst, len, ge->mem[src]);
            ge_mem_store8(ge, 0xFE, (uint8_t)(r7 >> 8));
            ge_mem_store8(ge, 0xFF, (uint8_t)(r7 & 0xFF));
            break;
        }
        case SL_OPCODE: {
            uint16_t r7 = (uint16_t)((ge->mem[0xFE] << 8) | ge->mem[0xFF]);
            alu_sl(ge, &r7, dst, len, ge->mem[src]);
            ge_mem_store8(ge, 0xFE, (uint8_t)(r7 >> 8));
            ge_mem_store8(ge, 0xFF, (uint8_t)(r7 & 0xFF));
            break;
        }
        default:
            ge_log(LOG_ERR, "EXEC_SS: unknown SS opcode 0x%02x\n", ge->rFO);
            break;
    }
}

/* EPER "examine abnormal conditions": load the channel-1 peripheral status
 * into RO so the qualitative-result decode (DU95 = !RO1 && !RO2 && RO6 = "no
 * error") reflects the real status. Our integrated-reader feed is always
 * clean (no parity/format error; CE02 even ignores the parity check), so the
 * status is no-error: RO6 set, error bits (RO1/RO2) clear. A real error would
 * be reported here once error injection is modelled. */
static void CE_chan1_status(struct ge *ge)
{
    /* Default: RO6=1 (operation OK), RO1=RO2=0 (no error) -> DU95=1 ("no
     * error"). When inject_chan1_status is non-zero a test/harness is injecting
     * an abnormal condition: report that status byte instead (e.g. 0x42 sets
     * RO1, so DU95 reads "error" and the EPER examine branch fires). */
    ge->rRO = ge->inject_chan1_status ? ge->inject_chan1_status : 0x40;
}

static void CI40(struct ge *ge) { CO40(ge); }
static void CI41(struct ge *ge) { CO41(ge); }

/* Arithmetic-unit mode selection (cp06 ch.190/196 and CPU[7] timing sheets
 * fo.43-45).  The manual issues combinations of these lines and admits the
 * resulting byte through CI68/CI69 later in the same state:
 *
 *   CI45+CI46       AND
 *   CI45+CI47       XOR
 *   CI45+CI46+CI47  OR
 *
 * Other combinations belong to decimal/subtract families not yet transcribed.
 */
static void CI45(struct ge *ge) { ge->ua_controls.logic = 1; }
static void CI46(struct ge *ge) { ge->ua_controls.decimal_and = 1; }
static void CI47(struct ge *ge) { ge->ua_controls.subtract_xor = 1; }


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
/* "UA in NI": the same command selects which byte of BO drives the UA and
 * which half of NI the UA output drives (cpu fo. 125-126). gemu latches the
 * UA output (BO byte + RO, binary, carry through URPE) at command time; the
 * NI knot then reads the latch through NS_UA2/NS_UA1.
 *
 * Used by the modified-address indexing micro-cycle (CPU[7] p64): ED|EC adds
 * the change-register low byte (CI69, carry FFs freshly reset by CO49) and
 * EF|EE adds the high byte with the propagated carry (CI68).
 *
 * CI68 masks BO bits 15-12 to the displacement's quartet 3: the operand field
 * is a 12-bit displacement (bits 12-14 modifier, 15 the modify flag, both
 * stripped during fetch), and the fetch leaves quartet 4 of the operand
 * register holding counting-network residue that must not enter the add. The
 * hardware mechanism for excluding it has not been located in the timing
 * tables — open question; recheck the UA pages before trusting bit-level
 * fidelity here. */
/* Flow chart E4/E5 box "0 -> V_4 [R007]" (dwg 14023130, transcribed in
 * reference_operand_fetch_flowchart): on a MODIFIED address the top quartet of
 * the operand register is explicitly ZEROED at fetch (the modifier+flag nibble
 * is stripped; the change register is added later in ED|EC->EF|EE). Without
 * this, gemu's NI quartet-4 default (the counting network) leaks program-
 * counter bits into V's quartet 4 for code running at addresses >= 0x1000 —
 * the "fetch residue" the old CI68 nibble mask compensated for. The silicon
 * has no such mask (cp06 ch.087/088: all eight BO->UA muxes identical). */
static void NI4_ZERO(struct ge *ge) { ge->kNI.ni4 = NS_ZERO; }

static void CI68(struct ge *ge) {
    uint8_t bo = (uint8_t)(ge->rBO >> 8);
    uint8_t ro = (uint8_t)ge->rRO;

    if (ge->ua_controls.logic) {
        if (ge->ua_controls.decimal_and && ge->ua_controls.subtract_xor)
            ge->rUA = bo | ro;
        else if (ge->ua_controls.decimal_and)
            ge->rUA = bo & ro;
        else if (ge->ua_controls.subtract_xor)
            ge->rUA = bo ^ ro;
        else
            ge->rUA = ro;
    } else {
        unsigned sum = ge->ua_controls.subtract_xor
                     ? ro + (uint8_t)~bo + ge->URPE
                     : bo + ro + ge->URPE;
        ge->rUA  = (uint8_t)sum;
        ge->URPE = sum > 0xff;
    }
    ge->kNI.ni4 = NS_UA2; ge->kNI.ni3 = NS_UA1;
}
static void CI69(struct ge *ge) {
    uint8_t bo = (uint8_t)ge->rBO;
    uint8_t ro = (uint8_t)ge->rRO;

    if (ge->ua_controls.logic) {
        if (ge->ua_controls.decimal_and && ge->ua_controls.subtract_xor)
            ge->rUA = bo | ro;
        else if (ge->ua_controls.decimal_and)
            ge->rUA = bo & ro;
        else if (ge->ua_controls.subtract_xor)
            ge->rUA = bo ^ ro;
        else
            ge->rUA = ro;
    } else {
        unsigned sum = ge->ua_controls.subtract_xor
                     ? ro + (uint8_t)~bo + ge->URPE
                     : bo + ro + ge->URPE;
        ge->rUA  = (uint8_t)sum;
        ge->URPE = sum > 0xff;
    }
    ge->kNI.ni2 = NS_UA2; ge->kNI.ni1 = NS_UA1;
}

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

static void CI89(struct ge* ge) { ge->ALTO = 1; ge->halted = 1; }

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
    /* Set VICU.
     *
     * The CPU docs tie CE08 to the VICU-support path: TO19 + RETO sets RAVI
     * (indexed as RAV12), and the later RB111 timing edge stores RACI.
     * gemu models that latch pair directly even though the wider VICU
     * ecosystem is still only partially implemented. */
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

/* CE16 — "Carica Buffer Stampante / Load Printer Buffer" (channel-2 OUTPUT, rSI
 * transfer state 02/03; flow chart 14023130₁, CPU[7] render-pg 36). Hands the
 * character just read from memory into RO to the integrated printer over the
 * channel-2 output line. The printer sink renders it through the GE graphic set.
 * No-op if no channel-2 sink is attached. */
static void CE16(struct ge *ge) {
    channel_accept_output(ge, &ge->channel2, (uint8_t)(ge->rRO & 0xff));
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


/* Interrupt acknowledge. The nine interruption/LPSR states are per-clock
 * charts now (msl-states.c, cp07 fo.22-31); this pseudo-command remains for
 * the request handshake gemu abstracts: hardware re-entry is masked by the
 * loaded FA06, while gemu's RINT is a test-driven flag that must drop once
 * the interruption is taken. */
static void INT_ACK(struct ge *ge) { ge->RINT = 0; }
