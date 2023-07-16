/**
 * @file  signals.h
 * @brief Signals
 *
 * This file implements the combinatorial signals of the machine as
 * inline helpers.
 */

#ifndef SIGNALS_H
#define SIGNALS_H

#include <stdint.h>
#include "bit.h"
#include "ge.h"

#define SIG( name ) static inline uint8_t name (struct ge *ge)

/**
 * @defgroup priority-network Cycle attribution priority network
 * @{
 */

SIG(RESIA) { return !ge->RESI; }
SIG(RIA01) { return  ge->RIA0; }
SIG(RIA2A) { return !ge->RIA2; }
SIG(RIA3A) { return !ge->RIA3; }

SIG(RIUCA) { return !(RIA01(ge) && RESIA(ge) && RIA2A(ge) && RIA3A(ge)); }

/* adding RIUCA here breaks machine startup */
SIG(RES01) { return !(RESIA(ge) /* && RIUCA(ge) */); }

SIG(RESI)  { return ge->RESI; }

/** Cycle assigned to channel 1 */
SIG(RES0)  { return RES01(ge); }

/**
 * Cycle assigned to channel 2
 */
SIG(RES2) {
    /* cpu fo. 116 */
    /* maybe this equation is incorrect in manual? it's
     * documented as `!RIA2`, but it seems it should not
     * be negated. */
    return !ge->RIA3 & !ge->RESI & ge->RIA2;
}

/**
 * Cycle assigned to channel 3
 */
SIG(RES3) {
    /* cpu fo. 115 */
    return ge->RIA3 & !ge->RESI;
}

/**
 * Cycle assigned to CPU
 */
SIG(RIUC) {
    return ge->RIA0 & !ge->RESI & !ge->RIA3 & !ge->RIA2;
}

/** @} */

/**
 * DC16 - Jump Condition Verified
 *
 * This signal is true when the currently executing jump condition is
 * verified.
 */
SIG(verified_condition) {
    /* cpu fo 56, 57 */
    uint8_t M = ge->rL1;
    uint8_t M7 = BIT(M, 7);
    uint8_t M6 = BIT(M, 6);
    uint8_t M5 = BIT(M, 5);
    uint8_t M4 = BIT(M, 4);

    uint8_t FA5 = BIT(ge->ffFA, 5);
    uint8_t FA4 = BIT(ge->ffFA, 4);

    return (((ge->rFO == JC_OPCODE) &&
             ((M7 && !FA4 && !FA5) ||
              (M6 && !FA4 &&  FA5) ||
              (M5 &&  FA4 && !FA5) ||
              (M4 &&  FA4 &&  FA5))) ||
            (ge->rFO == JS1_OPCODE && ge->rL1 == JS1_2NDCHAR && ge->JS1) ||
            (ge->rFO == JS2_OPCODE && ge->rL1 == JS2_2NDCHAR && ge->JS2) ||
            0);
}

/**
 * @defgroup selector Console Register Selector Signals
 * @{
 */

/** Selected register V4 */
SIG(AF10) { return ge->register_selector == RS_V4;      }

/** Selected register L3 */
SIG(AF20) { return ge->register_selector == RS_L3;      }

/** Selected register L1 */
SIG(AF21) { return ge->register_selector == RS_L1;      }

/** Selected register V3 */
SIG(AF30) { return ge->register_selector == RS_V3;      }

/** Selected register V1 */
SIG(AF31) { return ge->register_selector == RS_V1;      }

/** Selected normal operation */
SIG(AF32) { return ge->register_selector == RS_NORM;    }

/** Selected register R1 - L2 */
SIG(AF40) { return ge->register_selector == RS_R1_L2;   }

/** Selected register V1 - SCR */
SIG(AF41) { return ge->register_selector == RS_V1_SCR;  }

/** Selected register PO */
SIG(AF42) { return ge->register_selector == RS_PO;      }

/** Selected register SO */
SIG(AF43) { return ge->register_selector == RS_SO;      }

/** Selected register V2 */
SIG(AF50) { return ge->register_selector == RS_V2;      }

/** Selected register V1 - LETT */
SIG(AF51) { return ge->register_selector == RS_V1_LETT; }

/** Selected register FI-UR */
SIG(AF52) { return ge->register_selector == RS_FI_UR;   }

/** Selected register FO */
SIG(AF53) { return ge->register_selector == RS_FO;      }
/** @} */

static inline uint16_t ge_counting_network_output(struct ge *ge) {
    if (ge->counting_network.cmds.from_zero) {
        return ge->rBO + 1;
    }
    else {
        return ge->rBO;
    }
}

/**
 * @defgroup knots Knots
 * @{
 */

/**
 * Knot driven by P0, V1, V2, V4, L1, R1, V3 and L3.
 *
 * In addition, the NO knot contains:
 *   - the forcings from program
 *   - the signals of forcing from console (AM switches)
 */
static inline uint16_t NO_knot(struct ge *ge)
{
    switch (ge->kNO.cmd) {
        case KNOT_PO_IN_NO:       return ge->rPO;
        case KNOT_V1_IN_NO:       return ge->rV1;
        case KNOT_V2_IN_NO:       return ge->rV2;
        case KNOT_V3_IN_NO:       return ge->rV3;
        case KNOT_V4_IN_NO:       return ge->rV4;
        case KNOT_L1_IN_NO:       return ge->rL1;
        case KNOT_L2_IN_NO:       return ge->rL2;
        case KNOT_L3_IN_NO:       return ge->rL3;
        case KNOT_FORCE_IN_NO_21: return ge->kNO.forcings;
        case KNOT_FORCE_IN_NO_43: return ge->kNO.forcings << 8;
        case KNOT_AM_IN_NO:       return ge->console_switches.AM;
        case KNOT_RI_IN_NO_43:    return ge->rRI << 8;
    }
}

/**
 * Knot driven by SO or SI. Content is stored to SA.
 *
 * Driven
 *  - by the SO register when the work cycle has been attributed to the
 *    CPU or to channel 1, if the rotary switch is the "central" position
 *    (AF326=1)
 *  - by the SI register (less four significant bits) when the cycle has
 *    been attributed to channel 2
 *
 * Additionally, individual bits might be set
 *  - NA00: forced to 1 when the work cycle is attributed to channel 1 or
 *    channel 3
 *  - NA03: forced to 1 when the work cycle has been attributed to the CPU
 *    and the rotary switch is not in the "central" position (AF32C = 1))
 *
 * Stored in SA register during T010. (cpu fo. 128)
 */
static inline uint8_t NA_knot(struct ge *ge) {
    uint8_t na = 0;

    if (RES0(ge) || (RIUC(ge) && AF32(ge)))
        na = ge->rSO;

    if (RES2(ge))
        na = ge->rSI & 0x0f;

    if (RES0(ge) || RES3(ge))
        na = na | 0x01;

    if (RIUC(ge) && !AF32(ge))
        na = na | 0x08;

    return na;
}

static inline uint8_t NI_source(struct ge *ge, enum knot_ni_source source) {
    uint8_t is_cn = ((ge->kNI.ni1 == NS_CN1) &&
                     (ge->kNI.ni2 == NS_CN2) &&
                     (ge->kNI.ni3 == NS_CN3) &&
                     (ge->kNI.ni4 == NS_CN4));

    uint16_t cn = is_cn ? ge_counting_network_output(ge) : 0;

    switch (source) {
        case NS_CN1: return (cn & 0x000f) >>  0;
        case NS_CN2: return (cn & 0x00f0) >>  4;
        case NS_CN3: return (cn & 0x0f00) >>  8;
        case NS_CN4: return (cn & 0xf000) >> 12;
        case NS_RO1: return (ge->rRO & 0x0f) >> 0;
        case NS_RO2: return (ge->rRO & 0xf0) >> 4;
        case NS_UA2: return 0;
        case NS_UA1: return 0;
    }
}

/**
 * NI Knot
 *
 * It may be driven by:
 *  - the outputs of the counting network. This occurs always during the
 *    1st phase. During the 2nd phase the driving occurs only if the
 *    corresponding commands of RO in NI and of UA in NI are not present.
 *
 *  - the 1st or And part of RO which may be transferred in anyone of the 4
 *    parts of NI (CI60 - CI67) during the 2nd phase;
 *
 *  - the UA output always during the 2nd phase. The UA may drive the 8 most
 *    significant (CI68) or the 8 less significant (CI69) bits. The same
 *    commands determine also which part of BO must drive the UA to perform
 *    the operation.
 *
 * The quartets of UA may drive the NI knot only if the corresponding RO
 * commands in NI are not present. These in fact have the highest priority.
 * The priority of the various load operations is obtained acting on the
 * signals generation (cpu fo. 125, 126).
 */
static inline uint16_t NI_knot(struct ge *ge) {
    uint16_t ni1 = NI_source(ge, ge->kNI.ni1);
    uint16_t ni2 = NI_source(ge, ge->kNI.ni2);
    uint16_t ni3 = NI_source(ge, ge->kNI.ni3);
    uint16_t ni4 = NI_source(ge, ge->kNI.ni4);

    return ((ni4 << 12) |
            (ni3 <<  8) |
            (ni2 <<  4) |
            (ni1 <<  0));
}

/** @} */

/**
 * @defgroup msldetail Detailed MSL Condition signals
 * @{
 */

SIG(DI01A) { return !(BIT(ge->rSA, 0) && !BIT(ge->rSA, 1)); }
SIG(DI011) { return !DI01A(ge); }
SIG(DI02A) { return !(BIT(ge->rSA, 0) && BIT(ge->rSA, 1)); }
SIG(DI021) { return !DI02A(ge); }
SIG(DI03A) { return !(BIT(ge->rSA, 0) && BIT(ge->rSA, 1)); }
SIG(DI031) { return !DI03A(ge); }
SIG(DI06A) { return !(!BIT(ge->rSA, 7) && BIT(ge->rSA, 6) && BIT(ge->rSA, 2)); }
SIG(DI062) { return !DI06A(ge); }
SIG(DI10A) { return !(BIT(ge->rSA, 7) && BIT(ge->rSA, 6) && BIT(ge->rSA, 5) && !BIT(ge->rSA, 4)); }
SIG(DI101) { return !DI10A(ge); }
SIG(DI11A) { return !(BIT(ge->rSA, 3) && DI101(ge) && !BIT(ge->rSA, 2)); }
SIG(DI111) { return !DI11A(ge); }
SIG(DI12A) { return !(!BIT(ge->rSA, 3) && DI101(ge)); }
SIG(DI121) { return !DI12A(ge); }
SIG(DI14A) { return !(BIT(ge->rSA, 7) && !BIT(ge->rSA, 5) && BIT(ge->rSA, 6)); }
SIG(DI141) { return !DI14A(ge); }
SIG(DI15A) { return !(DI141(ge) && !BIT(ge->rSA, 3)); }
SIG(DI151) { return !DI15A(ge); }
SIG(DI17A) { return !(!BIT(ge->rSA, 1) && DI121(ge) && !BIT(ge->rSA, 2)); }
SIG(DI18A) { return !(!BIT(ge->rSA, 2) && DI121(ge) && BIT(ge->rSA, 1)); }
SIG(DI181) { return !DI18A(ge); }
SIG(DI18B) { return !DI181(ge); }
SIG(DI19A) { return !(!BIT(ge->rSA, 1) && DI121(ge) && BIT(ge->rSA, 2)); }
SIG(DI20A) { return !(BIT(ge->rSA, 1) && BIT(ge->rSA, 2) && DI121(ge)); }
SIG(DI201) { return DI20A(ge); }
SIG(DI21A) { return !( DI141(ge) && BIT(ge->rSA, 4) && BIT(ge->rSA, 3) && !BIT(ge->rSA, 2)); }
SIG(DI211) { return !DI21A(ge); }
SIG(DI22A) { return !(DI141(ge) && BIT(ge->rSA, 4) && BIT(ge->rSA, 3) && BIT(ge->rSA, 2)); }
SIG(DI23A) { return !(DI141(ge) && BIT(ge->rSA, 3) && !BIT(ge->rSA, 4)); }
SIG(DI231) { return !DI23A(ge); }
SIG(DI24A) { return !(DI231(ge) && BIT(ge->rSA, 2)); };
SIG(DI25A) { return !( DI231(ge) && !BIT(ge->rSA, 1) && !BIT(ge->rSA, 2)); };
SIG(DI27A) { return !(!BIT(ge->rSA, 4) && !BIT(ge->rSA, 6) && BIT(ge->rSA, 7)); }
SIG(DI271) { return !DI27A(ge); }
SIG(DI28A) { return !(DI271(ge) && !BIT(ge->rSA, 5)); }
SIG(DI281) { return !DI28A(ge); }
SIG(DI28B) { return !DI281(ge); }
SIG(DI29A) { return !(DI271(ge) && BIT(ge->rSA, 5) && BIT(ge->rSA, 3)); };
SIG(DI291) { return !DI29A(ge); }
SIG(DI48A) { return !(!BIT(ge->rSA, 4) && !BIT(ge->rSA, 5) && !BIT(ge->rSA, 6) && !BIT(ge->rSA, 7)); }
SIG(DI481) { return !DI48A(ge); }
SIG(DI58A) { return !(BIT(ge->rSA, 3) && !BIT(ge->rSA, 6)); }
SIG(DI581) { return !DI58A(ge); }
SIG(DI69A) { return !(DI481(ge) && !BIT(ge->rSA, 2)); }
SIG(DI691) { return !DI69A(ge); }
SIG(DI57A) { return !(!BIT(ge->rSA, 1) && DI581(ge) && DI691(ge)); }
SIG(DI572) { return !DI57A(ge); }
SIG(DI57B) { return DI57A(ge) ; }
SIG(DI79A) { return !(DI151(ge) && DI021(ge)); }
SIG(DI82A) { return 0; } // TODO: MISSING MANUAL PAGE!!
SIG(DI83A) { return 1; } // TODO: MISSING MANUAL PAGE!!
SIG(DI84A) { return !(DI011(ge) && DI291(ge)); }
SIG(DI85A) { return !(DI291(ge) && DI031(ge)); }
SIG(DI86A) { return !(!BIT(ge->rSA, 0) && DI291(ge)); }
SIG(DI87A) { return !(!BIT(ge->rSA, 1) && DI291(ge)); }
SIG(DI91A) { return !(DI031(ge) && DI211(ge)); }
SIG(DI931) { return !(DI21A(ge) && DI29A(ge) && DI21A(ge) && DI15A(ge)); }
SIG(DI93A) { return !DI931(ge); }
SIG(DI94A) { return !(BIT(ge->rSA, 0) && DI931(ge)); }
SIG(DI95A) { return !(DI931(ge) && DI031(ge)); }
SIG(DI971) { return !(DI29A(ge) && DI25A(ge) && DI24A(ge) && DI29A(ge)); }
SIG(DI97A) { return !DI971(ge); }

SIG(DO01A) { return !(BIT(ge->rFO, 6) && !BIT(ge->rFO, 3) && !BIT(ge->rFO, 7)); }
SIG(DO011) { return !DO01A(ge); }
SIG(DO02A) { return !(BIT(ge->rFO, 3) && !BIT(ge->rFO, 7) && BIT(ge->rFO, 6)); }
SIG(DO021) { return !(DO02A(ge) && DO02A(ge)); }
SIG(DO04A) { return !(!BIT(ge->rFO, 5) && BIT(ge->rFO, 7)); }
SIG(DO041) { return !DO04A(ge); }
SIG(DO07A) { return !(!BIT(ge->rFO, 0) && !BIT(ge->rFO, 6) && DO041(ge)); }
SIG(DO071) { return !DO07A(ge); }

// TODO: doesn't work for nop/lon/loff ecc
SIG(DE00A) { return 0;  !(DO011(ge) && DI062(ge)); }
SIG(DE001) { return !DE00A(ge); }
SIG(DE07A) { return !(DO071(ge) && DI062(ge)); }
SIG(DE23A) { return !(DE001(ge) && BIT(ge->rFO, 4) && BIT(ge->rL1, 5)); }
SIG(DE231) { return !(DE23A(ge) && DE23A(ge)); }

SIG(DA25A) { return !(DI111(ge) && DO021(ge)); }

SIG(PC011); SIG(PC111); SIG(PC211);
SIG(DU161) { return !(BIT(ge->rSA, 1) && PC111(ge) && PC211(ge) && PC111(ge)); }
SIG(DU18A) { return !(ge->RIG3 && BIT(ge->rL2, 7)); }
SIG(DU19A) { return !(ge->RIG1 && PC011(ge) && !ge->RACI); }
SIG(DU201) { return !(DU18A(ge) && DU19A(ge)); }
SIG(EC56A) { return !(DI201(ge) && BIT(ge->rL2, 7)); }
SIG(EC69A) { return !(AF41(ge) && DI572(ge));}
SIG(EC70A) { return !(AF51(ge) && DI572(ge)); }
SIG(ED70A) { return !(!ge->AINI && DI971(ge)); }
SIG(ED75A) { return !(ge->AINI && DI011(ge) && DI291(ge)); }
SIG(ED79A) { return !(DI291(ge) && DU161(ge) && BIT(ge->rSA, 0)); }
SIG(ED91A) { return !(DE231(ge) && DU201(ge)); }

SIG(DE00A0) { return !DE00A(ge); }
SIG(DE07A0) { return !DE07A(ge); }
SIG(DE08A0) { return !(!BIT(ge->rFO, 1) && DI062(ge) && DO071(ge)); }

SIG(DA25A0) { return !DA25A(ge); }
SIG(DI11A0) { return !DI11A(ge); }
SIG(DI12A0) { return !DI12A(ge); }
SIG(DI17A0) { return !DI17A(ge); }
SIG(DI18A0) { return !DI18A(ge); }
SIG(DI18B0) { return !DI18B(ge); }
SIG(DI19A0) { return !DI19A(ge); }
SIG(DI20A0) { return !DI20A(ge); }
SIG(DI21A0) { return !DI21A(ge); }
SIG(DI22A0) { return !DI22A(ge); }
SIG(DI24A0) { return !DI24A(ge); }
SIG(DI25A0) { return !DI25A(ge); }
SIG(DI28A0) { return !DI28A(ge); }
SIG(DI28B0) { return !DI28B(ge); }
SIG(DI29A0) { return !DI29A(ge); }
SIG(DI57A0) { return !DI57A(ge); }
SIG(DI57B0) { return !DI57B(ge); }
SIG(DI60A0) { return 1; } // TODO: Missing page in manual (!)
SIG(DI79A0) { return !DI79A(ge); }
SIG(DI82A0) { return !DI82A(ge); }
SIG(DI83A0) { return !DI83A(ge); }
SIG(DI84A0) { return !DI84A(ge); }
SIG(DI85A0) { return !DI85A(ge); }
SIG(DI86A0) { return !DI86A(ge); }
SIG(DI87A0) { return !DI87A(ge); }
SIG(DI91A0) { return !DI91A(ge); }
SIG(DI93A0) { return !DI93A(ge); }
SIG(DI94A0) { return !DI94A(ge); }
SIG(DI95A0) { return !DI95A(ge); }
SIG(DI97A0) { return !DI97A(ge); }

SIG(EC56A0) { return !EC56A(ge); }
SIG(EC69A0) { return !EC69A(ge); }
SIG(EC70A0) { return !EC70A(ge); }
SIG(ED70A0) { return !ED70A(ge); }
SIG(ED75A0) { return !ED75A(ge); }
SIG(ED79A0) { return !ED79A(ge); }
SIG(ED91A0) { return !ED91A(ge); }

/** @} */

/**
 * @defgroup peripherals Signals from IO Peripherals
 * @{
 */

/* MC */
SIG(AITE)  { return ge->console_switches.SITE; }
SIG(AITEA) { return !AITE(ge); }

/* RI */
SIG(LU081) { return 0; }
SIG(LUPO1) { return 0; }
SIG(FINI1) { return 0; }

/* PI */
SIG(FUSE1) { return 0; }
SIG(FINA1) { return 0; }

/* ST3 */
SIG(MARE3) { return 0; }
SIG(TE303) { return 0; }
SIG(FINE3) { return 0; }

/* ST4 */
SIG(MARE4) { return 0; }
SIG(TE304) { return 0; }
SIG(FINE4) { return 0; }

/** @} */

SIG(PC111); SIG(PC131); SIG(PC141); SIG(PC121);

SIG(PB11A) { return !(FINA1(ge) && PC111(ge)); }
SIG(PB13A) { return !(TE303(ge) && PC131(ge)); }
SIG(PB14A) { return !(TE304(ge) && PC141(ge)); }

SIG(RT121) { /* UNIV 1.2µs */ return 0; } // triggered by CE10 in TI10, if (RUF11 && REN21)

SIG(RB101) { return !(AITEA(ge) && PB11A(ge) && PB13A(ge) && PB14A(ge)); }
SIG(RB121) { /* UNIV 1.2µs */ return RB101(ge); }
SIG(RB12A) { return !RB121(ge); }
SIG(RB01A) { return !(RT121(ge) && RB101(ge)); }
SIG(RB111) { return !(RB12A(ge) && RB01A(ge)); }

SIG(PF12A) { return !(FINI1(ge) && PC121(ge)); }
SIG(PF13A) { return !(FINE3(ge) && PC131(ge)); }
SIG(PF14A) { return !(FINE4(ge) && PC141(ge)); }
SIG(RF101) { return !(PF12A(ge) && PF13A(ge) && PF14A(ge)); }

/**
 * @defgroup channel1 Channel 1 Selection Reset
 *
 * From the Intermediate Block Diagram, fo. 10
 *
 * @{
 */

SIG(PC11A); SIG(PC111) { return !PC11A(ge); };
SIG(PC12A); SIG(PC121) { return !PC12A(ge); };
SIG(PC13A); SIG(PC131) { return !PC13A(ge); };
SIG(PC14A); SIG(PC141) { return !PC14A(ge); };

SIG(RUF11) { return  ge->RUF1; }
SIG(RUF1A) { return !ge->RUF1; }
SIG(RASI1) { return  ge->RASI; }
SIG(TO501) { return ge->current_clock == TO50; }

SIG(PELEA) { return !(LU081(ge) && LUPO1(ge)); }
SIG(RELO1) { return !(PELEA(ge) && RUF1A(ge)); }
SIG(PAM4A) { return !(RELO1(ge) && RASI1(ge) && PC121(ge)); }
SIG(PM11A) { return !(FUSE1(ge) && PC111(ge)); }
SIG(PM13A) { return !(MARE3(ge) && PC131(ge)); }
SIG(PM14A) { return !(MARE4(ge) && PC141(ge)); }
SIG(RM101) { return !(PM11A(ge) && PM13A(ge) && PM14A(ge)); }
SIG(PAM1A) { return !(RASI1(ge) && RM101(ge)); }
SIG(RS011) { return !(PAM4A(ge) && PAM1A(ge)); }
SIG(PIM1A) { return !((TO501(ge) && RS011(ge)) || (RB111(ge) && RUF11(ge))); }
SIG(PIC1A) { return !ge->PIC1; }
SIG(PUC11) { return !(PIC1A(ge) && PIM1A(ge)); }
SIG(PUC1)  { return PUC11(ge); }

SIG(PC01A) { return !(!BIT(ge->rL2, 3) && !BIT(ge->rL2, 0)); }
SIG(PC011) { return !PC01A(ge); }
SIG(PC03A) { return !(!BIT(ge->rL2, 0) && BIT(ge->rL2, 3)); }
SIG(PC031) { return !PC03A(ge); }

/** @} */

SIG(PUC26) { return ge->PUC2; }
SIG(PUC36) { return ge->PUC3; }

/**
 * @defgroup busyconnectorlogic Busy Connector Logic
 *
 * From the Intermediate Block Diagram, fo. 9 (5,6-B,C,D)
 *
 * @{
 */

SIG(PB061)    { return  ge->PB06; }
SIG(PB06A)    { return !ge->PB06; }
SIG(PB071)    { return  ge->PB07; }
SIG(PB07A)    { return !ge->PB07; }
SIG(PB261)    { return  ge->PB26; }
SIG(PB26A)    { return !ge->PB26; }
SIG(PB361)    { return  ge->PB36; }
SIG(PB36A)    { return !ge->PB36; }
SIG(PB371)    { return  ge->PB37; }
SIG(PB37A)    { return !ge->PB37; }
SIG(PUC21)    { return  ge->PUC2; }
SIG(PUC2A)    { return !ge->PUC2; }
SIG(PUC31)    { return  ge->PUC3; }
SIG(PUC3A)    { return !ge->PUC3; }

SIG(PC11A)    { return !(PUC11(ge) && PB071(ge) && PB061(ge)); }
SIG(PC12A)    { return !(PUC11(ge) && PB071(ge) && PB06A(ge)); }
SIG(PC13A)    { return !(PUC11(ge) && PB07A(ge) && PB06A(ge)); }
SIG(PC14A)    { return !(PUC11(ge) && PB07A(ge) && PB061(ge)); }
SIG(PC21A)    { return !(PUC21(ge) && PB261(ge)); }
SIG(PC22A)    { return !(PUC21(ge) && PB26A(ge)); }
SIG(PC31A)    { return !(PUC31(ge) && PB371(ge) && PB361(ge)); }
SIG(PC32A)    { return !(PUC31(ge) && PB371(ge) && PB36A(ge)); }
SIG(PC33A)    { return !(PUC31(ge) && PB37A(ge) && PB36A(ge)); }
SIG(PC34A)    { return !(PUC31(ge) && PB37A(ge) && PB361(ge)); }

SIG(SEPEI)    { return !(PC11A(ge) && PC21A(ge) && PC31A(ge)); }
SIG(PU002)    { return !(PC12A(ge) && PC22A(ge) && PC32A(ge)); }
SIG(PU003)    { return !(PC13A(ge) && PC33A(ge)); }
SIG(PU004)    { return !(PC14A(ge) && PC34A(ge)); }

SIG(PUB01_d1) { return !(SEPEI(ge) && BIT(ge->rL1, 7) && BIT(ge->rL1, 6)); }
SIG(PUB01_d2) { return !(PU002(ge) && BIT(ge->rL1, 7) && BIT(ge->rL1, 6)); }
SIG(PUB01_d3) { return !(PU003(ge) && BIT(ge->rL1, 7) && BIT(ge->rL1, 6)); }
SIG(PUB01_d4) { return !(PU004(ge) && BIT(ge->rL1, 7) && BIT(ge->rL1, 6)); }

/** Selected connector busy condition */
SIG(PUB01)    { return !(PUB01_d1(ge) && PUB01_d2(ge) && PUB01_d3(ge) && PUB01_d4(ge)); }

SIG(PC211)    { return !PC21A(ge); }

/** @} */

SIG(DU871) { return !(!ge->RACI && ge->RASI); }
SIG(DU881) { return !(BIT(ge->ffFA, 2) && ge->PUC2); }

SIG(DU89A) { return !(DU871(ge) && DU881(ge) && PC011(ge) && DU881(ge)); }

SIG(DU90A) { return !(PUC26(ge) && !BIT(ge->rRO, 0)); }
SIG(DU91A) { return !(BIT(ge->rRO, 0) && !BIT(ge->rRO, 3) && PUC36(ge)); }

/** Selected channel busy */
SIG(DU92)  { return !(DU90A(ge) && DU91A(ge)); }

SIG(DU93A) { return !(BIT(ge->rL2, 7) && BIT(ge->rL2, 5)); }

/** LPER external operation */
SIG(DU93)  { return !DU93A(ge); }

SIG(DU95A) { return !(!BIT(ge->rRO, 1) && !BIT(ge->rRO, 2) && BIT(ge->rRO, 6)); }
SIG(DU95) { return !DU95A(ge); }

SIG(DU96A) { return !(BIT(ge->rL2, 7) && BIT(ge->rL2, 7)); }
SIG(DU96) { return !DU96A(ge); }

SIG(DU97A) { return !((ge->PUC2 && BIT(ge->rL2, 0)) ||
                      (BIT(ge->rL2, 0) && BIT(ge->rL2, 3))); }
SIG(DU97) { return !DU97A(ge); }

SIG(DU98) { return !(DU89A(ge) && PC03A(ge)); }

#endif
