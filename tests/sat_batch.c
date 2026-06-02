#include "utest.h"

#include "../sat_batches.h"
#include "../cap.h"

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

UTEST(sat_batch, cpu_functional_scatter_prepares)
{
    static unsigned char image[MEM_SIZE];
    unsigned lo = 0, hi = 0;
    uint16_t entry = 0;
    char note[256];

    if (!sat_file_exists("Site_Acceptance_Test/funktionalcpu.cap")) {
        printf("  [SKIP] Site_Acceptance_Test/funktionalcpu.cap not found\n");
        return;
    }

    ASSERT_EQ(sat_batch_prepare_image("Site_Acceptance_Test", "cpu-functional",
                                      image, &lo, &hi, &entry,
                                      note, sizeof(note)), 0);
    ASSERT_EQ((int)lo, 0x0000);
    ASSERT_EQ((int)entry, 0x0000);
    ASSERT_EQ((int)image[0x0100], 0x43);
    ASSERT_EQ((int)image[0x0101], 0xF0);
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
