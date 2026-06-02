#ifndef SAT_BATCHES_H
#define SAT_BATCHES_H

#include <stddef.h>
#include <stdint.h>

#include "ge.h"

enum sat_batch_launch {
    SAT_BATCH_IMAGE = 1,
    SAT_BATCH_READER = 2,
};

struct sat_batch_info {
    const char *id;
    const char *title;
    const char *summary;
    enum sat_batch_launch launch;
};

int sat_batch_count(void);
const struct sat_batch_info *sat_batch_info_at(int idx);
const struct sat_batch_info *sat_batch_find(const char *id);

int sat_batch_prepare_image(const char *root, const char *id,
                            unsigned char *image,
                            unsigned *lo, unsigned *hi, uint16_t *entry,
                            char *note, size_t note_sz);

int sat_batch_prepare_deck(const char *root, const char *id,
                           const char *out_path,
                           char *note, size_t note_sz);

#endif
