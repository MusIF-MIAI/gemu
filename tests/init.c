#include "tests.h"

START_TEST(initialitiation_state)
{
    struct ge g;
    int r;

    r = ge_init(&g);
    ck_assert_int_eq(r, 0);

    ge_clear(&g);
    ck_assert_int_eq(g.AINI, 0);
    r = ge_load(&g, NULL, 0);
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(g.AINI, 1);
    r = ge_start(&g);
    ck_assert_int_eq(r, 0);
    ck_assert_uint_eq(g.rSO, 0x80);
    ge_run_cycle(&g);
    ck_assert_uint_eq(g.rSO, 0xc8);
}

Suite *init_init_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("INITIALITIAION");

    /* Core test case */
    tc_core = tcase_create("load_c8");

    tcase_add_test(tc_core, initialitiation_state);

    suite_add_tcase(s, tc_core);

    return s;
}
