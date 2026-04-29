/*
 * SPDX-License-Identifier: Unlicense
 *
 * CMocka coverage for sealed-box encryption success and failure paths.
 */

#include "../src/salt_test_internal.h"
#include "test_suites.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cmocka.h>
#include <sodium.h>

static int failing_sodium_init(void) {
    return -1;
}

static unsigned char *zero_check_alloc_ptr = NULL;
static size_t zero_check_alloc_size = 0U;
static bool zero_check_observed = false;

static void *failing_alloc(size_t size) {
    (void)size;
    return NULL;
}

static void *zero_check_alloc(size_t size) {
    unsigned char *ptr = (unsigned char *)malloc(size);

    if (ptr != NULL) {
        /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memset(ptr, 0xA5, size);
        zero_check_alloc_ptr = ptr;
        zero_check_alloc_size = size;
    }
    return ptr;
}

static void zero_check_free(void *ptr) {
    unsigned char *bytes = (unsigned char *)ptr;
    size_t i;

    if (ptr == zero_check_alloc_ptr) {
        for (i = 0U; i < zero_check_alloc_size; ++i) {
            assert_int_equal(bytes[i], 0);
        }
        zero_check_observed = true;
    }

    free(ptr);
}

/* NOLINTNEXTLINE(readability-non-const-parameter) -- signature must match libsodium ABI. */
static int failing_crypto_box_seal(unsigned char *ciphertext, const unsigned char *message,
                                   unsigned long long message_len,
                                   const unsigned char *public_key) {
    (void)ciphertext;
    (void)message;
    (void)message_len;
    (void)public_key;
    return -1;
}

/* NOLINTNEXTLINE(readability-non-const-parameter) -- signature must match libsodium ABI. */
static char *failing_bin2base64(char *b64, size_t b64_maxlen, const unsigned char *bin,
                                size_t bin_len, int variant) {
    (void)b64;
    (void)b64_maxlen;
    (void)bin;
    (void)bin_len;
    (void)variant;
    return NULL;
}

int salt_test_setup(void **state) {
    (void)state;
    zero_check_alloc_ptr = NULL;
    zero_check_alloc_size = 0U;
    zero_check_observed = false;
    salt_reset_test_hooks();
    return 0;
}

int salt_test_teardown(void **state) {
    (void)state;
    zero_check_alloc_ptr = NULL;
    zero_check_alloc_size = 0U;
    zero_check_observed = false;
    salt_reset_test_hooks();
    return 0;
}

static void encode_public_key(const unsigned char *public_key, char *buffer, size_t size) {
    assert_non_null(sodium_bin2base64(buffer, size, public_key, crypto_box_PUBLICKEYBYTES,
                                      sodium_base64_VARIANT_ORIGINAL));
}

static void generate_test_keypair(unsigned char *public_key, unsigned char *secret_key,
                                  char *key_b64, size_t key_b64_size) {
    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, key_b64_size);
}

static void assert_round_trip_payload(const unsigned char *message, size_t message_len) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    size_t output_size = salt_required_base64_output_len(message_len);
    char *output = (char *)malloc(output_size);
    unsigned char *ciphertext = (unsigned char *)malloc(message_len + crypto_box_SEALBYTES);
    unsigned char *decrypted = (unsigned char *)malloc(message_len);
    char error[256] = {0};
    size_t written = 0U;
    size_t ciphertext_len = 0U;
    const char *end = NULL;

    assert_non_null(output);
    assert_non_null(ciphertext);
    assert_non_null(decrypted);

    generate_test_keypair(public_key, secret_key, key_b64, sizeof(key_b64));

    assert_int_equal(salt_seal_base64(message, message_len, key_b64, output, output_size, &written,
                                      error, sizeof(error)),
                     SALT_OK);
    assert_true(written > 0U);

    assert_int_equal(sodium_base642bin(ciphertext, message_len + crypto_box_SEALBYTES, output,
                                       strlen(output), NULL, &ciphertext_len, &end,
                                       sodium_base64_VARIANT_ORIGINAL),
                     0);
    assert_non_null(end);
    assert_int_equal(*end, '\0');
    assert_int_equal(ciphertext_len, message_len + crypto_box_SEALBYTES);

    assert_int_equal(
        crypto_box_seal_open(decrypted, ciphertext, ciphertext_len, public_key, secret_key), 0);
    assert_memory_equal(decrypted, message, message_len);

    sodium_memzero(secret_key, sizeof(secret_key));
    sodium_memzero(decrypted, message_len);
    sodium_memzero(ciphertext, message_len + crypto_box_SEALBYTES);
    free(decrypted);
    free(ciphertext);
    free(output);
}

void test_salt_seal_base64_success(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    const unsigned char message[] = "hello";
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};
    size_t written = 0U;

    (void)state;

    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, sizeof(key_b64));

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, key_b64, output,
                                      sizeof(output), &written, error, sizeof(error)),
                     SALT_OK);
    assert_true(written > 0U);
    assert_true(output[0] != '\0');

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_salt_seal_base64_round_trip_success(void **state) {
    const unsigned char message[] = "round-trip secret";

    (void)state;

    assert_round_trip_payload(message, sizeof(message) - 1U);
}

void test_salt_seal_base64_round_trip_binary_payload(void **state) {
    unsigned char message[1024];
    size_t i;

    (void)state;

    for (i = 0U; i < sizeof(message); ++i) {
        message[i] = (unsigned char)((i * 37U) & 0xFFU);
    }

    assert_round_trip_payload(message, sizeof(message));
}

void test_salt_seal_base64_round_trip_large_payload(void **state) {
    size_t message_len = (size_t)(64U * 1024U);
    unsigned char *message;
    size_t i;

    (void)state;

    message = (unsigned char *)malloc(message_len);
    assert_non_null(message);

    for (i = 0U; i < message_len; ++i) {
        message[i] = (unsigned char)(i & 0xFFU);
    }

    assert_round_trip_payload(message, message_len);
    sodium_memzero(message, message_len);
    free(message);
}

void test_salt_seal_base64_round_trip_max_payload(void **state) {
    size_t message_len = SALT_MAX_MESSAGE_LEN;
    unsigned char *message;
    size_t i;

    (void)state;

    message = (unsigned char *)malloc(message_len);
    assert_non_null(message);

    for (i = 0U; i < message_len; ++i) {
        message[i] = (unsigned char)((i * 13U) & 0xFFU);
    }

    assert_round_trip_payload(message, message_len);
    sodium_memzero(message, message_len);
    free(message);
}

void test_salt_seal_base64_ciphertext_zeroed_before_free(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    const unsigned char message[] = "verify zeroing";
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};
    size_t written = 0U;

    (void)state;

    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, sizeof(key_b64));
    salt_set_test_hooks(NULL, zero_check_alloc, zero_check_free, NULL, NULL);

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, key_b64, output,
                                      sizeof(output), &written, error, sizeof(error)),
                     SALT_OK);
    assert_true(written > 0U);
    assert_true(zero_check_observed);

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_salt_seal_base64_invalid_public_key(void **state) {
    const unsigned char message[] = "hello";
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, "not-valid-base64", output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "invalid public key"));
}

void test_salt_seal_base64_oversized_public_key_rejected(void **state) {
    const unsigned char message[] = "hello";
    char public_key_b64[128];
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(public_key_b64, 'A', sizeof(public_key_b64) - 1U);
    public_key_b64[sizeof(public_key_b64) - 1U] = '\0';

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, public_key_b64, output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "invalid public key"));
}

void test_salt_seal_base64_empty_message_rejected(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    char output[sodium_base64_ENCODED_LEN(crypto_box_SEALBYTES, sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, sizeof(key_b64));

    assert_int_equal(salt_seal_base64((const unsigned char *)"", 0U, key_b64, output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "plaintext length"));

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_salt_seal_base64_null_arguments_rejected(void **state) {
    const unsigned char message[] = "hello";
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    assert_int_equal(salt_seal_base64(NULL, sizeof(message) - 1U, "anything", output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "invalid null argument"));

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, NULL, output, sizeof(output),
                                      NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "invalid null argument"));

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, "anything", NULL,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "invalid null argument"));
}

void test_salt_seal_base64_empty_public_key_rejected(void **state) {
    const unsigned char message[] = "hello";
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, "", output, sizeof(output),
                                      NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "public key must not be empty"));
}

void test_salt_seal_base64_output_buffer_too_small(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    const unsigned char message[] = "hello";
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    char output[4] = {0};
    char error[256] = {0};

    (void)state;

    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, sizeof(key_b64));

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, key_b64, output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "output buffer too small"));

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_salt_seal_base64_too_large_message_rejected(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    unsigned char *message = NULL;
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    char output[64] = {0};
    char error[256] = {0};

    (void)state;

    message = (unsigned char *)malloc(SALT_MAX_MESSAGE_LEN + 1U);
    assert_non_null(message);
    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    memset(message, 'A', SALT_MAX_MESSAGE_LEN + 1U);

    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, sizeof(key_b64));

    assert_int_equal(salt_seal_base64(message, SALT_MAX_MESSAGE_LEN + 1U, key_b64, output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INPUT);
    assert_non_null(strstr(error, "plaintext length"));

    sodium_memzero(secret_key, sizeof(secret_key));
    sodium_memzero(message, SALT_MAX_MESSAGE_LEN + 1U);
    free(message);
}

void test_salt_seal_base64_sodium_init_failure(void **state) {
    const unsigned char message[] = "hello";
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    salt_set_test_hooks(failing_sodium_init, NULL, NULL, NULL, NULL);

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, "anything", output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_CRYPTO);
    assert_non_null(strstr(error, "failed to initialize libsodium"));
}

void test_salt_seal_base64_malloc_failure(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    const unsigned char message[] = "hello";
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, sizeof(key_b64));
    salt_set_test_hooks(NULL, failing_alloc, NULL, NULL, NULL);

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, key_b64, output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INTERNAL);
    assert_non_null(strstr(error, "failed to allocate ciphertext buffer"));

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_salt_seal_base64_encryption_failure(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    const unsigned char message[] = "hello";
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, sizeof(key_b64));
    salt_set_test_hooks(NULL, NULL, NULL, failing_crypto_box_seal, NULL);

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, key_b64, output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_CRYPTO);
    assert_non_null(strstr(error, "failed to encrypt sealed box"));

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_salt_seal_base64_encoding_failure(void **state) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    const unsigned char message[] = "hello";
    char key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                           sodium_base64_VARIANT_ORIGINAL)];
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];
    char error[256] = {0};

    (void)state;

    assert_int_equal(crypto_box_keypair(public_key, secret_key), 0);
    encode_public_key(public_key, key_b64, sizeof(key_b64));
    salt_set_test_hooks(NULL, NULL, NULL, NULL, failing_bin2base64);

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, key_b64, output,
                                      sizeof(output), NULL, error, sizeof(error)),
                     SALT_ERR_INTERNAL);
    assert_non_null(strstr(error, "failed to encode ciphertext"));

    sodium_memzero(secret_key, sizeof(secret_key));
}

void test_salt_seal_base64_null_error_buffer_guard(void **state) {
    const unsigned char message[] = "hello";
    char output[sodium_base64_ENCODED_LEN(sizeof(message) - 1U + crypto_box_SEALBYTES,
                                          sodium_base64_VARIANT_ORIGINAL)];

    (void)state;

    salt_set_test_hooks(failing_sodium_init, NULL, NULL, NULL, NULL);

    assert_int_equal(salt_seal_base64(message, sizeof(message) - 1U, "anything", output,
                                      sizeof(output), NULL, NULL, 0U),
                     SALT_ERR_CRYPTO);
}

void test_salt_seal_base64_binary_payload(void **state) {
    unsigned char message[256];
    size_t i;

    (void)state;

    for (i = 0U; i < sizeof(message); ++i) {
        message[i] = (unsigned char)i;
    }

    assert_round_trip_payload(message, sizeof(message));
}

void test_salt_seal_base64_unicode_payload(void **state) {
    const unsigned char message[] = "Hello 世界 🔐";

    (void)state;

    assert_round_trip_payload(message, sizeof(message) - 1U);
}

int run_salt_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_success, salt_test_setup,
                                        salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_round_trip_success, salt_test_setup,
                                        salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_round_trip_binary_payload,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_round_trip_large_payload,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_round_trip_max_payload,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_ciphertext_zeroed_before_free,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_invalid_public_key, salt_test_setup,
                                        salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_oversized_public_key_rejected,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_empty_message_rejected,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_null_arguments_rejected,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_empty_public_key_rejected,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_output_buffer_too_small,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_too_large_message_rejected,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_sodium_init_failure, salt_test_setup,
                                        salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_malloc_failure, salt_test_setup,
                                        salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_encryption_failure, salt_test_setup,
                                        salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_encoding_failure, salt_test_setup,
                                        salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_null_error_buffer_guard,
                                        salt_test_setup, salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_binary_payload, salt_test_setup,
                                        salt_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_seal_base64_unicode_payload, salt_test_setup,
                                        salt_test_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
