/*
 * SPDX-License-Identifier: Unlicense
 *
 * CMocka coverage for strict JSON key parsing and JSON output encoding behavior.
 */

/* POSIX feature-test macro must be named exactly. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _POSIX_C_SOURCE 200809L

#include "../test_cli_shared.h"
#include "../test_suites.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

void test_salt_cli_json_key_missing_key_field_rejected(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k",   (char *)"{\"key_id\":\"kid\"}",
                    (char *)"-f",   (char *)"json", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "must include string field 'key'"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_empty_key_field_rejected(void **state) {
    char *argv[] = {(char *)"salt", (char *)"-k",   (char *)"{\"key\":\"  \"}",
                    (char *)"-f",   (char *)"json", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "must not be empty"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_unknown_field_rejected(void **state) {
    char *argv[] = {
        (char *)"salt",         (char *)"--key", (char *)"{\"key\":\"base64-key\",\"extra\":\"x\"}",
        (char *)"--key-format", (char *)"json",  (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "unsupported field"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_duplicate_field_rejected(void **state) {
    char *argv[] = {(char *)"salt",
                    (char *)"--key",
                    (char *)"{\"key\":\"base64-key\",\"key\":\"other\"}",
                    (char *)"--key-format",
                    (char *)"json",
                    (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "duplicate field 'key'"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_malformed_object_rejected(void **state) {
    char *argv[] = {
        (char *)"salt",         (char *)"--key", (char *)"{\"key\":\"base64-key\" trailing",
        (char *)"--key-format", (char *)"json",  (char *)"hello"};
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

void test_salt_cli_json_key_whitespace_success(void **state) {
    char *argv[] = {(char *)"salt",
                    (char *)"--output",
                    (char *)"json",
                    (char *)"--key",
                    (char *)"  {  \"key_id\"  :  \"kid-space\" ,  \"key\"  :  \"base64-key\"  }  ",
                    (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_SMALL_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stdout_buffer, "\"key_id\":\"kid-space\""));
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_empty_object_rejected(void **state) {
    char *argv[] = {(char *)"salt",         (char *)"--key", (char *)"{}",
                    (char *)"--key-format", (char *)"json",  (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "must include string field 'key'"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_missing_colon_rejected(void **state) {
    char *argv[] = {(char *)"salt",         (char *)"--key", (char *)"{\"key\" \"base64-key\"}",
                    (char *)"--key-format", (char *)"json",  (char *)"hello"};
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

void test_salt_cli_json_key_non_string_key_rejected(void **state) {
    char *argv[] = {(char *)"salt",         (char *)"--key", (char *)"{\"key\":123}",
                    (char *)"--key-format", (char *)"json",  (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "must include string field 'key'"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_duplicate_key_id_rejected(void **state) {
    char *argv[] = {(char *)"salt",
                    (char *)"--key",
                    (char *)"{\"key\":\"base64-key\",\"key_id\":\"a\",\"key_id\":\"b\"}",
                    (char *)"--key-format",
                    (char *)"json",
                    (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "duplicate field 'key_id'"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_non_string_key_id_rejected(void **state) {
    char *argv[] = {
        (char *)"salt",         (char *)"--key", (char *)"{\"key\":\"base64-key\",\"key_id\":123}",
        (char *)"--key-format", (char *)"json",  (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "field 'key_id' must be a string"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_trailing_content_rejected(void **state) {
    char *argv[] = {
        (char *)"salt",         (char *)"--key", (char *)"{\"key\":\"base64-key\"} trailing",
        (char *)"--key-format", (char *)"json",  (char *)"hello"};
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

void test_salt_cli_json_key_id_max_length_accepted(void **state) {
    char key_id[SALT_MAX_KEY_ID_LEN + 1U];
    char key_json[512];
    char *argv[] = {(char *)"salt",  (char *)"--output", (char *)"json",
                    (char *)"--key", key_json,           (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(key_id, 'k', SALT_MAX_KEY_ID_LEN);
    key_id[SALT_MAX_KEY_ID_LEN] = '\0';
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    assert_true((size_t)snprintf(key_json, sizeof(key_json),
                                 "{\"key_id\":\"%s\",\"key\":\"base64-key\"}",
                                 key_id) < sizeof(key_json));

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(6, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stdout_buffer, "\"key_id\":"));
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_key_id_too_long_rejected(void **state) {
    char key_id[SALT_MAX_KEY_ID_LEN + 2U];
    char key_json[600];
    char *argv[] = {(char *)"salt", (char *)"--key", key_json, (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(key_id, 'k', SALT_MAX_KEY_ID_LEN + 1U);
    key_id[SALT_MAX_KEY_ID_LEN + 1U] = '\0';
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    assert_true((size_t)snprintf(key_json, sizeof(key_json),
                                 "{\"key_id\":\"%s\",\"key\":\"base64-key\"}",
                                 key_id) < sizeof(key_json));

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "key_id"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_repeated_invocations_isolated(void **state) {
    char *argv_first[] = {(char *)"salt",  (char *)"--output",   (char *)"json",
                          (char *)"--key", (char *)"base64-key", (char *)"--key-id",
                          (char *)"kid-1", (char *)"hello"};
    char *argv_second[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"again"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_first = test_tmpfile_checked();
    FILE *err_first = test_tmpfile_checked();
    FILE *out_second = test_tmpfile_checked();
    FILE *err_second = test_tmpfile_checked();
    char first_out[TEST_SMALL_BUFFER_SIZE];
    char second_out[TEST_SMALL_BUFFER_SIZE];
    char stderr_buffer[128];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);

    cli_expected_message = "hello";
    cli_expected_key = "base64-key";
    assert_int_equal(salt_cli_run_with_streams(8, argv_first, in_stream, out_first, err_first),
                     SALT_OK);

    cli_expected_message = "again";
    cli_expected_key = "base64-key";
    assert_int_equal(salt_cli_run_with_streams(4, argv_second, in_stream, out_second, err_second),
                     SALT_OK);

    read_stream(out_first, first_out, sizeof(first_out));
    read_stream(out_second, second_out, sizeof(second_out));
    read_stream(err_first, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer, "");
    read_stream(err_second, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer, "");
    assert_non_null(strstr(first_out, "\"encrypted_value\":"));
    assert_string_equal(second_out, "ZW5jb2RlZA==\n");

    (void)fclose(in_stream);
    (void)fclose(out_first);
    (void)fclose(err_first);
    (void)fclose(out_second);
    (void)fclose(err_second);
}

void test_salt_cli_json_output_utf8_key_id_preserved(void **state) {
    char *argv[] = {(char *)"salt",       (char *)"--output", (char *)"json",   (char *)"--key",
                    (char *)"base64-key", (char *)"--key-id", (char *)"密钥🔐", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(8, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stdout_buffer, "密钥🔐"));
    assert_string_equal(stderr_buffer, "");

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_output_invalid_utf8_key_id_rejected(void **state) {
    char key_id_bytes[] = {(char)0xC3, (char)0x28, '\0'};
    char *argv[] = {(char *)"salt",       (char *)"--output", (char *)"json", (char *)"--key",
                    (char *)"base64-key", (char *)"--key-id", key_id_bytes,   (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(8, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "invalid UTF-8"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_output_invalid_utf8_key_id_multibyte_rejected(void **state) {
    char key_id_bytes[] = {(char)0xE2, (char)0x28, (char)0xA1, '\0'};
    char *argv[] = {(char *)"salt",       (char *)"--output", (char *)"json", (char *)"--key",
                    (char *)"base64-key", (char *)"--key-id", key_id_bytes,   (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(8, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "invalid UTF-8"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_output_truncated_utf8_key_id_rejected(void **state) {
    char key_id_bytes[] = {(char)0xF0, (char)0x9F, '\0'};
    char *argv[] = {(char *)"salt",       (char *)"--output", (char *)"json", (char *)"--key",
                    (char *)"base64-key", (char *)"--key-id", key_id_bytes,   (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(8, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "invalid UTF-8"));

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_json_output_locale_independent(void **state) {
    const char *requested_locales[] = {"C", "C.UTF-8", "en_US.UTF-8"};
    char *argv[] = {(char *)"salt",       (char *)"--output", (char *)"json",   (char *)"--key",
                    (char *)"base64-key", (char *)"--key-id", (char *)"密钥🔐", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    char baseline_output[TEST_STDOUT_BUFFER_SIZE];
    char stdout_buffer[TEST_STDOUT_BUFFER_SIZE];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];
    char original_locale[128];
    const char *current = setlocale(LC_ALL, NULL);
    size_t i;
    bool baseline_set = false;

    (void)state;

    if (current != NULL) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)snprintf(original_locale, sizeof(original_locale), "%s", current);
    } else {
        original_locale[0] = '\0';
    }

    for (i = 0U; i < (sizeof(requested_locales) / sizeof(requested_locales[0])); ++i) {
        FILE *out_stream;
        FILE *err_stream;

        if (setlocale(LC_ALL, requested_locales[i]) == NULL) {
            continue;
        }

        out_stream = test_tmpfile_checked();
        err_stream = test_tmpfile_checked();
        assert_non_null(out_stream);
        assert_non_null(err_stream);

        salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
        cli_expected_message = "hello";
        cli_expected_key = "base64-key";

        assert_int_equal(salt_cli_run_with_streams(8, argv, in_stream, out_stream, err_stream),
                         SALT_OK);
        read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
        read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
        assert_non_null(strstr(stdout_buffer, "\"key_id\":\"密钥🔐\""));
        assert_string_equal(stderr_buffer, "");

        if (!baseline_set) {
            /* clang-format off */
            /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
            (void)snprintf(baseline_output, sizeof(baseline_output), "%s", stdout_buffer);
            /* clang-format on */
            baseline_set = true;
        } else {
            assert_string_equal(stdout_buffer, baseline_output);
        }

        (void)fclose(out_stream);
        (void)fclose(err_stream);
        assert_int_equal(fseek(in_stream, 0L, SEEK_SET), 0);
    }

    assert_true(baseline_set);
    if (original_locale[0] != '\0') {
        (void)setlocale(LC_ALL, original_locale);
    }

    (void)fclose(in_stream);
}

int run_cli_json_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_missing_key_field_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_empty_key_field_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_unknown_field_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_duplicate_field_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_malformed_object_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_whitespace_success, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_empty_object_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_missing_colon_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_non_string_key_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_duplicate_key_id_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_non_string_key_id_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_trailing_content_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_id_max_length_accepted,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_key_id_too_long_rejected, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_repeated_invocations_isolated,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_output_utf8_key_id_preserved,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_output_invalid_utf8_key_id_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_json_output_invalid_utf8_key_id_multibyte_rejected, cli_test_setup,
            cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_output_truncated_utf8_key_id_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_json_output_locale_independent,
                                        cli_test_setup, cli_test_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
