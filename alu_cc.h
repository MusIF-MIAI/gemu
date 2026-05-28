#ifndef ALU_CC_H
#define ALU_CC_H

#include "ge.h"

/*
 * GE-120 condition code.
 *
 * The machine keeps a 2-bit condition code in the special-conditions
 * register ffFI, bits 4 (high) and 5 (low).  At clock TO10 ffFI is
 * snapshotted into ffFA, and a Jump-on-Condition (JC) instruction tests
 * it via verified_condition() in signals.h:
 *
 *     cc = (FA4 << 1) | FA5
 *     JC mask bit M7 -> cc 0,  M6 -> cc 1,  M5 -> cc 2,  M4 -> cc 3.
 *
 * AUTHORITATIVE CC mapping (decoded from CPU manual chapter 5, §5.5.2 / §5.6.1
 * / §5.10.2 qualitative results tables):
 *
 *   COMPARE ops (CMC/CI/CMP/CMR/CMQ):
 *     cc=0  "not possible" (never set by compare ops)
 *     cc=1  first < second                          (FA04=0, FA05=1)
 *     cc=2  first == second                         (FA04=1, FA05=0)
 *     cc=3  first > second                          (FA04=1, FA05=1)
 *
 *   ADD-MAGNITUDE ops (AB add-binary; AD add-decimal-unsigned; AMR):
 *     cc=0  result zero, no overflow                (FA04=0, FA05=0)
 *     cc=1  result nonzero, no overflow             (FA04=0, FA05=1)
 *     cc=2  overflow, partial result zero           (FA04=1, FA05=0)
 *     cc=3  overflow, partial result nonzero        (FA04=1, FA05=1)
 *
 *   SIGNED-RESULT ops (SB subtract-binary; AP/SP/MP/DP/CMP packed;
 *                      SMR; PKS/UPKS/EDT sign):
 *     cc=0  "not possible" (never set by signed-result ops)
 *     cc=1  result < 0 (negative)                   (FA04=0, FA05=1)
 *     cc=2  result == 0                             (FA04=1, FA05=0)
 *     cc=3  result > 0 (positive)                   (FA04=1, FA05=1)
 *
 * The enum names are defined to match all three tables simultaneously.
 * ALU_CC_LOW == ALU_CC_NEG == 1; ALU_CC_EQUAL == ALU_CC_ZERO == 2;
 * ALU_CC_HIGH == ALU_CC_POS == 3.
 *
 * ALU_CC_OVF == 0: used for the overflow / "not-possible" slot.  The
 * per-op overflow CC encoding is uncertain for some ops (see TODO comments
 * in individual functions); kept as 0 pending hardware verification.
 *
 * When wiring JC in Wave 5, remember the mask mapping in
 * verified_condition(): M7->cc0, M6->cc1, M5->cc2, M4->cc3.
 */
enum alu_cc {
    ALU_CC_LOW   = 1, ALU_CC_NEG  = 1,   /* first<second / result<0            */
    ALU_CC_EQUAL = 2, ALU_CC_ZERO = 2,   /* first==second / result==0          */
    ALU_CC_HIGH  = 3, ALU_CC_POS  = 3,   /* first>second / result>0            */
    ALU_CC_OVF   = 0,                    /* overflow / "not possible" slot;
                                            per-op overflow CC is uncertain —
                                            keep as 0 and leave a TODO comment  */
};

/* Store a 2-bit condition code (0..3) into ffFI bits 4 (hi) and 5 (lo). */
void alu_set_cc(struct ge *ge, uint8_t cc);

/* Read the current 2-bit condition code from ffFI bits 4,5. */
uint8_t alu_get_cc(struct ge *ge);

#endif /* ALU_CC_H */
