/*
 * SPDX-License-Identifier: Unlicense
 *
 * libFuzzer harness for end-to-end CLI option and stdin interaction paths.
 *
 * Drives argv construction directly from fuzzer bytes via a token table.
 * Each input byte (mod table size) either selects a known CLI flag/value
 * token or splices in a chunk of fuzzer-controlled bytes. This reaches
 * arbitrary permutations of every documented option, including
 * pathological combinations the prior branch-table design could never
 * construct.
 */

/* NOLINTBEGIN(bugprone-reserved-identifier) -- _POSIX_C_SOURCE required for open_memstream */
#define _POSIX_C_SOURCE 200809L
/* NOLINTEND(bugprone-reserved-identifier) */

#include "salt.h"
#include "fuzz_common.h"

#include <sodium.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_MAX_ARGV 32
#define FUZZ_ARG_BUF ((size_t)(96U * 1024U)) /* > SALT_MAX_KEY_INPUT_LEN + GitHub 48 KB cap */
#define FUZZ_STDIN_BUF ((size_t)(96U * 1024U))
#define FUZZ_OUT_BUF ((size_t)(256U * 1024U))
#define FUZZ_ERR_BUF ((size_t)(16U * 1024U))

static int fuzz_cli_ready;
static char fuzz_valid_public_key[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                            sodium_base64_VARIANT_ORIGINAL)];

static const char *const fuzz_token_table[] = {
    "-k",
    "-i",
    "-o",
    "-f",
    "-h",
    "-V",
    "--",
    "-",
    "--key",
    "--key-id",
    "--output",
    "--key-format",
    "--help",
    "--version",
    "--key=AAAA",
    "--key-id=fuzz",
    "--output=text",
    "--output=json",
    "--key-format=base64",
    "--key-format=json",
    "text",
    "json",
    "auto",
    "base64",
    "-",
    "hello",
    "fuzz-key-id",
    "",
    "--unknown",
    "---",
    /*
     * SENTINEL: inject a length-prefixed fuzzer-controlled argv value instead
     * of a fixed token so one harness covers both structured and arbitrary
     * argv bytes.
     */
    NULL,
};
#define FUZZ_TOKEN_COUNT (sizeof(fuzz_token_table) / sizeof(fuzz_token_table[0]))

static int fuzz_cli_init_valid_key(void) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];

    if (sodium_init() < 0) {
        return -1;
    }
    if (fuzz_seed_keypair(public_key, secret_key, 0x43U) != 0) {
        sodium_memzero(secret_key, sizeof(secret_key));
        return -1;
    }
    if (sodium_bin2base64(fuzz_valid_public_key, sizeof(fuzz_valid_public_key), public_key,
                          sizeof(public_key), sodium_base64_VARIANT_ORIGINAL) == NULL) {
        sodium_memzero(public_key, sizeof(public_key));
        sodium_memzero(secret_key, sizeof(secret_key));
        return -1;
    }
    sodium_memzero(public_key, sizeof(public_key));
    sodium_memzero(secret_key, sizeof(secret_key));
    fuzz_cli_ready = 1;
    return 0;
}

/*
 * Pull a length-prefixed chunk of fuzz bytes into a NUL-terminated string.
 * Embedded NULs are replaced with 'A' so argv values stay cstring-safe;
 * the early-NUL truncation behavior is exercised by fuzz_boundary.
 */
static size_t fuzz_consume_value(const uint8_t *data, size_t size, size_t *cursor, char *dst,
                                 size_t dst_size) {
    size_t want;
    size_t avail;
    size_t copy_len;
    size_t i;

    if (*cursor >= size || dst_size == 0U) {
        if (dst_size > 0U) {
            dst[0] = '\0';
        }
        return 0U;
    }
    want = data[*cursor];
    if (++(*cursor) < size) {
        want |= ((size_t)data[*cursor]) << 8;
        ++(*cursor);
    }
    avail = size - *cursor;
    copy_len = (want < avail) ? want : avail;
    if (copy_len > dst_size - 1U) {
        copy_len = dst_size - 1U;
    }
    if (copy_len > 0U) {
        /* Annex K memcpy_s is unavailable on glibc; size bounded by copy_len. */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memcpy(dst, data + *cursor, copy_len);
        for (i = 0U; i < copy_len; ++i) {
            if (dst[i] == '\0') {
                dst[i] = 'A';
            }
        }
    }
    dst[copy_len] = '\0';
    *cursor += copy_len;
    return copy_len;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static char argv_pool[FUZZ_MAX_ARGV][FUZZ_ARG_BUF];
    static char stdin_buf[FUZZ_STDIN_BUF];
    static char out_buf[FUZZ_OUT_BUF];
    static char err_buf[FUZZ_ERR_BUF];
    char *argv[FUZZ_MAX_ARGV + 1];
    int argc = 0;
    size_t cursor = 0U;
    size_t stdin_len = 0U;
    int inject_valid_key = 0;
    FILE *in_stream;
    FILE *out_stream;
    FILE *err_stream;

    if (!fuzz_cli_ready && fuzz_cli_init_valid_key() != 0) {
        return 0;
    }
    if (size == 0U || data == NULL) {
        return 0;
    }

    (void)strcpy(argv_pool[0], "salt");
    argv[argc++] = argv_pool[0];

    inject_valid_key = ((data[cursor] & 0x80U) != 0U);
    ++cursor;

    while (cursor < size && argc < FUZZ_MAX_ARGV) {
        uint8_t selector = data[cursor++];
        size_t idx = (size_t)selector % FUZZ_TOKEN_COUNT;
        const char *token = fuzz_token_table[idx];
        if (token == NULL) {
            (void)fuzz_consume_value(data, size, &cursor, argv_pool[argc], FUZZ_ARG_BUF);
        } else if (inject_valid_key && (selector & 0x40U) != 0U) {
            size_t key_len = strlen(fuzz_valid_public_key);
            if (key_len >= FUZZ_ARG_BUF) {
                key_len = FUZZ_ARG_BUF - 1U;
            }
            /* Annex K memcpy_s is unavailable on glibc; size bounded by key_len. */
            /* clang-format off */
            /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
            (void)memcpy(argv_pool[argc], fuzz_valid_public_key, key_len);
            /* clang-format on */
            argv_pool[argc][key_len] = '\0';
        } else {
            size_t token_len = strlen(token);
            if (token_len >= FUZZ_ARG_BUF) {
                token_len = FUZZ_ARG_BUF - 1U;
            }
            /* Annex K memcpy_s is unavailable on glibc; size bounded by token_len. */
            /* clang-format off */
            /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
            (void)memcpy(argv_pool[argc], token, token_len);
            /* clang-format on */
            argv_pool[argc][token_len] = '\0';
        }
        argv[argc] = argv_pool[argc];
        argc++;
    }
    argv[argc] = NULL;

    if (cursor < size) {
        stdin_len = size - cursor;
        if (stdin_len > sizeof(stdin_buf)) {
            stdin_len = sizeof(stdin_buf);
        }
        /* Annex K memcpy_s is unavailable on glibc; size bounded by stdin_len. */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memcpy(stdin_buf, data + cursor, stdin_len);
    }

    in_stream = fmemopen(stdin_buf, stdin_len, "r");
    out_stream = fmemopen(out_buf, sizeof(out_buf), "w");
    err_stream = fmemopen(err_buf, sizeof(err_buf), "w");
    if (in_stream == NULL || out_stream == NULL || err_stream == NULL) {
        if (in_stream != NULL) {
            (void)fclose(in_stream);
        }
        if (out_stream != NULL) {
            (void)fclose(out_stream);
        }
        if (err_stream != NULL) {
            (void)fclose(err_stream);
        }
        return 0;
    }

    (void)salt_cli_run_with_streams(argc, argv, in_stream, out_stream, err_stream);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
    return 0;
}
