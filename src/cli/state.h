/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI state lifecycle management.
 *
 * This header is internal to the salt implementation and should not be
 * installed or exposed to downstream consumers. It provides state
 * initialization and cleanup functions used by src/main.c.
 */

#ifndef SALT_CLI_STATE_H
#define SALT_CLI_STATE_H

#include "salt.h"
#include "internal.h"
#include "output.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Consolidated CLI state for encryption workflow.
 *
 * This structure holds all transient data for a single CLI invocation:
 * command-line options, parsed keys, plaintext inputs, and output buffers.
 * All sensitive fields (keys, plaintext, output) are sodium_mlock'd during
 * initialization and sodium_munlock'd (zeroed) during cleanup to minimize
 * swap exposure.
 */
struct salt_cli_state {
    const char *key_option;
    const char *key_id_option;
    enum salt_cli_output_mode output_mode;
    enum salt_cli_key_format key_format;
    bool show_help;
    bool show_version;
    const char *plaintext_arg;
    bool plaintext_from_stdin;
    bool key_from_stdin;
    char *stdin_key;
    char *stdin_plaintext;
    char *argv_plaintext_copy;
    size_t argv_plaintext_cap;
    char *trimmed_key_input;
    size_t trimmed_key_input_cap;
    char parsed_key_b64[SALT_MAX_KEY_INPUT_LEN + 1U];
    char parsed_key_id[SALT_MAX_KEY_ID_LEN + 1U];
    bool parsed_has_key_id;
    const char *effective_key_id;
    const unsigned char *message;
    size_t message_len;
    char *output;
    size_t output_cap;
    size_t output_written;
    bool fixed_buffers_locked;
    char error[256];
};

/*
 * Initialize CLI state structure.
 *
 * Zeroes all fields and applies sodium_mlock to sensitive buffers (key
 * scratch, error message) to keep them resident and out of swap.
 *
 * Parameters:
 *   state: Pointer to uninitialized state structure.
 *
 * Memory Safety:
 *   - Locks parsed_key_b64, parsed_key_id, and error buffers.
 *   - Best-effort; lock failures (e.g., RLIMIT_MEMLOCK) are non-fatal.
 *
 * Example:
 *   struct salt_cli_state state;
 *   salt_cli_state_init(&state);
 */
void salt_cli_state_init(struct salt_cli_state *state);

/*
 * Clean up CLI state structure.
 *
 * Scrubs and releases all dynamically allocated sensitive buffers.
 * sodium_munlock zeroes memory before unlocking, ensuring scrubbing even
 * if explicit zeroing were skipped.
 *
 * Parameters:
 *   state: Pointer to initialized state structure.
 *
 * Memory Safety:
 *   - Unlocks and zeroes all mlock'd regions.
 *   - Safe to call multiple times on same state (idempotent).
 *   - NULL-safe: returns immediately if state is NULL.
 *
 * Example:
 *   salt_cli_state_cleanup(&state);
 */
void salt_cli_state_cleanup(struct salt_cli_state *state);

/*
 * Resolve effective key ID from options and parsed input.
 *
 * Key ID precedence:
 *   1. Explicit --key-id/-i option (always wins)
 *   2. Parsed key_id from JSON key input
 *   3. NULL if neither provided
 *
 * Parameters:
 *   state: CLI state with parsed key data and command-line options.
 *
 * Side Effects:
 *   Sets state->effective_key_id to the resolved key ID pointer.
 *
 * Example:
 *   salt_cli_resolve_effective_key_id(&state);
 *   if (state.effective_key_id != NULL) { ... }
 */
void salt_cli_resolve_effective_key_id(struct salt_cli_state *state);

#endif
