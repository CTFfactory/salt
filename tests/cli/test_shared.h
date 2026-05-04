/*
 * SPDX-License-Identifier: Unlicense
 *
 * Shared support for CLI-focused cmocka tests.
 */

#ifndef SALT_TEST_CLI_SHARED_H
#define SALT_TEST_CLI_SHARED_H

#include "../src/salt_test_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct cli_test_context {
    const char *expected_message;
    const char *expected_key;
    const char *fake_encoded;
    size_t alloc_call_count;
    size_t fail_alloc_on_call;
    size_t stream_fread_bytes;
    int stream_ferror_code;
    int stream_fgetc_code;
} cli_test_context;

#define TEST_STDOUT_BUFFER_SIZE 1024
#define TEST_SMALL_BUFFER_SIZE 256

cli_test_context *cli_get_ctx(void);

#define cli_expected_message (cli_get_ctx()->expected_message)
#define cli_expected_key (cli_get_ctx()->expected_key)
#define cli_fake_encoded (cli_get_ctx()->fake_encoded)
#define cli_alloc_call_count (cli_get_ctx()->alloc_call_count)
#define cli_fail_alloc_on_call (cli_get_ctx()->fail_alloc_on_call)
#define cli_stream_fread_bytes (cli_get_ctx()->stream_fread_bytes)
#define cli_stream_ferror_code (cli_get_ctx()->stream_ferror_code)
#define cli_stream_fgetc_code (cli_get_ctx()->stream_fgetc_code)

void *cli_failing_alloc(size_t size);
void *cli_fail_on_nth_alloc(size_t size);
void *cli_interrupting_alloc(size_t size);
void cli_passthrough_free(void *ptr);

size_t cli_stream_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int cli_stream_ferror(FILE *stream);
int cli_stream_ferror_interrupt(FILE *stream);
int cli_stream_fgetc(FILE *stream);
int cli_stream_ungetc(int c, FILE *stream);
size_t cli_stream_fread_interrupt(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t cli_stream_fread_raise_sigint(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t cli_stream_fread_raise_sigterm(void *ptr, size_t size, size_t nmemb, FILE *stream);

int cli_fake_error(const unsigned char *message, size_t message_len, const char *public_key_b64,
                   char *output, size_t output_size, size_t *output_written, char *error,
                   size_t error_size);
int cli_fake_zero_output(const unsigned char *message, size_t message_len,
                         const char *public_key_b64, char *output, size_t output_size,
                         size_t *output_written, char *error, size_t error_size);
int cli_fake_success(const unsigned char *message, size_t message_len, const char *public_key_b64,
                     char *output, size_t output_size, size_t *output_written, char *error,
                     size_t error_size);
int cli_fake_signal_after_seal(const unsigned char *message, size_t message_len,
                               const char *public_key_b64, char *output, size_t output_size,
                               size_t *output_written, char *error, size_t error_size);

int cli_test_setup(void **state);
int cli_test_teardown(void **state);

FILE *test_tmpfile_checked(void);
FILE *test_fopen_checked(const char *path, const char *mode);
void read_stream(FILE *stream, char *buffer, size_t buffer_size);
FILE *stream_from_text(const char *text);
FILE *stream_with_write_fail_after(size_t writes_before_fail);
int count_open_fds(void);
void assert_string_starts_with(const char *actual, const char *prefix);

#endif
