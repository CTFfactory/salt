/*
 * SPDX-License-Identifier: Unlicense
 *
 * Suite runners for cmocka test groups.
 */

#ifndef SALT_TEST_SUITES_H
#define SALT_TEST_SUITES_H

int run_salt_tests(void);
int run_cli_contract_tests(void);
int run_cli_io_tests(void);
int run_cli_json_tests(void);
int run_cli_runtime_tests(void);
int run_cli_helper_tests(void);
int run_cli_characterization_tests(void);
int run_fuzz_regression_tests(void);

#endif
