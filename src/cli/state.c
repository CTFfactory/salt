/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI state lifecycle implementation.
 *
 * This file implements initialization and cleanup for the consolidated
 * CLI state structure, ensuring all sensitive buffers are locked in
 * resident memory and scrubbed on release.
 */

#include "state.h"
#include "test_hooks.h"

#include <sodium.h>
#include <stdlib.h>

static void salt_cli_secure_unlock_region(void *ptr, size_t len) {
    if (ptr == NULL || len == 0U) {
        return;
    }
    if (sodium_munlock(ptr, len) != 0) {
        sodium_memzero(ptr, len);
    }
}

static void salt_cli_secure_free_region(void *ptr, size_t len) {
    if (ptr == NULL) {
        return;
    }
    salt_cli_secure_unlock_region(ptr, len);
    salt_cli_free_impl(ptr);
}

void salt_cli_state_init(struct salt_cli_state *state) {
    if (state == NULL) { /* GCOVR_EXCL_BR_LINE */
        return;          /* GCOVR_EXCL_BR_LINE */
    }

    *state = (struct salt_cli_state){0};
    state->output_mode = SALT_CLI_OUTPUT_TEXT;
    state->key_format = SALT_CLI_KEY_FORMAT_AUTO;

    /*
     * Lock fixed-size key buffers and the error scratch in resident memory so
     * decoded key material and deterministic diagnostics stay out of swap.
     *
     * POSIX mlock() can fail due to resource constraints (RLIMIT_MEMLOCK, physical
     * memory availability, etc.). We cast results to void to indicate these failures
     * are non-fatal:
     * - Failure typically means RLIMIT_MEMLOCK is too low (common in containers,
     *   sanitizer, and valgrind runs).
     * - Cleanup via sodium_munlock() still wipes memory even if mlock failed.
     * - The locked state is best-effort hardening; sensitive data is still scrubbed
     *   at cleanup time (MEM03-C compliance).
     *
     * Best practices for embedders:
     * - Increase ulimit -l (RLIMIT_MEMLOCK) before calling salt_cli_state_init() if
     *   mlock failures must be avoided in production environments.
     * - Monitor logs for mlock failures in sanitizer/test runs; they do not indicate
     *   security issues (cleanup still wipes memory).
     */
    (void)sodium_mlock(state->parsed_key_b64, sizeof(state->parsed_key_b64));
    (void)sodium_mlock(state->parsed_key_id, sizeof(state->parsed_key_id));
    (void)sodium_mlock(state->error, sizeof(state->error));
    state->fixed_buffers_locked = true;
}

void salt_cli_state_cleanup(struct salt_cli_state *state) {
    if (state == NULL) { /* GCOVR_EXCL_BR_LINE */
        return;          /* GCOVR_EXCL_BR_LINE */
    }

    /*
     * Best-effort scrubbing of transient sensitive buffers before release.
     * sodium_munlock() zeroes the locked region in addition to unlocking,
     * giving us the wipe guarantee required by the CLI hardening rules.
     * Reset the structure afterwards so repeated cleanup calls remain safe.
     */
    if (state->output != NULL) {
        salt_cli_secure_free_region(state->output, state->output_cap);
    }
    if (state->stdin_key != NULL) {
        salt_cli_secure_free_region(state->stdin_key, SALT_MAX_KEY_INPUT_LEN + 1U);
    }
    if (state->stdin_plaintext != NULL) {
        salt_cli_secure_free_region(state->stdin_plaintext, SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U);
    }
    if (state->argv_plaintext_copy != NULL) {
        salt_cli_secure_free_region(state->argv_plaintext_copy, state->argv_plaintext_cap);
    }
    if (state->trimmed_key_input != NULL) { /* GCOVR_EXCL_BR_LINE */
        salt_cli_secure_free_region(state->trimmed_key_input, state->trimmed_key_input_cap);
    }
    if (state->fixed_buffers_locked) {
        salt_cli_secure_unlock_region(state->parsed_key_b64, sizeof(state->parsed_key_b64));
        salt_cli_secure_unlock_region(state->parsed_key_id, sizeof(state->parsed_key_id));
        salt_cli_secure_unlock_region(state->error, sizeof(state->error));
    }

    *state = (struct salt_cli_state){0};
}

void salt_cli_resolve_effective_key_id(struct salt_cli_state *state) {
    if (state->key_id_option != NULL && state->key_id_option[0] != '\0') {
        state->effective_key_id = state->key_id_option;
    } else if (state->parsed_has_key_id) {
        state->effective_key_id = state->parsed_key_id;
    }
}
