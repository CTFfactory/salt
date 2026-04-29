/*
 * SPDX-License-Identifier: Unlicense
 *
 * Differential round-trip libFuzzer harness.
 *
 * Drives success and failure branches around the sealed-box contract rather
 * than only feeding arbitrary plaintext into a single mostly-linear happy
 * path. The harness still proves the main invariant — a successful seal must
 * decode, decrypt, and reproduce the original plaintext — but it also
 * exercises key-decoding and output-buffer validation plus negative checks on
 * corrupted ciphertext.
 */

#include "salt.h"
#include "fuzz_common.h"

#include <sodium.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDTRIP_MAX_PLAINTEXT SALT_GITHUB_SECRET_MAX_VALUE_LEN
#define ROUNDTRIP_MAX_CIPHERTEXT (ROUNDTRIP_MAX_PLAINTEXT + crypto_box_SEALBYTES)
#define ROUNDTRIP_MAX_B64                                                                          \
    sodium_base64_ENCODED_LEN(ROUNDTRIP_MAX_CIPHERTEXT, sodium_base64_VARIANT_ORIGINAL)
#define ROUNDTRIP_ERR_BUF 512U
#define ROUNDTRIP_CTRL_BYTES 3U

static int roundtrip_ready;
static unsigned char roundtrip_public_key[crypto_box_PUBLICKEYBYTES];
static unsigned char roundtrip_secret_key[crypto_box_SECRETKEYBYTES];
static char roundtrip_public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                               sodium_base64_VARIANT_ORIGINAL)];

static int roundtrip_init(void) {
    if (sodium_init() < 0) {
        return -1;
    }
    if (fuzz_seed_keypair(roundtrip_public_key, roundtrip_secret_key, 0x45U) != 0) {
        return -1;
    }
    if (sodium_bin2base64(roundtrip_public_key_b64, sizeof(roundtrip_public_key_b64),
                          roundtrip_public_key, sizeof(roundtrip_public_key),
                          sodium_base64_VARIANT_ORIGINAL) == NULL) {
        return -1;
    }
    roundtrip_ready = 1;
    return 0;
}

static void roundtrip_fill_bytes(unsigned char *dst, size_t dst_len, const uint8_t *seed,
                                 size_t seed_len) {
    size_t i;

    if (dst == NULL || dst_len == 0U) {
        return;
    }
    if (seed == NULL || seed_len == 0U) {
        /* Annex K memset_s is unavailable on glibc; size bounded by dst_len. */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memset(dst, 'A', dst_len);
        return;
    }
    for (i = 0U; i < dst_len; ++i) {
        dst[i] = seed[i % seed_len];
    }
}

static size_t roundtrip_select_plaintext_len(uint8_t mode, const uint8_t *seed, size_t seed_len) {
    static const size_t presets[] = {
        0U,
        1U,
        2U,
        3U,
        15U,
        16U,
        31U,
        32U,
        63U,
        64U,
        255U,
        256U,
        1023U,
        1024U,
        4095U,
        4096U,
        8192U,
        16384U,
        ROUNDTRIP_MAX_PLAINTEXT - 1U,
        ROUNDTRIP_MAX_PLAINTEXT,
    };
    size_t want = 0U;

    switch (mode & 0x3U) {
    case 0U:
        return (seed_len <= ROUNDTRIP_MAX_PLAINTEXT) ? seed_len : ROUNDTRIP_MAX_PLAINTEXT;
    case 1U:
        if (seed_len == 0U) {
            return presets[0];
        }
        return presets[seed[0] % (sizeof(presets) / sizeof(presets[0]))];
    case 2U:
        if (seed_len > 0U) {
            want = seed[0];
        }
        if (seed_len > 1U) {
            want |= ((size_t)seed[1]) << 8;
        }
        return want % (ROUNDTRIP_MAX_PLAINTEXT + 1U);
    default:
        return ROUNDTRIP_MAX_PLAINTEXT;
    }
}

static bool roundtrip_build_key_variant(uint8_t mode, char *dst, size_t dst_size) {
    size_t base_len;
    size_t trimmed_len;

    if (dst == NULL || dst_size == 0U) {
        return false;
    }

    base_len = strlen(roundtrip_public_key_b64);
    switch (mode % 6U) {
    case 0U:
        if (base_len >= dst_size) {
            return false;
        }
        /* Annex K memcpy_s is unavailable on glibc; size bounded by base_len. */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memcpy(dst, roundtrip_public_key_b64, base_len + 1U);
        return true;
    case 1U:
        trimmed_len = base_len;
        while (trimmed_len > 0U && roundtrip_public_key_b64[trimmed_len - 1U] == '=') {
            --trimmed_len;
        }
        if (trimmed_len == 0U || trimmed_len >= dst_size) {
            return false;
        }
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memcpy(dst, roundtrip_public_key_b64, trimmed_len);
        dst[trimmed_len] = '\0';
        return true;
    case 2U:
        dst[0] = '\0';
        return false;
    case 3U:
        if (base_len < 5U) {
            dst[0] = '\0';
            return false;
        }
        trimmed_len = base_len - 4U;
        if (trimmed_len >= dst_size) {
            return false;
        }
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memcpy(dst, roundtrip_public_key_b64, trimmed_len);
        dst[trimmed_len] = '\0';
        return false;
    case 4U:
        if (base_len + 2U > dst_size) {
            return false;
        }
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memcpy(dst, roundtrip_public_key_b64, base_len);
        dst[base_len] = '!';
        dst[base_len + 1U] = '\0';
        return false;
    default:
        if (base_len + 3U > dst_size) {
            return false;
        }
        dst[0] = ' ';
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memcpy(dst + 1U, roundtrip_public_key_b64, base_len);
        dst[base_len + 1U] = '\n';
        dst[base_len + 2U] = '\0';
        return false;
    }
}

static size_t roundtrip_select_output_size(uint8_t mode, size_t required) {
    switch (mode & 0x3U) {
    case 0U:
        return required;
    case 1U:
        return ROUNDTRIP_MAX_B64;
    case 2U:
        return (required > 1U) ? (required - 1U) : 1U;
    default:
        return 8U;
    }
}

static char roundtrip_flip_base64_char(char ch) {
    if (ch != 'A' && ch != '=') {
        return 'A';
    }
    return 'B';
}

static size_t roundtrip_build_ciphertext_variant(uint8_t mode, char *dst, size_t dst_size,
                                                 const char *src, size_t src_len) {
    size_t idx;

    if (dst == NULL || dst_size == 0U || src == NULL || src_len + 2U > dst_size) {
        abort();
    }
    /* Annex K memcpy_s is unavailable on glibc; size bounded by src_len. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memcpy(dst, src, src_len);
    dst[src_len] = '\0';

    switch (mode & 0x3U) {
    case 0U:
        return src_len;
    case 1U:
        if (src_len > 1U) {
            dst[src_len - 1U] = '\0';
            return src_len - 1U;
        }
        return src_len;
    case 2U:
        dst[src_len] = '!';
        dst[src_len + 1U] = '\0';
        return src_len + 1U;
    default:
        for (idx = 0U; idx < src_len; ++idx) {
            if (dst[idx] != '=') {
                dst[idx] = roundtrip_flip_base64_char(dst[idx]);
                return src_len;
            }
        }
        return src_len;
    }
}

static void roundtrip_verify_success(const char *ciphertext_b64, size_t ciphertext_b64_len,
                                     const unsigned char *plaintext, size_t plaintext_len,
                                     unsigned char *ciphertext, unsigned char *decrypted) {
    size_t ciphertext_len = 0U;

    if (sodium_base642bin(ciphertext, ROUNDTRIP_MAX_CIPHERTEXT, ciphertext_b64, ciphertext_b64_len,
                          NULL, &ciphertext_len, NULL, sodium_base64_VARIANT_ORIGINAL) != 0) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)fprintf(stderr, "roundtrip: salt produced invalid base64\n");
        abort();
    }
    if (ciphertext_len != plaintext_len + crypto_box_SEALBYTES) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)fprintf(stderr, "roundtrip: ciphertext length mismatch: got %zu expected %zu\n",
                      ciphertext_len, plaintext_len + crypto_box_SEALBYTES);
        abort();
    }
    if (crypto_box_seal_open(decrypted, ciphertext, ciphertext_len, roundtrip_public_key,
                             roundtrip_secret_key) != 0) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)fprintf(stderr, "roundtrip: decrypt failed\n");
        abort();
    }
    if (sodium_memcmp(decrypted, plaintext, plaintext_len) != 0) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)fprintf(stderr, "roundtrip: decrypted plaintext differs from input\n");
        abort();
    }
}

static void roundtrip_verify_negative_variant(uint8_t mode, const char *ciphertext_b64,
                                              size_t ciphertext_b64_len, size_t plaintext_len,
                                              unsigned char *ciphertext, unsigned char *decrypted) {
    size_t ciphertext_len = 0U;
    int open_rc;

    if ((mode & 0x3U) == 2U) {
        if (sodium_base642bin(ciphertext, ROUNDTRIP_MAX_CIPHERTEXT, ciphertext_b64,
                              ciphertext_b64_len, NULL, &ciphertext_len, NULL,
                              sodium_base64_VARIANT_ORIGINAL) == 0) {
            /* clang-format off */
            /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
            (void)fprintf(stderr, "roundtrip: trailing-junk variant unexpectedly decoded\n");
            /* clang-format on */
            abort();
        }
        return;
    }

    if (sodium_base642bin(ciphertext, ROUNDTRIP_MAX_CIPHERTEXT, ciphertext_b64, ciphertext_b64_len,
                          NULL, &ciphertext_len, NULL, sodium_base64_VARIANT_ORIGINAL) != 0) {
        return;
    }
    if (ciphertext_len != plaintext_len + crypto_box_SEALBYTES) {
        return;
    }

    open_rc = crypto_box_seal_open(decrypted, ciphertext, ciphertext_len, roundtrip_public_key,
                                   roundtrip_secret_key);
    if (open_rc == 0) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)fprintf(stderr, "roundtrip: corrupted ciphertext variant decrypted unexpectedly\n");
        abort();
    }
}

struct roundtrip_case {
    uint8_t key_mode;
    uint8_t output_mode;
    uint8_t ciphertext_mode;
    uint8_t plaintext_mode;
    const uint8_t *payload;
    size_t payload_len;
    size_t plaintext_len;
    size_t required_output;
    size_t output_size;
    bool key_valid;
    bool expect_success;
};

static void roundtrip_parse_case(const uint8_t *data, size_t size, struct roundtrip_case *tc) {
    if (tc == NULL) {
        abort();
    }

    /* Annex K memset_s is unavailable on glibc; size bounded by sizeof(*tc). */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(tc, 0, sizeof(*tc));
    tc->payload = data;
    tc->payload_len = size;

    if (size >= 1U) {
        tc->key_mode = data[0];
        ++tc->payload;
        --tc->payload_len;
    }
    if (size >= 2U) {
        tc->output_mode = data[1];
        ++tc->payload;
        --tc->payload_len;
    }
    if (size >= 3U) {
        tc->ciphertext_mode = data[2];
        tc->plaintext_mode = data[2] >> 2;
        ++tc->payload;
        --tc->payload_len;
    }

    tc->plaintext_len =
        roundtrip_select_plaintext_len(tc->plaintext_mode, tc->payload, tc->payload_len);
    tc->required_output = salt_required_base64_output_len(tc->plaintext_len);
    tc->output_size = roundtrip_select_output_size(tc->output_mode, tc->required_output);
}

static void roundtrip_assert_expected_failure(const struct roundtrip_case *tc, int seal_rc,
                                              const char *error, unsigned char *plaintext) {
    if (tc == NULL || error == NULL || plaintext == NULL) {
        abort();
    }

    /* NOLINTNEXTLINE(misc-redundant-expression) -- fuzz harness explicit validation */
    if (seal_rc == SALT_OK || seal_rc != SALT_ERR_INPUT) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)fprintf(stderr, "roundtrip: expected input failure, got rc=%d (%s)\n", seal_rc,
                      error);
        abort();
    }
    sodium_memzero(plaintext, tc->plaintext_len);
}

static void roundtrip_assert_expected_success(const struct roundtrip_case *tc, int seal_rc,
                                              size_t encrypted_len, const char *error,
                                              const char *encrypted_b64, char *ciphertext_variant,
                                              unsigned char *plaintext, unsigned char *ciphertext,
                                              unsigned char *decrypted) {
    size_t variant_len;

    if (tc == NULL || error == NULL || encrypted_b64 == NULL || ciphertext_variant == NULL ||
        plaintext == NULL || ciphertext == NULL || decrypted == NULL) {
        abort();
    }

    if (seal_rc != SALT_OK) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)fprintf(stderr, "roundtrip: seal failed unexpectedly: %s\n", error);
        abort();
    }
    if (encrypted_len == 0U || encrypted_len >= tc->output_size ||
        encrypted_len >= ROUNDTRIP_MAX_B64) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)fprintf(stderr, "roundtrip: implausible encrypted length %zu\n", encrypted_len);
        abort();
    }

    variant_len =
        roundtrip_build_ciphertext_variant(tc->ciphertext_mode, ciphertext_variant,
                                           ROUNDTRIP_MAX_B64 + 2U, encrypted_b64, encrypted_len);
    if ((tc->ciphertext_mode & 0x3U) == 0U) {
        roundtrip_verify_success(ciphertext_variant, variant_len, plaintext, tc->plaintext_len,
                                 ciphertext, decrypted);
    } else {
        roundtrip_verify_negative_variant(tc->ciphertext_mode, ciphertext_variant, variant_len,
                                          tc->plaintext_len, ciphertext, decrypted);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static unsigned char plaintext[ROUNDTRIP_MAX_PLAINTEXT];
    static unsigned char ciphertext[ROUNDTRIP_MAX_CIPHERTEXT];
    static unsigned char decrypted[ROUNDTRIP_MAX_PLAINTEXT];
    static char encrypted_b64[ROUNDTRIP_MAX_B64];
    static char ciphertext_variant[ROUNDTRIP_MAX_B64 + 2U];
    static char public_key_variant[sizeof(roundtrip_public_key_b64) + 4U];
    struct roundtrip_case tc;
    size_t encrypted_len = 0U;
    char error[ROUNDTRIP_ERR_BUF];
    int seal_rc;

    if (!roundtrip_ready && roundtrip_init() != 0) {
        return 0;
    }

    roundtrip_parse_case(data, size, &tc);
    roundtrip_fill_bytes(plaintext, tc.plaintext_len, tc.payload, tc.payload_len);
    tc.key_valid =
        roundtrip_build_key_variant(tc.key_mode, public_key_variant, sizeof(public_key_variant));
    tc.expect_success = (tc.plaintext_len > 0U) && tc.key_valid && tc.required_output != 0U &&
                        tc.output_size >= tc.required_output;

    /* Annex K memset_s is unavailable on glibc; size bounded by sizeof(error). */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(error, 0, sizeof(error));
    encrypted_b64[0] = '\0';
    encrypted_len = 0U;
    seal_rc = salt_seal_base64(plaintext, tc.plaintext_len, public_key_variant, encrypted_b64,
                               tc.output_size, &encrypted_len, error, sizeof(error));

    if (!tc.expect_success) {
        roundtrip_assert_expected_failure(&tc, seal_rc, error, plaintext);
        return 0;
    }

    roundtrip_assert_expected_success(&tc, seal_rc, encrypted_len, error, encrypted_b64,
                                      ciphertext_variant, plaintext, ciphertext, decrypted);

    sodium_memzero(plaintext, tc.plaintext_len);
    sodium_memzero(ciphertext, sizeof(ciphertext));
    sodium_memzero(decrypted, sizeof(decrypted));
    return 0;
}
