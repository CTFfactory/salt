/*
 * SPDX-License-Identifier: Unlicense
 *
 * salt CLI entrypoint and orchestration layer.
 *
 * This file intentionally keeps only:
 * - process entrypoint hardening (main)
 * - usage/version rendering
 * - salt_cli_run* orchestration
 *
 * Other helper logic lives in dedicated modules (src/cli_args.c, src/cli_input.c,
 * src/cli_prepare.c, src/cli_test_hooks.c, src/cli_state.c, src/cli_signal.c,
 * src/cli_stream.c, src/cli_parse.c, src/cli_output.c).
 */

#include "cli_args.h"
#include "cli_input.h"
#include "cli_output.h"
#include "cli_prepare.h"
#include "cli_signal.h"
#include "cli_state.h"
#include "cli_stream.h"
#include "salt.h"

#include <sodium.h>

#include <stdio.h>
#include <sys/resource.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

static const size_t SALT_CLI_STACK_WIPE_BYTES = 8192U;

static int print_usage(FILE *stream, const char *program, char *error, size_t error_size) {
    return salt_cli_write_stream(
        stream, error, error_size, "failed to write help output",
        "usage: %s [OPTIONS] [PLAINTEXT]\n"
        "\n"
        "Encrypt plaintext using libsodium sealed boxes for GitHub secrets.\n"
        "\n"
        "Options:\n"
        "  -k, --key VALUE         Public key input (base64, JSON key object, or '-')\n"
        "  -i, --key-id ID         Key ID override/inject for JSON output\n"
        "  -o, --output FORMAT     Output format: text|json (default: text)\n"
        "  -f, --key-format MODE   Key format: auto|base64|json (default: auto)\n"
        "  -h, --help              Show this help text and exit\n"
        "  -V, --version           Show version information and exit\n"
        "\n"
        "Input rules:\n"
        "  PLAINTEXT omitted or '-' reads plaintext from stdin.\n"
        "  --key - reads key input from stdin.\n"
        "  Key and plaintext cannot both be read from stdin in one invocation.\n"
        "\n"
        "Security:\n"
        "  Sensitive plaintext should be passed via stdin; positional arguments\n"
        "  are visible in process listings (ps, /proc/<pid>/cmdline).\n"
        "\n"
        "Examples:\n"
        "  %s -k '<base64-public-key>' 'hello'\n"
        "  %s -o json -k '{\"key_id\":\"123\",\"key\":\"<base64-public-key>\"}' 'hello'\n"
        "  echo 'hello' | %s -o json -k '{\"key_id\":\"123\",\"key\":\"<base64-public-key>\"}' -\n",
        program, program, program, program);
}

static int print_version(FILE *stream, char *error, size_t error_size) {
    return salt_cli_write_stream(stream, error, error_size, "failed to write version output",
                                 "salt %s\n", SALT_VERSION);
}

int salt_cli_run_with_streams(int argc, char **argv, FILE *in_stream, FILE *out_stream,
                              FILE *err_stream) {
    struct salt_cli_state state;
    int rc = SALT_ERR_INPUT;

    if (argc <= 0 || argv == NULL || in_stream == NULL || out_stream == NULL ||
        err_stream == NULL) {
        return SALT_ERR_INPUT;
    }

    salt_cli_state_init(&state);
    salt_cli_ensure_signal_handlers();
    salt_cli_reset_interrupted();

    rc = salt_cli_args_parse_arguments(argc, argv, out_stream, err_stream, &state, print_usage,
                                       print_version);
    if (rc != SALT_OK || state.show_help || state.show_version) {
        if ((state.show_help || state.show_version) && rc != SALT_OK) {
            (void)salt_cli_write_stream(err_stream, NULL, 0U, "failed to write diagnostic",
                                        "error: %s\n", state.error);
        }
        goto cleanup;
    }

    rc = salt_cli_input_load_key_input(in_stream, err_stream, &state);
    if (rc != SALT_OK) {
        goto cleanup;
    }

    rc = salt_cli_input_load_plaintext(in_stream, err_stream, &state);
    if (rc != SALT_OK) {
        goto cleanup;
    }

    rc = salt_cli_prepare_output(err_stream, &state);
    if (rc != SALT_OK) {
        goto cleanup;
    }

    rc = salt_cli_emit_output(out_stream, state.output_mode, state.output, state.effective_key_id,
                              state.error, sizeof(state.error));
    if (rc != SALT_OK) {
        if (salt_cli_write_stream(err_stream, NULL, 0U, "failed to write diagnostic", "error: %s\n",
                                  state.error) != SALT_OK) {
            rc = SALT_ERR_IO;
        }
    }

cleanup:
    salt_cli_state_cleanup(&state);
    /*
     * Wipe stack space used by callees (parsers, libsodium primitives,
     * stdio buffering) that may have spilled key fragments or plaintext into
     * stack frames now popped. The CLI's own fixed-size scratch buffers are
     * wiped via the sodium_munlock calls in salt_cli_state_cleanup().
     */
    sodium_stackzero(SALT_CLI_STACK_WIPE_BYTES);
    return rc;
}

int salt_cli_run(int argc, char **argv, FILE *out_stream, FILE *err_stream) {
    return salt_cli_run_with_streams(argc, argv, stdin, out_stream, err_stream);
}

#ifndef SALT_NO_MAIN
int main(int argc, char **argv) {
    /*
     * Disable core dumps and ptrace attach so plaintext, decoded keys, and
     * other transient secrets cannot be extracted from a core file or by an
     * attaching debugger sharing this UID. Both calls are best-effort: a
     * lower setrlimit may already have been imposed by the parent, and
     * PR_SET_DUMPABLE is Linux-specific. Library callers that link against
     * salt without using main() are responsible for their own hardening.
     */
    {
        struct rlimit no_core = {0, 0};
        (void)setrlimit(RLIMIT_CORE, &no_core);
    }
#ifdef PR_SET_DUMPABLE
    (void)prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
#endif
    return salt_cli_run(argc, argv, stdout, stderr);
}
#endif
