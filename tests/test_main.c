/*
 * SPDX-License-Identifier: Unlicense
 *
 * CMocka test runner that dispatches behavior-oriented suites.
 */

#include "test_suites.h"

int main(void) {
    int status = 0;

    status += run_salt_tests();
    status += run_cli_contract_tests();
    status += run_cli_io_tests();
    status += run_cli_json_tests();
    status += run_cli_runtime_tests();
    status += run_cli_helper_tests();
    status += run_cli_characterization_tests();
    status += run_fuzz_regression_tests();

    return status;
}
