/**
 * @file  alu_bin.h
 * @brief Binary and decimal (unpacked) arithmetic helpers for the GE-120/130.
 *
 * These are pure, stateless helpers that read/write ge->mem[] directly
 * and update the 2-bit condition code via alu_set_cc().
 *
 * Operand model (verified against cpu_6.txt §5.5.1 and §5.5.2):
 *   - Addresses point to the RIGHTMOST (least-significant) byte of a field.
 *   - Length argument is the field length in positions (bytes); the field
 *     occupies addresses [addr - len + 1 .. addr].
 *   - Operations are performed right-to-left (LSD first).
 *   - Big-endian byte order: most-significant byte at the lowest address.
 *
 * Operand-length rules (manual §5.5.1 / §5.5.2):
 *   - If L2 > L1 the instruction is executed as if L2 = L1 (operand 2 is
 *     truncated to the length of operand 1).
 *   - If L2 < L1 the second operand is zero-extended to the left.
 *
 * CC encoding (cpu_6.txt §5.10.2, FA04 = high bit, FA05 = low bit):
 *   cc = (FA04 << 1) | FA05
 *
 * AB CC:  0 = sum zero,  1 = sum non-zero,
 *         2 = overflow+zero,  3 = overflow+non-zero.
 *
 * SB CC:  0 = impossible,  1 = result < 0 (stored complemented to 2^(8*L1)),
 *         2 = result == 0,  3 = result > 0.
 *
 * AD/SD CC:  0 = result zero,  1 = result non-zero,
 *            2 = not possible,  3 = not possible.
 *   (AD/SD treat operands as unsigned positive integers; zones are ignored.
 *    A carry beyond the field boundary is silently dropped.)
 *
 * Note on AD/SD CC: the individual §5.5.1.1 / §5.5.1.2 body tables are not
 * legibly reproduced by the OCR; values above are inferred from the summary
 * table at §5.10.2 and by analogy with MVQ which shares the same format.
 * Confidence: medium.  Human re-inspection of the page images is recommended.
 */

#ifndef ALU_BIN_H
#define ALU_BIN_H

#include "ge.h"
#include "alu_cc.h"

/**
 * AB – Add Binary (opcode 0xFE).
 *
 * Adds the binary integer at [b_addr-b_len+1..b_addr] to the binary integer
 * at [a_addr-a_len+1..a_addr].  Result stored in the first operand field.
 * Second operand is not modified.
 *
 * @param ge     Emulator state
 * @param a_addr Address of the rightmost byte of the first (destination) operand
 * @param a_len  Length of the first operand in bytes (1..16)
 * @param b_addr Address of the rightmost byte of the second (source) operand
 * @param b_len  Length of the second operand in bytes (1..16)
 */
void alu_ab(struct ge *ge,
            uint16_t a_addr, uint8_t a_len,
            uint16_t b_addr, uint8_t b_len);

/**
 * SB – Subtract Binary (opcode 0xFF).
 *
 * Subtracts the second operand from the first operand; result stored in the
 * first operand field.  If the mathematical result is negative it is stored
 * in two's-complement form (complemented to 2^(8*a_len)).
 * Second operand is not modified.
 */
void alu_sb(struct ge *ge,
            uint16_t a_addr, uint8_t a_len,
            uint16_t b_addr, uint8_t b_len);

/**
 * AD – Add Decimal (opcode 0xFA).
 *
 * Adds two unsigned, unpacked (zoned) decimal fields right-to-left.
 * Zones are ignored during the arithmetic.  Result stored in the first
 * operand field (zones of the result positions are set to 0x00 per the
 * "zones not processed" rule; the caller / CPU sequencer is responsible for
 * zone restoration if required).
 *
 * Carry beyond the field boundary is silently dropped.
 * Second operand is not modified.
 */
void alu_ad(struct ge *ge,
            uint16_t a_addr, uint8_t a_len,
            uint16_t b_addr, uint8_t b_len);

/**
 * SD – Subtract Decimal (opcode 0xFB).
 *
 * Subtracts the second unpacked decimal field from the first.  If a borrow
 * propagates beyond the most-significant position the result wraps modulo
 * 10^a_len (consistent with the manual's description of unsigned positive
 * integers with no sign handling in AD/SD).
 *
 * Note: the manual states operands are treated as positive integers; the
 * handling of underflow (borrow beyond the field) is not explicitly described
 * for SD in the available OCR text.  The implementation wraps modulo 10^L1
 * and sets cc=1 (non-zero) on underflow; this should be verified against page
 * images.
 * Second operand is not modified.
 */
void alu_sd(struct ge *ge,
            uint16_t a_addr, uint8_t a_len,
            uint16_t b_addr, uint8_t b_len);

#endif /* ALU_BIN_H */
