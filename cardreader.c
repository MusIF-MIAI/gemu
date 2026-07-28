/*
 * cardreader.c - Connector-2 punch-card-reader peripheral for the GE-120 emulator.
 *
 * Feeds a .cap punch-card deck into the CPU via the existing integrated-reader
 * handshake (reader_setup_to_send / reader_clear_sending), mirroring the
 * manual cadence demonstrated in tests/initial-load.c.
 *
 * This is a behavioural twin of the Pico reader that replaces the real machine's
 * CRZ transport (rpi-pico-card-reader/src/feeder.c, wire_tx.c). The same state
 * machine, the same LUPOB predicate, the same trigger, the same loader-card
 * rule -- so a deck that boots here boots on the iron, and a trace from one can
 * be read against a trace from the other.
 *
 * The wire order, as measured on the bench in July 2026:
 *
 *   1. The CPU puts an order byte on RE00-07 and strobes TU00N (CE10, states
 *      ca and ab). The reader latches it. NOTHING is strobed back.
 *   2. The CPU raises TU03N (CE09, state b8 TI10). THAT is what starts the card
 *      moving. Presenting on the command instead hands the card to a channel
 *      that has not armed its transfer yet, and the card is lost.
 *   3. Columns go out one presentation per input cycle, each with an LU08N
 *      strobe; the last one carries FININ.
 *   4. FININ, and the data that rode with it, STAY on the pins until the next
 *      read command, the next TU03N, or a timeout.
 *
 * Within step 3 the per-character cadence is the one tests/initial-load.c
 * documents. on_clock runs once per machine cycle at TO00:
 *
 *   reader_setup_to_send(...)   -- before the b8/b9 cycle
 *   [ge_run_cycle b8/b9]        -- machine reads the nibble into mem[]
 *   reader_clear_sending(...)   -- before the b1 cycle
 *   [ge_run_cycle b1]           -- machine packs the nibble
 *
 * The IPL reads exactly ONE card: 80 columns nibble-packed into 40 bytes at
 * address 0, then executed. Every card after that is pulled by a PER read the
 * loaded program issues itself -- which arrives here as another command, another
 * feed, another burst. The machine does not read decks; programs do.
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

/*
 * Feeding state machine, the same one the Pico reader runs
 * (rpi-pico-card-reader/src/feeder.c, enum feeder_state).
 *
 *   ARMED_WAIT  a card is under the read station; nothing is moving. A read
 *               command latches (cmd_armed) but strobes nothing.
 *   PRESENTING  the CPU's TU03N feed arrived and the card is being read out,
 *               one presentation per input cycle.
 *   CARD_DONE   the last column has gone by with FININ riding it. FININ is
 *               still standing, and so is the last nibble. It is released by
 *               the next read command, by a TU03N, or by a timeout.
 *   DONE        the deck ran out: FIDEN active, reader busy.
 *   ERROR       LUREN / LUSEN: nothing is delivered.
 */
enum cr_state {
    CR_ARMED_WAIT,
    CR_PRESENTING,
    CR_CARD_DONE,
    CR_DONE,
    CR_ERROR,
};

/*
 * Fallbacks, in elementary cycles (one cycle = 4 us on a 120).
 *
 * CR_CMD_TIMEOUT is the Pico's D: the longest a latched read command waits for
 * the feed before presenting anyway (50 ms there). It should never fire against
 * a healthy machine -- CE09 raises TU03N in state b8, a handful of cycles after
 * the command -- so if it does, something upstream stopped feeding.
 *
 * CR_FININ_TIMEOUT is the Pico's finin_to_us (1 ms): how long FININ may stand
 * before the reader drops it unasked.
 */
#define CR_CMD_TIMEOUT_CYCLES   12500   /* 50 ms */
#define CR_FININ_TIMEOUT_CYCLES   250   /*  1 ms */

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

    /* A presentation is on the lines and the machine has not consumed it yet.
     * The next on_clock retires it; the one after presents again. That gap is
     * the b1 pack cycle. */
    int awaiting_retire;

    /* Set to 1 when the end-of-card presentation (the one carrying FININ) was
     * put on the lines. */
    int end_of_card_presented;

    /* A read command is latched and waiting for the CPU's TU03N feed. Nothing
     * is strobed until then: the wire order is command, feed, strobes, and
     * presenting on the command hands the card to a channel that is not armed
     * yet. cmd_wait counts down to the autofeed fallback. */
    int cmd_armed;
    int cmd_wait;

    /* Cycles left before a standing FININ is released unasked. */
    int finin_wait;

    /* Diagnostics: feeds honoured, and feeds we had to invent. */
    unsigned n_feeds;
    unsigned n_autofeeds;

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
    int post_loader_pack;

    /* the ge_peri node we allocated (kept for potential future use) */
    struct ge_peri peri;
};

/*
 * Find the bootstrap loader card in a mixed deck.
 *
 * This is the SHARED rule -- the Pico reader runs the identical one
 * (rpi-pico-card-reader/src/deck.c deck_find_loader_card), so a deck that
 * bootstraps here bootstraps on the iron:
 *
 *   marker  a row-8 punch in column 3, i.e. cols[2] == 0x0100 exactly
 *   window  card indices 0..4 inclusive, first match wins
 *
 * Card 0 has to be in the window. A captured box deck leads with a title card
 * and puts the loader at 1 or later, but a deck synthesized by gasm --boot /
 * --bootge leads with the loader itself; a window starting at 1 feeds a body
 * card to the IPL, which then nibble-packs 40 bytes of program payload to
 * address 0 and executes them.
 *
 * Returns the card index, or -1 if no card in the window carries the marker.
 * The TC_HEX decode of the head is checked too, but only to WARN: the marker is
 * what selects, on both sides of the wire.
 */
static int cr_find_hollerith_loader_card(struct cap_deck *deck)
{
    int ncards = cap_num_cards(deck);
    int limit = ncards < 5 ? ncards : 5;

    for (int i = 0; i < limit; i++) {
        int ncols = cap_card_ncols(deck, i);
        const uint16_t *cols;
        uint8_t b[6];

        if (ncols < 12)
            continue;

        cols = cap_card_columns(deck, i);
        if (!cols)
            continue;

        if (cols[2] != 0x0100)
            continue;

        for (int j = 0; j < 12; j++) {
            uint8_t nib = transcode_column(cols[j], TC_HEX) & 0x0f;
            if ((j & 1) == 0)
                b[j >> 1] = (uint8_t)(nib << 4);
            else
                b[j >> 1] |= nib;
        }

        /* A real bootstrap card opens with repeated `PER 0x80` orders. If this
         * one does not, it is still the card the machine will read -- say so
         * rather than quietly picking a different one. */
        if (!(b[0] == 0x9e && b[1] == 0x80 &&
              b[2] == 0x00 && b[4] == 0x9e && b[5] == 0x80))
            ge_log(LOG_READER,
                   "cardreader: card %d carries the row-8 loader marker but "
                   "decodes to %02x %02x %02x .. %02x %02x, not the usual "
                   "9E 80 00 .. 9E 80\n",
                   i, b[0], b[1], b[2], b[4], b[5]);

        return i;
    }

    return -1;
}

/* -------------------------------------------------------------------------
 * Which mode is this card read in
 * ------------------------------------------------------------------------- */

/*
 * The effective read mode for the card currently under the station. Same rule
 * as the Pico's effective_mode() (src/feeder.c):
 *
 *   - the loader card is always read in the mode the deck was registered with
 *     (TC_HEX for a real bootstrap card). The CPU's mode-select only offers
 *     normal and binary, and either would corrupt an A-F hex nibble, so it must
 *     not override this one card.
 *   - after the loader, program cards are by-pass / column-binary. The loader's
 *     own "set by-pass" is what selects that on the iron; which RE byte it
 *     actually sends is still open (loader.s reads it as 0x40 under Z=0x80,
 *     gemu latches by-pass on 0x20), so both sides force COLBIN here rather
 *     than depend on the answer.
 *   - otherwise: whatever the CPU last latched via COCON, or raw binary.
 */
static enum transcode_mode cr_effective_mode(struct ge *ge,
                                             struct cardreader_ctx *ctx)
{
    if (ctx->pack)
        return ctx->mode;
    if (ctx->card_idx == ctx->loader_card)
        return ctx->mode;
    if (ctx->post_loader_pack)
        return TC_COLBIN;
    if (ge->integrated_reader.active_valid)
        return ge->integrated_reader.active_mode;
    return TC_BINARY;
}

/* Whether this card's columns go out as hi/lo nibble pairs (a whole byte per
 * column) or one presentation per column (a hex nibble). */
static int cr_pack_now(struct cardreader_ctx *ctx, enum transcode_mode m)
{
    (void)m;
    return ctx->pack || (ctx->post_loader_pack &&
                         ctx->card_idx != ctx->loader_card);
}

/* -------------------------------------------------------------------------
 * Peripheral callbacks
 * ------------------------------------------------------------------------- */

/* Move the cursor to the next card, or run out of deck. */
static void cr_next_card(struct cardreader_ctx *ctx)
{
    ctx->col_idx = 0;
    ctx->half = 0;
    ctx->card_idx++;
    while (ctx->card_idx < cap_num_cards(ctx->deck) &&
           cap_card_ncols(ctx->deck, ctx->card_idx) == 0)
        ctx->card_idx++;

    if (ctx->card_idx >= cap_num_cards(ctx->deck)) {
        ctx->state = CR_DONE;
        ge_log(LOG_READER, "cardreader: deck exhausted\n");
    } else {
        ctx->state = CR_ARMED_WAIT;
    }
}

/*
 * Drop FININ and the data that rode with it.
 *
 * On the wire, FININ is asserted with the last presentation of a card and STAYS
 * asserted after the strobe -- as does the last nibble on LU00-07. It comes down
 * on one of three things: the next read command, a TU03N, or a timeout. This is
 * the release; note that it deliberately does not touch LUPOB (see
 * cr_lupob_update and its callers for why the fronts land where they do).
 */
static void cr_finin_release(struct ge *ge, struct cardreader_ctx *ctx)
{
    ge->integrated_reader.fini = 0;
    ge->integrated_reader.data = 0;
    ctx->finin_wait = 0;
}

/*
 * LUPOB / LUPOR, one predicate, exactly the Pico's (src/feeder.c lupob_update).
 *
 * Ready means: a deck is in the machine, nothing is being presented, no command
 * is waiting for its feed, no FININ is standing, and we are parked either
 * before a card or after one. Everything else -- presenting, error, deck
 * exhausted, no deck at all -- is busy.
 *
 * The bench reading that settles the polarity: on the wire LOW is ready and
 * HIGH is busy, so an unpowered or unarmed reader reads busy. gemu's internal
 * LUPO1 ("reader free") is the logical sense, i.e. the inverse of the wire.
 *
 * Consequence worth knowing when reading a trace: in the steady loader loop
 * there is NO ready front between cards. The next read command releases FININ
 * and latches cmd_armed before this runs, so the reader goes straight from one
 * card's presentation to the next card's wait without ever reporting ready.
 */
static void cr_lupob_update(struct ge *ge, struct cardreader_ctx *ctx)
{
    struct ge_integrated_reader *r = &ge->integrated_reader;

    r->lupor = (ctx->deck != NULL) &&
               !ctx->cmd_armed &&
               !r->fini &&
               (ctx->state == CR_ARMED_WAIT || ctx->state == CR_CARD_DONE);
}

/* The feed has arrived (or we gave up waiting for it): start reading out the
 * card under the station. */
static void cr_trigger_present(struct ge *ge, struct cardreader_ctx *ctx)
{
    enum transcode_mode m;

    ctx->cmd_armed = 0;
    ctx->cmd_wait = 0;

    if (ctx->state == CR_CARD_DONE)
        cr_next_card(ctx);
    if (ctx->state != CR_ARMED_WAIT)
        return;                      /* deck exhausted, or already presenting */

    /* POM01 is re-evaluated per card from the mode this card will be read in:
     * active for a raw/by-pass read, inactive for the transcoded hex loader. */
    m = cr_effective_mode(ge, ctx);
    ge->integrated_reader.pom01 = (m == TC_BINARY || m == TC_COLBIN);

    ctx->state = CR_PRESENTING;
    ge_log(LOG_READER, "cardreader: feed -> presenting card %d (mode %d)\n",
           ctx->card_idx, (int)m);
}

/*
 * on_clock: called at TO00, the first clock of every new machine cycle.
 *
 * The order of business here is the order the wire shows on the bench:
 *
 *   1. TU00N carries a read command   -> latch it, drop any standing FININ.
 *      (reader.c does that half; what arrives here is cmd_pending.)
 *   2. TU03N arrives (CE09, state b8) -> THIS is what starts the card moving.
 *   3. presentations, one per input cycle, until the last column carries FININ.
 *   4. FININ stands until the next command, the next TU03N, or the timeout.
 *
 * Within step 3 the cadence is the one tests/initial-load.c documents:
 *
 *     [b9 cycle, lu08=1]   machine reads the nibble
 *     on_clock: retire     lu08 -> 0, and return
 *     [b1 cycle, lu08=0]   machine packs the nibble
 *     on_clock: present    lu08 -> 1
 *
 * so a presentation is never replaced in the same cycle it was consumed.
 */
static int cardreader_on_clock(struct ge *ge, void *opaque)
{
    struct cardreader_ctx *ctx = (struct cardreader_ctx *)opaque;
    struct ge_integrated_reader *r = &ge->integrated_reader;
    int feed;
    int pack_now;

    /* LESAB: a reader is on the connector. A strap, not a signal -- it is true
     * for as long as this peripheral is registered. */
    r->lesab = 1;

    /* TU03N, read as the previous cycle's pulse (CE09 fires at TI10, late). */
    feed = r->tu03;
    r->tu03 = 0;
    if (feed)
        ctx->n_feeds++;

    /* LUSEN (out-of-service) / LUREN (error or card jam): the reader cannot
     * deliver. Present nothing and report busy, so a read parks or completes in
     * error rather than getting data. Both default 0. */
    if (r->lusen || r->luren) {
        ctx->state = CR_ERROR;
        ctx->cmd_armed = 0;
        r->cmd_pending = 0;
        cr_lupob_update(ge, ctx);
        return 0;
    }
    if (ctx->state == CR_ERROR)
        ctx->state = (ctx->card_idx < cap_num_cards(ctx->deck))
                   ? CR_ARMED_WAIT : CR_DONE;

    /* Card reject (RE 0x48): this card leaves unread, and no transfer follows. */
    if (r->cmd_reject) {
        r->cmd_reject = 0;
        cr_finin_release(ge, ctx);
        if (ctx->state == CR_CARD_DONE || ctx->state == CR_ARMED_WAIT)
            cr_next_card(ctx);
        ge_log(LOG_READER, "cardreader: card reject -> card %d\n", ctx->card_idx);
    }

    if (ctx->state == CR_DONE) {
        r->fiden = 1;              /* FIDEN: end of sequence */
        r->cmd_pending = 0;
        ctx->cmd_armed = 0;
        cr_lupob_update(ge, ctx);
        return 0;
    }

    /* A read command latches; it does not strobe. FININ came down in reader.c,
     * on the command itself, before anything here touches LUPOB. */
    if (r->cmd_pending) {
        r->cmd_pending = 0;
        if (ctx->state == CR_ARMED_WAIT || ctx->state == CR_CARD_DONE) {
            ctx->cmd_armed = 1;
            ctx->cmd_wait = CR_CMD_TIMEOUT_CYCLES;
            ctx->finin_wait = 0;
        }
        /* A command arriving mid-presentation is dropped, as on the Pico. */
    }

    /* TU03N is the trigger. */
    if (feed) {
        if (ctx->cmd_armed) {
            cr_finin_release(ge, ctx);
            cr_trigger_present(ge, ctx);
        } else if (ctx->state == CR_CARD_DONE) {
            /* A feed with nothing pending is the reader physically putting the
             * finished card out and bringing the next one under the station. */
            cr_finin_release(ge, ctx);
            cr_next_card(ctx);
        }
    } else if (ctx->cmd_armed && ctx->cmd_wait > 0 && --ctx->cmd_wait == 0) {
        ctx->n_autofeeds++;
        ge_log(LOG_READER,
               "cardreader: no TU03N within %d cycles; presenting anyway "
               "(autofeed %u)\n", CR_CMD_TIMEOUT_CYCLES, ctx->n_autofeeds);
        cr_finin_release(ge, ctx);
        cr_trigger_present(ge, ctx);
    }

    /* A standing FININ that nobody came for. */
    if (ctx->finin_wait > 0 && --ctx->finin_wait == 0) {
        ge_log(LOG_READER, "cardreader: FININ held %d cycles; releasing\n",
               CR_FININ_TIMEOUT_CYCLES);
        cr_finin_release(ge, ctx);
    }

    if (ctx->state != CR_PRESENTING) {
        cr_lupob_update(ge, ctx);
        return 0;
    }

    /* Retire the previous presentation, and wait a cycle before the next: that
     * gap is the machine's b1 pack cycle. */
    if (ctx->awaiting_retire) {
        ctx->awaiting_retire = 0;
        reader_clear_sending(ge);      /* lu08 down; FININ and data stay put */
        if (ctx->end_of_card_presented) {
            ctx->end_of_card_presented = 0;
            ctx->state = CR_CARD_DONE;
            ctx->finin_wait = CR_FININ_TIMEOUT_CYCLES;
        }
        cr_lupob_update(ge, ctx);
        return 0;
    }

    /*
     * Only present while the machine is in the channel-1 transfer phase. RASI
     * is set at state ab, just before the input-wait loop; before that a
     * presentation would be ignored and the column consumed for nothing.
     */
    if (!ge->RASI || r->lu08) {
        cr_lupob_update(ge, ctx);
        return 0;
    }

    {
        int ncols = cap_card_ncols(ctx->deck, ctx->card_idx);
        const uint16_t *cols = cap_card_columns(ctx->deck, ctx->card_idx);
        enum transcode_mode m = cr_effective_mode(ge, ctx);
        uint8_t byte;
        uint8_t present;
        int is_last_col;
        int is_last;

        if (ncols <= 0 || !cols || ctx->col_idx >= ncols) {
            ctx->state = CR_DONE;
            cr_lupob_update(ge, ctx);
            return 0;
        }

        byte = transcode_column(cols[ctx->col_idx], m);
        pack_now = cr_pack_now(ctx, m);
        is_last_col = (ctx->col_idx == ncols - 1);

        /*
         * The channel-1 input transfer packs TWO presentations into one memory
         * byte. A by-pass column carries a whole byte, so it goes out as a
         * hi-then-lo nibble pair and the packer rebuilds it; a hex loader
         * column is already one nibble and goes out whole. FININ rides the LOW
         * nibble of the last column.
         */
        if (pack_now) {
            present = (ctx->half == 0) ? (uint8_t)((byte >> 4) & 0x0f)
                                       : (uint8_t)(byte & 0x0f);
            is_last = (ctx->half == 1) && is_last_col;
        } else {
            present = byte;
            is_last = is_last_col;
        }

        ge_log(LOG_READER,
               "cardreader: presenting card %d col %d half %d byte=0x%02x "
               "val=0x%02x end=%d\n",
               ctx->card_idx, ctx->col_idx, ctx->half, byte, present, is_last);

        /* PICON: first column of a card. BI20: the low nibble of a packed
         * binary column. Both are harness-side wires, not COCA pins. */
        r->picon = (ctx->col_idx == 0);
        r->bi20  = (pack_now && ctx->half == 1);

        reader_setup_to_send(ge, present, is_last ? 1 : 0);
        ctx->end_of_card_presented = is_last;
        ctx->awaiting_retire = 1;

        /* Advance the cursor. Within a packed column the low nibble follows
         * without moving on; the card boundary is NOT crossed here -- the card
         * stays under the station until a feed says otherwise. */
        if (pack_now && ctx->half == 0) {
            ctx->half = 1;
        } else {
            ctx->half = 0;
            if (!is_last_col)
                ctx->col_idx++;
        }
    }

    cr_lupob_update(ge, ctx);
    return 0;
}

static int cardreader_deinit(struct ge *ge, void *opaque)
{
    struct cardreader_ctx *ctx = (struct cardreader_ctx *)opaque;
    if (ge)
        ge->integrated_reader.lesab = 0;   /* the unit leaves the connector */
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
    ctx->state = CR_ARMED_WAIT;

    /* A plain `cardreader_register(..., TC_NORMAL)` on a real mixed deck should
     * start from the Hollerith bootstrap loader, not from whichever control
     * card happens to be first in the capture. Once identified, read that one
     * card in TC_HEX; later cards still switch to binary via the normal
     * active-mode/by-pass path. */
    if (!pack && mode == TC_NORMAL && first_card == 0) {
        auto_loader = cr_find_hollerith_loader_card(deck);

        /* No marker anywhere in the window. If the deck is a single card,
         * that card IS the loader -- there is nothing else it could be, and
         * the IPL is going to nibble-pack it to 0x0000 and execute it either
         * way. This is how a `gasm --card` boot card boots, and it matches the
         * Pico's own single-card fallback (deck.c: loader_card = 0 when the
         * deck holds one card). */
        if (auto_loader < 0) {
            int ncards = 0, only = -1;
            for (int i = 0; i < cap_num_cards(deck); i++) {
                if (cap_card_ncols(deck, i) > 0) {
                    if (only < 0)
                        only = i;
                    ncards++;
                }
            }
            if (ncards == 1) {
                auto_loader = only;
                ge_log(LOG_READER,
                       "cardreader: one-card deck; card %d is its own loader\n",
                       only);
            }
        }
    }
    if (auto_loader >= 0)
        ctx->mode = TC_HEX;
    ctx->post_loader_pack = !ctx->pack && ctx->mode == TC_HEX;

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

    /* LESAB: a card reader is on connector 2. A strap, true from the moment the
     * unit is on the connector -- not from its first clock -- because other
     * peripherals ask this question before the reader has run (printer.c uses it
     * to know whose order a shared wait belongs to). */
    ge->integrated_reader.lesab = 1;

    /* Initialise the ge_peri node embedded in ctx */
    ctx->peri.next     = NULL;
    ctx->peri.init     = NULL;
    ctx->peri.on_pulse = NULL;
    ctx->peri.on_clock = cardreader_on_clock;
    ctx->peri.deinit   = cardreader_deinit;
    ctx->peri.ctx      = ctx;

    return ge_register_peri(ge, &ctx->peri);
}
