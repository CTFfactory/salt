/*
 * SPDX-License-Identifier: Unlicense
 *
 * Core sealed-box encryption helpers.
 *
 * Provides base64-first wrappers around libsodium sealed boxes with strict
 * input validation, deterministic error reporting, and explicit buffer wiping.
 */

#include "salt_test_internal.h"

#include <sodium.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static _Thread_local salt_sodium_init_fn salt_sodium_init_impl = sodium_init;
static _Thread_local salt_alloc_fn salt_alloc_impl = malloc;
static _Thread_local salt_free_fn salt_free_impl = free;
static _Thread_local salt_crypto_box_seal_fn salt_crypto_box_seal_impl = crypto_box_seal;
static _Thread_local salt_bin2base64_fn salt_bin2base64_impl = sodium_bin2base64;
static const size_t SALT_STACK_WIPE_BYTES = 4096U;

/* Test hooks are thread-local so parallel callers do not share override state. */

static void set_error(char *error, size_t error_size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

static void set_error(char *error, size_t error_size, const char *fmt, ...) {
    va_list args;

    if (error == NULL || error_size == 0U) {
        return;
    }

    va_start(args, fmt);
    /* Annex K vsnprintf_s is unavailable on glibc; va_list is initialized by va_start above. */
    /* NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    /* NOLINTBEGIN(clang-analyzer-valist.Uninitialized) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    (void)vsnprintf(error, error_size, fmt, args);
#pragma GCC diagnostic pop
    /* NOLINTEND(clang-analyzer-valist.Uninitialized) */
    /* NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    va_end(args);
}

static size_t salt_bounded_strlen(const char *value, size_t max_len) {
    const char *nul;

    if (value == NULL) {
        return 0U;
    }

    nul = (const char *)memchr(value, '\0', max_len);
    return (nul != NULL) ? (size_t)(nul - value) : max_len;
}

static int decode_public_key(const char *public_key_b64, unsigned char *public_key) {
    static const int variants[] = {sodium_base64_VARIANT_ORIGINAL,
                                   sodium_base64_VARIANT_ORIGINAL_NO_PADDING};
    const size_t max_public_key_b64_len =
        sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES, sodium_base64_VARIANT_ORIGINAL) - 1U;
    size_t decoded_len = 0U;
    size_t public_key_b64_len;

    if (public_key_b64 == NULL || public_key == NULL) { /* GCOVR_EXCL_BR_LINE */
        return SALT_ERR_INPUT;
    }

    public_key_b64_len = salt_bounded_strlen(public_key_b64, max_public_key_b64_len + 1U);
    if (public_key_b64_len == 0U || public_key_b64_len > max_public_key_b64_len) {
        return SALT_ERR_INPUT;
    }

    /* Accept padded and unpadded base64 forms used by common APIs/tools. */
    for (size_t i = 0U; i < (sizeof(variants) / sizeof(variants[0])); ++i) {
        const char *end = NULL;

        if (sodium_base642bin(public_key, crypto_box_PUBLICKEYBYTES, public_key_b64,
                              public_key_b64_len, NULL, &decoded_len, &end, variants[i]) == 0 &&
            decoded_len == crypto_box_PUBLICKEYBYTES && /* GCOVR_EXCL_BR_LINE */
            end != NULL &&                              /* GCOVR_EXCL_BR_LINE */
            *end == '\0') {                             /* GCOVR_EXCL_BR_LINE */
            return SALT_OK;
        }
    }

    return SALT_ERR_INPUT;
}

size_t salt_required_base64_output_len(size_t message_len) {
    _Static_assert(SALT_MAX_MESSAGE_LEN <= (SIZE_MAX - crypto_box_SEALBYTES),
                   "SALT_MAX_MESSAGE_LEN must not overflow sealed-box size arithmetic");

    if (message_len > (SIZE_MAX - crypto_box_SEALBYTES)) { /* GCOVR_EXCL_BR_LINE */
        return 0U;
    }

    return sodium_base64_ENCODED_LEN(message_len + crypto_box_SEALBYTES,
                                     sodium_base64_VARIANT_ORIGINAL);
}

#ifdef SALT_NO_MAIN
void salt_set_test_hooks(salt_sodium_init_fn sodium_init_hook, salt_alloc_fn alloc_hook,
                         salt_free_fn free_hook, salt_crypto_box_seal_fn crypto_box_seal_hook,
                         salt_bin2base64_fn bin2base64_hook) {
    salt_sodium_init_impl = (sodium_init_hook != NULL) ? sodium_init_hook : sodium_init;
    salt_alloc_impl = (alloc_hook != NULL) ? alloc_hook : malloc;
    salt_free_impl = (free_hook != NULL) ? free_hook : free;
    salt_crypto_box_seal_impl =
        (crypto_box_seal_hook != NULL) ? crypto_box_seal_hook : crypto_box_seal;
    salt_bin2base64_impl = (bin2base64_hook != NULL) ? bin2base64_hook : sodium_bin2base64;
}

void salt_reset_test_hooks(void) {
    salt_set_test_hooks(NULL, NULL, NULL, NULL, NULL);
}
#endif

int salt_seal_base64(const unsigned char *message, size_t message_len, const char *public_key_b64,
                     char *output, size_t output_size, size_t *output_written, char *error,
                     size_t error_size) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char *ciphertext = NULL;
    size_t ciphertext_len = 0U;
    const char *encoded = NULL;
    int status = SALT_ERR_INTERNAL;

    if (message == NULL || public_key_b64 == NULL || output == NULL) {
        set_error(error, error_size, "invalid null argument");
        return SALT_ERR_INPUT;
    }

    if (message_len == 0U || message_len > SALT_MAX_MESSAGE_LEN) {
        set_error(error, error_size, "plaintext length must be between 1 and %zu bytes",
                  (size_t)SALT_MAX_MESSAGE_LEN);
        return SALT_ERR_INPUT;
    }

    if (public_key_b64[0] == '\0') {
        set_error(error, error_size, "public key must not be empty");
        return SALT_ERR_INPUT;
    }

    /* libsodium must be initialized once before any crypto primitive or helper. */
    if (salt_sodium_init_impl() < 0) {
        set_error(error, error_size, "failed to initialize libsodium");
        return SALT_ERR_CRYPTO;
    }

    /*
     * Best-effort lock of the decoded-public-key scratch after initialization.
     * mlock can fail under a low RLIMIT_MEMLOCK; sodium_munlock still wipes the
     * region during cleanup.
     */
    (void)sodium_mlock(public_key, sizeof(public_key));

    if (decode_public_key(public_key_b64, public_key) != SALT_OK) {
        set_error(error, error_size, "invalid public key encoding or length");
        status = SALT_ERR_INPUT;
        goto cleanup;
    }

    ciphertext_len = message_len + crypto_box_SEALBYTES;
    {
        size_t required = salt_required_base64_output_len(message_len);
        if (required == 0U || output_size < required) {
            set_error(error, error_size, "output buffer too small");
            status = SALT_ERR_INPUT;
            goto cleanup;
        }
    }

    ciphertext = (unsigned char *)salt_alloc_impl(ciphertext_len);
    if (ciphertext == NULL) {
        set_error(error, error_size, "failed to allocate ciphertext buffer");
        status = SALT_ERR_INTERNAL;
        goto cleanup;
    }
    (void)sodium_mlock(ciphertext, ciphertext_len);

    if (salt_crypto_box_seal_impl(ciphertext, message, (unsigned long long)message_len,
                                  public_key) != 0) {
        set_error(error, error_size, "failed to encrypt sealed box");
        status = SALT_ERR_CRYPTO;
        goto cleanup;
    }

    encoded = salt_bin2base64_impl(output, output_size, ciphertext, ciphertext_len,
                                   sodium_base64_VARIANT_ORIGINAL);
    if (encoded == NULL) {
        set_error(error, error_size, "failed to encode ciphertext");
        status = SALT_ERR_INTERNAL;
        goto cleanup;
    }

    if (output_written != NULL) { /* GCOVR_EXCL_BR_LINE */
        *output_written = strlen(encoded);
    }
    status = SALT_OK;

cleanup:
    /*
     * Scrub key/ciphertext material on every exit path (success and
     * failure). sodium_munlock zeroes the region before unlocking, so a
     * separate sodium_memzero on the same range would be redundant.
     */
    (void)sodium_munlock(public_key, sizeof(public_key));
    if (ciphertext != NULL) {
        (void)sodium_munlock(ciphertext, ciphertext_len);
        salt_free_impl(ciphertext);
    }
    /*
     * Wipe stack space used by callees (decode_public_key, vsnprintf,
     * crypto_box_seal, sodium_bin2base64) that may have spilled fragments
     * of the public key, ephemeral keypair, or ciphertext into stack
     * frames now popped. 4 KB comfortably covers the deepest frame on the
     * platforms libsodium supports.
     */
    sodium_stackzero(SALT_STACK_WIPE_BYTES);
    return status;
}
