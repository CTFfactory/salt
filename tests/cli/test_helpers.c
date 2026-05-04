/*
 * SPDX-License-Identifier: Unlicense
 *
 * CMocka coverage for CLI parser helper paths and hook behavior.
 */

#include "test_shared.h"
#include "../test_suites.h"

#include "../../src/cli/state.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sodium.h>

#include <cmocka.h>

static size_t g_cli_probe_ferror_calls = 0U;

static int cli_stream_ferror_after_probe(FILE *stream) {
    (void)stream;
    ++g_cli_probe_ferror_calls;
    return (g_cli_probe_ferror_calls >= 2U) ? 1 : 0;
}

static int cli_stream_ungetc_fail(int c, FILE *stream) {
    (void)c;
    (void)stream;
    return EOF;
}

void test_salt_cli_helper_parse_key_json_escaped_sequences(void **state) {
    char key_b64[64];
    char key_id[64];
    char error[TEST_SMALL_BUFFER_SIZE] = {0};
    bool has_key_id = false;

    (void)state;

    assert_int_equal(
        salt_cli_test_parse_key_json_object(
            "{\"key\":\"line\\nslash\\/quote\\\"tab\\tback\\\\\",\"key_id\":\"kid\\r\"}", key_b64,
            sizeof(key_b64), key_id, sizeof(key_id), &has_key_id, error, sizeof(error)),
        SALT_OK);
    assert_string_equal(key_b64, "line\nslash/quote\"tab\tback\\");
    assert_string_equal(key_id, "kid");
    assert_true(has_key_id);
}

void test_salt_cli_helper_parse_key_json_unknown_escape_rejected(void **state) {
    char key_b64[64];
    char key_id[64];
    char error[TEST_SMALL_BUFFER_SIZE] = {0};
    bool has_key_id = false;

    (void)state;

    /* \u, \b, \f, and other unsupported escapes must be rejected, not silently
     * collapsed to a literal character. */
    assert_int_equal(salt_cli_test_parse_key_json_object("{\"key\":\"a\\u0041b\"}", key_b64,
                                                         sizeof(key_b64), key_id, sizeof(key_id),
                                                         &has_key_id, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "must include string field 'key'"));

    assert_int_equal(salt_cli_test_parse_key_json_object("{\"key\":\"abc\\bdef\"}", key_b64,
                                                         sizeof(key_b64), key_id, sizeof(key_id),
                                                         &has_key_id, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "must include string field 'key'"));
}

void test_salt_cli_helper_parse_key_json_trailing_backslash_rejected(void **state) {
    char key_b64[64];
    char key_id[64];
    char error[TEST_SMALL_BUFFER_SIZE] = {0};
    bool has_key_id = false;

    (void)state;

    assert_int_equal(salt_cli_test_parse_key_json_object("{\"key\":\"abc\\", key_b64,
                                                         sizeof(key_b64), key_id, sizeof(key_id),
                                                         &has_key_id, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "must include string field 'key'"));
}

void test_salt_cli_helper_parse_key_json_control_byte_rejected(void **state) {
    char key_b64[64];
    char key_id[64];
    char error[TEST_SMALL_BUFFER_SIZE] = {0};
    bool has_key_id = false;
    static const char raw_in_key[] = {'{', '"', 'k',  'e', 'y', '"', ':',
                                      '"', 'a', 0x01, 'b', '"', '}', 0};
    static const char raw_in_key_id[] = {'{', '"', 'k', 'e', 'y',  '"', ':', '"', 'A', 'A',
                                         '"', ',', '"', 'k', 'e',  'y', '_', 'i', 'd', '"',
                                         ':', '"', 'i', 'd', 0x1F, '!', '"', '}', 0};
    static const char raw_in_field[] = {'{', '"', 'k', 0x02, '"', ':', '"', 'v', '"', '}', 0};

    (void)state;

    assert_int_equal(salt_cli_test_parse_key_json_object(raw_in_key, key_b64, sizeof(key_b64),
                                                         key_id, sizeof(key_id), &has_key_id, error,
                                                         sizeof(error)),
                     SALT_ERR_INPUT);

    assert_int_equal(salt_cli_test_parse_key_json_object(raw_in_key_id, key_b64, sizeof(key_b64),
                                                         key_id, sizeof(key_id), &has_key_id, error,
                                                         sizeof(error)),
                     SALT_ERR_INPUT);

    assert_int_equal(salt_cli_test_parse_key_json_object(raw_in_field, key_b64, sizeof(key_b64),
                                                         key_id, sizeof(key_id), &has_key_id, error,
                                                         sizeof(error)),
                     SALT_ERR_INPUT);
}

void test_salt_cli_helper_parse_key_json_escape_buffer_too_small(void **state) {
    char key_b64[1];
    char key_id[64];
    char error[TEST_SMALL_BUFFER_SIZE] = {0};
    bool has_key_id = false;

    (void)state;

    assert_int_equal(salt_cli_test_parse_key_json_object("{\"key\":\"\\n\"}", key_b64,
                                                         sizeof(key_b64), key_id, sizeof(key_id),
                                                         &has_key_id, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "must include string field 'key'"));
}

void test_salt_cli_helper_parse_key_json_fuzz_regression(void **state) {
    uint32_t seed = 0xC0FFEEU;
    size_t i;

    (void)state;

    for (i = 0U; i < 500U; ++i) {
        char input[192];
        char key_b64[128];
        char key_id[128];
        char error[TEST_SMALL_BUFFER_SIZE] = {0};
        bool has_key_id = false;
        size_t j;
        int rc;

        for (j = 0U; j < sizeof(input) - 1U; ++j) {
            seed = (1103515245U * seed + 12345U);
            input[j] = (char)((seed >> 16) & 0x7F);
            if (input[j] == '\0') {
                input[j] = '{';
            }
        }
        input[sizeof(input) - 1U] = '\0';

        rc = salt_cli_test_parse_key_json_object(input, key_b64, sizeof(key_b64), key_id,
                                                 sizeof(key_id), &has_key_id, error, sizeof(error));
        assert_true(rc == SALT_OK || rc == SALT_ERR_INPUT);
    }
}

void test_salt_cli_helper_parse_key_input_leading_whitespace(void **state) {
    char key_b64[64];
    char key_id[64];
    char error[TEST_SMALL_BUFFER_SIZE] = {0};
    bool has_key_id = false;

    (void)state;

    assert_int_equal(salt_cli_test_parse_key_input("   base64-key", 0, key_b64, sizeof(key_b64),
                                                   key_id, sizeof(key_id), &has_key_id, error,
                                                   sizeof(error)),
                     SALT_OK);
    assert_string_equal(key_b64, "base64-key");
    assert_false(has_key_id);
}

void test_salt_cli_helper_reset_stream_hooks(void **state) {
    FILE *in_stream = stream_from_text("abc");
    char *output = NULL;
    size_t output_len = 0U;
    char error[TEST_SMALL_BUFFER_SIZE] = {0};

    (void)state;

    salt_cli_set_stream_test_hooks(cli_stream_fread, cli_stream_ferror, cli_stream_fgetc,
                                   cli_stream_ungetc);
    salt_cli_reset_stream_test_hooks();

    assert_int_equal(salt_cli_test_read_stream_limited(in_stream, 16U, &output, &output_len, error,
                                                       sizeof(error)),
                     SALT_OK);
    assert_int_equal(output_len, 3U);
    assert_string_equal(output, "abc");

    free(output);
    (void)fclose(in_stream);
}

void test_salt_cli_helper_interrupted_read(void **state) {
    FILE *in_stream = stream_from_text("abc");
    char *output = NULL;
    size_t output_len = 0U;
    char error[TEST_SMALL_BUFFER_SIZE] = {0};

    (void)state;

    salt_cli_test_set_interrupted(true);
    assert_int_equal(salt_cli_test_read_stream_limited(in_stream, 16U, &output, &output_len, error,
                                                       sizeof(error)),
                     SALT_ERR_SIGNAL);
    assert_non_null(strstr(error, "interrupted"));
    assert_null(output);
    assert_int_equal(output_len, 0U);

    salt_cli_test_set_interrupted(false);
    (void)fclose(in_stream);
}

void test_salt_cli_helper_invalid_arguments(void **state) {
    FILE *in_stream = stream_from_text(NULL);
    char *output = NULL;
    size_t output_len = 0U;
    char key_b64[64];
    char key_id[64];
    char error[TEST_SMALL_BUFFER_SIZE] = {0};
    bool has_key_id = false;

    (void)state;

    assert_int_equal(
        salt_cli_test_read_stream_limited(in_stream, 16U, NULL, NULL, error, sizeof(error)),
        SALT_ERR_INPUT);
    assert_non_null(strstr(error, "invalid stream arguments"));

    /* Annex K memset_s is unavailable on glibc; sizes bounded by the fixed buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(error, 0, sizeof(error));
    assert_int_equal(salt_cli_test_read_stream_limited(in_stream, SIZE_MAX, &output, &output_len,
                                                       error, sizeof(error)),
                     SALT_ERR_INTERNAL);
    assert_non_null(strstr(error, "input size overflow"));
    assert_null(output);
    assert_int_equal(output_len, 0U);

    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    /* Annex K memset_s is unavailable on glibc; sizes bounded by the fixed buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(error, 0, sizeof(error));
    assert_int_equal(salt_cli_test_parse_key_json_object(NULL, key_b64, sizeof(key_b64), key_id,
                                                         sizeof(key_id), &has_key_id, error,
                                                         sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "invalid JSON key parser arguments"));

    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    /* Annex K memset_s is unavailable on glibc; sizes bounded by the fixed buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(error, 0, sizeof(error));
    assert_int_equal(salt_cli_test_parse_key_input(NULL, 0, key_b64, sizeof(key_b64), key_id,
                                                   sizeof(key_id), &has_key_id, error,
                                                   sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "invalid key input arguments"));

    (void)fclose(in_stream);
}

void test_salt_cli_helper_read_error(void **state) {
    FILE *in_stream = stream_from_text(NULL);
    char *output = NULL;
    size_t output_len = 0U;
    char error[TEST_SMALL_BUFFER_SIZE] = {0};

    (void)state;

    cli_stream_fread_bytes = 0U;
    cli_stream_ferror_code = 1;
    cli_stream_fgetc_code = EOF;
    salt_cli_set_stream_test_hooks(cli_stream_fread, cli_stream_ferror, cli_stream_fgetc,
                                   cli_stream_ungetc);

    assert_int_equal(salt_cli_test_read_stream_limited(in_stream, 16U, &output, &output_len, error,
                                                       sizeof(error)),
                     SALT_ERR_IO);
    assert_non_null(strstr(error, "failed to read from stdin"));
    assert_null(output);

    (void)fclose(in_stream);
}

void test_salt_cli_helper_small_key_buffer(void **state) {
    char key_b64[4];
    char key_id[8];
    char error[TEST_SMALL_BUFFER_SIZE] = {0};
    bool has_key_id = false;

    (void)state;

    assert_int_equal(salt_cli_test_parse_key_input("base64-key", 1, key_b64, sizeof(key_b64),
                                                   key_id, sizeof(key_id), &has_key_id, error,
                                                   sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "buffer too small"));
}

void test_salt_cli_helper_optional_error_buffer_paths(void **state) {
    FILE *in_stream = stream_from_text("hello");
    FILE *in_stream_ferror = stream_from_text("hello");
    FILE *in_stream_too_large = stream_from_text("hello");
    char *output = NULL;
    size_t output_len = 0U;
    salt_cli_fread_fn fread_hook = cli_stream_fread;
    salt_cli_ferror_fn ferror_hook = cli_stream_ferror;
    salt_cli_fgetc_fn fgetc_hook = cli_stream_fgetc;
    salt_cli_ungetc_fn ungetc_hook = cli_stream_ungetc;

    (void)state;

    salt_cli_test_set_interrupted(true);
    assert_int_equal(
        salt_cli_test_read_stream_limited(in_stream, 16U, &output, &output_len, NULL, 0U),
        SALT_ERR_SIGNAL);
    assert_null(output);
    salt_cli_test_set_interrupted(false);

    assert_int_equal(salt_cli_test_read_stream_limited(NULL, 16U, &output, &output_len, NULL, 0U),
                     SALT_ERR_INPUT);

    salt_cli_set_test_hooks(cli_failing_alloc, NULL, NULL);
    assert_int_equal(
        salt_cli_test_read_stream_limited(in_stream, 16U, &output, &output_len, NULL, 0U),
        SALT_ERR_INTERNAL);
    assert_null(output);
    salt_cli_set_test_hooks(NULL, NULL, NULL);

    cli_stream_fread_bytes = 0U;
    cli_stream_ferror_code = 1;
    cli_stream_fgetc_code = EOF;
    salt_cli_set_stream_test_hooks(fread_hook, ferror_hook, fgetc_hook, ungetc_hook);
    assert_int_equal(
        salt_cli_test_read_stream_limited(in_stream_ferror, 16U, &output, &output_len, NULL, 0U),
        SALT_ERR_IO);
    assert_null(output);

    cli_stream_fread_bytes = 8U;
    cli_stream_ferror_code = 0;
    cli_stream_fgetc_code = 'X';
    salt_cli_set_stream_test_hooks(fread_hook, ferror_hook, fgetc_hook, ungetc_hook);
    assert_int_equal(
        salt_cli_test_read_stream_limited(in_stream_too_large, 8U, &output, &output_len, NULL, 0U),
        SALT_ERR_INPUT);
    assert_null(output);

    (void)fclose(in_stream);
    (void)fclose(in_stream_ferror);
    (void)fclose(in_stream_too_large);
}

void test_salt_cli_helper_probe_ferror_after_full_read(void **state) {
    FILE *in_stream = stream_from_text(NULL);
    char *output = NULL;
    size_t output_len = 0U;
    char error[TEST_SMALL_BUFFER_SIZE] = {0};

    (void)state;

    g_cli_probe_ferror_calls = 0U;
    cli_stream_fread_bytes = 8U;
    cli_stream_fgetc_code = EOF;
    salt_cli_set_stream_test_hooks(cli_stream_fread, cli_stream_ferror_after_probe,
                                   cli_stream_fgetc, cli_stream_ungetc);

    assert_int_equal(salt_cli_test_read_stream_limited(in_stream, 8U, &output, &output_len, error,
                                                       sizeof(error)),
                     SALT_ERR_IO);
    assert_non_null(strstr(error, "failed to read from stdin"));
    assert_null(output);
    assert_int_equal(output_len, 0U);

    (void)fclose(in_stream);
}

void test_salt_cli_helper_probe_ungetc_failure(void **state) {
    FILE *in_stream = stream_from_text(NULL);
    char *output = NULL;
    size_t output_len = 0U;
    char error[TEST_SMALL_BUFFER_SIZE] = {0};

    (void)state;

    cli_stream_fread_bytes = 8U;
    cli_stream_ferror_code = 0;
    cli_stream_fgetc_code = 'X';
    salt_cli_set_stream_test_hooks(cli_stream_fread, cli_stream_ferror, cli_stream_fgetc,
                                   cli_stream_ungetc_fail);

    assert_int_equal(salt_cli_test_read_stream_limited(in_stream, 8U, &output, &output_len, error,
                                                       sizeof(error)),
                     SALT_ERR_IO);
    assert_non_null(strstr(error, "failed to restore stdin after size probe"));
    assert_null(output);
    assert_int_equal(output_len, 0U);

    (void)fclose(in_stream);
}

void test_salt_cli_helper_state_cleanup_idempotent(void **state) {
    struct salt_cli_state cli_state;

    (void)state;

    salt_cli_test_state_init(&cli_state);

    cli_state.output = (char *)malloc(32U);
    assert_non_null(cli_state.output);
    cli_state.output_cap = 32U;
    (void)sodium_mlock(cli_state.output, cli_state.output_cap);

    cli_state.trimmed_key_input = (char *)malloc(32U);
    assert_non_null(cli_state.trimmed_key_input);
    cli_state.trimmed_key_input_cap = 32U;
    (void)sodium_mlock(cli_state.trimmed_key_input, cli_state.trimmed_key_input_cap);

    salt_cli_test_state_cleanup(&cli_state);
    assert_null(cli_state.output);
    assert_int_equal(cli_state.output_cap, 0U);
    assert_null(cli_state.trimmed_key_input);
    assert_int_equal(cli_state.trimmed_key_input_cap, 0U);
    assert_false(cli_state.fixed_buffers_locked);

    salt_cli_test_state_cleanup(&cli_state);
    assert_null(cli_state.output);
    assert_int_equal(cli_state.output_cap, 0U);
    assert_null(cli_state.trimmed_key_input);
    assert_int_equal(cli_state.trimmed_key_input_cap, 0U);
    assert_false(cli_state.fixed_buffers_locked);
}

void test_salt_cli_helper_non_null_hook_branches(void **state) {
    salt_free_fn free_hook = cli_passthrough_free;
    salt_cli_fread_fn fread_hook = cli_stream_fread;
    salt_cli_ferror_fn ferror_hook = cli_stream_ferror;
    salt_cli_fgetc_fn fgetc_hook = cli_stream_fgetc;
    salt_cli_ungetc_fn ungetc_hook = cli_stream_ungetc;

    (void)state;

    salt_cli_set_test_hooks(NULL, free_hook, NULL);
    salt_cli_set_output_len_test_hook(NULL);
    salt_cli_set_stream_test_hooks(fread_hook, ferror_hook, fgetc_hook, ungetc_hook);
    salt_cli_set_stream_test_hooks(NULL, NULL, NULL, NULL);
    salt_cli_reset_stream_test_hooks();
}

void test_salt_cli_helper_state_null_safety(void **state) {
    FILE *test_stream = stream_from_text("test");

    (void)state;

    /* Test defensive NULL checks in state_init and state_cleanup */
    salt_cli_test_state_init(NULL);
    salt_cli_test_state_cleanup(NULL);

    /* Ensure default stream hooks are exercised by resetting and using them */
    salt_cli_reset_stream_test_hooks();

    /* Exercise read through default hooks to cover default fread/ferror paths */
    char *output = NULL;
    size_t output_len = 0U;
    assert_int_equal(
        salt_cli_test_read_stream_limited(test_stream, 64U, &output, &output_len, NULL, 0U),
        SALT_OK);
    assert_non_null(output);
    free(output);

    (void)fclose(test_stream);
}

int run_cli_helper_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_parse_key_json_escaped_sequences,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_parse_key_json_unknown_escape_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_helper_parse_key_json_trailing_backslash_rejected, cli_test_setup,
            cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_parse_key_json_control_byte_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_parse_key_json_escape_buffer_too_small,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_parse_key_json_fuzz_regression,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_parse_key_input_leading_whitespace,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_reset_stream_hooks, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_interrupted_read, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_invalid_arguments, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_read_error, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_small_key_buffer, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_optional_error_buffer_paths,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_probe_ferror_after_full_read,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_probe_ungetc_failure, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_state_cleanup_idempotent,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_non_null_hook_branches, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_helper_state_null_safety, cli_test_setup,
                                        cli_test_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
