/*
 * SPDX-License-Identifier: Unlicense
 *
 * Phase 2 deterministic fuzz-invariant regressions.
 *
 * This file converts high-value fuzz harness invariants into deterministic
 * cmocka tests that run in `make check` and CI smoke gates:
 * - Roundtrip invariants (encrypt → decrypt = plaintext)
 * - Boundary conditions (max plaintext size, empty input, exact limits)
 * - Output encoding correctness (base64 validity, length calculations)
 * - Malformed input rejection (truncated keys, invalid base64, corrupted data)
 */

/* POSIX feature-test macro must be named exactly. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _POSIX_C_SOURCE 200809L

#include "test_cli_shared.h"
#include "test_suites.h"

#include <sodium.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

/* ============================================================================
 * Roundtrip Invariant Tests
 * ============================================================================ */

void test_roundtrip_single_byte_plaintext(void **state) {
    unsigned char plaintext[1] = {'A'};
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];
    unsigned char ciphertext[1024];
    size_t ciphertext_len = 0U;
    unsigned char decrypted[1];

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    assert_non_null(sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                                      sizeof(public_key), sodium_base64_VARIANT_ORIGINAL));

    /* Encrypt */
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext), public_key_b64, encrypted,
                                      sizeof(encrypted), &encrypted_len, error, sizeof(error)),
                     SALT_OK);
    assert_true(encrypted_len > 0U);

    /* Decode */
    assert_int_equal(sodium_base642bin(ciphertext, sizeof(ciphertext), encrypted, encrypted_len,
                                       NULL, &ciphertext_len, NULL, sodium_base64_VARIANT_ORIGINAL),
                     0);
    assert_int_equal(ciphertext_len, sizeof(plaintext) + crypto_box_SEALBYTES);

    /* Decrypt */
    assert_int_equal(
        crypto_box_seal_open(decrypted, ciphertext, ciphertext_len, public_key, secret_key), 0);

    /* Verify roundtrip */
    assert_memory_equal(decrypted, plaintext, sizeof(plaintext));

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_roundtrip_empty_plaintext_rejected(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    assert_non_null(sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                                      sizeof(public_key), sodium_base64_VARIANT_ORIGINAL));

    /* Empty plaintext should be rejected */
    assert_int_equal(salt_seal_base64(NULL, 0U, public_key_b64, encrypted, sizeof(encrypted),
                                      &encrypted_len, error, sizeof(error)),
                     SALT_ERR_INPUT);

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_roundtrip_max_plaintext_size(void **state) {
    unsigned char *plaintext = NULL;
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    char *encrypted = NULL;
    size_t max_plaintext = SALT_GITHUB_SECRET_MAX_VALUE_LEN;
    size_t encrypted_len = 0U;
    size_t required_output = 0U;
    size_t encrypted_alloc = 0U;
    char error[256];
    unsigned char *ciphertext = NULL;
    size_t ciphertext_len = 0U;
    unsigned char *decrypted = NULL;

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    assert_non_null(sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                                      sizeof(public_key), sodium_base64_VARIANT_ORIGINAL));

    plaintext = malloc(max_plaintext);
    assert_non_null(plaintext);
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(plaintext, 'X', max_plaintext);

    required_output = salt_required_base64_output_len(max_plaintext);
    assert_true(required_output > 0U);
    encrypted_alloc = (required_output > 0U) ? required_output : 1U;

    encrypted = malloc(encrypted_alloc);
    assert_non_null(encrypted);

    /* Encrypt at max size */
    assert_int_equal(salt_seal_base64(plaintext, max_plaintext, public_key_b64, encrypted,
                                      required_output, &encrypted_len, error, sizeof(error)),
                     SALT_OK);
    assert_true(encrypted_len > 0U);

    ciphertext = malloc(max_plaintext + crypto_box_SEALBYTES);
    assert_non_null(ciphertext);

    /* Decode */
    assert_int_equal(sodium_base642bin(ciphertext, max_plaintext + crypto_box_SEALBYTES, encrypted,
                                       encrypted_len, NULL, &ciphertext_len, NULL,
                                       sodium_base64_VARIANT_ORIGINAL),
                     0);
    assert_int_equal(ciphertext_len, max_plaintext + crypto_box_SEALBYTES);

    decrypted = malloc(max_plaintext);
    assert_non_null(decrypted);

    /* Decrypt */
    assert_int_equal(
        crypto_box_seal_open(decrypted, ciphertext, ciphertext_len, public_key, secret_key), 0);

    /* Verify roundtrip */
    assert_memory_equal(decrypted, plaintext, max_plaintext);

    sodium_memzero(plaintext, max_plaintext);
    sodium_memzero(secret_key, sizeof(secret_key));
    free(plaintext);
    free(encrypted);
    free(ciphertext);
    free(decrypted);
}

/* ============================================================================
 * Boundary Condition Tests
 * ============================================================================ */

void test_boundary_plaintext_size_exactly_max(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key"};
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    /* Exactly SALT_GITHUB_SECRET_MAX_VALUE_LEN bytes */
    size_t max_size = SALT_GITHUB_SECRET_MAX_VALUE_LEN;
    char *large_input = malloc(max_size + 1U);

    (void)state;

    assert_non_null(large_input);
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(large_input, 'M', max_size);
    large_input[max_size] = '\0';

    FILE *in_stream = stream_from_text(large_input);

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_key = "base64-key";

    /* Should succeed at exactly the max size */
    assert_int_equal(salt_cli_run_with_streams(3, argv, in_stream, out_stream, err_stream),
                     SALT_OK);

    free(large_input);
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_boundary_plaintext_size_one_over_max_rejected(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key"};
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    /* One byte over SALT_GITHUB_SECRET_MAX_VALUE_LEN */
    size_t over_max = SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U;
    char *large_input = malloc(over_max + 1U);
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(large_input);
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(large_input, 'M', over_max);
    large_input[over_max] = '\0';

    FILE *in_stream = stream_from_text(large_input);

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_key = "base64-key";

    /* Should reject input that's too large */
    assert_int_equal(salt_cli_run_with_streams(3, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_INPUT);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_non_null(strstr(stderr_buffer, "exceeds maximum"));

    free(large_input);
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_boundary_key_length_exact_base64_size(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    unsigned char plaintext[] = "test";
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    assert_non_null(sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                                      sizeof(public_key), sodium_base64_VARIANT_ORIGINAL));

    /* Verify the key is exactly the expected base64 size with padding */
    assert_int_equal(
        strlen(public_key_b64),
        sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES, sodium_base64_VARIANT_ORIGINAL) - 1U);

    /* Should encrypt successfully with properly sized key */
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext) - 1U, public_key_b64, encrypted,
                                      sizeof(encrypted), &encrypted_len, error, sizeof(error)),
                     SALT_OK);

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_boundary_truncated_key_rejected(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    char truncated_key[32];
    unsigned char plaintext[] = "test";
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    assert_non_null(sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                                      sizeof(public_key), sodium_base64_VARIANT_ORIGINAL));

    /* Truncate the key to 30 characters (too short) */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)strncpy(truncated_key, public_key_b64, 30);
    truncated_key[30] = '\0';

    /* Should reject truncated key */
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext) - 1U, truncated_key, encrypted,
                                      sizeof(encrypted), &encrypted_len, error, sizeof(error)),
                     SALT_ERR_INPUT);

    sodium_memzero(secret_key, sizeof(secret_key));
}

/* ============================================================================
 * Output Encoding Tests
 * ============================================================================ */

void test_output_encoding_produces_valid_base64(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    unsigned char plaintext[] = "Hello, World!";
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];
    unsigned char decoded[1024];
    size_t decoded_len = 0U;
    size_t i;

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    assert_non_null(sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                                      sizeof(public_key), sodium_base64_VARIANT_ORIGINAL));

    /* Encrypt */
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext) - 1U, public_key_b64, encrypted,
                                      sizeof(encrypted), &encrypted_len, error, sizeof(error)),
                     SALT_OK);
    assert_true(encrypted_len > 0U);

    /* Verify all characters are valid base64 */
    for (i = 0U; i < encrypted_len; ++i) {
        char ch = encrypted[i];
        assert_true((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                    (ch >= '0' && ch <= '9') || ch == '+' || ch == '/' || ch == '=');
    }

    /* Verify it decodes successfully */
    assert_int_equal(sodium_base642bin(decoded, sizeof(decoded), encrypted, encrypted_len, NULL,
                                       &decoded_len, NULL, sodium_base64_VARIANT_ORIGINAL),
                     0);
    assert_true(decoded_len > 0U);

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_output_encoding_length_calculation_correct(void **state) {
    size_t plaintext_len = 100U;
    size_t required_output;
    size_t ciphertext_len;
    size_t expected_base64_len;

    (void)state;

    /* Calculate required output size */
    required_output = salt_required_base64_output_len(plaintext_len);
    assert_true(required_output > 0U);

    /* Verify it accounts for ciphertext overhead */
    ciphertext_len = plaintext_len + crypto_box_SEALBYTES;
    expected_base64_len = sodium_base64_ENCODED_LEN(ciphertext_len, sodium_base64_VARIANT_ORIGINAL);

    /* Required output should match or exceed base64 encoding size */
    assert_true(required_output >= expected_base64_len);
}

void test_output_encoding_insufficient_buffer_rejected(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    unsigned char plaintext[] = "Hello";
    char encrypted[16]; /* Too small for output */
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    assert_non_null(sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                                      sizeof(public_key), sodium_base64_VARIANT_ORIGINAL));

    /* Should reject insufficient output buffer */
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext) - 1U, public_key_b64, encrypted,
                                      sizeof(encrypted), &encrypted_len, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "output buffer"));

    sodium_memzero(secret_key, sizeof(secret_key));
}

/* ============================================================================
 * Malformed Input Rejection Tests
 * ============================================================================ */

void test_malformed_invalid_base64_in_key_rejected(void **state) {
    unsigned char plaintext[] = "test";
    char invalid_key[] = "not!valid@base64#chars";
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);

    /* Should reject key with invalid base64 characters */
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext) - 1U, invalid_key, encrypted,
                                      sizeof(encrypted), &encrypted_len, error, sizeof(error)),
                     SALT_ERR_INPUT);
}

void test_malformed_key_with_interior_whitespace_rejected(void **state) {
    unsigned char plaintext[] = "test";
    char whitespace_key[] = "AAAA BBBB CCCC";
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);

    /* Should reject key with interior whitespace */
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext) - 1U, whitespace_key, encrypted,
                                      sizeof(encrypted), &encrypted_len, error, sizeof(error)),
                     SALT_ERR_INPUT);
}

void test_malformed_base64_key_with_trailing_junk_rejected(void **state) {
    unsigned char plaintext[] = "test";
    char trailing_junk_key[] = "rQB7XCRJP1Z+jY4mN1pK5tWvE3Gd0aHfXkBSYr9hzVE=###";
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext) - 1U, trailing_junk_key,
                                      encrypted, sizeof(encrypted), &encrypted_len, error,
                                      sizeof(error)),
                     SALT_ERR_INPUT);
}

void test_malformed_null_plaintext_pointer_rejected(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    assert_non_null(sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                                      sizeof(public_key), sodium_base64_VARIANT_ORIGINAL));

    /* NULL plaintext with non-zero length should be rejected */
    assert_int_equal(salt_seal_base64(NULL, 10U, public_key_b64, encrypted, sizeof(encrypted),
                                      &encrypted_len, error, sizeof(error)),
                     SALT_ERR_INPUT);

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_malformed_null_key_pointer_rejected(void **state) {
    unsigned char plaintext[] = "test";
    char encrypted[1024];
    size_t encrypted_len = 0U;
    char error[256];

    (void)state;

    assert_true(sodium_init() >= 0);

    /* NULL key should be rejected */
    assert_int_equal(salt_seal_base64(plaintext, sizeof(plaintext) - 1U, NULL, encrypted,
                                      sizeof(encrypted), &encrypted_len, error, sizeof(error)),
                     SALT_ERR_INPUT);
}

/* ============================================================================
 * Suite Runner
 * ============================================================================ */

int run_fuzz_regression_tests(void) {
    const struct CMUnitTest tests[] = {
        /* Roundtrip invariants */
        cmocka_unit_test(test_roundtrip_single_byte_plaintext),
        cmocka_unit_test(test_roundtrip_empty_plaintext_rejected),
        cmocka_unit_test(test_roundtrip_max_plaintext_size),

        /* Boundary conditions */
        cmocka_unit_test_setup_teardown(test_boundary_plaintext_size_exactly_max, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_boundary_plaintext_size_one_over_max_rejected,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test(test_boundary_key_length_exact_base64_size),
        cmocka_unit_test(test_boundary_truncated_key_rejected),

        /* Output encoding */
        cmocka_unit_test(test_output_encoding_produces_valid_base64),
        cmocka_unit_test(test_output_encoding_length_calculation_correct),
        cmocka_unit_test(test_output_encoding_insufficient_buffer_rejected),

        /* Malformed input rejection */
        cmocka_unit_test(test_malformed_invalid_base64_in_key_rejected),
        cmocka_unit_test(test_malformed_key_with_interior_whitespace_rejected),
        cmocka_unit_test(test_malformed_base64_key_with_trailing_junk_rejected),
        cmocka_unit_test(test_malformed_null_plaintext_pointer_rejected),
        cmocka_unit_test(test_malformed_null_key_pointer_rejected),
    };

    return cmocka_run_group_tests_name("Fuzz regressions", tests, NULL, NULL);
}
