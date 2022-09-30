#include <check.h>

#include "../ge.h"

#define NOP2_OPCODE 0x07

START_TEST(alpha_state)
{
    uint8_t mem[2] = {NOP2_OPCODE, 0xAA};
    struct ge g;
    int r;


    r = ge_init(&g);
    ck_assert_int_eq(r, 0);

    ge_clear(&g);
    ck_assert_int_eq(g.AINI, 0);
    r = ge_load(&g, mem, sizeof(mem));
    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(g.AINI, 0);
    r = ge_start(&g);
    ck_assert_int_eq(r, 0);
    ck_assert_uint_eq(g.rSO, 0x80);
    ck_assert_uint_eq(g.rPO, 0x0000);
    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe2 || g.rSO == 0xe3);
    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe0);
    ge_run_cycle(&g);
    ck_assert_uint_eq(g.rPO, 0x0002);
    ck_assert_int_eq(g.rFO, NOP2_OPCODE);
    ck_assert_int_eq(g.rL1 & 0xFF, 0xAA);
    ck_assert(g.rSO == 0x64 || g.rSO == 0x65);
}

Suite * init_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ALPHA");

    /* Core test case */
    tc_core = tcase_create("alpha_state");

    tcase_add_test(tc_core, alpha_state);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = init_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}

