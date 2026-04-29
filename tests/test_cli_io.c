/*
 * SPDX-License-Identifier: Unlicense
 *
 * CMocka coverage for CLI stdin, output, and non-JSON parser I/O paths.
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

size_t cli_output_len_overflow(size_t message_len) {
    (void)message_len;
    return 0U;
}

void test_salt_cli_json_output_requires_key_id(void **state) {
    char *argv[] = {(char *)"salt",  (char *)"--output",   (char *)"json",
                    (char *)"--key", (char *)"base64-key", (char *)"Hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "Hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_non_null(strstr(stderr_buffer, "JSON output requires key_id"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_output_with_key_id_flag(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"-o", (char *)"json",       (char *)"-k",
                    (char *)"base64-key", (char *)"-i", (char *)"0123456789", (char *)"Hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_SMALL_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "Hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(8, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stdout_buffer, "\"encrypted_value\":\"ZW5jb2RlZA==\""));
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"0123456789\""));
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_output_with_key_object(void **state) {
    char *argv[] = {(char *)"salt",
                    (char *)"--output",
                    (char *)"json",
                    (char *)"--key",
                    (char *)"{\"key_id\":\"kid-1\",\"key\":\"base64-key\"}",
                    (char *)"Hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_SMALL_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "Hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"kid-1\""));
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_value_from_stdin_with_json_key(void **state) {
    char *argv[] = {(char *)"salt",
                    (char *)"--output",
                    (char *)"json",
                    (char *)"--key",
                    (char *)"{\"key_id\":\"kid-2\",\"key\":\"base64-key\"}",
                    (char *)"-"};
    FILE *in_stream = stream_from_text("Hello World!");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_SMALL_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "Hello World!";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"kid-2\""));
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_key_from_stdin_with_value_arg(void **state) {
    char *argv[] = {(char *)"salt",  (char *)"--output", (char *)"json",
                    (char *)"--key", (char *)"-",        (char *)"Hello World!"};
    FILE *in_stream = stream_from_text("{\"key_id\":\"kid-3\",\"key\":\"base64-key\"}\n");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_SMALL_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "Hello World!";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"kid-3\""));
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_dual_stdin_rejected(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"-", (char *)"-"};
    FILE *in_stream = stream_from_text("ignored");
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
    assert_non_null(strstr(stderr_buffer, "cannot both be read from stdin"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_output_text_option_explicit(void **state) {
    char *argv[] = {(char *)"salt",  (char *)"--output",   (char *)"text",
                    (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "ZW5jb2RlZA==\n");
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_output_write_failure_returns_io(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_fopen_checked("/dev/full", "w");
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_IO);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "failed to write encrypted output"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_output_write_failure_with_diagnostic_write_failure_stays_io(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_fopen_checked("/dev/full", "w");
    FILE *err_stream = stream_with_write_fail_after(0U);

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_IO);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_key_format_auto_option_explicit(void **state) {
    char *argv[] = {(char *)"salt",         (char *)"--key", (char *)"base64-key",
                    (char *)"--key-format", (char *)"auto",  (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "ZW5jb2RlZA==\n");
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_key_format_json_requires_json_input(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k",   (char *)"base64-key",
                    (char *)"-f",   (char *)"json", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "invalid JSON key object"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_key_format_base64_forces_plain_key(void **state) {
    char *argv[] = {(char *)"salt",
                    (char *)"--key",
                    (char *)"  {\"key_id\":\"kid\",\"key\":\"base64-key\"}  ",
                    (char *)"--key-format",
                    (char *)"base64",
                    (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "{\"key_id\":\"kid\",\"key\":\"base64-key\"}";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_OK);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_empty_plaintext_from_stdin_rejected(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"-"};
    FILE *in_stream = stream_from_text("");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "plaintext must not be empty"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_empty_key_from_stdin_rejected(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"-", (char *)"hello"};
    FILE *in_stream = stream_from_text("   \n\t");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "key input from stdin is empty"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_key_stdin_alloc_failure(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"-", (char *)"hello"};
    FILE *in_stream = stream_from_text("{\"key\":\"base64-key\"}");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    cli_fail_alloc_on_call = 1U;
    salt_cli_set_test_hooks(cli_fail_on_nth_alloc, NULL, NULL);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INTERNAL);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "failed to allocate input buffer"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_plaintext_stdin_alloc_failure(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"-"};
    FILE *in_stream = stream_from_text("hello");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    cli_fail_alloc_on_call = 2U;
    salt_cli_set_test_hooks(cli_fail_on_nth_alloc, NULL, NULL);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INTERNAL);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "failed to allocate input buffer"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_key_stdin_too_large(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"-", (char *)"hello"};
    FILE *in_stream;
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];
    char *large_input;

    (void)state;

    large_input = (char *)malloc(SALT_MAX_KEY_INPUT_LEN + 2U);
    assert_non_null(large_input);
    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(large_input, 'A', SALT_MAX_KEY_INPUT_LEN + 1U);
    large_input[SALT_MAX_KEY_INPUT_LEN + 1U] = '\0';

    in_stream = stream_from_text(large_input);
    free(large_input);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "exceeds maximum length"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_plaintext_stdin_too_large(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"-"};
    FILE *in_stream;
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];
    char *large_input;

    (void)state;

    large_input = (char *)malloc(SALT_GITHUB_SECRET_MAX_VALUE_LEN + 2U);
    assert_non_null(large_input);
    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(large_input, 'B', SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U);
    large_input[SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U] = '\0';

    in_stream = stream_from_text(large_input);
    free(large_input);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "exceeds maximum length"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_key_argv_too_large(void **state) {
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];
    char *large_key;
    char *argv[5];

    (void)state;

    large_key = (char *)malloc(SALT_MAX_KEY_INPUT_LEN + 2U);
    assert_non_null(large_key);
    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(large_key, 'A', SALT_MAX_KEY_INPUT_LEN + 1U);
    large_key[SALT_MAX_KEY_INPUT_LEN + 1U] = '\0';

    argv[0] = (char *)"salt";
    argv[1] = (char *)"-k";
    argv[2] = large_key;
    argv[3] = (char *)"hello";
    argv[4] = NULL;

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "key input exceeds maximum length"));

    free(large_key);
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_plaintext_argv_too_large(void **state) {
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];
    char *large_plaintext;
    char *argv[5];

    (void)state;

    large_plaintext = (char *)malloc(SALT_GITHUB_SECRET_MAX_VALUE_LEN + 2U);
    assert_non_null(large_plaintext);
    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(large_plaintext, 'C', SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U);
    large_plaintext[SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U] = '\0';

    argv[0] = (char *)"salt";
    argv[1] = (char *)"-k";
    argv[2] = (char *)"base64-key";
    argv[3] = large_plaintext;
    argv[4] = NULL;

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "plaintext exceeds maximum length"));

    free(large_plaintext);
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_output_escapes_special_chars(void **state) {
    char *argv[] = {(char *)"salt",          (char *)"--output",   (char *)"json",
                    (char *)"--key",         (char *)"base64-key", (char *)"--key-id",
                    (char *)"kid\"\\\n\r\t", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[512];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_fake_encoded = "A\"\\\n\r\t\x01";

    assert_int_equal(salt_cli_run_with_streams(8, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    assert_non_null(strstr(stdout_buffer, "\\\""));
    assert_non_null(strstr(stdout_buffer, "\\\\"));
    assert_non_null(strstr(stdout_buffer, "\\n"));
    assert_non_null(strstr(stdout_buffer, "\\r"));
    assert_non_null(strstr(stdout_buffer, "\\t"));
    assert_non_null(strstr(stdout_buffer, "\\u0001"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_wrapper_function(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"hello"};
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[64];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";
    assert_int_equal(salt_cli_run(4, argv, out_stream, err_stream), SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "ZW5jb2RlZA==\n");
    assert_string_equal(stderr_buffer, "");

    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_empty_key_argument_rejected(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"   ", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "public key must not be empty"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_output_alloc_failure(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    cli_fail_alloc_on_call = 3U;
    salt_cli_set_test_hooks(cli_fail_on_nth_alloc, NULL, cli_fake_success);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INTERNAL);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "failed to allocate output buffer"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_argv_plaintext_alloc_failure(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    cli_fail_alloc_on_call = 2U;
    salt_cli_set_test_hooks(cli_fail_on_nth_alloc, NULL, cli_fake_success);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INTERNAL);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "failed to allocate plaintext buffer"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_argv_path_alloc_failure_sweep_first_three(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"hello"};

    (void)state;

    for (size_t i = 0U; i < 3U; ++i) {
        FILE *in_stream = stream_from_text(NULL);
        FILE *out_stream = test_tmpfile_checked();
        FILE *err_stream = test_tmpfile_checked();
        char stderr_buffer[TEST_SMALL_BUFFER_SIZE];
        char stdout_buffer[64];

        cli_fail_alloc_on_call = i + 1U;
        salt_cli_set_test_hooks(cli_fail_on_nth_alloc, NULL, cli_fake_success);
        assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                         SALT_ERR_INTERNAL);
        read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
        read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
        assert_string_equal(stdout_buffer, "");
        assert_non_null(strstr(stderr_buffer, "failed to allocate"));

        (void)fclose(in_stream);
        (void)fclose(out_stream);
        (void)fclose(err_stream);
    }
}

void test_salt_cli_output_size_overflow(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    salt_cli_set_output_len_test_hook(cli_output_len_overflow);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INTERNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_non_null(strstr(stderr_buffer, "output size overflow"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_plaintext_omitted_reads_stdin(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key"};
    FILE *in_stream = stream_from_text("hello");
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

    assert_int_equal(salt_cli_run_with_streams(3, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "ZW5jb2RlZA==\n");
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_empty_key_id_option_falls_back_to_parsed_key_id(void **state) {
    char *argv[] = {(char *)"salt",
                    (char *)"-o",
                    (char *)"json",
                    (char *)"-k",
                    (char *)"{\"key_id\":\"kid-from-json\",\"key\":\"base64-key\"}",
                    (char *)"-i",
                    (char *)"",
                    (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_SMALL_BUFFER_SIZE];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(8, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"kid-from-json\""));
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

int run_cli_io_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_salt_cli_json_output_requires_key_id, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_output_with_key_id_flag, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_output_with_key_object, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_value_from_stdin_with_json_key,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_key_from_stdin_with_value_arg, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_dual_stdin_rejected, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_output_text_option_explicit, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_output_write_failure_returns_io,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_output_write_failure_with_diagnostic_write_failure_stays_io,
            cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_key_format_auto_option_explicit,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_key_format_json_requires_json_input,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_key_format_base64_forces_plain_key,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_empty_plaintext_from_stdin_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_empty_key_from_stdin_rejected, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_key_stdin_alloc_failure, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_plaintext_stdin_alloc_failure, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_key_stdin_too_large, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_plaintext_stdin_too_large, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_key_argv_too_large, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_plaintext_argv_too_large, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_output_escapes_special_chars,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_wrapper_function, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_empty_key_argument_rejected, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_output_alloc_failure, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_argv_plaintext_alloc_failure, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_argv_path_alloc_failure_sweep_first_three,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_output_size_overflow, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_plaintext_omitted_reads_stdin, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_empty_key_id_option_falls_back_to_parsed_key_id, cli_test_setup,
            cli_test_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
