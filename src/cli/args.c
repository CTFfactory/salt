/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI argument parsing implementation.
 */

#include "args.h"

#include "stream.h"

#include <getopt.h>
#include <string.h>

/* GCOVR_EXCL_START */
/*
 * CERT: argv array has argc+1 elements; last element is NULL
 * (standard C convention: argv[0] through argv[argc-1], with argv[argc] = NULL).
 * This guarantees safe bounds checking when accessing argv[optind - 1] as long as
 * optind <= argc.
 */
static const char *salt_cli_invalid_option_token(int argc, char **argv) {
    if (optind > 0 && optind <= argc && argv[optind - 1] != NULL) {
        return argv[optind - 1];
    }
    return "<unknown>";
}
/* GCOVR_EXCL_STOP */

static size_t salt_cli_bounded_strlen(const char *value, size_t max_len) {
    size_t i;

    if (value == NULL) {
        return 0U;
    }
    for (i = 0U; i < max_len; ++i) {
        if (value[i] == '\0') {
            return i;
        }
    }
    return max_len;
}

static int salt_cli_handle_option(int argc, char **argv, FILE *err_stream,
                                  struct salt_cli_state *state, int opt) {
    switch (opt) {
    case 'k':
        state->key_option = optarg;
        return SALT_OK;
    case 'i':
        state->key_id_option = optarg;
        return SALT_OK;
    case 'o':
        if (strcmp(optarg, "text") == 0) { /* GCOVR_EXCL_BR_LINE */
            state->output_mode = SALT_CLI_OUTPUT_TEXT;
            return SALT_OK;
        }
        if (strcmp(optarg, "json") == 0) {
            state->output_mode = SALT_CLI_OUTPUT_JSON;
            return SALT_OK;
        }
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INPUT,
                                             "error: invalid output format '%s' "
                                             "(expected text|json)\n",
                                             optarg);
    case 'f':
        if (strcmp(optarg, "auto") == 0) { /* GCOVR_EXCL_BR_LINE */
            state->key_format = SALT_CLI_KEY_FORMAT_AUTO;
            return SALT_OK;
        }
        if (strcmp(optarg, "base64") == 0) {
            state->key_format = SALT_CLI_KEY_FORMAT_BASE64;
            return SALT_OK;
        }
        if (strcmp(optarg, "json") == 0) {
            state->key_format = SALT_CLI_KEY_FORMAT_JSON;
            return SALT_OK;
        }
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INPUT,
                                             "error: invalid key format '%s' "
                                             "(expected auto|base64|json)\n",
                                             optarg);
    case 'h':
        state->show_help = true;
        return SALT_OK;
    case 'V':
        state->show_version = true;
        return SALT_OK;
    case ':':
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INPUT,
                                             "error: option requires an argument: -%c\n", optopt);
    case '?':
    default:
        if (optopt != 0) {
            return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                                 SALT_ERR_INPUT, "error: invalid option: -%c\n",
                                                 optopt);
        }
        /*
         * glibc getopt_long sets optopt to 0 for unknown long options; surface
         * the offending token when available and otherwise fall back to a
         * stable placeholder.
         */
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INPUT, "error: invalid option: %s\n",
                                             salt_cli_invalid_option_token(argc, argv));
    }
}

int salt_cli_args_parse_arguments(int argc, char **argv, FILE *out_stream, FILE *err_stream,
                                  struct salt_cli_state *state,
                                  salt_cli_usage_writer_fn usage_writer,
                                  salt_cli_version_writer_fn version_writer) {
    static const struct option long_options[] = {{"key", required_argument, NULL, 'k'},
                                                 {"key-id", required_argument, NULL, 'i'},
                                                 {"output", required_argument, NULL, 'o'},
                                                 {"key-format", required_argument, NULL, 'f'},
                                                 {"help", no_argument, NULL, 'h'},
                                                 {"version", no_argument, NULL, 'V'},
                                                 {NULL, 0, NULL, 0}};
    int opt;
    int option_index = 0;

    opterr = 0;
    /*
     * Use optind = 0 (glibc) to fully reinitialize getopt's internal static
     * state (notably __nextchar). Setting optind = 1 only changes the next
     * argv index and lets stale pointers from a prior invocation survive,
     * which has been observed under repeated in-process invocation (fuzzing
     * or library reuse) to read freed stack memory.
     */
    optind = 0;

    while ((opt = getopt_long(argc, argv, ":k:i:o:f:hV", long_options, &option_index)) != -1) {
        int rc = salt_cli_handle_option(argc, argv, err_stream, state, opt);
        if (rc != SALT_OK) {
            return rc;
        }
    }

    if (state->show_help) {
        return usage_writer(out_stream, argv[0], state->error, sizeof(state->error));
    }
    if (state->show_version) {
        return version_writer(out_stream, state->error, sizeof(state->error));
    }

    if (state->key_option == NULL) {
        int rc = salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                               SALT_ERR_INPUT, "error: --key/-k is required\n");
        if (rc == SALT_ERR_IO) { /* GCOVR_EXCL_BR_LINE */
            return rc;
        }
        rc = usage_writer(err_stream, argv[0], state->error, sizeof(state->error));
        return (rc == SALT_OK) ? SALT_ERR_INPUT : rc;
    }
    if (state->key_option[0] == '\0') {
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INPUT,
                                             "error: --key/-k value must not be empty\n");
    }
    if (state->key_id_option != NULL && state->key_id_option[0] != '\0') {
        size_t key_id_len =
            salt_cli_bounded_strlen(state->key_id_option, (size_t)SALT_MAX_KEY_ID_LEN + 1U);
        if (key_id_len > (size_t)SALT_MAX_KEY_ID_LEN) {
            return salt_cli_report_stream_status(
                err_stream, state->error, sizeof(state->error), SALT_ERR_INPUT,
                "error: --key-id/-i exceeds maximum length (%u bytes)\n", SALT_MAX_KEY_ID_LEN);
        }
    }

    if (optind < argc) {
        state->plaintext_arg = argv[optind++];
    }
    if (optind < argc) {
        int rc =
            salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                          SALT_ERR_INPUT, "error: too many positional arguments\n");
        if (rc == SALT_ERR_IO) {
            return rc;
        }
        rc = usage_writer(err_stream, argv[0], state->error, sizeof(state->error));
        return (rc == SALT_OK) ? SALT_ERR_INPUT : rc;
    }

    state->key_from_stdin = (strcmp(state->key_option, "-") == 0);
    state->plaintext_from_stdin =
        (state->plaintext_arg == NULL || strcmp(state->plaintext_arg, "-") == 0);
    if (state->key_from_stdin && state->plaintext_from_stdin) {
        return salt_cli_report_stream_status(
            err_stream, state->error, sizeof(state->error), SALT_ERR_INPUT,
            "error: key and plaintext cannot both be read from stdin\n");
    }

    return SALT_OK;
}
