/*
 * SPDX-License-Identifier: Unlicense
 *
 * End-to-end CLI cold-start benchmarks.
 *
 * Spawns ./bin/salt repeatedly across several representative CLI flows and
 * times each invocation with CLOCK_MONOTONIC.
 *
 * These measurements intentionally include fork/exec/wait, dynamic linking,
 * and scheduler noise. Treat them as end-to-end cold-start integration checks,
 * not as a micro-optimization benchmark.
 */

#define _POSIX_C_SOURCE 200809L

#include "bench_common.h"
#include "salt.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define SPAWN_DEFAULT_ITERATIONS 100U
#define SPAWN_DEFAULT_WARMUP 5U
#define SPAWN_STDIN_ITERATIONS 40U
#define SPAWN_STDIN_WARMUP 3U
#define SPAWN_KEY_B64 "rQB7XCRJP1Z+jY4mN1pK5tWvE3Gd0aHfXkBSYr9hzVE="
#define SPAWN_KEY_JSON "{\"key\":\"" SPAWN_KEY_B64 "\",\"key_id\":\"bench-key-123\"}"
#define SPAWN_CASE_COUNT 6U

struct spawn_ctx {
    const char *binary;
    const char *key_arg;
    const char *output_format;
    const char *plaintext_arg;
    const char *stdin_data;
    size_t stdin_len;
};

static char g_large_plaintext[SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U];

static int write_all(int fd, const char *buf, size_t len) {
    size_t written = 0U;

    while (written < len) {
        ssize_t rc = write(fd, buf + written, len - written);
        if (rc > 0) {
            written += (size_t)rc;
            continue;
        }
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        return -1;
    }
    return 0;
}

static int wait_child(pid_t pid, int *status) {
    for (;;) {
        pid_t rc = waitpid(pid, status, 0);
        if (rc == pid) {
            return 0;
        }
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        return -1;
    }
}

static void fill_large_plaintext(void) {
    (void)memset(g_large_plaintext, 'P', SALT_GITHUB_SECRET_MAX_VALUE_LEN);
    g_large_plaintext[SALT_GITHUB_SECRET_MAX_VALUE_LEN] = '\0';
}

static int spawn_run(void *vctx) {
    struct spawn_ctx *c = (struct spawn_ctx *)vctx;
    pid_t pid;
    int devnull_in = -1;
    int devnull_out = -1;
    int stdin_pipe[2] = {-1, -1};
    int status = 0;

    if (c->stdin_data != NULL) {
        if (pipe(stdin_pipe) != 0) {
            return -1;
        }
    } else {
        devnull_in = open("/dev/null", O_RDONLY);
        if (devnull_in < 0) {
            return -1;
        }
    }

    devnull_out = open("/dev/null", O_WRONLY);
    if (devnull_out < 0) {
        if (stdin_pipe[0] >= 0) {
            (void)close(stdin_pipe[0]);
            (void)close(stdin_pipe[1]);
        }
        if (devnull_in >= 0) {
            (void)close(devnull_in);
        }
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        if (stdin_pipe[0] >= 0) {
            (void)close(stdin_pipe[0]);
            (void)close(stdin_pipe[1]);
        }
        if (devnull_in >= 0) {
            (void)close(devnull_in);
        }
        (void)close(devnull_out);
        return -1;
    }
    if (pid == 0) {
        char *argv[8];
        size_t argc = 0U;

        if (c->stdin_data != NULL) {
            (void)close(stdin_pipe[1]);
            (void)dup2(stdin_pipe[0], STDIN_FILENO);
            (void)close(stdin_pipe[0]);
        } else {
            (void)dup2(devnull_in, STDIN_FILENO);
            (void)close(devnull_in);
        }
        (void)dup2(devnull_out, STDOUT_FILENO);
        (void)dup2(devnull_out, STDERR_FILENO);
        (void)close(devnull_out);

        argv[argc++] = (char *)c->binary;
        argv[argc++] = (char *)"-k";
        argv[argc++] = (char *)c->key_arg;
        if (c->output_format != NULL) {
            argv[argc++] = (char *)"-o";
            argv[argc++] = (char *)c->output_format;
        }
        argv[argc++] = (char *)c->plaintext_arg;
        argv[argc] = NULL;

        execve(c->binary, argv, environ);
        _exit(127);
    }

    if (stdin_pipe[0] >= 0) {
        int write_rc;

        (void)close(stdin_pipe[0]);
        write_rc = write_all(stdin_pipe[1], c->stdin_data, c->stdin_len);
        (void)close(stdin_pipe[1]);
        if (write_rc != 0) {
            (void)close(devnull_out);
            if (wait_child(pid, &status) != 0) {
                return -1;
            }
            return -1;
        }
    } else if (devnull_in >= 0) {
        (void)close(devnull_in);
    }
    (void)close(devnull_out);

    if (wait_child(pid, &status) != 0) {
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return -1;
    }
    return 0;
}

static void usage(FILE *s, const char *prog) {
    (void)fprintf(s,
                  "usage: %s [--binary PATH] [--json PATH] [--filter SUBSTR]\n"
                  "          [--iterations N] [--warmup N] [--text]\n",
                  prog);
}

int main(int argc, char **argv) {
    struct spawn_ctx cases_ctx[SPAWN_CASE_COUNT];
    struct bench_case cases[SPAWN_CASE_COUNT];
    struct bench_options opts = {NULL, NULL, 1, 0U, (size_t)-1};
    static const struct option longopts[] = {
        {"binary", required_argument, NULL, 'b'}, {"json", required_argument, NULL, 'j'},
        {"filter", required_argument, NULL, 'f'}, {"iterations", required_argument, NULL, 'i'},
        {"warmup", required_argument, NULL, 'w'}, {"text", no_argument, NULL, 't'},
        {"help", no_argument, NULL, 'h'},         {NULL, 0, NULL, 0},
    };
    const char *binary = "./bin/salt";
    int c;

    fill_large_plaintext();

    while ((c = getopt_long(argc, argv, "b:j:f:i:w:th", longopts, NULL)) != -1) {
        switch (c) {
        case 'b':
            binary = optarg;
            break;
        case 'j':
            opts.json_path = optarg;
            break;
        case 'f':
            opts.filter = optarg;
            break;
        case 'i':
            if (bench_parse_size_arg(optarg, "iterations", "bench-spawn", &opts.iterations) != 0) {
                return 2;
            }
            break;
        case 'w':
            if (bench_parse_size_arg(optarg, "warmup", "bench-spawn", &opts.warmup) != 0) {
                return 2;
            }
            break;
        case 't':
            opts.text = 1;
            break;
        case 'h':
            usage(stdout, argv[0]);
            return 0;
        default:
            usage(stderr, argv[0]);
            return 2;
        }
    }

    cases_ctx[0] = (struct spawn_ctx){binary, SPAWN_KEY_B64, NULL, "hello", NULL, 0U};
    cases_ctx[1] = (struct spawn_ctx){binary, SPAWN_KEY_JSON, "json", "hello", NULL, 0U};
    cases_ctx[2] = (struct spawn_ctx){binary, SPAWN_KEY_B64,     NULL,
                                      "-",    g_large_plaintext, SALT_GITHUB_SECRET_MAX_VALUE_LEN};
    cases_ctx[3] = (struct spawn_ctx){binary, SPAWN_KEY_JSON,    "json",
                                      "-",    g_large_plaintext, SALT_GITHUB_SECRET_MAX_VALUE_LEN};
    cases_ctx[4] =
        (struct spawn_ctx){binary, SPAWN_KEY_B64, NULL, g_large_plaintext, NULL, 0U};
    cases_ctx[5] = (struct spawn_ctx){binary, "-", "json", "hello", SPAWN_KEY_JSON,
                                      sizeof(SPAWN_KEY_JSON) - 1U};

    cases[0] = (struct bench_case){
        "cli_cold_start_text_short", spawn_run, &cases_ctx[0], SPAWN_DEFAULT_ITERATIONS,
        SPAWN_DEFAULT_WARMUP,        0U};
    cases[1] = (struct bench_case){
        "cli_cold_start_json_short", spawn_run, &cases_ctx[1], SPAWN_DEFAULT_ITERATIONS,
        SPAWN_DEFAULT_WARMUP,        0U};
    cases[2] = (struct bench_case){"cli_cold_start_text_stdin_48KiB",
                                   spawn_run,
                                   &cases_ctx[2],
                                   SPAWN_STDIN_ITERATIONS,
                                   SPAWN_STDIN_WARMUP,
                                   0U};
    cases[3] = (struct bench_case){"cli_cold_start_json_stdin_48KiB",
                                   spawn_run,
                                   &cases_ctx[3],
                                   SPAWN_STDIN_ITERATIONS,
                                   SPAWN_STDIN_WARMUP,
                                   0U};
    cases[4] = (struct bench_case){"cli_cold_start_text_argv_48KiB",
                                   spawn_run,
                                   &cases_ctx[4],
                                   SPAWN_STDIN_ITERATIONS,
                                   SPAWN_STDIN_WARMUP,
                                   0U};
    cases[5] = (struct bench_case){"cli_cold_start_json_key_stdin_short",
                                   spawn_run,
                                   &cases_ctx[5],
                                   SPAWN_DEFAULT_ITERATIONS,
                                   SPAWN_DEFAULT_WARMUP,
                                   0U};

    return (bench_run_all(cases, SPAWN_CASE_COUNT, &opts) == 0) ? 0 : 1;
}
