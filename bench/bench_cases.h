/*
 * SPDX-License-Identifier: Unlicense
 *
 * Registration entry points for individual benchmark suites.
 *
 * Each register function fills up to `cap` entries in `out` and returns the
 * number of cases written, or 0 on initialization failure.
 */

#ifndef SALT_BENCH_CASES_H
#define SALT_BENCH_CASES_H

#include "bench_common.h"

#include <stddef.h>

size_t bench_seal_register(struct bench_case *out, size_t cap);
void bench_seal_cleanup(void);

size_t bench_base64_register(struct bench_case *out, size_t cap);
void bench_base64_cleanup(void);

size_t bench_parse_register(struct bench_case *out, size_t cap);
void bench_parse_cleanup(void);

size_t bench_output_register(struct bench_case *out, size_t cap);
void bench_output_cleanup(void);

#endif /* SALT_BENCH_CASES_H */
