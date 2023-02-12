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

struct ge;

/**
 * DC16 - Jump Condition Verified
 *
 * This signal is true when the currently executing jump condition is
 * verified.
 */
static inline uint8_t verified_condition(struct ge *ge) {
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
static inline uint8_t AF10(struct ge *ge) { return ge->register_selector == RS_V4;      }

/** Selected register L3 */
static inline uint8_t AF20(struct ge *ge) { return ge->register_selector == RS_L3;      }

/** Selected register L1 */
static inline uint8_t AF21(struct ge *ge) { return ge->register_selector == RS_L1;      }

/** Selected register V3 */
static inline uint8_t AF30(struct ge *ge) { return ge->register_selector == RS_V3;      }

/** Selected register V1 */
static inline uint8_t AF31(struct ge *ge) { return ge->register_selector == RS_V1;      }

/** Selected normal operation */
static inline uint8_t AF32(struct ge *ge) { return ge->register_selector == RS_NORM;    }

/** Selected register R1 - L2 */
static inline uint8_t AF40(struct ge *ge) { return ge->register_selector == RS_R1_L2;   }

/** Selected register V1 - SCR */
static inline uint8_t AF41(struct ge *ge) { return ge->register_selector == RS_V1_SCR;  }

/** Selected register PO */
static inline uint8_t AF42(struct ge *ge) { return ge->register_selector == RS_PO;      }

/** Selected register SO */
static inline uint8_t AF43(struct ge *ge) { return ge->register_selector == RS_SO;      }

/** Selected register V2 */
static inline uint8_t AF50(struct ge *ge) { return ge->register_selector == RS_V2;      }

/** Selected register V1 - LETT */
static inline uint8_t AF51(struct ge *ge) { return ge->register_selector == RS_V1_LETT; }

/** Selected register FI-UR */
static inline uint8_t AF52(struct ge *ge) { return ge->register_selector == RS_FI_UR;   }

/** Selected register FO */
static inline uint8_t AF53(struct ge *ge) { return ge->register_selector == RS_FO;      }
/** @} */


#endif
