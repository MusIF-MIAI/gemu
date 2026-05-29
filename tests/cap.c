#include <stdio.h>

#include "utest.h"
#include "../cap.h"

UTEST(cap, parse_funktional)
{
    /* The DUMP1 decks live outside this git repo, so they are absent in a CI
     * checkout. Skip gracefully when the deck is not present (mirrors the skip
     * pattern in tests/bootstrap.c); run the real assertions otherwise. */
    static const char path[] = "../DUMP1/funktionalcpu.cap";
    FILE *probe = fopen(path, "r");
    if (!probe) {
        printf("  [SKIP] %s not found\n", path);
        return;
    }
    fclose(probe);

    struct cap_deck *d = cap_load(path);
    ASSERT_TRUE(d != NULL);
    ASSERT_EQ(cap_num_cards(d), 228);
    ASSERT_EQ(cap_card_ncols(d, 0), 80);
    ASSERT_EQ((int)cap_card_columns(d, 0)[0], 0x0010);
    cap_free(d);
}

UTEST(cap, missing_file)
{
    struct cap_deck *d = cap_load("DUMP1/does-not-exist.cap");
    ASSERT_TRUE(d == NULL);
}
