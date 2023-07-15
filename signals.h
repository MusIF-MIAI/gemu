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

/**
 * Cycle assigned to channel 1
 */
SIG(RES0) {
    /* cpu fo. 116 */
    return ge->RESI;
}

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

SIG(DI10A) { return !(BIT(ge->rSA, 7) && BIT(ge->rSA, 6) && BIT(ge->rSA, 5) && !BIT(ge->rSA, 4)); }

SIG(DI101) { return !DI10A(ge); }
SIG(DI12A) { return !(!BIT(ge->rSA, 3) && DI101(ge)); }

SIG(DI121) { return !DI12A(ge); }
SIG(DI17A) { return !(!BIT(ge->rSA, 1) && DI121(ge) && !BIT(ge->rSA, 2)); }


SIG(DI18A) { return !(!BIT(ge->rSA, 2) && DI121(ge) && BIT(ge->rSA, 1)); }
SIG(DI181) { return !DI18A(ge); }
SIG(DI18B) { return !DI181(ge); }

SIG(DI19A) { return !(!BIT(ge->rSA, 1) && DI121(ge) && BIT(ge->rSA, 2)); }
SIG(DI20A) { return !(BIT(ge->rSA, 1) && BIT(ge->rSA, 2) && DI121(ge)); }

SIG(DI27A) { return !(!BIT(ge->rSA, 4) && !BIT(ge->rSA, 5) && BIT(ge->rSA, 7)); }
SIG(DI271) { return !DI27A(ge); }
SIG(DI28A) { return !(DI271(ge) && !BIT(ge->rSA, 5)); }

SIG(DI281) { return !DI28A(ge); }
SIG(DI28B) { return !DI281(ge); }

SIG(DI48A) { return !(!BIT(ge->rSA, 4) && !BIT(ge->rSA, 5) && !BIT(ge->rSA, 6) && !BIT(ge->rSA, 7)); }
SIG(DI481) { return !DI48A(ge); }

SIG(DI69A) { return !(DI481(ge) && !BIT(ge->rSA, 2)); }
SIG(DI691) { return !DI69A(ge); }
SIG(DI58A) { return !(BIT(ge->rSA, 3) && !BIT(ge->rSA, 6)); }
SIG(DI581) { return !DI58A(ge); }
SIG(DI57A) { return !(!BIT(ge->rSA, 1) && DI581(ge) && DI691(ge)); }
SIG(DI572) { return !DI57A(ge); }
SIG(DI57B) { return DI57A(ge) ; }

SIG(DO01A) { return !( BIT(ge->rFO, 6) && !BIT(ge->rFO, 3) && !BIT(ge->rFO, 7)); }
SIG(DO011) { return !DO01A(ge); }
SIG(DI06A) { return !(!BIT(ge->rSA, 7) && BIT(ge->rSA, 6) && BIT(ge->rSA, 2)); }
SIG(DI062) { return !DI06A(ge); }

// TODO: doesn't work for nop/lon/loff ecc
SIG(DE00A) { return 0;  !(DO011(ge) && DI062(ge)); }

SIG(DO04A) { return !(!BIT(ge->rFO, 5) && BIT(ge->rFO, 7)); }
SIG(DO041) { return !DO04A(ge); }
SIG(DO07A) { return !(!BIT(ge->rFO, 0) && !BIT(ge->rFO, 6) && DO041(ge)); }
SIG(DO071) { return !DO07A(ge); }
SIG(DE07A) { return !(DO071(ge) && DI062(ge)); }

SIG(EC69A) { return !(AF41(ge) && DI572(ge));}
SIG(EC70A) { return !(AF51(ge) && DI572(ge)); }
SIG(DI14A) { return !( BIT(ge->rSA, 7) && !BIT(ge->rSA, 5) && BIT(ge->rSA, 6)); }
SIG(DI141) { return !DI14A(ge); }

SIG(DI23A) { return !( DI141(ge) && BIT(ge->rSA, 3) && !BIT(ge->rSA, 4)); }
SIG(DI231) { return !DI23A(ge); }
SIG(DI24A) { return !(DI231(ge) && BIT(ge->rSA, 2)); };
SIG(DI25A) { return !( DI231(ge) && !BIT(ge->rSA, 1) && !BIT(ge->rSA, 2)); };
SIG(DI29A) { return !(DI271(ge) && BIT(ge->rSA, 5) && BIT(ge->rSA, 3)); };
SIG(DI971) { return !(DI29A(ge) && DI25A(ge) && DI24A(ge) && DI29A(ge)); }
SIG(DI97A) { return !DI971(ge); }
SIG(DI201) { return DI20A(ge); }
SIG(EC56A) { return !(DI201(ge) && BIT(ge->rL2, 7)); }
SIG(ED70A) { return !(!ge->AINI && DI971(ge)); }
SIG(DI21A) { return !( DI141(ge) && BIT(ge->rSA, 4) && BIT(ge->rSA, 3) && !BIT(ge->rSA, 2)); }
SIG(DI211) { return !DI21A(ge); }

SIG(DI03A) { return !(BIT(ge->rSA, 0) && BIT(ge->rSA, 1)); }
SIG(DI031) { return !DI03A(ge); }
SIG(DI91A) { return !(DI031(ge) && DI211(ge)); }

SIG(DI15A) { return !( DI141(ge) && !BIT(ge->rSA, 3)); }
SIG(DI931) { return !(DI21A(ge) && DI29A(ge) && DI21A(ge) && DI15A(ge)); }

SIG(DI93A) { return !DI931(ge); }
SIG(DI94A) { return !(BIT(ge->rSA, 0) && DI931(ge)); }
SIG(DI95A) { return !(DI931(ge) && DI031(ge)); }

SIG(DI12A0) { return !DI12A(ge); }
SIG(DI17A0) { return !DI17A(ge); }
SIG(DI18A0) { return !DI18A(ge); }
SIG(DI18B0) { return !DI18B(ge); }
SIG(DI19A0) { return !DI19A(ge); }
SIG(DI60A0) { return 1; } // TODO: Missing page in manual (!)
SIG(DI20A0) { return !DI20A(ge); }
SIG(DI28A0) { return !DI28A(ge); }
SIG(DI28B0) { return !DI28B(ge); }
SIG(DI57A0) { return !DI57A(ge); }
SIG(DI57B0) { return !DI57B(ge); }
SIG(DE00A0) { return !DE00A(ge); }
SIG(DE07A0) { return !DE07A(ge); }
SIG(DE08A0) { return !(!BIT(ge->rFO, 1) && DI062(ge) && DO071(ge)); }
SIG(EC56A0) { return !EC56A(ge); }
SIG(EC69A0) { return !EC69A(ge); }
SIG(EC70A0) { return !EC70A(ge); }
SIG(DI97A0) { return !DI97A(ge); }
SIG(ED70A0) { return !ED70A(ge); }
SIG(DI21A0) { return !DI21A(ge); }
SIG(DI91A0) { return !DI91A(ge); }
SIG(DI25A0) { return !DI25A(ge); }
SIG(DI93A0) { return !DI93A(ge); }
SIG(DI94A0) { return !DI94A(ge); }
SIG(DI95A0) { return !DI95A(ge); }

/** @} */

#endif
