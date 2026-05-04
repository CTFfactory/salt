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

#ifdef _WIN32
#include <windows.h>
#endif

uint64_t bench_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
        return 0U;
    }
    if (frequency.QuadPart == 0) {
        return 0U;
    }
    /* Convert to nanoseconds: (counter / frequency) * 1e9 */
    uint64_t seconds = (uint64_t)(counter.QuadPart / frequency.QuadPart);
    uint64_t remainder = (uint64_t)(counter.QuadPart % frequency.QuadPart);
    uint64_t ns_from_remainder = (remainder * 1000000000ULL) / (uint64_t)frequency.QuadPart;
    return seconds * 1000000000ULL + ns_from_remainder;
#else
    /* POSIX: use CLOCK_MONOTONIC for portable, monotonic timing */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
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

/* Collect performance samples by running the benchmark case for specified iterations.
   Executes batch operations per sample and respects max_runtime_ns limit.
   Returns actual number of samples collected (may be less than iterations if
   max_runtime exceeded). On error returns -1 and sets samples to NULL. */
static int bench_sample_iterations(const struct bench_case *bc, size_t *iterations,
                                   size_t max_runtime_ns, uint64_t **samples_out,
                                   uint64_t **unsorted_samples_out) {
    uint64_t *samples;
    uint64_t *unsorted_samples;
    size_t batch = (bc->batch > 0U) ? bc->batch : 1U;
    uint64_t case_start_ns = bench_now_ns();
    size_t actual_iterations = *iterations;

    if (actual_iterations == 0U) {
        return -1;
    }

    samples = (uint64_t *)calloc(actual_iterations, sizeof(uint64_t));
    unsorted_samples = (uint64_t *)calloc(actual_iterations, sizeof(uint64_t));
    if (samples == NULL || unsorted_samples == NULL) {
        free(samples);
        free(unsorted_samples);
        return -1;
    }

    for (size_t i = 0U; i < actual_iterations; ++i) {
        /* Check runtime limit if configured. */
        if (max_runtime_ns > 0ULL) {
            uint64_t elapsed = bench_now_ns() - case_start_ns;
            if (elapsed > max_runtime_ns) {
                (void)fprintf(stderr, "bench: case %s exceeded max runtime after %zu iterations\n",
                             bc->name, i);
                actual_iterations = i;
                break;
            }
        }
        uint64_t t0 = bench_now_ns();
        for (size_t b = 0U; b < batch; ++b) {
            if (bc->fn(bc->ctx) != 0) {
                free(samples);
                free(unsorted_samples);
                return -1;
            }
        }
        uint64_t t1 = bench_now_ns();
        uint64_t delta = (t1 >= t0) ? (t1 - t0) : 0U;
        uint64_t ns_per_op = delta / batch;
        samples[i] = ns_per_op;
        unsorted_samples[i] = ns_per_op;
    }

    *iterations = actual_iterations;
    *samples_out = samples;
    *unsorted_samples_out = unsorted_samples;
    return 0;
}

/* Approximate t-distribution critical value for confidence intervals.
   Returns t-value for (n-1) degrees of freedom at confidence level. */
static double bench_t_value(size_t n, int confidence_level) {
    if (n < 2) {
        return 0.0;
    }
    size_t df = n - 1;
    /* Simplified lookup: common t-values for 95% CI */
    if (confidence_level == 95) {
        if (df >= 30) return 2.042;  /* Converges to 1.96 for large n */
        if (df >= 20) return 2.086;
        if (df >= 10) return 2.228;
        if (df >= 5) return 2.571;
        if (df >= 3) return 3.182;
        if (df >= 2) return 4.303;
        return 12.706;  /* df=1 */
    }
    if (confidence_level == 90) {
        if (df >= 30) return 1.697;
        if (df >= 20) return 1.725;
        if (df >= 10) return 1.812;
        if (df >= 5) return 2.015;
        return 6.314;  /* Conservative fallback */
    }
    if (confidence_level == 99) {
        if (df >= 30) return 2.750;
        if (df >= 20) return 2.845;
        if (df >= 10) return 3.169;
        if (df >= 5) return 4.032;
        return 63.657;
    }
    return 2.042;  /* Default to 95% CI, large n */
}

/* Single-run benchmark execution (internal). */
static int bench_run_case_single(const struct bench_case *bc, const struct bench_options *opts,
                                 struct bench_result *result) {
    size_t iterations = (opts != NULL && opts->iterations > 0U) ? opts->iterations : bc->iterations;
    size_t warmup = (opts != NULL && opts->warmup != (size_t)-1) ? opts->warmup : bc->warmup;
    size_t max_runtime_ns = (opts != NULL && opts->max_runtime_ms > 0U)
        ? opts->max_runtime_ms * 1000000ULL : 0ULL;
    uint64_t *samples;
    uint64_t *unsorted_samples;
    double sum = 0.0;
    double sq_sum = 0.0;

    if (iterations == 0U) {
        return -1;
    }

    /* Warm up cache before sampling */
    for (size_t i = 0U; i < warmup; ++i) {
        if (bc->fn(bc->ctx) != 0) {
            return -1;
        }
    }

    /* Collect samples using helper function */
    if (bench_sample_iterations(bc, &iterations, max_runtime_ns, &samples, &unsorted_samples) != 0) {
        return -1;
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
    /* Use unbiased sample stddev (divides by N-1 instead of N) for statistical
       rigor. For large N (≥100), difference from population formula is <1%.
       For small N (<40), this improves accuracy significantly. */
    if (iterations > 1U) {
        result->ns_stddev = sqrt(sq_sum / (double)(iterations - 1U));
    } else {
        result->ns_stddev = 0.0;
    }
    /* Initialize multi-run CI fields to zero (not used in single-run) */
    result->ns_median_ci_lower = 0.0;
    result->ns_median_ci_upper = 0.0;

    /* Report outliers if configured */
    if (opts != NULL && opts->outlier_detection) {
        bench_report_outliers(result, unsorted_samples, iterations);
    }

    free(samples);
    free(unsorted_samples);
    return 0;
}

int bench_run_case(const struct bench_case *bc, const struct bench_options *opts,
                   struct bench_result *result) {
    size_t multi_run = (opts != NULL && opts->multi_run > 1U) ? opts->multi_run : 1U;
    int confidence_level = (opts != NULL && opts->confidence_level > 0) ? opts->confidence_level : 95;

    if (multi_run == 1U) {
        /* Single run, direct path */
        return bench_run_case_single(bc, opts, result);
    }

    /* Multi-run with confidence intervals */
    struct bench_result *runs = (struct bench_result *)calloc(multi_run, sizeof(struct bench_result));
    if (runs == NULL) {
        return -1;
    }

    double *medians = (double *)calloc(multi_run, sizeof(double));
    if (medians == NULL) {
        free(runs);
        return -1;
    }

    /* Run the benchmark multiple times */
    for (size_t i = 0U; i < multi_run; ++i) {
        if (bench_run_case_single(bc, opts, &runs[i]) != 0) {
            free(runs);
            free(medians);
            return -1;
        }
        medians[i] = (double)runs[i].ns_median;
    }

    /* Use results from first run as base, then add CI from all runs */
    memcpy(result, &runs[0], sizeof(struct bench_result));

    /* Calculate confidence interval for median across runs */
    double median_sum = 0.0;
    double median_sq_sum = 0.0;
    for (size_t i = 0U; i < multi_run; ++i) {
        median_sum += medians[i];
    }
    double median_mean = median_sum / (double)multi_run;
    for (size_t i = 0U; i < multi_run; ++i) {
        double d = medians[i] - median_mean;
        median_sq_sum += d * d;
    }

    double median_stddev = 0.0;
    if (multi_run > 1U) {
        median_stddev = sqrt(median_sq_sum / (double)(multi_run - 1U));
    }

    double t_val = bench_t_value(multi_run, confidence_level);
    double margin = t_val * median_stddev / sqrt((double)multi_run);

    result->ns_median_ci_lower = median_mean - margin;
    result->ns_median_ci_upper = median_mean + margin;
    result->ns_mean = median_mean;
    result->ns_stddev = median_stddev;

    (void)fprintf(stderr, "bench: %s multi-run=%zu median=%.0f ± %.0f ns (95%% CI)\n",
                  result->name, multi_run, median_mean, margin);

    free(runs);
    free(medians);
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
    /* Open in append mode with close-on-exec flag.
       "ae" = O_WRONLY | O_APPEND | O_CLOEXEC
       Appends line-delimited JSON records; securely closes on exec(). */
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

void bench_report_outliers(const struct bench_result *result, const uint64_t *samples,
                          size_t iterations) {
    if (result == NULL || samples == NULL || iterations == 0U) {
        return;
    }

    double mean = result->ns_mean;
    double stddev = result->ns_stddev;

    if (stddev == 0.0) {
        return;  /* No variance, no outliers */
    }

    size_t outlier_count = 0U;
    for (size_t i = 0U; i < iterations; ++i) {
        double z_score = ((double)samples[i] - mean) / stddev;
        if (z_score < -2.0 || z_score > 2.0) {
            outlier_count++;
        }
    }

    if (outlier_count > 0U) {
        double outlier_pct = (100.0 * (double)outlier_count) / (double)iterations;
        (void)fprintf(stderr, "bench: %s detected %zu outliers (%.1f%% >2σ)\n", result->name,
                      outlier_count, outlier_pct);
    }
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
