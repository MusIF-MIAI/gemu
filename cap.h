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

/*
 * cap_load_scattered - load a self-addressed binary card deck into a flat image.
 *
 * Each data card (>= 11 columns) is decoded (TC_COLBIN) and read as a
 * self-describing record: b[8]=LL, b[9..10]=load address (big-endian),
 * b[11..11+LL]=LL+1 payload bytes. The payload is *scattered* to `addr` in
 * `image` (a caller-provided 65536-byte buffer). Cards are gated by the deck's
 * dominant 8-byte prefix (cols 0-7); if no dominant prefix is found, any
 * self-consistent record is accepted (loose). Bytes not written keep their
 * existing value in `image` (caller should pre-zero or pre-seed as desired).
 *
 * This mirrors the gdis `--image` depunch so a .cap deck loads identically
 * whether depunched offline or fed to the emulator directly. It honours each
 * card's embedded load address (the deck format), independent of the (separate,
 * cycle-faithful) card-reader bootstrap.
 *
 * On success returns the number of cards loaded (>0) and sets *lo/*hi to the
 * lowest/highest address written (so [*lo, *hi] is the populated span; entry is
 * conventionally *lo). Returns -1 on parse error or if nothing loaded. `mode` is
 * an `enum transcode_mode` (pass TC_COLBIN for binary decks).
 */
int cap_load_scattered(const char *path, int mode,
                       unsigned char *image /* 65536 bytes */,
                       unsigned *lo, unsigned *hi);
#endif
