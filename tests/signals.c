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

/* CU01's partial command, cp06 ch.252-7 (see
 * docs/transcriptions/cu01-partial-command.md).  Four function classes ANDed
 * with the beta-band decode, OR'd together.  This pins the resulting opcode
 * partition, which is the whole reason the network matters: exactly the
 * instructions that finish in beta may set S001, and every family that
 * continues into the executive band must not. */

static uint8_t cm01_in_beta(uint8_t opcode)
{
    struct ge g;
    ge_init(&g);
    g.rSA = 0x64;          /* beta band: DI06 asserted */
    g.rFO = opcode;
    return CM01A0(&g);
}

UTEST(signals, cm01_claims_every_instruction_that_finishes_in_beta)
{
    /* DO00 class -- the console/control codes.  These are the ones the old
     * DE00A stub existed for: FO bit 6 is clear, so DO01 cannot reach them. */
    ASSERT_TRUE(cm01_in_beta(0x02));   /* INS / ENS / LON / LOFF / LOLL */
    ASSERT_TRUE(cm01_in_beta(0x07));   /* NOP2 */
    ASSERT_TRUE(cm01_in_beta(0x0a));   /* HLT  */

    /* DO01 class -- jumps, including JRT */
    ASSERT_TRUE(cm01_in_beta(0x40));   /* JCC */
    ASSERT_TRUE(cm01_in_beta(0x41));   /* JRT */
    ASSERT_TRUE(cm01_in_beta(0x43));   /* JC  */
    ASSERT_TRUE(cm01_in_beta(0x47));   /* JU  */
    ASSERT_TRUE(cm01_in_beta(0x53));   /* JIE / JS1 / JS2 */

    ASSERT_TRUE(cm01_in_beta(0x68));   /* DO02 class -- LA   */
    ASSERT_TRUE(cm01_in_beta(0x9d));   /* DO06 class -- LPSR */
}

UTEST(signals, cm01_withholds_from_the_executive_families)
{
    /* Immediates, register and SS all continue into 60|62 and must NOT get
     * S001 in beta -- the executive loop is what returns them to alpha. */
    static const uint8_t continues[] = {
        0x91, 0x92, 0x94, 0x95, 0x96, 0x97,        /* TM MVI NI CMI CI XI  */
        0xb4, 0xbc, 0xbd, 0xbe, 0xbf,              /* STR LR CMR AMR SMR   */
        0xd2, 0xd4, 0xd5, 0xd6, 0xd7,              /* MVC NC CMC OC XC     */
        0xda, 0xde, 0xea, 0xec, 0xfa, 0xff,        /* PK EDT AP MP AD SB   */
    };
    for (size_t i = 0; i < sizeof(continues); i++)
        ASSERT_FALSE(cm01_in_beta(continues[i]));

    /* External operations route to CC, never to E2 */
    ASSERT_FALSE(cm01_in_beta(0x90));  /* RDC  */
    ASSERT_FALSE(cm01_in_beta(0x9c));  /* PERI */
    ASSERT_FALSE(cm01_in_beta(0x9e));  /* PER  */
}

UTEST(signals, cm01_is_gated_on_the_beta_band)
{
    /* Every leaf ANDs its class with DI06, so the command cannot assert from
     * a state outside 0110 01xx no matter what FO holds. */
    struct ge g;
    ge_init(&g);
    g.rFO = 0x43;                      /* JC: claimed by DO01 in beta */
    g.rSA = 0x64;
    ASSERT_TRUE(CM01A0(&g));
    g.rSA = 0xe2;                      /* alpha */
    ASSERT_FALSE(CM01A0(&g));
    g.rSA = 0x60;                      /* executive */
    ASSERT_FALSE(CM01A0(&g));
}
