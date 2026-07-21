#include "utest.h"

#include "../ge.h"
#include "../msl.h"
#include "../msl-timings.h"
#include "../opcodes.h"

/*
 * The MSL has no dispatch.
 *
 * The manual prints several timing sheets for some states -- 64|65|66 and the
 * executive band -- because it is organised by instruction family, not because
 * the hardware has more than one micro-sequence logic per state.  gemu used to
 * mirror the manual with a variant matrix selected per instruction; it now
 * mirrors the machine, one chart per state with every row carrying its own
 * gate.  These tests hold that line.
 */

/* States the manual prints as multiple sheets, all merged. */
static const uint8_t MULTI_SHEET_STATES[] = {
    0x40, 0x42, 0x50, 0x52, 0x60, 0x62, 0x64, 0x65, 0x66, 0xea, 0xeb,
};

UTEST(msl_dispatch, multi_sheet_states_are_a_single_chart)
{
    for (size_t i = 0; i < sizeof(MULTI_SHEET_STATES); i++) {
        uint8_t code = MULTI_SHEET_STATES[i];
        const struct msl_timing_state *st = msl_get_state(code);

        ASSERT_TRUE(st != NULL);
        ASSERT_TRUE(st->chart != NULL);
    }
}

/* Every sheet a merged state was built from stays named, so a row can still be
 * traced back to a physical page after the sheets were folded together. */
UTEST(msl_dispatch, merged_states_name_their_source_sheets)
{
    static const uint8_t MERGED[] = { 0x40, 0x42, 0x50, 0x52, 0x60, 0x62 };

    for (size_t i = 0; i < sizeof(MERGED); i++) {
        const struct msl_timing_state *st = msl_get_state(MERGED[i]);

        ASSERT_TRUE(st->chart_ref != NULL);
        ASSERT_TRUE(st->chart_ref[0] != '\0');
    }
}

/* The states share charts in pairs, exactly as the sheets do: the X bit that
 * distinguishes 40 from 42 is a pass counter, not a different circuit. */
UTEST(msl_dispatch, paired_states_share_one_chart)
{
    ASSERT_TRUE(msl_get_state(0x40)->chart == msl_get_state(0x42)->chart);
    ASSERT_TRUE(msl_get_state(0x50)->chart == msl_get_state(0x52)->chart);
    ASSERT_TRUE(msl_get_state(0x60)->chart == msl_get_state(0x62)->chart);
    ASSERT_TRUE(msl_get_state(0x64)->chart == msl_get_state(0x65)->chart);
    ASSERT_TRUE(msl_get_state(0x64)->chart == msl_get_state(0x66)->chart);
}

