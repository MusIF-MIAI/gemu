#ifndef CAP_H
#define CAP_H
#include <stdint.h>

#define CAP_COLS_PER_CARD 80

struct cap_deck;  /* opaque */

/* Parse a .cap file. Returns NULL on open/parse error. */
struct cap_deck *cap_load(const char *path);

/* Number of cards parsed. */
int cap_num_cards(const struct cap_deck *d);

/* Number of columns in card `i` (normally 80). 0 if i out of range. */
int cap_card_ncols(const struct cap_deck *d, int i);

/* Pointer to the column array (uint16_t, 12-bit values) for card `i`,
 * or NULL if i out of range. Length == cap_card_ncols(d,i). */
const uint16_t *cap_card_columns(const struct cap_deck *d, int i);

void cap_free(struct cap_deck *d);
#endif
