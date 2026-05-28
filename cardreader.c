/*
 * cardreader.c - Connector-2 punch-card-reader peripheral for the GE-120 emulator.
 *
 * Feeds a .cap punch-card deck into the CPU via the existing integrated-reader
 * handshake (reader_setup_to_send / reader_clear_sending), mirroring the
 * manual cadence demonstrated in tests/initial-load.c.
 *
 * Handshake cadence (derived from initial-load.c):
 *
 *   The machine enters the "input wait" loop (state b8 / b9).  While
 *   waiting it polls LU081 (lu08).  When lu08 == 0, the reader has no
 *   character ready, so the machine keeps looping.
 *
 *   Our on_clock callback runs once per machine cycle, at TO00 (the first
 *   clock pulse of the new cycle, before MSL logic executes).
 *
 *   Per-byte state machine:
 *     CR_IDLE:      lu08 == 0 and RASI==1 → present byte → CR_PRESENTED
 *     CR_PRESENTED: (next clock) → clear byte, lu08 → 0 → CR_IDLE or CR_CARD_DONE
 *     CR_CARD_DONE: end-of-card consumed; wait for RASI==0 → CR_IDLE
 *     CR_DONE:      deck exhausted; do nothing
 *
 * This reproduces the exact pattern:
 *   reader_setup_to_send(...)   -- before b8/b9 cycle
 *   [ge_run_cycle b8/b9]        -- machine reads nibble into mem[]
 *   reader_clear_sending(...)   -- before b1 cycle
 *   [ge_run_cycle b1]           -- machine packs nibble
 *
 * End-of-card / multi-card:
 *   The 'end' flag is set on the last column of the CURRENT card.  This
 *   signals the machine to run the load-end sequence (ea → eb → e3).
 *   After the end byte is consumed, the cardreader enters CR_CARD_DONE and
 *   waits for RASI to drop (the machine clears RASI in state b8 TO70).
 *   Once RASI is 0, CR_CARD_DONE → CR_IDLE with the pointer already at
 *   card N+1, col 0.  On the machine's next PER read (RASI=1 again), the
 *   cardreader presents the next card's bytes automatically.
 */

#include "cardreader.h"

#include <stdlib.h>
#include <stdio.h>

#include "cap.h"
#include "reader.h"
#include "log.h"

/* -------------------------------------------------------------------------
 * Private state
 * ------------------------------------------------------------------------- */

/* Feeding state machine */
enum cr_state {
    CR_IDLE,       /* no character currently presented; ready to feed next */
    CR_PRESENTED,  /* character has been presented; waiting for machine to consume */
    CR_CARD_DONE,  /* end-of-card byte was consumed; wait for RASI to drop before
                    * starting the next card (avoids presenting card N+1 bytes
                    * during the ea/eb end-of-transfer cleanup sequence) */
    CR_DONE,       /* deck exhausted; nothing more to send */
};

struct cardreader_ctx {
    struct cap_deck    *deck;
    enum transcode_mode mode;

    /* The loader card (the first card fed) is read in `mode` (TC_HEX for the
     * "130 CPU FUNCTIONAL TEST" deck). After the loader sets "by-pass", the
     * program cards are read in binary, so every card after the loader card
     * is fed as TC_BINARY. */
    int loader_card;

    int card_idx;   /* index of the current card in the deck */
    int col_idx;    /* index of the current column within the current card */

    /* pre-computed index of last non-empty card and its last column */
    int last_card;
    int last_col;

    enum cr_state state;

    /* Set to 1 when the end-of-card byte was presented (end=1 flag).
     * Used to transition to CR_CARD_DONE after CR_PRESENTED is cleared. */
    int end_of_card_presented;

    /* the ge_peri node we allocated (kept for potential future use) */
    struct ge_peri peri;
};

/* -------------------------------------------------------------------------
 * Helper: advance card/column, skipping empty cards.
 * Returns 1 if there is more data, 0 if the deck is exhausted.
 * ------------------------------------------------------------------------- */
static int cr_advance(struct cardreader_ctx *ctx)
{
    ctx->col_idx++;

    /* Move to next non-empty card if we finished this one */
    while (ctx->card_idx < cap_num_cards(ctx->deck)) {
        if (ctx->col_idx < cap_card_ncols(ctx->deck, ctx->card_idx))
            return 1; /* still inside current card */
        /* exhausted current card; try next */
        ctx->card_idx++;
        ctx->col_idx = 0;
        /* skip empty cards */
        while (ctx->card_idx < cap_num_cards(ctx->deck) &&
               cap_card_ncols(ctx->deck, ctx->card_idx) == 0)
            ctx->card_idx++;
    }

    return 0; /* deck exhausted */
}

/* -------------------------------------------------------------------------
 * Peripheral callbacks
 * ------------------------------------------------------------------------- */

/*
 * on_clock: called at TO00, the first clock of every new machine cycle.
 *
 * Feeding is gated on ge->RASI (channel 1 in transfer).  RASI is set by
 * the machine at state 0xab (TPER-CPER 5) just before it enters the
 * input-wait loop (state 0xb8).  Before that point, presenting bytes
 * would be silently ignored by the machine but would consume our deck.
 *
 * State machine transitions (per-card):
 *
 *   CR_IDLE:      if RASI==1 and lu08==0
 *                   → reader_setup_to_send(byte, end)
 *                   → record end_of_card_presented
 *                   → advance deck pointer to next column / next card
 *                   → CR_PRESENTED
 *
 *   CR_PRESENTED: (next clock after machine consumed the byte)
 *                   → reader_clear_sending()  (clears lu08 and fini)
 *                   → if end_of_card_presented → CR_CARD_DONE
 *                   → else                    → CR_IDLE  (then retry)
 *
 *   CR_CARD_DONE: end-of-card byte was consumed; do NOT yet present the next
 *                 card because RASI may still be 1 from the just-finished
 *                 transfer (it gets cleared by state_b8 TO70 / CI39).
 *                 Wait for RASI to drop to 0, then transition to CR_IDLE.
 *                 The existing RASI gate in CR_IDLE ensures no bytes are
 *                 presented until the machine starts a new transfer (RASI=1).
 *
 *   CR_DONE:      deck exhausted; do nothing.
 *
 * Multi-card sequencing
 * ---------------------
 * The deck pointer (card_idx, col_idx) is advanced BEFORE transitioning to
 * CR_PRESENTED.  When cr_advance() returns 0 (whole deck exhausted), state
 * goes to CR_DONE after the end byte is consumed.  When cr_advance() returns
 * 1 (more cards remain), state goes to CR_CARD_DONE; the pointer already
 * sits at card N+1, col 0, ready for the next PER read.
 */
static int cardreader_on_clock(struct ge *ge, void *opaque)
{
    struct cardreader_ctx *ctx = (struct cardreader_ctx *)opaque;

    if (ctx->state == CR_DONE)
        return 0;

    if (ctx->state == CR_CARD_DONE) {
        /* Wait for RASI to drop (end of the previous card's transfer).
         * Once RASI is 0 we return to CR_IDLE.  The RASI gate in CR_IDLE
         * will prevent feeding until the machine starts a new transfer. */
        if (!ge->RASI) {
            ctx->state = CR_IDLE;
        }
        return 0;
    }

    if (ctx->state == CR_PRESENTED) {
        /* The machine completed the input cycle; retire the previous char.
         *
         * Important: do NOT immediately present the next byte in the same
         * on_clock call.  The cadence from initial-load.c is:
         *   [b9 cycle with lu08=1]          <- machine reads nibble
         *   on_clock: clear                 <- lu08 → 0  (this call, return now)
         *   [b1 cycle with lu08=0]          <- machine packs nibble
         *   on_clock: present next byte     <- lu08 → 1  (next call)
         *   [b9 cycle with lu08=1]          <- machine reads next nibble
         *
         * Returning here ensures we wait one cycle (the b1 pack cycle) before
         * presenting the next byte.
         */
        reader_clear_sending(ge);  /* clears lu08, data, and fini */
        if (ctx->end_of_card_presented) {
            ctx->end_of_card_presented = 0;
            ctx->state = CR_CARD_DONE;
        } else {
            ctx->state = CR_IDLE;
        }
        return 0; /* wait one more cycle before presenting the next byte */
    }

    if (ctx->state == CR_IDLE) {
        /*
         * Only feed when the machine has entered the channel-1 transfer phase
         * (RASI==1).  This prevents consuming deck bytes during the peri-init
         * states (00, 80, c8 ... ab) where the machine is not yet waiting for
         * card data.
         */
        if (!ge->RASI)
            return 0; /* not in transfer phase yet */

        /* Only present if the integrated reader is not already busy */
        if (ge->integrated_reader.lu08 != 0)
            return 0; /* still being consumed from a previous cycle */

        if (ctx->card_idx >= cap_num_cards(ctx->deck)) {
            ctx->state = CR_DONE;
            return 0;
        }

        int ncols = cap_card_ncols(ctx->deck, ctx->card_idx);
        if (ncols == 0 || ctx->col_idx >= ncols) {
            /* Shouldn't happen if cr_advance is correct, but guard anyway */
            ctx->state = CR_DONE;
            return 0;
        }

        const uint16_t *cols = cap_card_columns(ctx->deck, ctx->card_idx);
        /* Loader card uses the registered mode (TC_HEX); program cards that
         * follow are read in binary ("by-pass"). */
        enum transcode_mode m = (ctx->card_idx == ctx->loader_card)
                                    ? ctx->mode : TC_BINARY;
        uint8_t byte = transcode_column(cols[ctx->col_idx], m);

        /* End-of-card: the GE reader raises FINI at every physical card
         * boundary, and (since the read length counter RAMO/RAMI is not yet
         * modelled) end-of-card is what terminates a PER read.  Signal end
         * on the last column of the CURRENT card, not the last column of the
         * whole deck, so each card read is bounded correctly. */
        int is_last = (ctx->col_idx == ncols - 1);

        ge_log(LOG_READER,
               "cardreader: presenting card %d col %d byte=0x%02x end=%d\n",
               ctx->card_idx, ctx->col_idx, byte, is_last);

        reader_setup_to_send(ge, byte, is_last ? 1 : 0);
        ctx->end_of_card_presented = is_last;
        ctx->state = CR_PRESENTED;

        /* Advance pointer so next IDLE knows which byte comes next.
         * For a multi-card deck this moves to (card+1, col 0) which is
         * ready to be presented when the machine starts the next PER read. */
        if (!cr_advance(ctx)) {
            /* Whole deck exhausted; the state will become CR_DONE via
             * CR_CARD_DONE once the end byte is consumed and RASI drops. */
        }
    }

    return 0;
}

static int cardreader_deinit(struct ge *ge, void *opaque)
{
    struct cardreader_ctx *ctx = (struct cardreader_ctx *)opaque;
    (void)ge;
    if (ctx) {
        cap_free(ctx->deck);
        free(ctx);
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int cardreader_register(struct ge *ge, const char *cap_path,
                        enum transcode_mode mode)
{
    return cardreader_register_from(ge, cap_path, mode, 0);
}

int cardreader_register_from(struct ge *ge, const char *cap_path,
                             enum transcode_mode mode, int first_card)
{
    struct cap_deck *deck = cap_load(cap_path);
    if (!deck) {
        fprintf(stderr, "cardreader: failed to load deck '%s'\n", cap_path);
        return -1;
    }

    struct cardreader_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        cap_free(deck);
        return -1;
    }

    ctx->deck  = deck;
    ctx->mode  = mode;
    ctx->state = CR_IDLE;

    /* Find the first non-empty card at or after first_card */
    ctx->card_idx = first_card < 0 ? 0 : first_card;
    ctx->col_idx  = 0;
    while (ctx->card_idx < cap_num_cards(deck) &&
           cap_card_ncols(deck, ctx->card_idx) == 0)
        ctx->card_idx++;

    /* The first card we feed is the loader card (read in `mode`); later cards
     * are program cards read in binary. */
    ctx->loader_card = ctx->card_idx;

    if (ctx->card_idx >= cap_num_cards(deck)) {
        /* All cards are empty — nothing to send */
        ctx->state     = CR_DONE;
        ctx->last_card = -1;
        ctx->last_col  = -1;
    } else {
        /* Pre-compute last non-empty card and its last column */
        ctx->last_card = ctx->card_idx; /* start with first non-empty */
        ctx->last_col  = 0;
        for (int i = ctx->card_idx; i < cap_num_cards(deck); i++) {
            int nc = cap_card_ncols(deck, i);
            if (nc > 0) {
                ctx->last_card = i;
                ctx->last_col  = nc - 1;
            }
        }
    }

    ge_log(LOG_READER,
           "cardreader: loaded '%s', %d cards, last non-empty card=%d col=%d\n",
           cap_path, cap_num_cards(deck), ctx->last_card, ctx->last_col);

    /* Initialise the ge_peri node embedded in ctx */
    ctx->peri.next     = NULL;
    ctx->peri.init     = NULL;
    ctx->peri.on_pulse = NULL;
    ctx->peri.on_clock = cardreader_on_clock;
    ctx->peri.deinit   = cardreader_deinit;
    ctx->peri.ctx      = ctx;

    return ge_register_peri(ge, &ctx->peri);
}
