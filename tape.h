#ifndef TAPE_H
#define TAPE_H

#include <stdint.h>

struct ge;

/*
 * MTC/MTH magnetic tape, a Standard-GE-100 controller on connector 3 or 4.
 * Functional model (see connector34.h): a length-prefixed record image; a read
 * TPER transfers the current record into CPU memory through the connector.
 *
 * Image format (host file):
 *   record:    uint16 big-endian length, then `length` data bytes
 *   tape mark: uint16 0x0000 (a zero-length record)
 *   end of medium: physical end of file
 *
 *   image_path : reel image (NULL = blank/empty reel)
 *   connector  : 3 or 4
 *   unit       : 0..63
 */
int tape_register(struct ge *ge, const char *image_path,
                  uint8_t connector, uint8_t unit);

#endif /* TAPE_H */
