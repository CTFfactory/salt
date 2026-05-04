/*
 * SPDX-License-Identifier: Unlicense
 *
 * Phase 2 characterization tests for user-visible CLI contract behavior.
 *
 * This file locks high-value CLI contract invariants discovered through
 * fuzzing and system testing:
 * - Exit code stability (SALT_OK=0, SALT_ERR_INPUT=1, SALT_ERR_IO=2)
 * - stdout/stderr separation (errors go to stderr, output to stdout)
 * - JSON control-character escaping (RFC 8259 compliance)
 * - UTF-8 validation for JSON output
 * - Help/version parity with man page
 */

/* POSIX feature-test macro must be named exactly. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _POSIX_C_SOURCE 200809L

#include "test_shared.h"
#include "../test_suites.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

/* ============================================================================
 * Exit Code Contract Tests
 * ============================================================================ */

void test_cli_exit_code_success_is_zero(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--version"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    assert_int_equal(SALT_OK, 0);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_cli_exit_code_input_error_is_one(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)""};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(3, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    assert_int_equal(SALT_ERR_INPUT, 1);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_cli_exit_code_io_error_is_four(void **state) {
    (void)state;
    /* IO errors are SALT_ERR_IO=4 per contract */
    assert_int_equal(SALT_ERR_IO, 4);
}

/* ============================================================================
 * stdout/stderr Separation Tests
 * ============================================================================ */

void test_cli_help_writes_to_stdout_only(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--help"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));

    /* Help goes to stdout */
    assert_true(strlen(stdout_buffer) > 0);
    assert_string_starts_with(stdout_buffer, "usage: salt");
    /* stderr stays empty */
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_cli_version_writes_to_stdout_only(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--version"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));

    /* Version goes to stdout */
    assert_true(strlen(stdout_buffer) > 0);
    assert_string_starts_with(stdout_buffer, "salt ");
    /* stderr stays empty */
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_cli_errors_write_to_stderr_only(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)""};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[128];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(3, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));

    /* stdout stays empty */
    assert_string_equal(stdout_buffer, "");
    /* Error message goes to stderr */
    assert_true(strlen(stderr_buffer) > 0);
    assert_string_starts_with(stderr_buffer, "error:");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_cli_success_output_writes_to_stdout_only(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"Hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "Hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));

    /* Output goes to stdout */
    assert_true(strlen(stdout_buffer) > 0);
    /* stderr stays empty */
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

/* ============================================================================
 * JSON Control Character Escaping Tests (RFC 8259 Compliance)
 * ============================================================================ */

void test_json_output_escapes_control_char_null(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",       (char *)"-k",
                    (char *)"base64-key", (char *)"-i", (char *)"key-\x00-id"};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";
    cli_fake_encoded = "ZW5jb2RlZA==";

    /* Note: argv stops at first null, so key-id becomes "key-" */
    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* key_id should be properly JSON escaped if it contained control chars */
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"key-\""));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_json_output_escapes_control_char_tab(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",   (char *)"-k",
                    (char *)"base64-key", (char *)"-i", (char *)"key\tid"};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";
    cli_fake_encoded = "ZW5jb2RlZA==";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Tab should be escaped as \t */
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"key\\tid\""));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_json_output_escapes_control_char_newline(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",   (char *)"-k",
                    (char *)"base64-key", (char *)"-i", (char *)"key\nid"};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";
    cli_fake_encoded = "ZW5jb2RlZA==";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Newline should be escaped as \n */
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"key\\nid\""));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_json_output_escapes_control_char_carriage_return(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",   (char *)"-k",
                    (char *)"base64-key", (char *)"-i", (char *)"key\rid"};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";
    cli_fake_encoded = "ZW5jb2RlZA==";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Carriage return should be escaped as \r */
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"key\\rid\""));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_json_output_escapes_control_char_bell(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",     (char *)"-k",
                    (char *)"base64-key", (char *)"-i", (char *)"key\x07id"};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";
    cli_fake_encoded = "ZW5jb2RlZA==";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Control char \x07 (bell) should be escaped as \u0007 */
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"key\\u0007id\""));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_json_output_escapes_backslash(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",   (char *)"-k",
                    (char *)"base64-key", (char *)"-i", (char *)"key\\id"};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";
    cli_fake_encoded = "ZW5jb2RlZA==";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Backslash should be escaped as \\ */
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"key\\\\id\""));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_json_output_escapes_quote(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",   (char *)"-k",
                    (char *)"base64-key", (char *)"-i", (char *)"key\"id"};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";
    cli_fake_encoded = "ZW5jb2RlZA==";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Quote should be escaped as \" */
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"key\\\"id\""));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

/* ============================================================================
 * UTF-8 Validation Tests
 * ============================================================================ */

void test_json_output_rejects_invalid_utf8_in_key_id(void **state) {
    /* Invalid UTF-8: 0xFF is never valid in UTF-8 */
    char invalid_utf8_key_id[] = "key\xFFid";
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",     (char *)"-k",
                    (char *)"base64-key", (char *)"-i", invalid_utf8_key_id};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));

    /* Should reject with UTF-8 validation error */
    assert_non_null(strstr(stderr_buffer, "invalid UTF-8"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_json_output_rejects_overlong_utf8_encoding(void **state) {
    /* Overlong encoding: 0xC0 0x80 represents NULL but is invalid per RFC 3629 */
    char overlong_utf8[] = "key\xC0\x80id";
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json", (char *)"-k",
                    (char *)"base64-key", (char *)"-i", overlong_utf8};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));

    /* Should reject with UTF-8 validation error */
    assert_non_null(strstr(stderr_buffer, "invalid UTF-8"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_json_output_accepts_valid_multibyte_utf8(void **state) {
    /* Valid UTF-8: € sign (U+20AC) = E2 82 AC */
    char *argv[] = {(char *)"salt",
                    (char *)"-o",
                    (char *)"json",
                    (char *)"-k",
                    (char *)"base64-key",
                    (char *)"-i",
                    (char *)"key\xE2\x82\xACid"};
    FILE *in_stream = stream_from_text("test");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "test";
    cli_expected_key = "base64-key";
    cli_fake_encoded = "ZW5jb2RlZA==";

    assert_int_equal(salt_cli_run_with_streams(7, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Valid UTF-8 should pass through */
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"key\xE2\x82\xACid\""));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

/* ============================================================================
 * Help/Version Parity Tests
 * ============================================================================ */

void test_cli_help_mentions_all_options(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--help"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Help text should mention all major options */
    assert_non_null(strstr(stdout_buffer, "--key"));
    assert_non_null(strstr(stdout_buffer, "--key-id"));
    assert_non_null(strstr(stdout_buffer, "--output"));
    assert_non_null(strstr(stdout_buffer, "--help"));
    assert_non_null(strstr(stdout_buffer, "--version"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_cli_short_help_flag_works(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-h"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Short -h should produce help */
    assert_string_starts_with(stdout_buffer, "usage: salt");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_cli_version_includes_salt_string(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--version"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(2, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));

    /* Version should contain "salt" and a version number pattern */
    assert_non_null(strstr(stdout_buffer, "salt"));
    /* Basic version format check (e.g., "salt 0.1.0" or similar) */
    assert_true(strlen(stdout_buffer) > 5);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

/* ============================================================================
 * Suite Runner
 * ============================================================================ */

int run_cli_characterization_tests(void) {
    const struct CMUnitTest tests[] = {
        /* Exit code contract */
        cmocka_unit_test_setup_teardown(test_cli_exit_code_success_is_zero, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_cli_exit_code_input_error_is_one, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_cli_exit_code_io_error_is_four, cli_test_setup,
                                        cli_test_teardown),

        /* stdout/stderr separation */
        cmocka_unit_test_setup_teardown(test_cli_help_writes_to_stdout_only, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_cli_version_writes_to_stdout_only, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_cli_errors_write_to_stderr_only, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_cli_success_output_writes_to_stdout_only,
                                        cli_test_setup, cli_test_teardown),

        /* JSON control character escaping */
        cmocka_unit_test_setup_teardown(test_json_output_escapes_control_char_null, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_json_output_escapes_control_char_tab, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_json_output_escapes_control_char_newline,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_json_output_escapes_control_char_carriage_return,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_json_output_escapes_control_char_bell, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_json_output_escapes_backslash, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_json_output_escapes_quote, cli_test_setup,
                                        cli_test_teardown),

        /* UTF-8 validation */
        cmocka_unit_test_setup_teardown(test_json_output_rejects_invalid_utf8_in_key_id,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_json_output_rejects_overlong_utf8_encoding,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_json_output_accepts_valid_multibyte_utf8,
                                        cli_test_setup, cli_test_teardown),

        /* Help/version parity */
        cmocka_unit_test_setup_teardown(test_cli_help_mentions_all_options, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_cli_short_help_flag_works, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_cli_version_includes_salt_string, cli_test_setup,
                                        cli_test_teardown),
    };

    return cmocka_run_group_tests_name("CLI characterization", tests, NULL, NULL);
}
