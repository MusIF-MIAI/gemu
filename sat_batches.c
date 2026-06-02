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

struct sat_batch_def {
    struct sat_batch_info info;
    const struct sat_source *sources;
    int nsources;
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
          "SAT step 1: the functional CPU deck staged through the scatter-image path.",
          SAT_BATCH_IMAGE },
        src_cpu_functional, (int)(sizeof(src_cpu_functional) / sizeof(src_cpu_functional[0]))
    },
    {
        { "card-reader-a", "Reading Test Channel A",
          "SAT step 2: the captured card-reader test deck.", SAT_BATCH_IMAGE },
        src_reader_a, (int)(sizeof(src_reader_a) / sizeof(src_reader_a[0]))
    },
    {
        { "printer-mechanical", "Printer Mechanical Test",
          "SAT step 3: the line-printer deck staged through the scatter-image path.",
          SAT_BATCH_IMAGE },
        src_printer_mech, (int)(sizeof(src_printer_mech) / sizeof(src_printer_mech[0]))
    },
    {
        { "control-program-cr", "Control Program CR",
          "Control-program utility deck with the serial Hollerith loader kept.",
          SAT_BATCH_READER },
        src_control_program, (int)(sizeof(src_control_program) / sizeof(src_control_program[0]))
    },
    {
        { "ls600-controller-sat", "LS600 Controller SAT Batch",
          "Sequencer Program followed by LS600 Controller Test, prepared per the SAT notes.",
          SAT_BATCH_READER },
        src_ls600_controller, (int)(sizeof(src_ls600_controller) / sizeof(src_ls600_controller[0]))
    },
    {
        { "ls600-transcoder-sat", "LS600 Transcoder SAT Batch",
          "Sequencer Program followed by LS600 Transcoder Test, prepared per the SAT notes.",
          SAT_BATCH_READER },
        src_ls600_transcoder, (int)(sizeof(src_ls600_transcoder) / sizeof(src_ls600_transcoder[0]))
    },
    {
        { "ls600-doe-sat", "LS600 D.O.E. SAT Batch",
          "Sequencer Program followed by the LS600 D.O.E. deck, prepared per the SAT notes.",
          SAT_BATCH_READER },
        src_ls600_doe, (int)(sizeof(src_ls600_doe) / sizeof(src_ls600_doe[0]))
    },
};

static int row8_loader_card(const struct cap_deck *deck)
{
    int ncards = cap_num_cards(deck);
    int limit = ncards < 5 ? ncards : 5;

    for (int i = 1; i < limit; i++) {
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

int sat_batch_prepare_image(const char *root, const char *id,
                            unsigned char *image,
                            unsigned *lo, unsigned *hi, uint16_t *entry,
                            char *note, size_t note_sz)
{
    char path[1024];
    const struct sat_batch_def *def = sat_batch_def_find(id);
    int rc;

    if (!def || def->info.launch != SAT_BATCH_IMAGE ||
        !root || !image || !lo || !hi || !entry || def->nsources != 1)
        return -1;

    snprintf(path, sizeof(path), "%s/%s", root, def->sources[0].file);
    memset(image, 0, MEM_SIZE);
    rc = cap_load_scattered(path, TC_COLBIN, image, lo, hi);
    if (rc < 0)
        return -1;

    *entry = (uint16_t)*lo;
    sat_note(note, note_sz, def);
    return 0;
}

int sat_batch_prepare_deck(const char *root, const char *id,
                           const char *out_path,
                           char *note, size_t note_sz)
{
    char path[1024];
    struct cap_deck *out;
    const struct sat_batch_def *def = sat_batch_def_find(id);

    if (!def || def->info.launch != SAT_BATCH_READER || !root || !out_path)
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

    if (cap_save(out, out_path) != 0) {
        cap_free(out);
        return -1;
    }

    cap_free(out);
    sat_note(note, note_sz, def);
    return 0;
}
