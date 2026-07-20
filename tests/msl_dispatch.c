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
    ASSERT_STREQ(variant_for(0x64, MVC_OPCODE, 0x02), "beta-ss");
}

UTEST(msl_dispatch, undocumented_codes_use_explicit_compatibility_sheet)
{
    ASSERT_STREQ(variant_for(0x64, 0x00, 0x00), "beta-undocumented");
    ASSERT_STREQ(variant_for(0x64, 0x80, 0x00), "beta-undocumented");
}

UTEST(msl_dispatch, downstream_pairs_keep_instruction_family)
{
    ASSERT_STREQ(variant_for(0x60, LR_OPCODE, 0xc0), "exec-register-60|62");
    ASSERT_STREQ(variant_for(0x62, MVI_OPCODE, 0xab), "exec-immediate-60|62");
    ASSERT_STREQ(variant_for(0x50, AMR_OPCODE, 0xc0), "exec-register-50|52");
    ASSERT_STREQ(variant_for(0x42, CMI_OPCODE, 0x42), "exec-immediate-40|42");
}
