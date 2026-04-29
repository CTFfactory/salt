/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI stream I/O and error reporting helpers.
 *
 * This header is internal to the salt implementation and should not be
 * installed or exposed to downstream consumers. It provides stream
 * operations and error reporting used by src/main.c.
 */

#ifndef SALT_CLI_STREAM_H
#define SALT_CLI_STREAM_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

/*
 * Set error message in buffer.
 *
 * Helper for deterministic error reporting with consistent snprintf usage.
 *
 * Parameters:
 *   error: Error message buffer (may be NULL).
 *   error_size: Size of error buffer.
 *   message: Error message string.
 *   rc: Return code to pass through.
 *
 * Return Values:
 *   Returns rc unchanged.
 *
 * Example:
 *   return salt_cli_set_error(error, sizeof(error), "failed to read", SALT_ERR_IO);
 */
int salt_cli_set_error(char *error, size_t error_size, const char *message, int rc);

/*
 * Flush stream and report any flush errors.
 *
 * Parameters:
 *   stream: Output stream to flush.
 *   error: Optional error buffer.
 *   error_size: Size of error buffer.
 *   message: Error message to use on failure.
 *
 * Return Values:
 *   SALT_OK (0): Flush succeeded.
 *   SALT_ERR_IO (4): Flush failed.
 */
int salt_cli_flush_stream(FILE *stream, char *error, size_t error_size, const char *message);

/*
 * Write formatted output to stream with error handling.
 *
 * Writes printf-style formatted output to the specified stream and flushes it.
 * On error, sets error buffer and returns SALT_ERR_IO.
 *
 * Parameters:
 *   stream: Output stream (stdout or stderr).
 *   error: Optional error message buffer.
 *   error_size: Size of error buffer.
 *   message: Error message to use on failure.
 *   fmt: printf-style format string.
 *   ...: Format arguments.
 *
 * Return Values:
 *   SALT_OK (0): Success; output written and flushed.
 *   SALT_ERR_IO (4): Write or flush failed.
 *
 * Example:
 *   rc = salt_cli_write_stream(stdout, error, sizeof(error),
 *                              "failed to write", "result: %s\n", result);
 */
int salt_cli_write_stream(FILE *stream, char *error, size_t error_size, const char *message,
                          const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/*
 * Report status to stream and return status code.
 *
 * Writes formatted diagnostic message to the specified stream and returns
 * the provided status code unchanged. Used for error reporting that needs
 * to preserve the original error code.
 *
 * Parameters:
 *   stream: Output stream (usually stderr).
 *   error: Optional error message buffer.
 *   error_size: Size of error buffer.
 *   rc: Status code to return.
 *   fmt: printf-style format string.
 *   ...: Format arguments.
 *
 * Return Values:
 *   Returns rc unchanged.
 *
 * Example:
 *   return salt_cli_report_stream_status(stderr, error, sizeof(error),
 *                                        SALT_ERR_INPUT, "error: %s\n", "invalid key");
 */
int salt_cli_report_stream_status(FILE *stream, char *error, size_t error_size, int rc,
                                  const char *fmt, ...) __attribute__((format(printf, 5, 6)));

#endif
