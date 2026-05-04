/*
 * SPDX-License-Identifier: Unlicense
 *
 * Microbenchmarks for salt_cli_parse_key_input() across input formats.
 */

#include "bench_common.h"
#include "bench_cases.h"
#include "salt.h"
#include "cli/internal.h"

#include <sodium.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct parse_ctx {
    const char *input;
    enum salt_cli_key_format format;
};

static int parse_run(void *vctx) {
    struct parse_ctx *c = (struct parse_ctx *)vctx;
    char key_b64[512];
    char key_id[256];
    bool has_id = false;
    char err[128];
    int rc = salt_cli_parse_key_input(c->input, c->format, key_b64, sizeof(key_b64),
                                      key_id, sizeof(key_id), &has_id, err, sizeof(err));
    return (rc == SALT_OK) ? 0 : -1;
}

/*
 * Pre-built deterministic inputs. The base64 key below is a real
 * 32-byte libsodium public key encoded in base64 (variant ORIGINAL).
 */
#define BENCH_PARSE_KEY_B64 "rQB7XCRJP1Z+jY4mN1pK5tWvE3Gd0aHfXkBSYr9hzVE="

static struct parse_ctx g_parse_b64 = {
    BENCH_PARSE_KEY_B64,
    SALT_CLI_KEY_FORMAT_BASE64,
};
static struct parse_ctx g_parse_json_with_id = {
    "{\"key\":\"" BENCH_PARSE_KEY_B64 "\",\"key_id\":\"k123\"}",
    SALT_CLI_KEY_FORMAT_JSON,
};
static struct parse_ctx g_parse_json_no_id = {
    "{\"key\":\"" BENCH_PARSE_KEY_B64 "\"}",
    SALT_CLI_KEY_FORMAT_JSON,
};
static struct parse_ctx g_parse_auto_base64 = {
    BENCH_PARSE_KEY_B64,
    SALT_CLI_KEY_FORMAT_AUTO,
};
static struct parse_ctx g_parse_auto_json_whitespace = {
    " \n\t{\"key\":\"" BENCH_PARSE_KEY_B64 "\",\"key_id\":\"kid-whitespace\"}\t",
    SALT_CLI_KEY_FORMAT_AUTO,
};
static struct parse_ctx g_parse_json_long_key_id = {
    "{\"key\":\"" BENCH_PARSE_KEY_B64
    "\",\"key_id\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}",
    SALT_CLI_KEY_FORMAT_JSON,
};

size_t bench_parse_register(struct bench_case *out, size_t cap) {
    static const struct {
        const char *name;
        struct parse_ctx *ctx;
        size_t iters;
        size_t warmup;
        size_t batch;
    } specs[] = {
        {"parse_base64", &g_parse_b64, 5000U, 50U, 200U},
        {"parse_json_with_id", &g_parse_json_with_id, 5000U, 50U, 200U},
        {"parse_json_no_id", &g_parse_json_no_id, 5000U, 50U, 200U},
        {"parse_auto_base64", &g_parse_auto_base64, 5000U, 50U, 200U},
        {"parse_auto_json_whitespace", &g_parse_auto_json_whitespace, 5000U, 50U, 200U},
        {"parse_json_long_key_id", &g_parse_json_long_key_id, 5000U, 50U, 200U},
    };
    size_t n = sizeof(specs) / sizeof(specs[0]);
    if (cap < n) {
        return 0U;
    }
    for (size_t i = 0U; i < n; ++i) {
        out[i].name = specs[i].name;
        out[i].fn = parse_run;
        out[i].ctx = specs[i].ctx;
        out[i].iterations = specs[i].iters;
        out[i].warmup = specs[i].warmup;
        out[i].batch = specs[i].batch;
    }
    return n;
}

void bench_parse_cleanup(void) {
    /* No persistent allocations. */
}
