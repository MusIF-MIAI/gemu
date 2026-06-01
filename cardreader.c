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

    /* Set to 1 when the last column of a card has been presented and the
     * cross-to-next-card advance has been deferred until the CPU raises the
     * TU03N end-of-card feed strobe (state ea). Honoured by the TU03 pulse
     * handler at the top of on_clock. The per-column advance within a card is
     * NOT gated by TU03 — that cadence stays on the lu08 read handshake. */
    int feed_pending;

    /* Packed / self-loading mode (cardreader_register_packed). The channel-1
     * input-transfer microcode always packs TWO presented nibbles into one
     * memory byte. A self-loading SMAC .cap holds the program as full COLBIN
     * bytes (1 col -> 1 byte), so to land each byte intact we present it as a
     * hi-then-lo nibble pair; the packer rebuilds the original byte. (This is
     * also how the real machine reads: the IPL packs the hex loader card, and
     * after "set by-pass" each binary column delivers a full byte — equivalent
     * to two packed nibbles here.) `half` tracks which nibble is pending. */
    int pack;
    int half;   /* 0 = high nibble pending, 1 = low nibble pending */

    /* the ge_peri node we allocated (kept for potential future use) */
    struct ge_peri peri;
};

/* Detect the functional-test Hollerith bootstrap loader among the early cards
 * of a mixed deck. The matching card is punched as hex nibbles and is the one
 * selected by a row-8 punch in column 3; after TC_HEX decode it begins with
 * repeated `PER 0x80` orders (`9E 80 ... 9E 80 ...`). Returns the card index,
 * or -1 if no such card is present. */
static int cr_find_hollerith_loader_card(struct cap_deck *deck)
{
    int ncards = cap_num_cards(deck);

    for (int i = 0; i < ncards && i < 16; i++) {
        int ncols = cap_card_ncols(deck, i);
        const uint16_t *cols;
        uint8_t b[6];

        if (ncols < 12)
            continue;

        cols = cap_card_columns(deck, i);
        if (!cols)
            continue;

        /* The correct CR10/Hollerith loader is identified in the deck notes by
         * a row-8 punch in column 3. */
        if (cols[2] != 0x0100)
            continue;

        for (int j = 0; j < 12; j++) {
            uint8_t nib = transcode_column(cols[j], TC_HEX) & 0x0f;
            if ((j & 1) == 0)
                b[j >> 1] = (uint8_t)(nib << 4);
            else
                b[j >> 1] |= nib;
        }

        if (b[0] == 0x9e && b[1] == 0x80 &&
            b[2] == 0x00 && b[4] == 0x9e && b[5] == 0x80)
            return i;
    }

    return -1;
}

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

    /* TU03N card-feed strobe (CE09), read as a one-cycle pulse: the CPU raises
     * it at end-of-card (state ea). We latch and clear it here at TO00 so it
     * reads as the previous cycle's pulse. A card-boundary advance deferred when
     * the last column was presented is the reader physically feeding the card
     * out and bringing the next under the read station — it happens in response
     * to this strobe rather than autonomously. */
    {
        int feed = ge->integrated_reader.tu03;
        ge->integrated_reader.tu03 = 0;
        if (feed && ctx->feed_pending) {
            cr_advance(ctx);
            ctx->feed_pending = 0;
        }
    }

    /* LUSEN (out-of-service) / LUREN (error or card jam): the reader cannot
     * deliver data — present nothing and report not-ready, so a read parks /
     * completes as unit-not-ready or in error rather than getting data. (Both
     * default 0: normal operation, no change.) */
    if (ge->integrated_reader.lusen || ge->integrated_reader.luren) {
        ge->integrated_reader.lupor = 0;
        return 0;
    }

    /* LUPOR (reader free / ready): asserted while the reader is not finished and
     * not presenting a byte. Held 0 whenever a byte is on the data lines
     * (lu08=1) so PELEA = !(LU08 . LUPO1) stays 1 (the read path is unchanged). */
    ge->integrated_reader.lupor =
        (ctx->state != CR_DONE) && !ge->integrated_reader.lu08;

    if (ctx->state == CR_DONE) {
        ge->integrated_reader.fiden = 1;   /* FIDEN: end-of-sequence (deck done) */
        return 0;
    }

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
        /* Read mode. If the CPU has driven a mode-select read command (the
         * loader's "set by-pass" — active_valid set by COCON; see reader.c) and
         * this is not a packed self-loading deck, the reader transcodes in the
         * CPU-selected active_mode. Otherwise the legacy harness selection holds:
         * packed decks use the registered COLBIN mode; legacy decks read the
         * loader card in the registered mode and the program cards in TC_BINARY
         * ("by-pass"). */
        enum transcode_mode m;
        if (ctx->pack)
            m = ctx->mode;
        else if (ctx->card_idx == ctx->loader_card)
            /* The loader card (first card) is read in the IPL's own loader mode
             * = the registered mode (TC_HEX for the real "4 hex loader cards",
             * which encode 0-F per column; the channel packs two columns into a
             * byte). The CPU-driven mode-select (active_mode, from COCON) only
             * offers NORMAL/BINARY and would corrupt an A-F hex nibble, so it
             * must NOT override the loader card — it applies to the PROGRAM
             * cards the loader's Set-by-Pass selects. */
            m = ctx->mode;
        else if (ge->integrated_reader.active_valid)
            m = ge->integrated_reader.active_mode;
        else
            m = TC_BINARY;
        uint8_t byte = transcode_column(cols[ctx->col_idx], m);

        /* End-of-card: the GE reader raises FINI (the controller "end" RIG1)
         * at every physical card boundary. Per CPU[4] §5.8.4.3 a transfer ends
         * on (a) the instruction length L1+1 being exhausted, or (b) this
         * peripheral "end". gemu now counts L1 down (the counting network honours
         * the decrement), but the L1-exhausted -> RIVE path isn't fully wired for
         * the integrated-reader handshake yet, so end-of-card (b) still bounds a
         * read here. (Note: RAMO/RAMI is the CPU cycle-period counter, §6.7.5, not
         * the read-length counter.) Signal end on the last column of the CURRENT
         * card, not the whole deck. */
        int is_last_col = (ctx->col_idx == ncols - 1);

        /* The channel-1 input transfer packs TWO presented nibbles into one
         * memory byte. In packed mode the .cap holds full COLBIN bytes, so we
         * present each as a hi-then-lo nibble pair and the packer rebuilds the
         * byte (1 col -> 1 byte). FINI rides the LOW nibble of the last column.
         * In legacy mode one full value is presented per column. */
        uint8_t present;
        int     is_last;
        if (ctx->pack) {
            present = (ctx->half == 0) ? (uint8_t)((byte >> 4) & 0x0f)
                                       : (uint8_t)(byte & 0x0f);
            is_last = (ctx->half == 1) && is_last_col;
        } else {
            present = byte;
            is_last = is_last_col;
        }

        ge_log(LOG_READER,
               "cardreader: presenting card %d col %d half %d byte=0x%02x val=0x%02x end=%d\n",
               ctx->card_idx, ctx->col_idx, ctx->half, byte, present, is_last);

        /* Drive the reader's observable feed-state lines for this character:
         *   POM01 — binary-mode (by-pass) indicator: high for raw binary reads.
         *   PICON — first-column check: high on column 0 of a card.
         *   BI20  — binary 2nd-nibble clock: high while the low nibble of a
         *           packed binary column is on the lines (the second sub-read).
         * Nothing in the CPU state logic consumes these yet; they make the read
         * mode / framing visible at the pins. */
        ge->integrated_reader.pom01 = (m == TC_BINARY || m == TC_COLBIN);
        ge->integrated_reader.picon = (ctx->col_idx == 0);
        ge->integrated_reader.bi20  = (ctx->pack && ctx->half == 1);

        reader_setup_to_send(ge, present, is_last ? 1 : 0);
        ctx->end_of_card_presented = is_last;
        ctx->state = CR_PRESENTED;

        /* Advance. In packed mode present the LOW nibble of the same column on
         * the next call (do not move the pointer); only after the low nibble do
         * we advance to the next column / card. For a multi-card deck cr_advance
         * moves to (card+1, col 0), ready for the next PER read. */
        if (ctx->pack && ctx->half == 0) {
            ctx->half = 1;
        } else {
            ctx->half = 0;
            if (is_last_col) {
                /* Card boundary: defer the cross-to-next-card feed until the
                 * CPU's TU03N end-of-card strobe (state ea). */
                ctx->feed_pending = 1;
            } else {
                cr_advance(ctx);
            }
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

static int cr_register(struct ge *ge, const char *cap_path,
                       enum transcode_mode mode, int first_card, int pack);

int cardreader_register(struct ge *ge, const char *cap_path,
                        enum transcode_mode mode)
{
    return cr_register(ge, cap_path, mode, 0, 0);
}

int cardreader_register_from(struct ge *ge, const char *cap_path,
                             enum transcode_mode mode, int first_card)
{
    return cr_register(ge, cap_path, mode, first_card, 0);
}

/* Self-loading SMAC deck: feed every card's COLBIN bytes as hi/lo nibble pairs
 * so the channel-1 packing transfer reconstructs the full bytes (1 col -> 1
 * byte), letting the deck's own loader chain (PER reads + MVC relocation)
 * assemble the program in memory. */
int cardreader_register_packed(struct ge *ge, const char *cap_path,
                               enum transcode_mode mode)
{
    return cr_register(ge, cap_path, mode, 0, 1);
}

static int cr_register(struct ge *ge, const char *cap_path,
                       enum transcode_mode mode, int first_card, int pack)
{
    struct cap_deck *deck = cap_load(cap_path);
    int auto_loader = -1;
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
    ctx->pack  = pack;
    ctx->half  = 0;
    ctx->state = CR_IDLE;

    /* A plain `cardreader_register(..., TC_NORMAL)` on a real mixed deck should
     * start from the Hollerith bootstrap loader, not from whichever control
     * card happens to be first in the capture. Once identified, read that one
     * card in TC_HEX; later cards still switch to binary via the normal
     * active-mode/by-pass path. */
    if (!pack && mode == TC_NORMAL && first_card == 0)
        auto_loader = cr_find_hollerith_loader_card(deck);
    if (auto_loader >= 0)
        ctx->mode = TC_HEX;

    /* Find the first non-empty card at or after first_card */
    ctx->card_idx = auto_loader >= 0 ? auto_loader : (first_card < 0 ? 0 : first_card);
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
