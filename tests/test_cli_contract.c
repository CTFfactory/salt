/*
 * SPDX-License-Identifier: Unlicense
 *
 * CMocka coverage for CLI contract, option parsing, and top-level runtime errors.
 */

/* POSIX feature-test macro must be named exactly. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _POSIX_C_SOURCE 200809L

#include "test_cli_shared.h"
#include "test_suites.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

void test_salt_cli_help(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--help"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_starts_with(stdout_buffer, "usage: salt [OPTIONS] [PLAINTEXT]\n");
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_version(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-V"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];
    char stderr_buffer[128];
    char expected[64];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)snprintf(expected, sizeof(expected), "salt %s\n", SALT_VERSION);
    assert_string_equal(stdout_buffer, expected);
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_missing_key_error(void **state) {
    char *argv[] = {(char *)"salt", (char *)"Hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_starts_with(stderr_buffer, "error: --key/-k is required\n"
                                             "usage: salt [OPTIONS] [PLAINTEXT]\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_invalid_output_format(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k",   (char *)"base64-key",
                    (char *)"-o",   (char *)"yaml", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer,
                        "error: invalid output format 'yaml' (expected text|json)\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_invalid_key_format(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k",  (char *)"base64-key",
                    (char *)"-f",   (char *)"xml", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer,
                        "error: invalid key format 'xml' (expected auto|base64|json)\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_missing_option_argument(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer, "error: option requires an argument: -k\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_too_many_positionals(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"hello",
                    (char *)"extra"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[512];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(5, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_starts_with(stderr_buffer, "error: too many positional arguments\n"
                                             "usage: salt [OPTIONS] [PLAINTEXT]\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_help_write_failure_returns_io(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--help"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_fopen_checked("/dev/full", "w");
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_IO);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer, "error: failed to write help output\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_help_immediate_write_failure_returns_io(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--help"};
    char readonly_buffer[32] = {0};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = fmemopen(readonly_buffer, sizeof(readonly_buffer), "r");
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_IO);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer, "error: failed to write help output\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_missing_key_diagnostic_write_failure_returns_io(void **state) {
    char *argv[] = {(char *)"salt", (char *)"Hello"};
    char readonly_buffer[32] = {0};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = fmemopen(readonly_buffer, sizeof(readonly_buffer), "r");

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_IO);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_missing_key_usage_write_failure_returns_io(void **state) {
    char *argv[] = {(char *)"salt", (char *)"Hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = stream_with_write_fail_after(1U);

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_IO);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_too_many_positionals_usage_write_failure_returns_io(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"hello",
                    (char *)"extra"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = stream_with_write_fail_after(1U);

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(salt_cli_run_with_streams(5, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_IO);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_too_many_positionals_diagnostic_write_failure_returns_io(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"hello",
                    (char *)"extra"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = stream_with_write_fail_after(0U);

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(salt_cli_run_with_streams(5, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_IO);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_usage_error(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--unknown"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: invalid option: --unknown\n");
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_alloc_failure(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[128];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(cli_failing_alloc, NULL, NULL);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INTERNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "internal error: failed to allocate key buffer\n");
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_seal_error(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[128];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_error);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: simulated failure\n");
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_zero_output_written(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[128];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_zero_output);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INTERNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: produced empty output\n");
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_success(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[64];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "ZW5jb2RlZA==\n");
    assert_string_equal(stderr_buffer, "");
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_invalid_short_option(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-z"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: invalid option: -z\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_invalid_long_option(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--unknown-flag"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: invalid option: --unknown-flag\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_null_arguments_rejected(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--help"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    assert_int_equal(salt_cli_run_with_streams(0, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    assert_int_equal(salt_cli_run_with_streams(2, NULL, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    assert_int_equal(salt_cli_run_with_streams(2, argv, NULL, out_stream, err_stream),
                     SALT_ERR_INPUT);
    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, NULL, err_stream),
                     SALT_ERR_INPUT);
    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, NULL),
                     SALT_ERR_INPUT);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_empty_key_argument_literal_empty(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: --key/-k value must not be empty\n");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

int run_cli_contract_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_salt_cli_help, cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_version, cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_missing_key_error, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_invalid_output_format, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_invalid_key_format, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_missing_option_argument, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_too_many_positionals, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_help_write_failure_returns_io, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_help_immediate_write_failure_returns_io,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_missing_key_diagnostic_write_failure_returns_io, cli_test_setup,
            cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_missing_key_usage_write_failure_returns_io,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_too_many_positionals_usage_write_failure_returns_io, cli_test_setup,
            cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_too_many_positionals_diagnostic_write_failure_returns_io, cli_test_setup,
            cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_usage_error, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_alloc_failure, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_seal_error, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_zero_output_written, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_success, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_invalid_short_option, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_invalid_long_option, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_null_arguments_rejected, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_empty_key_argument_literal_empty,
                                        cli_test_setup, cli_test_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
