#include "utest.h"

#include "../ge.h"
#include "../msl.h"
#include "../msl-timings.h"
#include "../opcodes.h"

static const char *variant_for(uint8_t state, uint8_t opcode, uint8_t aux)
{
    struct ge g;
    ge_init(&g);
    g.rSA = state;
    g.rFO = opcode;
    g.rL1 = aux;

    const struct msl_timing_state *timing = msl_get_state(state);
    const struct msl_timing_variant *variant = msl_select_variant(&g, timing);
    return variant ? variant->name : NULL;
}

UTEST(msl_dispatch, beta_families_select_manual_sheets)
{
    ASSERT_STREQ(variant_for(0x64, JC_OPCODE, 0xf0), "beta-control");
    ASSERT_STREQ(variant_for(0x64, JRT_OPCODE, 0x00), "beta-jrt");
    ASSERT_STREQ(variant_for(0x64, LA_OPCODE, 0xc0), "beta-la");
    ASSERT_STREQ(variant_for(0x64, LPSR_OPCODE, 0x00), "beta-lpsr");
    ASSERT_STREQ(variant_for(0x64, LR_OPCODE, 0xc0), "beta-register");
    ASSERT_STREQ(variant_for(0x64, MVI_OPCODE, 0xab), "beta-immediate");
    ASSERT_STREQ(variant_for(0x64, PER_OPCODE, 0x80), "beta-per");
    ASSERT_STREQ(variant_for(0x64, MVC_OPCODE, 0x02), "beta-mvc");
}

UTEST(msl_dispatch, undocumented_codes_use_explicit_compatibility_sheet)
{
    ASSERT_STREQ(variant_for(0x64, 0x00, 0x00), "beta-undocumented");
    ASSERT_STREQ(variant_for(0x64, 0x80, 0x00), "beta-undocumented");
}

/* These states no longer appear above: each is a single chart whose rows
 * carry their own gates, so there is no variant to select.  See exec_40 and
 * exec_50 in msl-states.c. */
UTEST(msl_dispatch, merged_states_have_no_variant_matrix)
{
    static const uint8_t merged[] = { 0x40, 0x42, 0x50, 0x52, 0x60, 0x62 };
    for (size_t i = 0; i < sizeof(merged); i++) {
        uint8_t code = merged[i];
        const struct msl_timing_state *st = msl_get_state(code);
        ASSERT_TRUE(st != NULL);
        ASSERT_TRUE(st->chart != NULL);
        ASSERT_TRUE(st->variants == NULL);
    }
}

/* The multi-sheet states are one MSL: the rows carrying no family term live
 * in the state's common chart, and every sheet they were factored out of is
 * named in chart_ref so the provenance survives the factoring. */
static const uint8_t MULTI_SHEET_STATES[] = {
    0x64, 0x65, 0x66,
};

UTEST(msl_dispatch, multi_sheet_states_carry_common_rows_and_provenance)
{
    for (size_t i = 0; i < sizeof(MULTI_SHEET_STATES); i++) {
        uint8_t code = MULTI_SHEET_STATES[i];
        const struct msl_timing_state *st = msl_get_state(code);

        ASSERT_TRUE(st != NULL);
        ASSERT_TRUE(st->variants != NULL);
        ASSERT_TRUE(st->chart != NULL);      /* common rows present */
        ASSERT_TRUE(st->chart_ref != NULL);  /* ...and traceable */
        ASSERT_TRUE(st->chart_ref[0] != '\0');
    }
}

/* A row belongs to exactly one place.  If a (clock, command) pair sits in the
 * common chart, no variant may repeat it: that would run the command twice in
 * the same clock, and it also means the factoring drifted out of step with
 * the sheets.  Guards the next family conversion against re-introducing the
 * per-sheet duplication this model exists to remove. */
UTEST(msl_dispatch, common_rows_are_not_repeated_by_any_variant)
{
    for (size_t i = 0; i < sizeof(MULTI_SHEET_STATES); i++) {
        uint8_t code = MULTI_SHEET_STATES[i];
        const struct msl_timing_state *st = msl_get_state(code);
        const struct msl_timing_variant *v;

        for (v = st->variants; v && v->match; v++) {
            const struct msl_timing_chart *row, *common;

            for (row = v->chart; row->clock < END_OF_STATUS; row++) {
                for (common = st->chart; common->clock < END_OF_STATUS;
                     common++) {
                    if (common->clock == row->clock &&
                        common->command == row->command) {
                        /* Report which sheet drifted. */
                        ASSERT_STREQ("no duplicate of a common row", v->name);
                    }
                }
            }
        }
    }
}
