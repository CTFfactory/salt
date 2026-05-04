/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI runtime hook state and SALT_NO_MAIN test-hook implementation.
 */

#include "test_hooks.h"

#include "signals.h"
#include "internal.h"

#include <stdlib.h>

_Thread_local salt_alloc_fn salt_cli_alloc_impl = malloc;
_Thread_local salt_free_fn salt_cli_free_impl = free;
_Thread_local salt_seal_base64_fn salt_cli_seal_impl = salt_seal_base64;
_Thread_local salt_required_output_len_fn salt_cli_required_output_len_impl =
    salt_required_base64_output_len;
_Thread_local salt_cli_fread_fn salt_cli_fread_impl = fread;
_Thread_local salt_cli_ferror_fn salt_cli_ferror_impl = ferror;
_Thread_local salt_cli_fgetc_fn salt_cli_fgetc_impl = fgetc;
_Thread_local salt_cli_ungetc_fn salt_cli_ungetc_impl = ungetc;

#ifdef SALT_NO_MAIN
void salt_cli_set_test_hooks(salt_alloc_fn alloc_hook, salt_free_fn free_hook,
                             salt_seal_base64_fn seal_hook) {
    salt_cli_alloc_impl = (alloc_hook != NULL) ? alloc_hook : malloc;
    salt_cli_free_impl = (free_hook != NULL) ? free_hook : free;
    salt_cli_seal_impl = (seal_hook != NULL) ? seal_hook : salt_seal_base64;
}

void salt_cli_reset_test_hooks(void) {
    salt_cli_set_test_hooks(NULL, NULL, NULL);
    salt_cli_required_output_len_impl = salt_required_base64_output_len;
    salt_cli_reset_interrupted();
    salt_cli_reset_stream_test_hooks();
}

void salt_cli_set_output_len_test_hook(salt_required_output_len_fn output_len_hook) {
    salt_cli_required_output_len_impl =
        (output_len_hook != NULL) ? output_len_hook : salt_required_base64_output_len;
}

void salt_cli_set_stream_test_hooks(salt_cli_fread_fn fread_hook, salt_cli_ferror_fn ferror_hook,
                                    salt_cli_fgetc_fn fgetc_hook, salt_cli_ungetc_fn ungetc_hook) {
    salt_cli_fread_impl = (fread_hook != NULL) ? fread_hook : fread;     /* GCOVR_EXCL_BR_LINE */
    salt_cli_ferror_impl = (ferror_hook != NULL) ? ferror_hook : ferror; /* GCOVR_EXCL_BR_LINE */
    salt_cli_fgetc_impl = (fgetc_hook != NULL) ? fgetc_hook : fgetc;     /* GCOVR_EXCL_BR_LINE */
    salt_cli_ungetc_impl = (ungetc_hook != NULL) ? ungetc_hook : ungetc; /* GCOVR_EXCL_BR_LINE */
}

void salt_cli_reset_stream_test_hooks(void) {
    salt_cli_set_stream_test_hooks(NULL, NULL, NULL, NULL);
}

int salt_cli_test_read_stream_limited(FILE *in_stream, size_t max_len, char **output,
                                      size_t *output_len, char *error, size_t error_size) {
    return cli_input_read_stream_limited(in_stream, max_len, output, output_len, error, error_size);
}

int salt_cli_test_parse_key_json_object(const char *json, char *key_b64, size_t key_b64_size,
                                        char *key_id, size_t key_id_size, bool *has_key_id,
                                        char *error, size_t error_size) {
    return salt_cli_parse_key_json_object_strict(json, key_b64, key_b64_size, key_id, key_id_size,
                                                 has_key_id, error, error_size);
}

int salt_cli_test_parse_key_input(const char *key_input, int key_format, char *key_b64,
                                  size_t key_b64_size, char *key_id, size_t key_id_size,
                                  bool *has_key_id, char *error, size_t error_size) {
    return salt_cli_parse_key_input(key_input, (enum salt_cli_key_format)key_format, key_b64,
                                    key_b64_size, key_id, key_id_size, has_key_id, error,
                                    error_size);
}

void salt_cli_test_set_interrupted(bool interrupted) {
    salt_cli_test_set_interrupted_flag(interrupted ? 1 : 0);
}

void salt_cli_test_state_init(struct salt_cli_state *state) {
    salt_cli_state_init(state);
}

void salt_cli_test_state_cleanup(struct salt_cli_state *state) {
    salt_cli_state_cleanup(state);
}
#endif
