/*
 * SPDX-License-Identifier: Unlicense
 *
 * Implementation of the salt benchmarking harness.
 */

#define _POSIX_C_SOURCE 200809L

#include "bench_common.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

uint64_t bench_now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int bench_parse_size_arg(const char *value, const char *flag, const char *tool_name, size_t *out) {
    char *end = NULL;
    unsigned long long parsed;

    if (flag == NULL || tool_name == NULL || out == NULL) {
        return -1;
    }
    if (value == NULL || *value == '\0') {
        (void)fprintf(stderr, "%s: --%s requires a non-empty integer\n", tool_name, flag);
        return -1;
    }

    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno != 0 || end == value || end == NULL || *end != '\0' ||
        parsed > (unsigned long long)SIZE_MAX) {
        (void)fprintf(stderr, "%s: invalid value for --%s: %s\n", tool_name, flag, value);
        return -1;
    }

    *out = (size_t)parsed;
    return 0;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    if (x < y)
        return -1;
    if (x > y)
        return 1;
    return 0;
}

int bench_run_case(const struct bench_case *bc, const struct bench_options *opts,
                   struct bench_result *result) {
    size_t iterations = (opts != NULL && opts->iterations > 0U) ? opts->iterations : bc->iterations;
    size_t warmup = (opts != NULL && opts->warmup != (size_t)-1) ? opts->warmup : bc->warmup;
    uint64_t *samples;
    double sum = 0.0;
    double sq_sum = 0.0;

    if (iterations == 0U) {
        return -1;
    }

    samples = (uint64_t *)calloc(iterations, sizeof(uint64_t));
    if (samples == NULL) {
        return -1;
    }

    for (size_t i = 0U; i < warmup; ++i) {
        if (bc->fn(bc->ctx) != 0) {
            free(samples);
            return -1;
        }
    }

    {
        size_t batch = (bc->batch > 0U) ? bc->batch : 1U;
        for (size_t i = 0U; i < iterations; ++i) {
            uint64_t t0 = bench_now_ns();
            for (size_t b = 0U; b < batch; ++b) {
                if (bc->fn(bc->ctx) != 0) {
                    free(samples);
                    return -1;
                }
            }
            uint64_t t1 = bench_now_ns();
            uint64_t delta = (t1 >= t0) ? (t1 - t0) : 0U;
            samples[i] = delta / batch;
        }
    }

    qsort(samples, iterations, sizeof(uint64_t), cmp_u64);

    for (size_t i = 0U; i < iterations; ++i) {
        sum += (double)samples[i];
    }
    double mean = sum / (double)iterations;
    for (size_t i = 0U; i < iterations; ++i) {
        double d = (double)samples[i] - mean;
        sq_sum += d * d;
    }

    result->name = bc->name;
    result->iterations = iterations;
    result->ns_min = samples[0];
    result->ns_median = samples[iterations / 2U];
    {
        size_t p95_idx = (size_t)((double)iterations * 0.95);
        if (p95_idx >= iterations) {
            p95_idx = iterations - 1U;
        }
        result->ns_p95 = samples[p95_idx];
    }
    result->ns_max = samples[iterations - 1U];
    result->ns_mean = mean;
    result->ns_stddev = sqrt(sq_sum / (double)iterations);

    free(samples);
    return 0;
}

static void format_ns(uint64_t ns, char *buf, size_t buf_size) {
    if (ns < 1000U) {
        (void)snprintf(buf, buf_size, "%lu ns", (unsigned long)ns);
    } else if (ns < 1000000U) {
        (void)snprintf(buf, buf_size, "%.2f us", (double)ns / 1000.0);
    } else if (ns < 1000000000U) {
        (void)snprintf(buf, buf_size, "%.2f ms", (double)ns / 1000000.0);
    } else {
        (void)snprintf(buf, buf_size, "%.2f s", (double)ns / 1000000000.0);
    }
}

void bench_emit_text(const struct bench_result *r) {
    char min_s[32];
    char med_s[32];
    char p95_s[32];
    double ops_per_sec = (r->ns_median > 0U) ? (1e9 / (double)r->ns_median) : 0.0;

    format_ns(r->ns_min, min_s, sizeof(min_s));
    format_ns(r->ns_median, med_s, sizeof(med_s));
    format_ns(r->ns_p95, p95_s, sizeof(p95_s));

    (void)printf("%-40s iters=%-7zu min=%-10s median=%-10s p95=%-10s %.0f ops/s\n", r->name,
                 r->iterations, min_s, med_s, p95_s, ops_per_sec);
}

int bench_emit_json(const char *path, const struct bench_result *r) {
    FILE *f;
    double ops_per_sec;
    /*
     * Defense-in-depth JSON name escaping. Current bench case names are
     * static identifiers, but encode '"' and '\\' and refuse control bytes
     * so future cases cannot corrupt the JSON record stream parsed by
     * bench-compare.sh.
     */
    char name_buf[128];
    size_t out = 0U;
    const unsigned char *src = (const unsigned char *)r->name;
    while (*src != '\0' && out + 2U < sizeof(name_buf)) {
        unsigned char c = *src++;
        if (c == '"' || c == '\\') {
            name_buf[out++] = '\\';
            name_buf[out++] = (char)c;
        } else if (c < 0x20U || c == 0x7FU) {
            return -1;
        } else {
            name_buf[out++] = (char)c;
        }
    }
    if (*src != '\0') {
        return -1;
    }
    name_buf[out] = '\0';

    if (path == NULL) {
        return 0;
    }
    f = fopen(path, "ae");
    if (f == NULL) {
        return -1;
    }
    ops_per_sec = (r->ns_median > 0U) ? (1e9 / (double)r->ns_median) : 0.0;
    (void)fprintf(f,
                  "{\"name\":\"%s\",\"unit\":\"ns/op\",\"iterations\":%zu,"
                  "\"ns_per_op_min\":%lu,\"ns_per_op_median\":%lu,"
                  "\"ns_per_op_p95\":%lu,\"ns_per_op_max\":%lu,"
                  "\"ns_per_op_mean\":%.2f,\"ns_per_op_stddev\":%.2f,"
                  "\"ops_per_sec_median\":%.2f}\n",
                  name_buf, r->iterations, (unsigned long)r->ns_min, (unsigned long)r->ns_median,
                  (unsigned long)r->ns_p95, (unsigned long)r->ns_max, r->ns_mean, r->ns_stddev,
                  ops_per_sec);
    (void)fclose(f);
    return 0;
}

int bench_run_all(const struct bench_case *cases, size_t n_cases,
                  const struct bench_options *opts) {
    int failures = 0;
    const char *filter = (opts != NULL) ? opts->filter : NULL;

    for (size_t i = 0U; i < n_cases; ++i) {
        struct bench_result r;

        if (filter != NULL && filter[0] != '\0' && strstr(cases[i].name, filter) == NULL) {
            continue;
        }

        if (bench_run_case(&cases[i], opts, &r) != 0) {
            (void)fprintf(stderr, "bench: case %s failed\n", cases[i].name);
            failures++;
            continue;
        }

        if (opts != NULL && opts->text) {
            bench_emit_text(&r);
        }
        if (opts != NULL && opts->json_path != NULL) {
            if (bench_emit_json(opts->json_path, &r) != 0) {
                (void)fprintf(stderr, "bench: failed to write JSON for %s\n", cases[i].name);
                failures++;
            }
        }
    }
    return failures;
}
