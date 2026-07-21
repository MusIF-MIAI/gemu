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

/* Arithmetic-unit function decode, cp06 ch.087 mode block (gates 22-34).
 * Pins the code the concentrator receives for each operation the timing
 * sheets pin down -- see docs/transcriptions/ua-function-decode.md. */

struct ua_case {
    const char *name;
    uint8_t ci45, ci46, ci47;
    uint8_t uco01, uco11, uco21, uco41, ucoa1;
};

static const struct ua_case UA_CASES[] = {
    /*  name              45 46 47   01 11 21 41 A1 */
    { "binary add",        0, 0, 0,   0, 0, 1, 0, 0 },
    { "binary subtract",   0, 0, 1,   0, 1, 0, 0, 1 },
    { "decimal add",       0, 1, 0,   1, 0, 0, 0, 1 },
    { "decimal subtract",  0, 1, 1,   0, 1, 0, 0, 1 },
    { "AND",               1, 1, 0,   0, 0, 1, 0, 1 },
    { "XOR",               1, 0, 1,   0, 0, 1, 1, 1 },
    { "OR",                1, 1, 1,   0, 0, 1, 1, 1 },
};

UTEST(signals, ua_function_decode_matches_the_sheet)
{
    for (size_t i = 0; i < sizeof(UA_CASES) / sizeof(UA_CASES[0]); i++) {
        const struct ua_case *c = &UA_CASES[i];
        struct ge g;

        ge_init(&g);
        g.ua_controls.logic        = c->ci45;
        g.ua_controls.decimal_and  = c->ci46;
        g.ua_controls.subtract_xor = c->ci47;

        ASSERT_EQ((uint8_t)UCO01(&g), c->uco01);
        ASSERT_EQ((uint8_t)UCO11(&g), c->uco11);
        ASSERT_EQ((uint8_t)UCO21(&g), c->uco21);
        ASSERT_EQ((uint8_t)UCO41(&g), c->uco41);
        ASSERT_EQ((uint8_t)UCOA1(&g), c->ucoa1);
        /* the /A forms are the printed complements */
        ASSERT_EQ((uint8_t)UCO2A(&g), (uint8_t)!c->uco21);
        ASSERT_EQ((uint8_t)UCO4A(&g), (uint8_t)!c->uco41);
        ASSERT_EQ((uint8_t)UCO0A(&g), (uint8_t)!c->uco01);
        ASSERT_EQ((uint8_t)UCO1A(&g), (uint8_t)!c->uco11);
    }
}

/* UCO01 is the ONLY line that isolates the decimal family, and it does so for
 * addition alone. Decimal and binary subtract share a code because CI46 enters
 * as CI46+CI47 and saturates -- in BCD a subtraction is an addition of the
 * ten's complement, so only decimal ADD needs its own line. */
UTEST(signals, uco01_is_the_decimal_add_line)
{
    struct ge g;

    ge_init(&g);
    g.ua_controls.decimal_and = 1;          /* CI46 alone: decimal add */
    ASSERT_TRUE(UCO01(&g));

    g.ua_controls.subtract_xor = 1;         /* +CI47: decimal subtract */
    ASSERT_FALSE(UCO01(&g));

    g.ua_controls.logic = 1;                /* +CI45: a logic op */
    ASSERT_FALSE(UCO01(&g));
}

/* CI50 gates the UA's WIDTH, not its function: cp06 ch.094 has CI50B = /CI501
 * feeding the UZE71/UZE81 NANDs, so raising it inhibits both zone enables. */
UTEST(signals, ci50_inhibits_the_upper_zone_enables)
{
    struct ge g;

    ge_init(&g);
    ASSERT_TRUE(UZE71_enabled(&g));
    ASSERT_TRUE(UZE81_enabled(&g));

    g.ua_controls.low_zone_only = 1;
    ASSERT_FALSE(UZE71_enabled(&g));
    ASSERT_FALSE(UZE81_enabled(&g));
}
