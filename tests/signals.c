#include "utest.h"

#include "../ge.h"
#include "../signals.h"

UTEST(signals, channel_selection)
{
    #define MAKEZCHAR(bit_03, bit_00) ((bit_03 << 3) | bit_00)

    /* from cpu fo. 73 */
    static const uint8_t channel_1 = MAKEZCHAR(0, 0);
    static const uint8_t channel_3 = MAKEZCHAR(1, 0);
    static const uint8_t channel_2 = MAKEZCHAR(0, 1);
    static const uint8_t channel_2_overlapped = MAKEZCHAR(1, 1);

    struct ge g;

    g.rL2 = channel_1;
    ASSERT_FALSE(PC031(&g));

    g.rL2 = channel_2;
    ASSERT_FALSE(PC031(&g));

    g.rL2 = channel_2_overlapped;
    ASSERT_FALSE(PC031(&g));

    g.rL2 = channel_3;
    ASSERT_TRUE(PC031(&g));
}
