#ifndef SAT_BATCHES_H
#define SAT_BATCHES_H

#include <stddef.h>
#include <stdint.h>

#include "ge.h"

struct sat_batch_info {
    const char *id;
    const char *title;
    const char *summary;
};

int sat_batch_count(void);
const struct sat_batch_info *sat_batch_info_at(int idx);
const struct sat_batch_info *sat_batch_find(const char *id);

/*
 * Compose a batch into a single .cap deck at out_path. Every batch is a deck:
 * there is no image-staging variant, because the machine has no way to receive
 * a program other than on cards.
 */
int sat_batch_prepare_deck(const char *root, const char *id,
                           const char *out_path,
                           char *note, size_t note_sz);

#endif
