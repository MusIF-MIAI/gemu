#include <check.h>

#include "../ge.h"

#define NOP2_OPCODE 0x07
#define HLT_OPCODE  0x0A

START_TEST(p_instruction_to_alpha)
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

START_TEST(test_int)
{
    uint8_t mem[2] = {HLT_OPCODE, 0xAA};
    struct ge g;
    int r;

    r = ge_init(&g);
    ck_assert_int_eq(r, 0);

    ge_clear(&g);
    r = ge_load(&g, mem, sizeof(mem));
    ck_assert_int_eq(r, 0);

    r = ge_start(&g);
    ck_assert_int_eq(r, 0);
    ck_assert_uint_eq(g.rSO, 0x80);

    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe2 || g.rSO == 0xe3);

    /* not sure about setting FI to 0... it is set by state 80
     * and i didn't notice where it's supposed to be reset */
    g.ffFI = 0;

    /* setting an interruption */
    g.RINT = 1;

    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xf0);
}

START_TEST(test_hlt)
{
    uint8_t mem[2] = {HLT_OPCODE, 0xAA};
    struct ge g;
    int r;

    r = ge_init(&g);
    ck_assert_int_eq(r, 0);

    ge_clear(&g);

    r = ge_load(&g, mem, sizeof(mem));
    ck_assert_int_eq(r, 0);

    r = ge_start(&g);
    ck_assert_int_eq(r, 0);
    ck_assert_uint_eq(g.rSO, 0x80);

    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe2 || g.rSO == 0xe3);

    ge_run_cycle(&g);
    ck_assert(g.ALTO);

    /* how does the machine behave after ALTO = 1? */
    /* how should the emulation do? */
}

START_TEST(pm_instruction_to_alpha)
{
    /* a jmp instruction taken from software/compile-print.txt */
    const uint8_t opcode = 0x47;
    const uint8_t character = 0xf0;
    const uint8_t arg1 = 0x0d;
    const uint8_t arg2 = 0xfc;

    uint8_t mem[] = {opcode, character, arg1, arg2};

    struct ge g;
    int r;

    r = ge_init(&g);
    ck_assert_int_eq(r, 0);

    ge_clear(&g);

    r = ge_load(&g, mem, sizeof(mem));
    ck_assert_int_eq(r, 0);

    r = ge_start(&g);
    ck_assert_int_eq(r, 0);
    ck_assert_uint_eq(g.rSO, 0x80);

    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe2 || g.rSO == 0xe3);

    ge_run_cycle(&g);
    ck_assert(g.rSO == 0xe0);

    ge_run_cycle(&g);
    ck_assert_uint_eq(g.rSO, 0xe4);

    ge_run_cycle(&g);
    ck_assert_uint_eq(g.rSO, 0xe6);

    ge_run_cycle(&g);
    ck_assert(g.rSO == 0x64 || g.rSO == 0x65);

    ck_assert_uint_eq(g.rPO, 4);
    ck_assert_uint_eq(g.rRO, arg2);
    ck_assert_uint_eq(g.rV1, arg2);
    ck_assert_uint_eq(g.rV2, arg2);
    ck_assert_uint_eq(g.rL1, character);
    ck_assert_uint_eq(g.rL2, arg1);
}

Suite * init_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ALPHA");

    /* Core test case */
    tc_core = tcase_create("alpha_state");

    tcase_add_test(tc_core, p_instruction_to_alpha);
    tcase_add_test(tc_core, pm_instruction_to_alpha);
    tcase_add_test(tc_core, test_int);
    tcase_add_test(tc_core, test_hlt);

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

