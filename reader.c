#include "ge.h"
#include "log.h"
#include "signals.h"
#include "connector34.h"

#define ENUMERATE_READER_COMMANDS \
    X(0x40, read,          "Read unchanged") \
    X(0x21, read_normal_1, "Read normal i" ) \
    X(0x01, read_normal_2, "Read normal ii") \
    X(0x24, read_mixed_1,  "Read mixed i"  ) \
    X(0x04, read_mixed_2,  "Read mixed ii" ) \
    X(0x20, read_binary,   "Read binary"   ) \
    X(0xa1, put_normal_1,  "Put normal i"  ) \
    X(0x81, put_normal_2,  "Put normal ii" ) \
    X(0xa4, put_mixed_1,   "Put mixed i"   ) \
    X(0x84, put_mixed_2,   "Put mixed ii"  ) \
    X(0xa0, put_binary,    "Put binary"    ) \
    X(0xac, put_manual,    "Put manual"    ) \
    X(0x44, exam_cond,     "Exam of conditions") \
    X(0x47, reset_error,   "Reset error"   ) \
    X(0x48, card_reject,   "Card_reject"   ) \
    X(0x0c, no_function,   "No function"   )

/*
 * TU00N, the command strobe. The CPU has put an order byte on RE00-07; this is
 * the edge that hands it to the reader.
 *
 * The command matrix is the one the Pico firmware implements (src/feeder.c
 * feeder_on_re_cmd), so a deck that drives one reader drives the other:
 *
 *   0x40             read unchanged   -- the IPL's own order, and the loader's
 *   0x21 0x01        read normal i/ii -- latch the Hollerith transcoder
 *   0x24 0x04        read mixed i/ii  -- likewise
 *   0x20             read binary      -- latch by-pass (column binary)
 *   0xa0             put binary       -- latch by-pass, but never a transfer
 *   0x44             exam conditions  -- nothing to do: the status lines are
 *                                        always valid, the CPU just samples them
 *   0x47             reset error      -- drop LUREN
 *   0x48             card reject      -- eject this card without reading it
 *   0x0c             no function      -- nothing
 *   anything else                     -- counted as an anomaly, feed undisturbed
 */
void reader_send_tu00(struct ge *ge)
{
    struct ge_integrated_reader *r = &ge->integrated_reader;
    uint8_t command = ge->rRE;
    int known = 0;
    int latch = 1;
    int is_read = 0;

    ge_log(LOG_READER, "EMIT TU201 (CE10)\n");

    /* TU00N read-strobe line: the CPU clocks the command byte (rRE) toward the
     * reader. Active for this one strobe. */
    r->tu00 = 1;

    switch (command) {
#define X(cmd, name, desc) \
        case cmd: \
            ge_log(LOG_READER, "    Command: %02x - %s\n", cmd, desc ); \
            known = 1; \
            break;
            ENUMERATE_READER_COMMANDS
#undef X
    }
    if (!known) {
        r->anomalies++;
        ge_log(LOG_READER, "    Command: %02x - UNKNOWN (anomaly %u)\n",
               command, (unsigned)r->anomalies);
    }

    /* COCON — mode-select clock. An explicit mode-select READ command drives the
     * N001/N002/DEBI/MI01/MI02 decode and latches the CPU-selected read mode
     * (active_mode/active_valid). The plain "read unchanged" (0x40) and the
     * put/reject/manual commands leave the mode as-is, so the bootstrap (which
     * issues 0x40) never engages the CPU mode and the cardreader keeps its
     * harness/default mode. This is the loader's "set by-pass" actually
     * switching normal<->binary. */
    r->mode_n001 = r->mode_n002 = r->mode_debi = r->mode_mi01 = r->mode_mi02 = 0;
    switch (command) {
    case 0x21: r->mode_n001 = 1; r->active_mode = TC_NORMAL; break; /* read normal i  */
    case 0x01: r->mode_n002 = 1; r->active_mode = TC_NORMAL; break; /* read normal ii */
    case 0x24: r->mode_mi01 = 1; r->active_mode = TC_NORMAL; break; /* read mixed i   */
    case 0x04: r->mode_mi02 = 1; r->active_mode = TC_NORMAL; break; /* read mixed ii  */
    case 0x20: r->mode_debi = 1; r->active_mode = TC_COLBIN; break; /* read by-pass / column-binary */
    case 0xa0: r->mode_debi = 1; r->active_mode = TC_COLBIN; break; /* channel code card: with by-pass */
    default:   latch = 0; break;                                    /* 0x40 etc.: keep mode */
    }
    if (latch) { r->cocon = 1; r->active_valid = 1; }

    switch (command) {
    case 0x40: case 0x21: case 0x01: case 0x24: case 0x04: case 0x20:
        is_read = 1;
        break;
    case 0x47:
        /* Reset error: clear the error line and let the feed resume. */
        r->luren = 0;
        break;
    case 0x48:
        /* Card reject: this card leaves the station unread. */
        r->cmd_reject = 1;
        break;
    default:
        break;
    }

    /* A read command latches and waits for the feed. Releasing a standing FININ
     * happens HERE, on the command -- before any ready/busy update -- which is
     * why the steady loader loop shows no LUPOB ready front between cards. */
    if (is_read) {
        r->fini = 0;
        r->cmd_pending = 1;
    }
}

void reader_setup_to_send(struct ge *ge, uint8_t data, uint8_t end)
{
    ge->integrated_reader.lu08 = 1;
    ge->integrated_reader.data = data;
    ge->integrated_reader.fini = end;

    /* A byte is now on the data lines: the reader is busy, not free.  Holding
     * LUPOR (reader free) at 0 whenever LU08=1 keeps PELEA = !(LU08 . LUPO1) at
     * 1, so the read data path is unchanged by the ready line. */
    ge->integrated_reader.lupor = 0;

    /* When end=1, set the end-of-transfer flip-flops.
     *
     * In the hardware, FINI1 contributes to RF101 through PF12A when the
     * integrated reader is selected on channel 1 (PC121=1). RF101 is then
     * stored into RIG1 on the channel timing edge. Here we short-cut that
     * path and set RIG1 directly because this helper is only used by the
     * integrated-reader path.
     *
     * PEC1 is a second shortcut, but one stage later. The manual routes
     * peripheral-end completion through the TO50/PIM11 reset chain; gemu now
     * records that a PEC1 set is pending here and commits it from pulse.c at
     * TO50, instead of raising PEC1 immediately in the peripheral helper.
     */
    if (end) {
        ge->RIG1 = 1;
        ge->PEC1_pending = 1;
    }

    if (RB111(ge)) {
        ge_log(LOG_READER, "XXX\n");
    }
}

/*
 * Retire the character strobe.
 *
 * LU08N comes down and that is all. FININ does NOT: on the wire the end-of-card
 * word is still standing on the pins after its strobe has gone, because the
 * presenter has stalled with nothing left to shift out. It stays there until
 * the next read command (reader_send_tu00), a TU03N, or the reader's own
 * timeout takes it down -- see cardreader.c cr_finin_release.
 *
 * The data lines are left alone for the same reason: what the GE sees between
 * cards is the last nibble of the last card, not zeroes.
 */
void reader_clear_sending(struct ge *ge)
{
    ge->integrated_reader.lu08 = 0;
}

void reader_send_tu10(struct ge *ge)
{
    ge_log(LOG_READER, "EMIT TU101 (CE09)\n");

    /* LENON ("not operable") inhibits the card-feed: a non-operable reader
     * does not advance under the CPU's feed strobe. */
    if (ge->integrated_reader.lenon) {
        ge_log(LOG_READER, "    Card feed INHIBITED (LENON / not operable)\n");
        return;
    }

    ge_log(LOG_READER, "    Card feed\n");

    /* TU03N card-feed/advance line. Modelled as an explicit pin (Phase 4
     * switches the cardreader's deck-advance onto this line). */
    ge->integrated_reader.tu03 = 1;
}

uint8_t reader_get_LU08(struct ge *ge)
{
    ge_log(LOG_READER, "reading LU081 -- character strobe\n");

    if (ge->integrated_reader.lu08) {
        ge_log(LOG_READER,
               "    wanting to send char: %02x\n",
               ge->integrated_reader.data);
    }

    return ge->integrated_reader.lu08;
}

uint8_t reader_get_LUPO1(struct ge *ge)
{
    /* LUPOR: reader free / ready. Defaults 0 (== the previous hardcoded stub),
     * so this is inert until Phase 3 drives `lupor`. */
    return ge->integrated_reader.lupor;
}

uint8_t reader_get_FINI1(struct ge *ge)
{
    ge_log(LOG_READER, "**** reading FINI1 %d\n", ge->integrated_reader.fini);
    return ge->integrated_reader.fini;
}

uint8_t connector_get_MARE(struct ge_connector *conn)
{
    ge_log(LOG_READER, "%s -- connector_get_MARE\n", conn->name);
    return conn->mare;
}

uint8_t connector_get_TE10(struct ge_connector *conn)
{
    ge_log(LOG_READER, "%s -- connector_get_TE10\n", conn->name);
    return conn->te10;
}

uint8_t connector_get_TE20(struct ge_connector *conn)
{
    ge_log(LOG_READER, "%s -- connector_get_TE20\n", conn->name);
    return conn->te20;
}

uint8_t connector_get_TE30(struct ge_connector *conn)
{
    ge_log(LOG_READER, "%s -- connector_get_TE30\n", conn->name);
    return conn->te30;
}

uint8_t connector_get_FINE(struct ge_connector *conn)
{
    ge_log(LOG_READER, "%s -- connector_get_FINE\n", conn->name);
    return conn->fine;
}

void connector_setup_to_send(struct ge *ge, struct ge_connector *conn, uint8_t data, uint8_t end)
{
    /* equivalent of lu08, but not sure if it's TE10 or TE20, seems or-red together
     * (intermediate fo. 11, D1, D2) */

    conn->te10 = 1;
    conn->te20 = 1;
    conn->data = data;
    conn->fine = end;

    /* Mirror the same end-of-transfer signalling as reader_setup_to_send.
     * The connector FINE inputs feed RF101 through PF13A/PF14A when the
     * external unit is selected; gemu still short-circuits the subsequent
     * RIG1 timing path here, but now defers PEC1 to the TO50 latch point. */
    if (end) {
        ge->RIG1 = 1;
        ge->PEC1_pending = 1;
    }

    if (RB111(ge)) {
        ge_log(LOG_READER, "XXX\n");
    }
}

void connector_clear_sending(struct ge_connector *conn)
{
    conn->te10 = 0;
    conn->te20 = 0;
    conn->data = 0;
    /* Clear end-of-card strobe to deassert FINE/RF10x for next card. */
    conn->fine = 0;
}

void connector_send_tu00(struct ge *ge, struct ge_connector *conn)
{
    uint8_t command = ge->rRE;

    switch (command) {
#define X(cmd, namex, desc) \
        case cmd: \
            ge_log(LOG_READER, "    connector %s got: %02x - %s\n", conn->name, cmd, desc ); \
            break;
            ENUMERATE_READER_COMMANDS
#undef X
    }

    /* Hand the order byte to a Standard-GE-100 controller (disk/tape) on this
     * connector, if one is attached. Inert when no connector-3/4 core exists. */
    if (ge->std_core)
        connector34_deliver_order(ge, conn);
}
