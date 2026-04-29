/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal test-only hook interfaces for deterministic unit and fuzz testing.
 */

#ifndef SALT_TEST_INTERNAL_H
#define SALT_TEST_INTERNAL_H

#include "salt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef void *(*salt_alloc_fn)(size_t size);
typedef void (*salt_free_fn)(void *ptr);
typedef int (*salt_sodium_init_fn)(void);
typedef int (*salt_crypto_box_seal_fn)(unsigned char *ciphertext, const unsigned char *message,
                                       unsigned long long message_len,
                                       const unsigned char *public_key);
typedef char *(*salt_bin2base64_fn)(char *b64, size_t b64_maxlen, const unsigned char *bin,
                                    size_t bin_len, int variant);
typedef int (*salt_seal_base64_fn)(const unsigned char *message, size_t message_len,
                                   const char *public_key_b64, char *output, size_t output_size,
                                   size_t *output_written, char *error, size_t error_size);
typedef size_t (*salt_required_output_len_fn)(size_t message_len);
typedef size_t (*salt_cli_fread_fn)(void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef int (*salt_cli_ferror_fn)(FILE *stream);
typedef int (*salt_cli_fgetc_fn)(FILE *stream);
typedef int (*salt_cli_ungetc_fn)(int c, FILE *stream);

#ifdef SALT_NO_MAIN
void salt_set_test_hooks(salt_sodium_init_fn sodium_init_hook, salt_alloc_fn alloc_hook,
                         salt_free_fn free_hook, salt_crypto_box_seal_fn crypto_box_seal_hook,
                         salt_bin2base64_fn bin2base64_hook);

void salt_reset_test_hooks(void);

void salt_cli_set_test_hooks(salt_alloc_fn alloc_hook, salt_free_fn free_hook,
                             salt_seal_base64_fn seal_hook);

void salt_cli_reset_test_hooks(void);

void salt_cli_set_output_len_test_hook(salt_required_output_len_fn output_len_hook);

void salt_cli_set_stream_test_hooks(salt_cli_fread_fn fread_hook, salt_cli_ferror_fn ferror_hook,
                                    salt_cli_fgetc_fn fgetc_hook, salt_cli_ungetc_fn ungetc_hook);

void salt_cli_reset_stream_test_hooks(void);

int salt_cli_test_read_stream_limited(FILE *in_stream, size_t max_len, char **output,
                                      size_t *output_len, char *error, size_t error_size);

int salt_cli_test_parse_key_json_object(const char *json, char *key_b64, size_t key_b64_size,
                                        char *key_id, size_t key_id_size, bool *has_key_id,
                                        char *error, size_t error_size);

int salt_cli_test_parse_key_input(const char *key_input, int key_format, char *key_b64,
                                  size_t key_b64_size, char *key_id, size_t key_id_size,
                                  bool *has_key_id, char *error, size_t error_size);

void salt_cli_test_set_interrupted(bool interrupted);

struct salt_cli_state;

void salt_cli_test_state_init(struct salt_cli_state *state);
void salt_cli_test_state_cleanup(struct salt_cli_state *state);
#endif

#endif
