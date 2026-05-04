/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI output preparation helpers.
 */

#ifndef SALT_CLI_PREPARE_H
#define SALT_CLI_PREPARE_H

#include "state.h"

#include <stdio.h>

int salt_cli_prepare_output(FILE *err_stream, struct salt_cli_state *state);

#endif
