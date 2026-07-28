/*
 * tests/sat_batch.c - the built-in Site Acceptance Test batches.
 *
 * Every batch composes to a .cap deck. There is no image-staging variant: the
 * machine takes programs on cards, so a batch is a stack of cards and nothing
 * else. What is asserted here is that each recipe composes, and that the
 * printer batch really does carry the operator's center card at the back.
 */

#include "utest.h"

#include "../sat_batches.h"
#include "../cap.h"
#include "../transcode.h"

#include <stdio.h>
#include <string.h>

static int sat_file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

UTEST(sat_batch, cpu_functional_composes_as_a_deck)
{
    static const char out_path[] = "/tmp/gemu_sat_cpu_functional.cap";
    struct cap_deck *deck;
    char note[256];

    if (!sat_file_exists("Site_Acceptance_Test/funktionalcpu.cap")) {
        printf("  [SKIP] Site_Acceptance_Test/funktionalcpu.cap not found\n");
        return;
    }

    ASSERT_EQ(sat_batch_prepare_deck("Site_Acceptance_Test", "cpu-functional",
                                     out_path, note, sizeof(note)), 0);
    deck = cap_load(out_path);
    ASSERT_TRUE(deck != NULL);
    /* The deck is passed through as-is: all 114 captured cards. */
    ASSERT_EQ(cap_num_cards(deck), 114);
    cap_free(deck);
}

UTEST(sat_batch, printer_mechanical_appends_the_center_card)
{
    static const char out_path[] = "/tmp/gemu_sat_printer_mech.cap";
    struct cap_deck *src, *out;
    const uint16_t *cols;
    int nsrc, nout;
    char note[256];

    if (!sat_file_exists("Site_Acceptance_Test/printermechanicaltest.cap")) {
        printf("  [SKIP] Site_Acceptance_Test/printermechanicaltest.cap not found\n");
        return;
    }

    /* Count the cards that actually carry columns: a .cap holds the same deck
     * twice (hex dump + hole art), and only the hex half parses to columns. */
    src = cap_load("Site_Acceptance_Test/printermechanicaltest.cap");
    ASSERT_TRUE(src != NULL);
    nsrc = 0;
    for (int i = 0; i < cap_num_cards(src); i++)
        if (cap_card_ncols(src, i) > 0)
            nsrc++;
    cap_free(src);

    ASSERT_EQ(sat_batch_prepare_deck("Site_Acceptance_Test", "printer-mechanical",
                                     out_path, note, sizeof(note)), 0);
    ASSERT_NE(strstr(note, "center card"), NULL);

    out = cap_load(out_path);
    ASSERT_TRUE(out != NULL);
    nout = cap_num_cards(out);
    /* Exactly one card more than the captured deck: the center card, at the
     * back, where the operator puts it. */
    ASSERT_EQ(nout, nsrc + 1);

    /* Read that last card the way the deck's startup PER does -- "read
     * unchanged", i.e. the raw low byte of each column. */
    ASSERT_EQ(cap_card_ncols(out, nout - 1), 80);
    cols = cap_card_columns(out, nout - 1);
    ASSERT_TRUE(cols != NULL);
    ASSERT_EQ((int)transcode_column(cols[0], TC_BINARY), 0x01); /* integrated  */
    ASSERT_EQ((int)transcode_column(cols[1], TC_BINARY), 0x01); /* no 2nd tr.  */
    ASSERT_EQ((int)transcode_column(cols[3], TC_BINARY), 0x01); /* normal drum */
    ASSERT_EQ((int)transcode_column(cols[4], TC_BINARY), 0x01); /* normal ribbon */
    ASSERT_EQ((int)transcode_column(cols[5], TC_BINARY), 0x00); /* END OF TEST HLT */
    cap_free(out);
}

UTEST(sat_batch, ls600_controller_batch_composes)
{
    static const char out_path[] = "/tmp/gemu_sat_ls600_controller.cap";
    struct cap_deck *deck;

    if (!sat_file_exists("Site_Acceptance_Test/sat-ls600.cap") ||
        !sat_file_exists("Site_Acceptance_Test/ls600-controller-test.cap")) {
        printf("  [SKIP] LS600 SAT media not found\n");
        return;
    }

    ASSERT_EQ(sat_batch_prepare_deck("Site_Acceptance_Test", "ls600-controller-sat",
                                     out_path, NULL, 0), 0);
    deck = cap_load(out_path);
    ASSERT_TRUE(deck != NULL);
    ASSERT_GT(cap_num_cards(deck), 120);
    cap_free(deck);
}
