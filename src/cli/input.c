/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI stream and input ingestion implementation.
 */

#include "input.h"

#include "signals.h"
#include "stream.h"
#include "test_hooks.h"

#include <sodium.h>
#include <stdint.h>
#include <string.h>

static size_t cli_bounded_strlen(const char *value, size_t max_len) {
    const char *nul = (const char *)memchr(value, '\0', max_len);
    /* memchr returns ptr >= value (or NULL), so subtraction (nul - value) is safe;
     * result fits in size_t since search was bounded to max_len bytes. */
    return (nul != NULL) ? (size_t)(nul - value) : max_len;
}

static void cli_secure_release_buffer(char *buffer, size_t len) {
    if (buffer == NULL) {
        return;
    }
    if (sodium_munlock(buffer, len) != 0) {
        sodium_memzero(buffer, len);
    }
    salt_cli_free_impl(buffer);
}

int cli_input_read_stream_limited(FILE *in_stream, size_t max_len, char **output,
                                  size_t *output_len, char *error, size_t error_size) {
    char *buffer;
    size_t bytes_read;
    int next_char;

    if (in_stream == NULL || output == NULL || output_len == NULL) { /* GCOVR_EXCL_BR_LINE */
        if (error != NULL && error_size > 0U) {
            (void)salt_cli_set_error(error, error_size, "invalid stream arguments", SALT_ERR_INPUT);
        }
        return SALT_ERR_INPUT;
    }

    if (max_len > (SIZE_MAX - 1U)) { /* GCOVR_EXCL_BR_LINE */
        return salt_cli_set_error(error, error_size, "input size overflow", SALT_ERR_INTERNAL);
    }

    if (salt_cli_check_interrupted(error, error_size) != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
        return SALT_ERR_SIGNAL;
    }

    /* Reserve one extra byte for NUL so downstream parsing is always bounded. */
    buffer = (char *)salt_cli_alloc_impl(max_len + 1U);
    if (buffer == NULL) {
        if (error != NULL && error_size > 0U) {
            (void)salt_cli_set_error(error, error_size, "failed to allocate input buffer",
                                     SALT_ERR_INTERNAL);
        }
        return SALT_ERR_INTERNAL;
    }
    /*
     * Best-effort lock to keep this stdin buffer (which may carry plaintext
     * or a public key) from being swapped to disk.
     *
     * This buffer is sensitive: it may contain plaintext user input or a public key.
     * Locking prevents the OS from paging it to swap storage, reducing forensic
     * recovery risk. However, POSIX mlock() can fail:
     * - RLIMIT_MEMLOCK too low (common in containers and sanitizer/valgrind runs)
     * - Insufficient physical memory
     * - Platform-specific constraints
     *
     * Failure is non-fatal because:
     * - The buffer is still wiped via sodium_munlock() in cleanup paths below.
     * - MEM03-C compliance (sensitive-buffer zeroing) is preserved regardless of
     *   mlock status.
     * - Logging or aborting on mlock failure would reduce usability without gaining
     *   security (zeroing still happens).
     */
    (void)sodium_mlock(buffer, max_len + 1U);

    bytes_read = salt_cli_fread_impl(buffer, 1U, max_len, in_stream);
    if (salt_cli_check_interrupted(error, error_size) != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
        cli_secure_release_buffer(buffer, max_len + 1U);
        return SALT_ERR_SIGNAL;
    }
    if (salt_cli_ferror_impl(in_stream) != 0) {
        (void)salt_cli_set_error(error, error_size, "failed to read from stdin", SALT_ERR_IO);
        cli_secure_release_buffer(buffer, max_len + 1U);
        return SALT_ERR_IO;
    }

    /* Detect oversized stdin by probing one extra byte without consuming it. */
    if (bytes_read == max_len) {
        next_char = salt_cli_fgetc_impl(in_stream);
        if (next_char == EOF) {
            if (salt_cli_ferror_impl(in_stream) != 0) {
                (void)salt_cli_set_error(error, error_size, "failed to read from stdin",
                                         SALT_ERR_IO);
                cli_secure_release_buffer(buffer, max_len + 1U);
                return SALT_ERR_IO;
            }
        } else {
            if (salt_cli_ungetc_impl(next_char, in_stream) == EOF) {
                (void)salt_cli_set_error(error, error_size,
                                         "failed to restore stdin after size probe", SALT_ERR_IO);
                cli_secure_release_buffer(buffer, max_len + 1U);
                return SALT_ERR_IO;
            }
            if (error != NULL && error_size > 0U) {
                (void)salt_cli_set_error(error, error_size, "stdin input exceeds maximum length",
                                         SALT_ERR_INPUT);
            }
            cli_secure_release_buffer(buffer, max_len + 1U);
            return SALT_ERR_INPUT;
        }
    }

    buffer[bytes_read] = '\0';
    *output = buffer;
    *output_len = bytes_read;
    return SALT_OK;
}

static int salt_cli_report_state_error(FILE *err_stream, const struct salt_cli_state *state,
                                       int rc) {
    return salt_cli_report_stream_status(err_stream, NULL, 0U, rc, "error: %s\n", state->error);
}

static int salt_cli_load_plaintext_from_argv(FILE *err_stream, struct salt_cli_state *state) {
    size_t arg_len =
        cli_bounded_strlen(state->plaintext_arg, SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U);
    size_t i;

    if (arg_len > SALT_GITHUB_SECRET_MAX_VALUE_LEN) {
        return salt_cli_report_stream_status(
            err_stream, state->error, sizeof(state->error), SALT_ERR_INPUT,
            "error: plaintext exceeds maximum length (%zu bytes; GitHub Actions caps secret "
            "values at 48 KB)\n",
            (size_t)SALT_GITHUB_SECRET_MAX_VALUE_LEN);
    }

    /* CWE-190: Integer overflow guard before size arithmetic (INT30-C) */
    if (arg_len > SIZE_MAX - 1U) {
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INPUT, "error: plaintext size overflow\n");
    }

    state->argv_plaintext_cap = arg_len + 1U;
    state->argv_plaintext_copy = (char *)salt_cli_alloc_impl(state->argv_plaintext_cap);
    if (state->argv_plaintext_copy == NULL) {
        return salt_cli_report_stream_status(
            err_stream, state->error, sizeof(state->error), SALT_ERR_INTERNAL,
            "internal error: failed to allocate plaintext buffer\n");
    }
    /*
     * Best-effort lock of the plaintext copy buffer to minimize swap exposure.
     *
     * Command-line arguments are visible to local processes via /proc/[pid]/cmdline
     * on Linux and similar on other OSes. We copy argv[N] (plaintext) to a mlock'd
     * buffer, then zero the original argv slot to prevent accidental disclosure.
     * Locking this copy prevents it from being paged to swap, reducing post-crash
     * forensic recovery risk.
     *
     * Failure is non-fatal: sodium_munlock() still wipes the buffer in cleanup,
     * preserving MEM03-C compliance (sensitive-buffer zeroing).
     *
     * See INT30-C overflow guard above: arg_len + 1U is always safe.
     */
    (void)sodium_mlock(state->argv_plaintext_copy, state->argv_plaintext_cap);
    for (i = 0U; i < arg_len; ++i) {
        state->argv_plaintext_copy[i] = state->plaintext_arg[i];
    }
    state->argv_plaintext_copy[arg_len] = '\0';
#ifndef SALT_NO_MAIN
    sodium_memzero((char *)(uintptr_t)state->plaintext_arg, arg_len);
#endif
    state->message = (const unsigned char *)state->argv_plaintext_copy;
    state->message_len = arg_len;
    return SALT_OK;
}

int salt_cli_input_load_key_input(FILE *in_stream, FILE *err_stream, struct salt_cli_state *state) {
    if (state->key_from_stdin) {
        size_t key_len = 0U;
        int rc = cli_input_read_stream_limited(in_stream, SALT_MAX_KEY_INPUT_LEN, &state->stdin_key,
                                               &key_len, state->error, sizeof(state->error));
        if (rc != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
            return salt_cli_report_state_error(err_stream, state, rc);
        }
        salt_cli_trim_whitespace_inplace(state->stdin_key, key_len + 1U);
        if (state->stdin_key[0] == '\0') {
            return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                                 SALT_ERR_INPUT,
                                                 "error: key input from stdin is empty\n");
        }
        state->key_option = state->stdin_key;
    }

    {
        size_t key_copy_len = cli_bounded_strlen(state->key_option, SALT_MAX_KEY_INPUT_LEN + 1U);
        size_t i;
        if (key_copy_len > SALT_MAX_KEY_INPUT_LEN) {
            return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                                 SALT_ERR_INPUT,
                                                 "error: key input exceeds maximum length "
                                                 "(%zu bytes)\n",
                                                 (size_t)SALT_MAX_KEY_INPUT_LEN);
        }
        state->trimmed_key_input = (char *)salt_cli_alloc_impl(key_copy_len + 1U);
        if (state->trimmed_key_input == NULL) {
            return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                                 SALT_ERR_INTERNAL,
                                                 "internal error: failed to allocate key buffer\n");
        }
        state->trimmed_key_input_cap = key_copy_len + 1U;
        /*
         * Best-effort lock of the key input buffer to minimize swap exposure.
         *
         * This buffer holds the key input (which may be base64-encoded or JSON) before
         * it is parsed and decoded. Although not the plaintext itself, it is sensitive
         * material that should remain in-core. Locking prevents it from being paged to
         * swap storage.
         *
         * Failure is non-fatal: sodium_munlock() still wipes the buffer in cleanup,
         * preserving MEM03-C compliance (sensitive-buffer zeroing). Aborting on
         * mlock failure would reduce usability without gaining security.
         */
        (void)sodium_mlock(state->trimmed_key_input, state->trimmed_key_input_cap);
        for (i = 0U; i < key_copy_len; ++i) {
            state->trimmed_key_input[i] = state->key_option[i];
        }
        state->trimmed_key_input[key_copy_len] = '\0';
    }

    salt_cli_trim_whitespace_inplace(state->trimmed_key_input, state->trimmed_key_input_cap);
    {
        int rc = salt_cli_parse_key_input(
            state->trimmed_key_input, state->key_format, state->parsed_key_b64,
            sizeof(state->parsed_key_b64), state->parsed_key_id, sizeof(state->parsed_key_id),
            &state->parsed_has_key_id, state->error, sizeof(state->error));
        if (rc != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
            return salt_cli_report_state_error(err_stream, state, rc);
        }
    }

    {
        int rc = salt_cli_check_interrupted(state->error, sizeof(state->error));
        if (rc != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
            return salt_cli_report_state_error(err_stream, state, rc);
        }
    }

    return SALT_OK;
}

int salt_cli_input_load_plaintext(FILE *in_stream, FILE *err_stream, struct salt_cli_state *state) {
    if (state->plaintext_from_stdin) {
        /*
         * GitHub Actions rejects secret values larger than 48 KB; bound the
         * stdin read at the same cap so the CLI never produces ciphertext
         * that the Secrets API would refuse.
         */
        int rc = cli_input_read_stream_limited(in_stream, SALT_GITHUB_SECRET_MAX_VALUE_LEN,
                                               &state->stdin_plaintext, &state->message_len,
                                               state->error, sizeof(state->error));
        if (rc != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
            return salt_cli_report_state_error(err_stream, state, rc);
        }
        state->message = (const unsigned char *)state->stdin_plaintext;
    } else {
        int rc = salt_cli_load_plaintext_from_argv(err_stream, state);
        if (rc != SALT_OK) {
            return rc;
        }
    }

    {
        int rc = salt_cli_check_interrupted(state->error, sizeof(state->error));
        if (rc != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
            return salt_cli_report_state_error(err_stream, state, rc);
        }
    }

    if (state->message_len == 0U) {
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INPUT,
                                             "error: plaintext must not be empty\n");
    }

    return SALT_OK;
}
