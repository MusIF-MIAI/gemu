#include "sat_batches.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cap.h"
#include "transcode.h"

enum sat_source_op {
    SAT_SRC_AS_IS = 0,
    SAT_SRC_TRIM_TITLE_SUMMARY,
    SAT_SRC_SERIAL_LOADER_PLUS_BODY,
};

struct sat_source {
    const char *file;
    enum sat_source_op op;
};

/* A card appended behind the composed sources — the operator's own card, put at
 * the back of the deck rather than punched into the program. */
struct sat_tail_card {
    const uint16_t *cols;
    int ncols;
};

struct sat_batch_def {
    struct sat_batch_info info;
    const struct sat_source *sources;
    int nsources;
    const struct sat_tail_card *tail;
};

static const struct sat_source src_cpu_functional[] = {
    { "funktionalcpu.cap", SAT_SRC_AS_IS },
};

static const struct sat_source src_reader_a[] = {
    { "reading-test-chain-01a.cap", SAT_SRC_AS_IS },
};

static const struct sat_source src_printer_mech[] = {
    { "printermechanicaltest.cap", SAT_SRC_AS_IS },
};

/* Printer Mechanic Test startup: the deck issues PER 0x00,0x0126, whose order
 * block is {00,40,00,4F,06,70}. LL is a length-1 field, so 0x4F reads 80 bytes —
 * exactly one 80-column card — "unchanged" into 0x0670. In the
 * real SAT stack that record comes from the required center card, which the
 * operator places behind the program deck. So punch one: a row-0 hole is 0x01
 * under the raw ("read unchanged") reader, a blank column is 0x00.
 *
 *   column 1: integrated subsystem        -> 0x01
 *   column 2: 2nd transport absent        -> 0x01
 *   column 3: (not evidenced)             -> blank
 *   column 4: normal drum                 -> 0x01
 *   column 5: normal ribbon               -> 0x01
 *   column 6: with END OF TEST HLT        -> blank (a 9-punch collapses to 0x00
 *                                            under the raw low-byte model)
 *
 * Confidence: 0x0673 / 0x0674 (columns 4 and 5) are directly tested by the deck
 * as 0x01. Column 6 follows the manual. Columns with no evidence stay blank. */
static const uint16_t printer_mech_center_card[80] = {
    0x0001, 0x0001, 0x0000, 0x0001, 0x0001, 0x0000,
    /* columns 7..80 blank */
};

static const struct sat_tail_card printer_mech_tail = {
    printer_mech_center_card, 80
};

static const struct sat_source src_control_program[] = {
    { "control-program-cr.cap", SAT_SRC_SERIAL_LOADER_PLUS_BODY },
};

static const struct sat_source src_ls600_controller[] = {
    { "sat-ls600.cap", SAT_SRC_SERIAL_LOADER_PLUS_BODY },
    { "ls600-controller-test.cap", SAT_SRC_TRIM_TITLE_SUMMARY },
};

static const struct sat_source src_ls600_transcoder[] = {
    { "sat-ls600.cap", SAT_SRC_SERIAL_LOADER_PLUS_BODY },
    { "ls600-transcoder-test.cap", SAT_SRC_TRIM_TITLE_SUMMARY },
};

static const struct sat_source src_ls600_doe[] = {
    { "sat-ls600.cap", SAT_SRC_SERIAL_LOADER_PLUS_BODY },
    { "ls600-doe.cap", SAT_SRC_TRIM_TITLE_SUMMARY },
};

static const struct sat_batch_def sat_batches[] = {
    {
        { "cpu-functional", "CPU Functional Test",
          "SAT step 1: the functional CPU deck, fed card by card through the reader." },
        src_cpu_functional, (int)(sizeof(src_cpu_functional) / sizeof(src_cpu_functional[0])),
        NULL
    },
    {
        { "card-reader-a", "Reading Test Channel A",
          "SAT step 2: the captured card-reader test deck." },
        src_reader_a, (int)(sizeof(src_reader_a) / sizeof(src_reader_a[0])),
        NULL
    },
    {
        { "printer-mechanical", "Printer Mechanical Test",
          "SAT step 3: the line-printer deck, with the required center card "
          "punched and placed behind it." },
        src_printer_mech, (int)(sizeof(src_printer_mech) / sizeof(src_printer_mech[0])),
        &printer_mech_tail
    },
    {
        { "control-program-cr", "Control Program CR",
          "Control-program utility deck with the serial Hollerith loader kept." },
        src_control_program, (int)(sizeof(src_control_program) / sizeof(src_control_program[0])),
        NULL
    },
    {
        { "ls600-controller-sat", "LS600 Controller SAT Batch",
          "Sequencer Program followed by LS600 Controller Test, prepared per the SAT notes." },
        src_ls600_controller, (int)(sizeof(src_ls600_controller) / sizeof(src_ls600_controller[0])),
        NULL
    },
    {
        { "ls600-transcoder-sat", "LS600 Transcoder SAT Batch",
          "Sequencer Program followed by LS600 Transcoder Test, prepared per the SAT notes." },
        src_ls600_transcoder, (int)(sizeof(src_ls600_transcoder) / sizeof(src_ls600_transcoder[0])),
        NULL
    },
    {
        { "ls600-doe-sat", "LS600 D.O.E. SAT Batch",
          "Sequencer Program followed by the LS600 D.O.E. deck, prepared per the SAT notes." },
        src_ls600_doe, (int)(sizeof(src_ls600_doe) / sizeof(src_ls600_doe[0])),
        NULL
    },
};

/* The shared loader-card rule (rpi-pico-card-reader src/deck.c
 * deck_find_loader_card): marker = a row-8 punch in column 3, searched over
 * card indices 0..4 inclusive, first match wins; with no marker fall back to
 * card 1 on a multi-card deck and card 0 otherwise. Card 0 must be in the
 * window: captured box decks lead with a title card, but a synthesized deck
 * (gasm --boot/--bootge) leads with the loader itself. */
static int row8_loader_card(const struct cap_deck *deck)
{
    int ncards = cap_num_cards(deck);
    int limit = ncards < 5 ? ncards : 5;

    for (int i = 0; i < limit; i++) {
        const uint16_t *cols = cap_card_columns(deck, i);
        if (cap_card_ncols(deck, i) >= 3 && cols && cols[2] == 0x0100)
            return i;
    }
    return (ncards > 1) ? 1 : 0;
}

static int append_card_range(struct cap_deck *out, const struct cap_deck *src,
                             int first, int last_inclusive)
{
    for (int i = first; i <= last_inclusive; i++) {
        int ncols = cap_card_ncols(src, i);
        const uint16_t *cols = cap_card_columns(src, i);
        if (ncols <= 0 || !cols)
            continue;
        if (cap_append_card(out, cols, ncols) != 0)
            return -1;
    }
    return 0;
}

static int append_source(struct cap_deck *out, const char *path, enum sat_source_op op)
{
    struct cap_deck *src = cap_load(path);
    int ncards;
    int rc = -1;

    if (!src)
        return -1;

    ncards = cap_num_cards(src);
    switch (op) {
        case SAT_SRC_AS_IS:
            rc = append_card_range(out, src, 0, ncards - 1);
            break;
        case SAT_SRC_TRIM_TITLE_SUMMARY:
            rc = (ncards >= 3) ? append_card_range(out, src, 1, ncards - 2) : -1;
            break;
        case SAT_SRC_SERIAL_LOADER_PLUS_BODY: {
            int loader = row8_loader_card(src);
            if (ncards < 7)
                rc = -1;
            else if (cap_append_card(out, cap_card_columns(src, loader),
                                     cap_card_ncols(src, loader)) != 0)
                rc = -1;
            else
                rc = append_card_range(out, src, 5, ncards - 2);
            break;
        }
        default:
            rc = -1;
            break;
    }

    cap_free(src);
    return rc;
}

static const struct sat_batch_def *sat_batch_def_find(const char *id)
{
    if (!id)
        return NULL;
    for (size_t i = 0; i < sizeof(sat_batches) / sizeof(sat_batches[0]); i++) {
        if (strcmp(sat_batches[i].info.id, id) == 0)
            return &sat_batches[i];
    }
    return NULL;
}

static void sat_note(char *note, size_t note_sz, const struct sat_batch_def *def)
{
    if (!note || note_sz == 0)
        return;
    snprintf(note, note_sz, "%s", def->info.summary);
}

int sat_batch_count(void)
{
    return (int)(sizeof(sat_batches) / sizeof(sat_batches[0]));
}

const struct sat_batch_info *sat_batch_info_at(int idx)
{
    if (idx < 0 || idx >= sat_batch_count())
        return NULL;
    return &sat_batches[idx].info;
}

const struct sat_batch_info *sat_batch_find(const char *id)
{
    const struct sat_batch_def *def = sat_batch_def_find(id);
    return def ? &def->info : NULL;
}

int sat_batch_prepare_deck(const char *root, const char *id,
                           const char *out_path,
                           char *note, size_t note_sz)
{
    char path[1024];
    struct cap_deck *out;
    const struct sat_batch_def *def = sat_batch_def_find(id);

    if (!def || !root || !out_path)
        return -1;

    out = cap_create();
    if (!out)
        return -1;

    for (int i = 0; i < def->nsources; i++) {
        snprintf(path, sizeof(path), "%s/%s", root, def->sources[i].file);
        if (append_source(out, path, def->sources[i].op) != 0) {
            cap_free(out);
            return -1;
        }
    }

    if (def->tail && cap_append_card(out, def->tail->cols, def->tail->ncols) != 0) {
        cap_free(out);
        return -1;
    }

    if (cap_save(out, out_path) != 0) {
        cap_free(out);
        return -1;
    }

    cap_free(out);
    sat_note(note, note_sz, def);
    return 0;
}
