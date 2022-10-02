#include <check.h>

#include "../ge.h"

#define NOP2_OPCODE 0x07

START_TEST(execute_nop)
{
    uint8_t mem[2] = {NOP2_OPCODE, 0xAA};
    struct ge g;
    int r;

    r = ge_init(&g);
    ck_assert_int_eq(r, 0);

    ge_clear(&g);

    r = ge_load(&g, mem, sizeof(mem));
    r = ge_start(&g);
    ck_assert_int_eq(r, 0);

    /* Initialisation */
    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* Alpha */
    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe0);

    ge_run_cycle(&g);
    ck_assert(g.rSO == 0x64 || g.rSO == 0x65);

    /* Beta */
    ck_assert_int_eq(g.rFO, NOP2_OPCODE);

    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe2 || g.rSO == 0xe3);
    ck_assert_uint_eq(g.rPO, 0x0002);
}

Suite * init_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("BETA");

    /* Core test case */
    tc_core = tcase_create("beta phase");

    tcase_add_test(tc_core, execute_nop);

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

