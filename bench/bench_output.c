/*
 * SPDX-License-Identifier: Unlicense
 *
 * Microbenchmarks for CLI text/JSON output serialization.
 */

/* NOLINTBEGIN(bugprone-reserved-identifier) -- _POSIX_C_SOURCE required for fmemopen */
#define _POSIX_C_SOURCE 200809L
/* NOLINTEND(bugprone-reserved-identifier) */

#include "bench_cases.h"
#include "bench_common.h"
#include "../src/cli_output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUTPUT_BENCH_BUF_SIZE (256U * 1024U)

struct output_ctx {
    enum salt_cli_output_mode mode;
    const char *encrypted_value;
    const char *key_id;
    char buffer[OUTPUT_BENCH_BUF_SIZE];
    FILE *stream;
};

static int output_run(void *vctx) {
    struct output_ctx *c = (struct output_ctx *)vctx;
    char error[256];

    if (c == NULL || c->stream == NULL) {
        return -1;
    }
    if (fseek(c->stream, 0L, SEEK_SET) != 0) {
        return -1;
    }
    c->buffer[0] = '\0';
    error[0] = '\0';
    return (salt_cli_emit_output(c->stream, c->mode, c->encrypted_value, c->key_id, error,
                                 sizeof(error)) == 0)
               ? 0
               : -1;
}

static int output_ctx_init(struct output_ctx *c, enum salt_cli_output_mode mode,
                           const char *encrypted_value, const char *key_id) {
    if (c == NULL || encrypted_value == NULL) {
        return -1;
    }
    c->mode = mode;
    c->encrypted_value = encrypted_value;
    c->key_id = key_id;
    c->buffer[0] = '\0';
    c->stream = fmemopen(c->buffer, sizeof(c->buffer), "w");
    return (c->stream != NULL) ? 0 : -1;
}

static void output_ctx_cleanup(struct output_ctx *c) {
    if (c == NULL || c->stream == NULL) {
        return;
    }
    (void)fclose(c->stream);
    c->stream = NULL;
}

static const char g_output_ciphertext_short[] = "ZW5jb2RlZA==";
static const char g_output_ciphertext_48kish[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

static const char g_output_key_id_short[] = "bench-kid";
static const char g_output_key_id_escape[] = "kid-\\\"\\n\\r\\t-001";

static struct output_ctx g_output_text_short;
static struct output_ctx g_output_json_short;
static struct output_ctx g_output_json_escape;
static struct output_ctx g_output_json_large;

size_t bench_output_register(struct bench_case *out, size_t cap) {
    static const struct {
        const char *name;
        struct output_ctx *ctx;
        enum salt_cli_output_mode mode;
        const char *encrypted_value;
        const char *key_id;
        size_t iters;
        size_t warmup;
        size_t batch;
    } specs[] = {
        {"output_emit_text_short", &g_output_text_short, SALT_CLI_OUTPUT_TEXT,
         g_output_ciphertext_short, NULL, 5000U, 50U, 200U},
        {"output_emit_json_short", &g_output_json_short, SALT_CLI_OUTPUT_JSON,
         g_output_ciphertext_short, g_output_key_id_short, 5000U, 50U, 150U},
        {"output_emit_json_escape_keyid", &g_output_json_escape, SALT_CLI_OUTPUT_JSON,
         g_output_ciphertext_short, g_output_key_id_escape, 5000U, 50U, 150U},
        {"output_emit_json_large", &g_output_json_large, SALT_CLI_OUTPUT_JSON,
         g_output_ciphertext_48kish, g_output_key_id_short, 2000U, 20U, 10U},
    };
    size_t n = sizeof(specs) / sizeof(specs[0]);

    if (cap < n) {
        return 0U;
    }
    for (size_t i = 0U; i < n; ++i) {
        if (output_ctx_init(specs[i].ctx, specs[i].mode, specs[i].encrypted_value, specs[i].key_id) !=
            0) {
            (void)fprintf(stderr, "bench: failed to init %s\n", specs[i].name);
            return 0U;
        }
        out[i].name = specs[i].name;
        out[i].fn = output_run;
        out[i].ctx = specs[i].ctx;
        out[i].iterations = specs[i].iters;
        out[i].warmup = specs[i].warmup;
        out[i].batch = specs[i].batch;
    }
    return n;
}

void bench_output_cleanup(void) {
    output_ctx_cleanup(&g_output_text_short);
    output_ctx_cleanup(&g_output_json_short);
    output_ctx_cleanup(&g_output_json_escape);
    output_ctx_cleanup(&g_output_json_large);
}
