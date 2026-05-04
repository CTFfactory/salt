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

/*
 * Write formatted error message to optional error buffer.
 *
 * Wraps vsnprintf for consistent error reporting. The return value is intentionally
 * discarded because:
 * - vsnprintf returns the number of characters that would have been written (on success)
 *   or -1 (on encoding error or other issues).
 * - For error buffer population, we accept either outcome; a truncated error message
 *   is still useful diagnostics (better than no message).
 * - This is a best-effort reporting path; vsnprintf failures should not cause
 *   secondary errors or cause us to skip error reporting.
 *
 * The cast-to-void (MISRA C:2012 Rule 17.7) signals this is intentional and
 * documents why we don't branch on the result.
 */
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
    if (nul == NULL) { /* GCOVR_EXCL_BR_LINE */
        return max_len;
    }

    return (size_t)(nul - value);
}

static int salt_validate_seal_inputs(const unsigned char *message, size_t message_len,
                                     const char *public_key_b64, size_t public_key_b64_len,
                                     const char *output, char *error, size_t error_size) {
    if (message == NULL || public_key_b64 == NULL || output == NULL) {
        set_error(error, error_size, "invalid null argument");
        return SALT_ERR_INPUT;
    }

    if (message_len == 0U || message_len > SALT_MAX_MESSAGE_LEN) {
        set_error(error, error_size, "plaintext length must be between 1 and %zu bytes",
                  (size_t)SALT_MAX_MESSAGE_LEN);
        return SALT_ERR_INPUT;
    }

    if (public_key_b64_len == 0U) {
        set_error(error, error_size, "public key must not be empty");
        return SALT_ERR_INPUT;
    }

    return SALT_OK;
}

static int salt_validate_output_capacity(size_t message_len, size_t output_size, char *error,
                                         size_t error_size) {
    size_t required = salt_required_base64_output_len(message_len);

    /*
     * Check for overflow sentinel (SIZE_MAX) and also verify buffer size.
     * SIZE_MAX indicates the base64 output would overflow size_t.
     */
    if (required == SIZE_MAX || output_size < required) {
        set_error(error, error_size, "output buffer too small");
        return SALT_ERR_INPUT;
    }

    return SALT_OK;
}

/* GCOVR_EXCL_START */
static void salt_secure_release(unsigned char *buffer, size_t buffer_len, bool buffer_locked) {
    if (buffer == NULL) {
        return;
    }

    if (buffer_locked) {
        if (sodium_munlock(buffer, buffer_len) != 0) {
            sodium_memzero(buffer, buffer_len);
        }
    } else {
        sodium_memzero(buffer, buffer_len);
    }
}
/* GCOVR_EXCL_STOP */

static int decode_public_key(const char *public_key_b64, size_t public_key_b64_len,
                             unsigned char *public_key) {
    static const int variants[] = {sodium_base64_VARIANT_ORIGINAL,
                                   sodium_base64_VARIANT_ORIGINAL_NO_PADDING};
    const size_t max_public_key_b64_len =
        sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES, sodium_base64_VARIANT_ORIGINAL) - 1U;
    size_t decoded_len = 0U;

    if (public_key_b64 == NULL || public_key == NULL) { /* GCOVR_EXCL_BR_LINE */
        return SALT_ERR_INPUT;
    }

    if (public_key_b64_len == 0U || public_key_b64_len > max_public_key_b64_len) {
        return SALT_ERR_INPUT;
    }

    /* Accept padded and unpadded base64 forms used by common APIs/tools. */
    for (size_t i = 0U; i < (sizeof(variants) / sizeof(variants[0])); ++i) {
        const char *end = NULL;

        const char *b64_end = public_key_b64 + public_key_b64_len;
        if (sodium_base642bin(public_key, crypto_box_PUBLICKEYBYTES, public_key_b64,
                              public_key_b64_len, NULL, &decoded_len, &end, variants[i]) == 0 &&
            decoded_len == crypto_box_PUBLICKEYBYTES && /* GCOVR_EXCL_BR_LINE */
            end != NULL &&                              /* GCOVR_EXCL_BR_LINE */
            end == b64_end) {                           /* GCOVR_EXCL_BR_LINE */
            return SALT_OK;
        }
    }

    sodium_memzero(public_key, crypto_box_PUBLICKEYBYTES);
    return SALT_ERR_INPUT;
}

size_t salt_required_base64_output_len(size_t message_len) {
    _Static_assert(SALT_MAX_MESSAGE_LEN <= (SIZE_MAX - crypto_box_SEALBYTES),
                   "SALT_MAX_MESSAGE_LEN must not overflow sealed-box size arithmetic");

    /*
     * Check for overflow when adding sealed-box overhead.
     * If message_len + crypto_box_SEALBYTES > SIZE_MAX, return SIZE_MAX.
     */
    if (message_len > (SIZE_MAX - crypto_box_SEALBYTES)) { /* GCOVR_EXCL_BR_LINE */
        return SIZE_MAX;
    }

    size_t sealed_len = message_len + crypto_box_SEALBYTES;

    /*
     * Base64 encoding expands to 4/3 of input size, rounded up.
     * sodium_base64_ENCODED_LEN(n, variant) computes (((n) + 2U) / 3U * 4U + 1U)
     * for typical variants. Detect overflow in this expansion.
     *
     * Maximum encoded length for sealed_len is approximately (4/3 * sealed_len + 1).
     * Overflow occurs if: 4/3 * sealed_len > SIZE_MAX - 1
     * Rearranged: sealed_len > (SIZE_MAX - 1) * 3 / 4
     */
    if (sealed_len > (SIZE_MAX - 1U) / 4U * 3U) { /* GCOVR_EXCL_BR_LINE */
        return SIZE_MAX;
    }

    return sodium_base64_ENCODED_LEN(sealed_len, sodium_base64_VARIANT_ORIGINAL);
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

int salt_seal_base64_keylen(const unsigned char *message, size_t message_len,
                            const char *public_key_b64, size_t public_key_b64_len, char *output,
                            size_t output_size, size_t *output_written, char *error,
                            size_t error_size) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char *ciphertext = NULL;
    size_t ciphertext_len = 0U;
    const char *encoded = NULL;
    bool public_key_locked = false;
    bool ciphertext_locked = false;
    int status = SALT_ERR_INTERNAL;

    /*
     * Initialize output_written on every path before validation checks.
     * Contract: If output_written is non-NULL, we ALWAYS write to it before return.
     * This simplifies caller logic: check rc, then read *output_written without
     * worrying about uninitialized reads on error paths.
     */
    if (output_written != NULL) {
        *output_written = 0U;
    }

    status = salt_validate_seal_inputs(message, message_len, public_key_b64, public_key_b64_len,
                                       output, error, error_size);
    if (status != SALT_OK) {
        return status;
    }

    /* libsodium must be initialized once before any crypto primitive or helper. */
    if (salt_sodium_init_impl() < 0) {
        set_error(error, error_size, "failed to initialize libsodium");
        return SALT_ERR_CRYPTO;
    }

    /*
     * Best-effort lock of the decoded-public-key scratch after initialization.
     *
     * POSIX mlock() can fail due to resource constraints (e.g., RLIMIT_MEMLOCK on Linux,
     * insufficient physical memory). libsodium's sodium_mlock() wraps mlock() and returns
     * an int status (0 on success, -1 on failure). We cast the result to void to indicate
     * this is intentionally non-fatal:
     * - Failure typically means RLIMIT_MEMLOCK is too low (common in containers, sanitizer,
     *   and valgrind runs).
     * - Defensive check: sodium_munlock() still wipes the region during cleanup even if
     *   mlock failed, so the security property (zeroing on release) is preserved.
     * - The locked state is a best-effort hardening; plaintext exposure risk remains if
     *   mlock fails, but at least the data is eventually scrubbed.
     */
    public_key_locked = (sodium_mlock(public_key, sizeof(public_key)) == 0);

    if (decode_public_key(public_key_b64, public_key_b64_len, public_key) != SALT_OK) {
        set_error(error, error_size, "invalid public key encoding or length");
        status = SALT_ERR_INPUT;
        goto cleanup;
    }

    ciphertext_len = message_len + crypto_box_SEALBYTES;
    status = salt_validate_output_capacity(message_len, output_size, error, error_size);
    if (status != SALT_OK) {
        goto cleanup;
    }

    ciphertext = (unsigned char *)salt_alloc_impl(ciphertext_len);
    if (ciphertext == NULL) {
        set_error(error, error_size, "failed to allocate ciphertext buffer");
        status = SALT_ERR_INTERNAL;
        goto cleanup;
    }
    /*
     * Best-effort lock of the ciphertext buffer to minimize swap exposure.
     *
     * Although the ciphertext is already encrypted (not plaintext), it contains
     * the sealed-box overhead which includes the ephemeral public key and the
     * authentication tag. Locking helps minimize forensic recovery risk.
     * Failure is non-fatal; see comments at first sodium_mlock call above.
     */
    ciphertext_locked = (sodium_mlock(ciphertext, ciphertext_len) == 0);

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
    salt_secure_release(public_key, sizeof(public_key), public_key_locked);
    if (ciphertext != NULL) {
        salt_secure_release(ciphertext, ciphertext_len, ciphertext_locked);
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

int salt_seal_base64(const unsigned char *message, size_t message_len, const char *public_key_b64,
                     char *output, size_t output_size, size_t *output_written, char *error,
                     size_t error_size) {
    const size_t max_public_key_b64_len =
        sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES, sodium_base64_VARIANT_ORIGINAL) - 1U;
    size_t public_key_b64_len = salt_bounded_strlen(public_key_b64, max_public_key_b64_len + 1U);

    return salt_seal_base64_keylen(message, message_len, public_key_b64, public_key_b64_len, output,
                                   output_size, output_written, error, error_size);
}

/* Implementation of plaintext-zeroing API variants. */
int salt_seal_base64_with_plaintext_zeroing_keylen(unsigned char *message, size_t message_len,
                                                   const char *public_key_b64,
                                                   size_t public_key_b64_len, char *output,
                                                   size_t output_size, size_t *output_written,
                                                   char *error, size_t error_size) {
    int status;

    /*
     * Initialize output_written on every path before validation checks.
     * Contract: If output_written is non-NULL, we ALWAYS write to it before return.
     * This ensures consistency with salt_seal_base64() and simplifies caller logic.
     */
    if (output_written != NULL) {
        *output_written = 0U;
    }

    if (message == NULL) {
        set_error(error, error_size, "invalid null argument");
        return SALT_ERR_INPUT;
    }

    /*
     * Call the standard salt_seal_base64() to perform the encryption.
     * Note: we cast away const from message since the parameter is unsigned char *
     * in this signature (caller explicitly opted into zeroing semantics).
     * Note: output_written is already initialized above, so salt_seal_base64
     * will update it if needed.
     */
    status = salt_seal_base64_keylen((const unsigned char *)message, message_len, public_key_b64,
                                     public_key_b64_len, output, output_size, output_written, error,
                                     error_size);

    /*
     * This API guarantees plaintext wiping on return when a non-NULL plaintext
     * buffer is provided with an accepted length. Zeroing is done on both success
     * and failure paths to minimize retention of secret material in caller memory.
     */
    if (message_len > 0U && message_len <= SALT_MAX_MESSAGE_LEN) {
        sodium_memzero(message, message_len);
    }

    return status;
}

int salt_seal_base64_with_plaintext_zeroing(unsigned char *message, size_t message_len,
                                            const char *public_key_b64, char *output,
                                            size_t output_size, size_t *output_written, char *error,
                                            size_t error_size) {
    const size_t max_public_key_b64_len =
        sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES, sodium_base64_VARIANT_ORIGINAL) - 1U;
    size_t public_key_b64_len = salt_bounded_strlen(public_key_b64, max_public_key_b64_len + 1U);

    return salt_seal_base64_with_plaintext_zeroing_keylen(message, message_len, public_key_b64,
                                                          public_key_b64_len, output, output_size,
                                                          output_written, error, error_size);
}
