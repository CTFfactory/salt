/*
 * SPDX-License-Identifier: Unlicense
 *
 * Shared helpers for salt libFuzzer harnesses.
 */

#ifndef SALT_FUZZ_COMMON_H
#define SALT_FUZZ_COMMON_H

#include <sodium.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline size_t fuzz_copy_cstr(char *dst, size_t dst_size, const uint8_t *src,
                                    size_t src_len) {
    size_t copy_len;

    if (dst == NULL || dst_size == 0U) {
        return 0U;
    }

    copy_len = (src_len < (dst_size - 1U)) ? src_len : (dst_size - 1U);
    if (src != NULL && copy_len > 0U) {
        /* Annex K memcpy_s is unavailable on glibc; sizes bounded by dst_size. */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memcpy(dst, src, copy_len);
    }
    dst[copy_len] = '\0';
    return copy_len;
}

static inline int fuzz_seed_keypair(unsigned char *public_key, unsigned char *secret_key,
                                    unsigned char seed_fill) {
    unsigned char seed[crypto_box_SEEDBYTES];

    /* Annex K memset_s is unavailable on glibc; size bounded by sizeof(seed). */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(seed, seed_fill, sizeof(seed));
    return crypto_box_seed_keypair(public_key, secret_key, seed);
}

#endif
