#ifndef TESTS_H
#define TESTS_H

#include <check.h>

#include "../ge.h"

#define NOP2_OPCODE 0x07
#define HLT_OPCODE  0x0A

Suite *init_alpha_suite(void);
Suite *init_beta_suite(void);
Suite *init_peri_suite(void);
Suite *init_init_suite(void);

#endif /* TESTS_H */
