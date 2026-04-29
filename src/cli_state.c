/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI state lifecycle implementation.
 *
 * This file implements initialization and cleanup for the consolidated
 * CLI state structure, ensuring all sensitive buffers are locked in
 * resident memory and scrubbed on release.
 */

#include "cli_state.h"

#include <sodium.h>
#include <stdlib.h>

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
        (void)sodium_munlock(state->output, state->output_cap);
        free(state->output);
    }
    if (state->stdin_key != NULL) {
        (void)sodium_munlock(state->stdin_key, SALT_MAX_KEY_INPUT_LEN + 1U);
        free(state->stdin_key);
    }
    if (state->stdin_plaintext != NULL) {
        (void)sodium_munlock(state->stdin_plaintext, SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U);
        free(state->stdin_plaintext);
    }
    if (state->argv_plaintext_copy != NULL) {
        (void)sodium_munlock(state->argv_plaintext_copy, state->argv_plaintext_cap);
        free(state->argv_plaintext_copy);
    }
    if (state->trimmed_key_input != NULL) { /* GCOVR_EXCL_BR_LINE */
        (void)sodium_munlock(state->trimmed_key_input, state->trimmed_key_input_cap);
        free(state->trimmed_key_input);
    }
    if (state->fixed_buffers_locked) {
        (void)sodium_munlock(state->parsed_key_b64, sizeof(state->parsed_key_b64));
        (void)sodium_munlock(state->parsed_key_id, sizeof(state->parsed_key_id));
        (void)sodium_munlock(state->error, sizeof(state->error));
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
