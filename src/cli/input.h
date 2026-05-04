/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI input loading helpers.
 */

#ifndef SALT_CLI_INPUT_H
#define SALT_CLI_INPUT_H

#include "state.h"

#include <stdio.h>

int salt_cli_input_load_key_input(FILE *in_stream, FILE *err_stream, struct salt_cli_state *state);
int salt_cli_input_load_plaintext(FILE *in_stream, FILE *err_stream, struct salt_cli_state *state);

#endif
