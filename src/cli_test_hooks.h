/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI runtime hook state and test-hook APIs.
 */

#ifndef SALT_CLI_TEST_HOOKS_H
#define SALT_CLI_TEST_HOOKS_H

#include "salt_test_internal.h"
#include "cli_state.h"

#ifdef __cplusplus
extern "C" {
#endif

extern _Thread_local salt_alloc_fn salt_cli_alloc_impl;
extern _Thread_local salt_free_fn salt_cli_free_impl;
extern _Thread_local salt_seal_base64_fn salt_cli_seal_impl;
extern _Thread_local salt_required_output_len_fn salt_cli_required_output_len_impl;
extern _Thread_local salt_cli_fread_fn salt_cli_fread_impl;
extern _Thread_local salt_cli_ferror_fn salt_cli_ferror_impl;
extern _Thread_local salt_cli_fgetc_fn salt_cli_fgetc_impl;
extern _Thread_local salt_cli_ungetc_fn salt_cli_ungetc_impl;

int cli_input_read_stream_limited(FILE *in_stream, size_t max_len, char **output,
                                  size_t *output_len, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
