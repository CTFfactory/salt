/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI argument parsing helpers.
 */

#ifndef SALT_CLI_ARGS_H
#define SALT_CLI_ARGS_H

#include "cli_state.h"

#include <stddef.h>
#include <stdio.h>

typedef int (*salt_cli_usage_writer_fn)(FILE *stream, const char *program, char *error,
                                        size_t error_size);
typedef int (*salt_cli_version_writer_fn)(FILE *stream, char *error, size_t error_size);

int salt_cli_args_parse_arguments(int argc, char **argv, FILE *out_stream, FILE *err_stream,
                                  struct salt_cli_state *state,
                                  salt_cli_usage_writer_fn usage_writer,
                                  salt_cli_version_writer_fn version_writer);

#endif
