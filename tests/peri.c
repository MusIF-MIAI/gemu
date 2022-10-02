#include "tests.h"

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

START_TEST(peri)
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
    ck_assert_int_eq(r, 0);

    r = ge_register_peri(&g, &p);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(ctx.test_init, 1);

    r = ge_start(&g);
    ck_assert_int_eq(r, 0);
    ge_run_cycle(&g);
    ck_assert_int_eq(ctx.test_clocks, 1);
    ck_assert_int_eq(ctx.test_pulses, END_OF_STATUS);

    r = ge_deinit(&g);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(ctx.test_deinit, 1);
}

START_TEST(peri_null)
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
    ck_assert_int_eq(r, 0);

    r = ge_register_peri(&g, &p);
    ck_assert_int_eq(r, 0);

    r = ge_start(&g);
    ck_assert_int_eq(r, 0);
    ge_run_cycle(&g);

    r = ge_deinit(&g);
    ck_assert_int_eq(r, 0);
}

Suite *init_peri_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("PERI");

    /* Core test case */
    tc_core = tcase_create("register peri");

    tcase_add_test(tc_core, peri);
    tcase_add_test(tc_core, peri_null);

    suite_add_tcase(s, tc_core);

    return s;
}
