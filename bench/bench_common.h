/*
 * SPDX-License-Identifier: Unlicense
 *
 * Minimal benchmarking harness for the salt project.
 *
 * Provides monotonic-clock timing, percentile statistics, and
 * line-delimited JSON / human text emitters so benchmark cases can be
 * registered uniformly and compared against a committed baseline.
 */

#ifndef SALT_BENCH_COMMON_H
#define SALT_BENCH_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef int (*bench_fn)(void *ctx);

struct bench_case {
    const char *name;
    bench_fn fn;
    void *ctx;
    size_t iterations;
    size_t warmup;
    size_t batch; /* function invocations per timed sample; 0 -> 1 */
};

struct bench_result {
    const char *name;
    size_t iterations;
    uint64_t ns_min;
    uint64_t ns_median;
    uint64_t ns_p95;
    uint64_t ns_max;
    double ns_mean;
    double ns_stddev;
};

struct bench_options {
    const char *filter;    /* substring filter, NULL or "" matches all */
    const char *json_path; /* if non-NULL, append line-delimited JSON */
    int text;              /* nonzero = human text output to stdout */
    size_t iterations;     /* 0 = use case default */
    size_t warmup;         /* SIZE_MAX = use case default */
};

uint64_t bench_now_ns(void);

/* Parse a non-empty decimal size argument for benchmark CLIs. */
int bench_parse_size_arg(const char *value, const char *flag, const char *tool_name, size_t *out);

/*
 * Run a single case honoring overrides in opts (iterations/warmup).
 * Returns 0 on success, nonzero if the case function returned nonzero.
 * On success, populates *result.
 */
int bench_run_case(const struct bench_case *bc, const struct bench_options *opts,
                   struct bench_result *result);

/* Emitters: text to stdout, JSON appended (line-delimited) to json_path. */
void bench_emit_text(const struct bench_result *result);
int bench_emit_json(const char *path, const struct bench_result *result);

/*
 * Run a list of cases through the harness, applying filter and emitters
 * as configured by opts. Returns the number of failed cases (0 = ok).
 */
int bench_run_all(const struct bench_case *cases, size_t n_cases, const struct bench_options *opts);

#endif /* SALT_BENCH_COMMON_H */
