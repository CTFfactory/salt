/*
 * SPDX-License-Identifier: Unlicense
 *
 * Aggregator entry point for the salt benchmark suite.
 */

#define _POSIX_C_SOURCE 200809L

#include "bench_common.h"
#include "bench_cases.h"

#include <sodium.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#define BENCH_MAX_CASES 32U

static void usage(FILE *s, const char *prog) {
    (void)fprintf(s,
                  "usage: %s [--text] [--json PATH] [--filter SUBSTR]\n"
                  "          [--iterations N] [--warmup N] [--max-runtime MS]\n"
                  "          [--outlier-detection]\n"
                  "          [--multi-run N] [--confidence-level PCT]\n",
                  prog);
}

int main(int argc, char **argv) {
    struct bench_case cases[BENCH_MAX_CASES] = {0};
    size_t n = 0U;
    struct bench_options opts;
    int rc;

    opts.filter = NULL;
    opts.json_path = NULL;
    opts.text = 1;
    opts.iterations = 0U;
    opts.warmup = (size_t)-1;
    opts.max_runtime_ms = 0U;
    opts.outlier_detection = 0;
    opts.multi_run = 0U;
    opts.confidence_level = 95;

    static const struct option longopts[] = {
        {"text", no_argument, NULL, 't'},
        {"json", required_argument, NULL, 'j'},
        {"filter", required_argument, NULL, 'f'},
        {"iterations", required_argument, NULL, 'i'},
        {"warmup", required_argument, NULL, 'w'},
        {"max-runtime", required_argument, NULL, 'm'},
        {"outlier-detection", no_argument, NULL, 'o'},
        {"multi-run", required_argument, NULL, 'r'},
        {"confidence-level", required_argument, NULL, 'c'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    int c;
    while ((c = getopt_long(argc, argv, "tj:f:i:w:m:or:c:h", longopts, NULL)) != -1) {
        switch (c) {
        case 't':
            opts.text = 1;
            break;
        case 'j':
            opts.json_path = optarg;
            break;
        case 'f':
            opts.filter = optarg;
            break;
        case 'i':
            if (bench_parse_size_arg(optarg, "iterations", "bench", &opts.iterations) != 0) {
                return 2;
            }
            break;
        case 'w':
            if (bench_parse_size_arg(optarg, "warmup", "bench", &opts.warmup) != 0) {
                return 2;
            }
            break;
        case 'm':
            if (bench_parse_size_arg(optarg, "max-runtime", "bench", &opts.max_runtime_ms) != 0) {
                return 2;
            }
            break;
        case 'o':
            opts.outlier_detection = 1;
            break;
        case 'r':
            if (bench_parse_size_arg(optarg, "multi-run", "bench", &opts.multi_run) != 0) {
                return 2;
            }
            break;
        case 'c': {
            char *end = NULL;
            unsigned long parsed;
            errno = 0;
            parsed = strtoul(optarg, &end, 10);
            if (errno != 0 || end == optarg || *end != '\0' || parsed > 99) {
                (void)fprintf(stderr, "bench: invalid confidence level: %s\n", optarg);
                return 2;
            }
            opts.confidence_level = (int)parsed;
            break;
        }
        case 'h':
            usage(stdout, argv[0]);
            return 0;
        default:
            usage(stderr, argv[0]);
            return 2;
        }
    }

    if (sodium_init() < 0) {
        (void)fprintf(stderr, "bench: sodium_init failed\n");
        return 1;
    }

    /* Truncate JSON output file so each run produces a clean record set. */
    if (opts.json_path != NULL) {
        int fd = open(opts.json_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                      S_IRUSR | S_IWUSR);
        FILE *f;
        if (fd < 0) {
            (void)fprintf(stderr, "bench: cannot open %s for writing\n", opts.json_path);
            return 1;
        }
        f = fdopen(fd, "w");
        if (f == NULL) {
            (void)close(fd);
            (void)fprintf(stderr, "bench: cannot open %s for writing\n", opts.json_path);
            return 1;
        }
        (void)fclose(f);
    }

    {
        size_t added;
        added = bench_seal_register(&cases[n], BENCH_MAX_CASES - n);
        if (added == 0U || added > BENCH_MAX_CASES - n) {
            return 1;
        }
        n += added;
        added = bench_base64_register(&cases[n], BENCH_MAX_CASES - n);
        if (added == 0U || added > BENCH_MAX_CASES - n) {
            return 1;
        }
        n += added;
        added = bench_parse_register(&cases[n], BENCH_MAX_CASES - n);
        if (added == 0U || added > BENCH_MAX_CASES - n) {
            return 1;
        }
        n += added;
        added = bench_output_register(&cases[n], BENCH_MAX_CASES - n);
        if (added == 0U || added > BENCH_MAX_CASES - n) {
            return 1;
        }
        n += added;
    }

    rc = bench_run_all(cases, n, &opts);

    bench_seal_cleanup();
    bench_base64_cleanup();
    bench_parse_cleanup();
    bench_output_cleanup();

    return (rc == 0) ? 0 : 1;
}
