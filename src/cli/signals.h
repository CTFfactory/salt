/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI signal handling for graceful interruption.
 *
 * This header is internal to the salt implementation and should not be
 * installed or exposed to downstream consumers. It provides signal
 * disposition setup and interruption checking used by src/main.c.
 */

#ifndef SALT_CLI_SIGNAL_H
#define SALT_CLI_SIGNAL_H

#include <stddef.h>

/*
 * Install signal handlers for SIGINT and SIGTERM.
 *
 * The CLI sets process-wide signal handlers to provide graceful interruption
 * for long-running stdin reads. Previous dispositions are saved so they can be
 * restored with salt_cli_restore_signal_handlers().
 *
 * Return Values:
 *   SALT_OK (0): Handlers installed successfully.
 *   SALT_ERR_INTERNAL (3): Failed to install one or more handlers.
 */
int salt_cli_ensure_signal_handlers(void);

/*
 * Restore previously saved SIGINT/SIGTERM dispositions.
 *
 * Best-effort cleanup; if no saved dispositions are present, this is a no-op.
 */
void salt_cli_restore_signal_handlers(void);

/*
 * Check if CLI operation has been interrupted by signal.
 *
 * Returns SALT_ERR_SIGNAL if a SIGINT or SIGTERM has been received since
 * the last reset, SALT_OK otherwise.
 *
 * Parameters:
 *   error: Optional error message buffer.
 *   error_size: Size of error buffer.
 *
 * Return Values:
 *   SALT_OK (0): No interruption detected.
 *   SALT_ERR_SIGNAL (5): Interrupted by SIGINT or SIGTERM.
 *
 * Example:
 *   if (salt_cli_check_interrupted(error, sizeof(error)) != SALT_OK) {
 *       return SALT_ERR_SIGNAL;
 *   }
 */
int salt_cli_check_interrupted(char *error, size_t error_size);

/*
 * Reset interruption flag (typically at start of new CLI invocation).
 *
 * This function is intended for test harnesses and library-style embeddings
 * that need to clear signal state between invocations.
 */
void salt_cli_reset_interrupted(void);

#ifdef SALT_NO_MAIN
/*
 * Set interruption flag directly for testing.
 *
 * This is a test-only hook that allows simulating signal interruption
 * without actually sending a signal. Production code should not use this.
 */
void salt_cli_test_set_interrupted_flag(int value);

/*
 * Force salt_cli_ensure_signal_handlers() to fail for tests.
 */
void salt_cli_test_force_signal_handler_install_failure(int value);
#endif

#endif
