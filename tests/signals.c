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

/* Counting network: CI41 + CI42 together are TWO INDEPENDENT QUARTET
 * COUNTERS, not one subtraction of 0x11.  cp06 ch.097 gate 24 builds BUD01 --
 * the term driving quartet 2's carry chain on ch.096 -- from the command
 * lines CA41A/CA42B/CA431 alone, with no carry term out of the bits 00-03
 * chain, so a borrow across bit 03 must not disturb quartet 2.
 *
 * It matters because cp07 fo.141 raises both commands from one gate and
 * fo.143 reads the quartets separately: L1's low byte holds one length per SS
 * operand, and the shorter operand running out must not shorten the other. */

static uint16_t count_both_quartets(uint16_t bo, int decreasing)
{
    struct ge g;

    ge_init(&g);
    g.rBO = bo;
    g.counting_network.cmds.from_zero = 1;
    g.counting_network.cmds.from_04   = 1;
    g.counting_network.cmds.stop_07   = 1;
    g.counting_network.cmds.decresing = decreasing;
    return ge_counting_network_output(&g);
}

UTEST(signals, quartet_counters_do_not_borrow_across_bit_03)
{
    /* No wrap: indistinguishable from a 0x11 subtraction. */
    ASSERT_EQ(count_both_quartets(0x0035, 1), 0x0024);

    /* Quartet 1 wraps. A single 0x11 subtract would give 0x1F, taking
     * quartet 2 down by TWO; independent counters give 0x2F. */
    ASSERT_EQ(count_both_quartets(0x0030, 1), 0x002f);

    /* Both wrap. */
    ASSERT_EQ(count_both_quartets(0x0000, 1), 0x00ff);

    /* Ascending, quartet 1 wrapping the other way. */
    ASSERT_EQ(count_both_quartets(0x000f, 0), 0x0010);

    /* The high byte is never touched -- this is a byte-local count. */
    ASSERT_EQ(count_both_quartets(0xab30, 1), 0xab2f);
}

/* Either command alone keeps the ordinary rippling behaviour. */
UTEST(signals, a_single_injection_still_ripples)
{
    struct ge g;

    ge_init(&g);
    g.rBO = 0x0030;
    g.counting_network.cmds.from_zero = 1;
    g.counting_network.cmds.stop_07   = 1;
    g.counting_network.cmds.decresing = 1;
    ASSERT_EQ(ge_counting_network_output(&g), 0x002f);   /* borrow crosses */

    ge_init(&g);
    g.rBO = 0x0030;
    g.counting_network.cmds.from_04   = 1;
    g.counting_network.cmds.stop_07   = 1;
    g.counting_network.cmds.decresing = 1;
    ASSERT_EQ(ge_counting_network_output(&g), 0x0020);   /* quartet 2 only */
}

/* Backplane option connectors, cp06 ch.002 "VARIANTI E OPZIONI". */

UTEST(signals, e04_selects_the_loading_connectors)
{
    struct ge g;

    ge_init(&g);                          /* E04 empty: connectors 2 and 3 */
    ASSERT_TRUE(FUL26(&g));
    ASSERT_TRUE(FUL36(&g));

    g.options.E04 = PONT_2N;              /* connectors 2 and 4 */
    ASSERT_TRUE(FUL26(&g));
    ASSERT_FALSE(FUL36(&g));

    g.options.E04 = PONT_2P;              /* connectors 4 and 3 */
    ASSERT_FALSE(FUL26(&g));
    ASSERT_TRUE(FUL36(&g));
}

/* FUL4G reads "this machine has the MAX instruction set": the 6us UCE 466
 * straps F04 with PONT2N and gets 0, the 4us and 2us models use PONT2P. */
UTEST(signals, f04_straps_the_machine_version)
{
    struct ge g;

    ge_init(&g);
    g.options.F04 = PONT_2N;              /* UCE 466, 6us, MIN */
    ASSERT_FALSE(FUL4G(&g));

    g.options.F04 = PONT_2P;              /* UCE 467/468, MAX, no interrupts */
    ASSERT_TRUE(FUL4G(&g));

    /* An EMPTY F04 is the interrupts-enabled variant of the same two fast
     * models, so it is also MAX -- not the MIN machine. This is the case the
     * real machine at Electric Dreams is in. */
    g.options.F04 = PONT_NONE;
    ASSERT_TRUE(FUL4G(&g));
}

/* The ch.002 note: S42 "LAMPS" in position DIAG forces FUL4F high whatever
 * the straps say, which is the {FUL4} the timing charts cite. */
UTEST(signals, s42_diag_overrides_the_version_strap)
{
    struct ge g;

    ge_init(&g);
    g.options.F04 = PONT_2N;              /* MIN machine: FUL4G low ... */
    ASSERT_FALSE(FUL4F(&g));

    g.options.S42_diag = 1;               /* ... but DIAG forces it high */
    ASSERT_TRUE(FUL4F(&g));
    ASSERT_TRUE(FUL4(&g));
}

/* gemu is strapped as the machine at Electric Dreams: UCE 468 processor
 * (2 usec, MAX, interruptions enabled).  Physical identification 2026-07-21:
 * BOTH option part numbers are PONT2N -- 0618034Z prints it on the board,
 * 0618035V is electrically the same strap under a different code.  (The F03
 * card is currently mislaid -- located in a 2018 photo -- and is modelled as
 * fitted, per the machine's intended configuration.) */
UTEST(signals, default_straps_are_a_uce468_with_off_table_capacity)
{
    struct ge g;
    ge_init(&g);

    ASSERT_EQ(ge_cpu_version_uce(&g), 468);
    ASSERT_EQ(ge_cycle_period_ns(&g), 2000);

    /* Card 05 carries PONT2N in BOTH positions on this machine -- a strap
     * combination the ch.001 table never defines.  Under the pin mechanism
     * (N shorts pins {1,4}, P shorts {1,3}) that reads (VAMA2, VEMB6,
     * VAMC2) = (1, 0, 0): no printed row, so the capacity reports 0. */
    ASSERT_TRUE(VAMA2(&g));
    ASSERT_FALSE(VEMB6(&g));
    ASSERT_FALSE(VAMC2(&g));
    ASSERT_EQ(ge_memory_capacity_k(&g), 0);

    /* ch.002 TAB.1, the UCE 468 rows */
    ASSERT_FALSE(FEL06(&g));
    ASSERT_FALSE(FEL16(&g));
    ASSERT_TRUE(FUL4G(&g));

    /* ch.002 TAB.3: E04 empty -> loading on connectors 2 and 3 */
    ASSERT_TRUE(FUL26(&g));
    ASSERT_TRUE(FUL36(&g));

    /* ch.002 TAB.2: F04 empty lets F03 choose; PONT2N = connector 3 alone */
    ASSERT_TRUE(INES3(&g));
    ASSERT_FALSE(INES4(&g));
}

/* The five printed ch.001 rows, driven through the straps under the card-pin
 * mechanism -- the fitted equations and the mechanism agree on every printed
 * row, and diverge only off-table (covered in the default-straps test). */
UTEST(signals, ch001_capacity_rows_reproduce_under_the_pin_mechanism)
{
    static const struct {
        enum ge_pont e05, f05;
        uint8_t a, b, c;
        uint16_t k;
    } rows[] = {
        { PONT_NONE, PONT_NONE, 1, 1, 1,  8 },
        { PONT_2N,   PONT_NONE, 1, 1, 0, 12 },
        { PONT_NONE, PONT_2N,   1, 0, 1, 16 },
        { PONT_2P,   PONT_2N,   0, 0, 1, 24 },
        { PONT_2N,   PONT_2P,   0, 0, 0, 32 },
    };
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++) {
        struct ge g;
        ge_init(&g);
        g.options.E05 = rows[i].e05;
        g.options.F05 = rows[i].f05;
        ASSERT_EQ((uint8_t)VAMA2(&g), rows[i].a);
        ASSERT_EQ((uint8_t)VEMB6(&g), rows[i].b);
        ASSERT_EQ((uint8_t)VAMC2(&g), rows[i].c);
        ASSERT_EQ(ge_memory_capacity_k(&g), rows[i].k);
    }
}
