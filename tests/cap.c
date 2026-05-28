#include "utest.h"
#include "../cap.h"

UTEST(cap, parse_funktional)
{
    struct cap_deck *d = cap_load("../DUMP1/funktionalcpu.cap");
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
