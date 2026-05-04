/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI output preparation implementation.
 */

#include "prepare.h"

#include "signals.h"
#include "stream.h"
#include "test_hooks.h"

#include <sodium.h>

static int salt_cli_report_state_error(FILE *err_stream, const struct salt_cli_state *state,
                                       int rc) {
    return salt_cli_report_stream_status(err_stream, NULL, 0U, rc, "error: %s\n", state->error);
}

int salt_cli_prepare_output(FILE *err_stream, struct salt_cli_state *state) {
    size_t output_size;
    int rc;

    salt_cli_resolve_effective_key_id(state);

    /* Size is computed from plaintext length and sealed-box overhead. */
    output_size = salt_cli_required_output_len_impl(state->message_len);
    if (output_size == 0U) {
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INTERNAL,
                                             "internal error: output size overflow\n");
    }

    state->output = (char *)salt_cli_alloc_impl(output_size);
    if (state->output == NULL) {
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INTERNAL,
                                             "internal error: failed to allocate output buffer\n");
    }
    state->output_cap = output_size;
    (void)sodium_mlock(state->output, state->output_cap);

    rc =
        salt_cli_seal_impl(state->message, state->message_len, state->parsed_key_b64, state->output,
                           output_size, &state->output_written, state->error, sizeof(state->error));
    if (rc != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
        return salt_cli_report_state_error(err_stream, state, rc);
    }

    rc = salt_cli_check_interrupted(state->error, sizeof(state->error));
    if (rc != SALT_OK) { /* GCOVR_EXCL_BR_LINE */
        return salt_cli_report_state_error(err_stream, state, rc);
    }

    if (state->output_written == 0U) {
        return salt_cli_report_stream_status(err_stream, state->error, sizeof(state->error),
                                             SALT_ERR_INTERNAL, "error: produced empty output\n");
    }

    return SALT_OK;
}
