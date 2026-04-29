/*
 * SPDX-License-Identifier: Unlicense
 *
 * Microbenchmarks for the base64 encoding step in isolation.
 */

#include "bench_common.h"
#include "bench_cases.h"

#include <sodium.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct b64_ctx {
    unsigned char *bin;
    size_t bin_len;
    char *out;
    size_t out_size;
};

static int b64_run(void *vctx) {
    struct b64_ctx *c = (struct b64_ctx *)vctx;
    const char *r = sodium_bin2base64(c->out, c->out_size, c->bin, c->bin_len,
                                      sodium_base64_VARIANT_ORIGINAL);
    return (r != NULL) ? 0 : -1;
}

static struct b64_ctx *b64_ctx_new(size_t bin_len) {
    unsigned char seed[randombytes_SEEDBYTES];
    struct b64_ctx *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        return NULL;
    }
    c->bin_len = bin_len;
    c->bin = malloc(bin_len);
    c->out_size = sodium_base64_ENCODED_LEN(bin_len, sodium_base64_VARIANT_ORIGINAL);
    c->out = malloc(c->out_size);
    if (c->bin == NULL || c->out == NULL) {
        free(c->bin);
        free(c->out);
        free(c);
        return NULL;
    }
    memset(seed, 0x7E, sizeof(seed));
    randombytes_buf_deterministic(c->bin, bin_len, seed);
    return c;
}

static void b64_ctx_free(struct b64_ctx *c) {
    if (c == NULL) {
        return;
    }
    free(c->bin);
    free(c->out);
    free(c);
}

static struct b64_ctx *g_b64_64;
static struct b64_ctx *g_b64_1k;
static struct b64_ctx *g_b64_64k;

size_t bench_base64_register(struct bench_case *out, size_t cap) {
    static const struct {
        const char *name;
        size_t len;
        size_t iters;
        size_t warmup;
        size_t batch;
        struct b64_ctx **slot;
    } specs[] = {
        {"base64_encode_64B", 64U, 5000U, 50U, 100U, &g_b64_64},
        {"base64_encode_1KiB", 1024U, 5000U, 50U, 10U, &g_b64_1k},
        {"base64_encode_64KiB", 65536U, 500U, 5U, 1U, &g_b64_64k},
    };
    size_t n = sizeof(specs) / sizeof(specs[0]);
    if (cap < n) {
        return 0U;
    }
    for (size_t i = 0U; i < n; ++i) {
        *specs[i].slot = b64_ctx_new(specs[i].len);
        if (*specs[i].slot == NULL) {
            (void)fprintf(stderr, "bench: failed to init %s\n", specs[i].name);
            return 0U;
        }
        out[i].name = specs[i].name;
        out[i].fn = b64_run;
        out[i].ctx = *specs[i].slot;
        out[i].iterations = specs[i].iters;
        out[i].warmup = specs[i].warmup;
        out[i].batch = specs[i].batch;
    }
    return n;
}

void bench_base64_cleanup(void) {
    b64_ctx_free(g_b64_64);
    b64_ctx_free(g_b64_1k);
    b64_ctx_free(g_b64_64k);
}
