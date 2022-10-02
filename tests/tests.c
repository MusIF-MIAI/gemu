#include "tests.h"

int main(void)
{
    int number_failed;
    SRunner *sr;

    sr = srunner_create(init_init_suite());
    srunner_add_suite(sr, init_alpha_suite());
    srunner_add_suite(sr, init_beta_suite());
    srunner_add_suite(sr, init_peri_suite());

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}

