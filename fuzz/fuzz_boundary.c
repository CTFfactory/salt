/*
 * SPDX-License-Identifier: Unlicense
 *
 * Boundary-focused libFuzzer harness.
 *
 * Pins inputs at the documented size limits ±1 to exercise off-by-one and
 * boundary-overflow paths that randomly sized inputs hit only by chance:
 *
 *   - SALT_MAX_KEY_INPUT_LEN (16 KiB) for --key
 *   - SALT_GITHUB_SECRET_MAX_VALUE_LEN (48 KiB) for plaintext
 *   - SALT_MAX_KEY_ID_LEN (256 B) for --key-id
 *   - SALT_MAX_MESSAGE_LEN (1 MiB) for the encrypted-value buffer ceiling
 *
 * For each invocation we pick a target boundary, derive lengths of
 * exactly N-1, N, and N+1 bytes (where the buffer can hold N+1) and feed
 * them through salt_seal_base64 plus the CLI front-end. This is the
 * classic guided-fuzzing pattern for catching overruns at allocation
 * boundaries.
 */

#include "salt.h"
#include "salt_cli_internal.h"
#include "fuzz_common.h"

#include <sodium.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOUNDARY_PLAINTEXT_BUF (SALT_GITHUB_SECRET_MAX_VALUE_LEN + 16U)
#define BOUNDARY_OUT_BUF (SALT_MAX_MESSAGE_LEN + 16U)
#define BOUNDARY_KEY_INPUT_BUF (SALT_MAX_KEY_INPUT_LEN + 16U)
#define BOUNDARY_KEY_ID_BUF (SALT_MAX_KEY_ID_LEN + 16U)
#define BOUNDARY_ERR_BUF 512U

static int boundary_ready;
static char boundary_valid_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                             sodium_base64_VARIANT_ORIGINAL)];

static int boundary_init(void) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];

    if (sodium_init() < 0) {
        return -1;
    }
    if (fuzz_seed_keypair(public_key, secret_key, 0x44U) != 0) {
        return -1;
    }
    if (sodium_bin2base64(boundary_valid_key_b64, sizeof(boundary_valid_key_b64), public_key,
                          sizeof(public_key), sodium_base64_VARIANT_ORIGINAL) == NULL) {
        sodium_memzero(secret_key, sizeof(secret_key));
        return -1;
    }
    sodium_memzero(public_key, sizeof(public_key));
    sodium_memzero(secret_key, sizeof(secret_key));
    boundary_ready = 1;
    return 0;
}

static void boundary_fill(uint8_t *dst, size_t len, const uint8_t *seed, size_t seed_len) {
    size_t i;
    if (seed_len == 0U) {
        /* Annex K memset_s is unavailable on glibc; size bounded by len. */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memset(dst, 'A', len);
        return;
    }
    for (i = 0U; i < len; ++i) {
        dst[i] = seed[i % seed_len];
    }
}

static void boundary_exercise(size_t plaintext_len, size_t key_input_len, size_t key_id_len,
                              const uint8_t *seed, size_t seed_len) {
    static unsigned char plaintext[BOUNDARY_PLAINTEXT_BUF];
    static char encrypted[BOUNDARY_OUT_BUF];
    static char key_input[BOUNDARY_KEY_INPUT_BUF];
    static char key_id[BOUNDARY_KEY_ID_BUF];
    char error[BOUNDARY_ERR_BUF];
    size_t encrypted_len = 0U;

    if (plaintext_len > sizeof(plaintext)) {
        plaintext_len = sizeof(plaintext);
    }
    if (key_input_len >= sizeof(key_input)) {
        key_input_len = sizeof(key_input) - 1U;
    }
    if (key_id_len >= sizeof(key_id)) {
        key_id_len = sizeof(key_id) - 1U;
    }

    boundary_fill(plaintext, plaintext_len, seed, seed_len);
    boundary_fill((uint8_t *)key_input, key_input_len, seed, seed_len);
    key_input[key_input_len] = '\0';
    boundary_fill((uint8_t *)key_id, key_id_len, seed, seed_len);
    key_id[key_id_len] = '\0';

    /* Direct seal path with a known-good key — exercises crypto_box_seal
     * length math at exactly the documented plaintext caps. */
    /* Annex K memset_s is unavailable on glibc; size bounded by sizeof(error). */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(error, 0, sizeof(error));
    (void)salt_seal_base64(plaintext, plaintext_len, boundary_valid_key_b64, encrypted,
                           sizeof(encrypted), &encrypted_len, error, sizeof(error));

    /* Parse path with a key blob pinned to SALT_MAX_KEY_INPUT_LEN ±1. */
    {
        char parsed_key[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL) +
                        4U];
        char parsed_key_id[SALT_MAX_KEY_ID_LEN + 1U];
        bool has_key_id = false;
        /* Annex K memset_s is unavailable on glibc; size bounded by sizeof(error). */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memset(error, 0, sizeof(error));
        (void)salt_cli_parse_key_input(key_input, SALT_CLI_KEY_FORMAT_AUTO, parsed_key,
                                       sizeof(parsed_key), parsed_key_id, sizeof(parsed_key_id),
                                       &has_key_id, error, sizeof(error));
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const size_t boundaries[] = {
        SALT_MAX_KEY_INPUT_LEN,
        SALT_GITHUB_SECRET_MAX_VALUE_LEN,
        SALT_MAX_KEY_ID_LEN,
        crypto_box_PUBLICKEYBYTES,
        sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES, sodium_base64_VARIANT_ORIGINAL) - 1U,
    };
    static const int deltas[] = {-1, 0, 1};
    size_t boundary_idx;
    size_t delta_idx;
    size_t plaintext_len;
    size_t key_input_len;
    size_t key_id_len;
    size_t boundary_value;
    int delta;

    if (!boundary_ready && boundary_init() != 0) {
        return 0;
    }
    if (size < 3U) {
        return 0;
    }

    boundary_idx = (size_t)data[0] % (sizeof(boundaries) / sizeof(boundaries[0]));
    delta_idx = (size_t)data[1] % (sizeof(deltas) / sizeof(deltas[0]));
    boundary_value = boundaries[boundary_idx];
    delta = deltas[delta_idx];

    plaintext_len =
        (delta < 0 && boundary_value == 0U) ? 0U : (size_t)((ssize_t)boundary_value + delta);

    /* Use the same boundary value (±delta) for the key blob and another
     * boundary for the key-id so a single execution touches multiple
     * limits at once. */
    key_input_len =
        (boundary_value > SALT_MAX_KEY_INPUT_LEN) ? SALT_MAX_KEY_INPUT_LEN : boundary_value;
    if (delta < 0 && key_input_len > 0U) {
        --key_input_len;
    } else if (delta > 0) {
        ++key_input_len;
    }

    key_id_len = (size_t)data[2] % (SALT_MAX_KEY_ID_LEN + 2U);

    boundary_exercise(plaintext_len, key_input_len, key_id_len, data + 3U, size - 3U);
    return 0;
}
