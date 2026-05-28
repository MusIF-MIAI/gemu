#ifndef CARDREADER_H
#define CARDREADER_H

/*
 * cardreader.h - Connector-2 punch-card-reader peripheral for the GE-120 emulator.
 *
 * Registers a peripheral that automatically feeds a .cap punch-card deck into
 * the CPU via the existing integrated-reader handshake (reader_setup_to_send /
 * reader_clear_sending), so the machine can load software without the test
 * harness manually calling those functions.
 *
 * Usage:
 *   ge_init(&g);
 *   ge_clear(&g);
 *   ge_load_1(&g);      // select connector 2
 *   ge_load(&g);
 *   cardreader_register(&g, "/path/to/deck.cap", TC_NORMAL);
 *   ge_start(&g);
 *   // run cycles normally; the peripheral feeds bytes automatically
 */

#include "ge.h"
#include "transcode.h"

/*
 * cardreader_register - load a .cap deck and register the card reader
 *                        peripheral on the GE instance.
 *
 * @ge:      the emulator instance to attach to
 * @cap_path: filesystem path of the .cap deck file
 * @mode:    TC_NORMAL (Hollerith transcoding) or TC_BINARY (raw passthrough)
 *
 * On success returns 0 and the peripheral is active.  Returns -1 if the
 * deck file cannot be opened/parsed, or if memory allocation fails.
 *
 * The peripheral feeds columns one at a time, presenting each transcoded
 * byte to the integrated reader strobe (lu08) when the machine is ready
 * (lu08 == 0).  Cards with zero columns are skipped.  The 'end' flag is
 * raised on the very last column of the last non-empty card, mirroring the
 * behaviour of the manual bootstrap in tests/initial-load.c.
 *
 * deinit: the peripheral frees all deck memory when ge_deinit is called.
 */
int cardreader_register(struct ge *ge, const char *cap_path,
                        enum transcode_mode mode);

/*
 * cardreader_register_from - like cardreader_register, but starts feeding at
 * card index `first_card` (skipping any empty cards from there).
 *
 * Used to skip a deck's title card and the non-matching loader cards. For the
 * "130 CPU FUNCTIONAL TEST" deck: card 0 = title, cards 1-4 = the four loaders
 * (the HOLLERITH card-reader loader has row-8 punched in column 3), cards 5+ =
 * program, last card = summary. Booting a HOLLERITH card reader starts at the
 * matching loader card; it then reads the program cards itself.
 */
int cardreader_register_from(struct ge *ge, const char *cap_path,
                             enum transcode_mode mode, int first_card);

#endif /* CARDREADER_H */
