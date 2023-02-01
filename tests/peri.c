#include "utest.h"

#include "../ge.h"

struct peri_ctx {
    int test_init;
    int test_clocks;
    int test_pulses;
    int test_deinit;
};

int peri_test_init(struct ge *ge, void *ctx)
{
    struct peri_ctx *peri_ctx = (struct peri_ctx*)ctx;

    peri_ctx->test_init++;
    return 0;
}

int peri_test_on_pulse(struct ge *ge, void *ctx)
{
    struct peri_ctx *peri_ctx = (struct peri_ctx*)ctx;

    peri_ctx->test_pulses++;
    return 0;
}

int peri_test_on_clock(struct ge *ge, void *ctx)
{
    struct peri_ctx *peri_ctx = (struct peri_ctx*)ctx;

    peri_ctx->test_clocks++;
    return 0;
}

int peri_test_deinit(struct ge *ge, void *ctx)
{
    struct peri_ctx *peri_ctx = (struct peri_ctx*)ctx;

    peri_ctx->test_deinit++;
    return 0;
}

UTEST(peripheral, peri)
{
    struct peri_ctx ctx = { 0 };
    struct ge_peri p;
    struct ge g;
    int r;

    p.init = &peri_test_init;
    p.on_pulse = &peri_test_on_pulse;
    p.on_clock = &peri_test_on_clock;
    p.deinit = &peri_test_deinit;
    p.ctx = &ctx;

    r = ge_init(&g);
    ASSERT_EQ(r, 0);

    r = ge_register_peri(&g, &p);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ctx.test_init, 1);

    ge_start(&g);

    ge_run_cycle(&g);
    ASSERT_EQ(ctx.test_clocks, 1);
    ASSERT_EQ(ctx.test_pulses, END_OF_STATUS);

    r = ge_deinit(&g);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ctx.test_deinit, 1);
}

UTEST(peripheral, peri_null)
{
    struct peri_ctx ctx = { 0 };
    struct ge_peri p;
    struct ge g;
    int r;

    p.init = NULL;
    p.on_pulse = NULL;
    p.on_clock = NULL;
    p.deinit = NULL;
    p.ctx = NULL;

    r = ge_init(&g);
    ASSERT_EQ(r, 0);

    r = ge_register_peri(&g, &p);
    ASSERT_EQ(r, 0);

    ge_start(&g);
    ge_run_cycle(&g);

    r = ge_deinit(&g);
    ASSERT_EQ(r, 0);
}
