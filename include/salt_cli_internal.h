/*
 * SPDX-License-Identifier: Unlicense
 *
 * Backward compatibility shim for relocated internal headers.
 *
 * The CLI internal interfaces have been reorganized:
 * - Key parsing interfaces moved to src/salt_cli_internal.h
 * - Output serialization interfaces moved to src/cli_output.h
 *
 * This shim preserves compatibility with existing build dependencies
 * while the implementation lives in src/.
 */

#ifndef SALT_CLI_INTERNAL_COMPAT_H
#define SALT_CLI_INTERNAL_COMPAT_H

#include "../src/salt_cli_internal.h"
#include "../src/cli_output.h"

#endif
