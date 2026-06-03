#ifndef DISK_H
#define DISK_H

#include <stdint.h>

struct ge;

/*
 * DSS 156-157 disk pack, a Standard-GE-100 controller on connector 3 or 4.
 * Functional model (see connector34.h): a flat sectored backing store; a read
 * TPER transfers the addressed sector into CPU memory through the connector.
 *
 *   image_path : raw pack image (sector*SECTOR_BYTES bytes); NULL = blank pack.
 *   connector  : 3 or 4.
 *   unit       : 0..63.
 *
 * Registers connector34_init() if needed, then attaches the device.
 */
int disk_register(struct ge *ge, const char *image_path,
                  uint8_t connector, uint8_t unit);

#endif /* DISK_H */
