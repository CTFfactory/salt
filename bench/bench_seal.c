/*
 * SPDX-License-Identifier: Unlicense
 *
 * Microbenchmarks for salt_seal_base64() across plaintext sizes.
 */

#include "bench_common.h"
#include "bench_cases.h"
#include "salt.h"

#include <sodium.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct seal_ctx {
    unsigned char *plaintext;
    size_t plaintext_len;
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    char *output;
    size_t output_size;
};

static int seal_run(void *vctx) {
    struct seal_ctx *c = (struct seal_ctx *)vctx;
    char err[128];
    size_t written = 0U;
    int rc = salt_seal_base64(c->plaintext, c->plaintext_len, c->public_key_b64,
                              c->output, c->output_size, &written, err, sizeof(err));
    return (rc == SALT_OK) ? 0 : -1;
}

static struct seal_ctx *seal_ctx_new(size_t plaintext_len) {
    unsigned char pk[crypto_box_PUBLICKEYBYTES];
    unsigned char sk[crypto_box_SECRETKEYBYTES];
    unsigned char seed[randombytes_SEEDBYTES];
    struct seal_ctx *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        return NULL;
    }
    memset(seed, 0x42, sizeof(seed));
    crypto_box_keypair(pk, sk);
    sodium_bin2base64(c->public_key_b64, sizeof(c->public_key_b64), pk, sizeof(pk),
                      sodium_base64_VARIANT_ORIGINAL);

    c->plaintext_len = plaintext_len;
    c->plaintext = malloc(plaintext_len);
    if (c->plaintext == NULL) {
        free(c);
        return NULL;
    }
    randombytes_buf_deterministic(c->plaintext, plaintext_len, seed);

    c->output_size = salt_required_base64_output_len(plaintext_len);
    c->output = malloc(c->output_size);
    if (c->output == NULL) {
        free(c->plaintext);
        free(c);
        return NULL;
    }
    sodium_memzero(sk, sizeof(sk));
    return c;
}

static void seal_ctx_free(struct seal_ctx *c) {
    if (c == NULL) {
        return;
    }
    free(c->plaintext);
    free(c->output);
    free(c);
}

/* Persistent contexts allocated once; freed at process exit (acceptable for bench tool). */
static struct seal_ctx *g_ctx_32;
static struct seal_ctx *g_ctx_1k;
static struct seal_ctx *g_ctx_48k;
static struct seal_ctx *g_ctx_1m;

size_t bench_seal_register(struct bench_case *out, size_t cap) {
    static const struct {
        const char *name;
        size_t len;
        size_t iters;
        size_t warmup;
        struct seal_ctx **slot;
    } specs[] = {
        {"seal_32B", 32U, 5000U, 50U, &g_ctx_32},
        {"seal_1KiB", 1024U, 2000U, 20U, &g_ctx_1k},
        {"seal_48KiB", SALT_GITHUB_SECRET_MAX_VALUE_LEN, 250U, 5U, &g_ctx_48k},
        {"seal_1MiB", 1024U * 1024U, 50U, 2U, &g_ctx_1m},
    };
    size_t n = sizeof(specs) / sizeof(specs[0]);
    if (cap < n) {
        return 0U;
    }
    for (size_t i = 0U; i < n; ++i) {
        *specs[i].slot = seal_ctx_new(specs[i].len);
        if (*specs[i].slot == NULL) {
            (void)fprintf(stderr, "bench: failed to init %s\n", specs[i].name);
            return 0U;
        }
        out[i].name = specs[i].name;
        out[i].fn = seal_run;
        out[i].ctx = *specs[i].slot;
        out[i].iterations = specs[i].iters;
        out[i].warmup = specs[i].warmup;
    }
    return n;
}

void bench_seal_cleanup(void) {
    seal_ctx_free(g_ctx_32);
    seal_ctx_free(g_ctx_1k);
    seal_ctx_free(g_ctx_48k);
    seal_ctx_free(g_ctx_1m);
}
