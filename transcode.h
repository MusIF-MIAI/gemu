#ifndef TRANSCODE_H
#define TRANSCODE_H
/*
 * transcode.h — GE-120 card-reader column transcoder.
 *
 * The card reader's transcoder converts each 80-column card to 80 GE machine
 * bytes.  Each column is represented in the .cap capture format as a 13-bit
 * value (bits 12..0; bit 12 = Hollerith row 12 / Y-zone, bit 11 = row 11 /
 * X-zone, bits 9..0 = rows 9..0 where row 0 is the 10-punch / zone-0).
 *
 * TC_NORMAL applies the empirically derived Hollerith→GE mapping.
 * TC_BINARY passes raw data through (low 8 bits of the column value).
 */

#include <stdint.h>

/* Read modes the GE-120 reader supports. */
enum transcode_mode {
    TC_NORMAL, /* apply the Hollerith→GE table                    */
    TC_BINARY, /* raw passthrough: return (uint8_t)(column & 0xFF) */
    TC_HEX,    /* loader hex-nibble: nibble = sum of set row-bit positions (0..9) */
    TC_COLBIN  /* by-pass column-binary: 1 col->1 byte, bit i <- card row B2R[i] */
};

/*
 * transcode_column - transcode one 13-bit cap column value to a GE byte.
 *
 * @column: 13-bit hole pattern as stored in a cap_deck (from cap_card_columns).
 *          Values above bit 12 are ignored.
 * @mode:   TC_NORMAL or TC_BINARY.
 *
 * TC_NORMAL:
 *   Looks up the 13-bit value (column & 0x1FFF) in the derived table.
 *   Unobserved column values (not present in the funktionalcpu oracle) return
 *   0x20 (GE space character), which is also the blank-column value.
 *
 * TC_BINARY:
 *   Returns the low 8 bits of the column value unchanged, providing a raw
 *   passthrough for binary-mode reads.
 *
 * Returns: GE machine byte (uint8_t).
 */
uint8_t transcode_column(uint16_t column, enum transcode_mode mode);

#endif /* TRANSCODE_H */
