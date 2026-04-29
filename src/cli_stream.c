/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI stream I/O and error reporting implementation.
 *
 * This file implements stream write helpers with consistent error handling
 * and flushing semantics.
 */

#include "cli_stream.h"
#include "salt.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int salt_cli_set_error(char *error, size_t error_size, const char *message, int rc) {
    if (error != NULL && error_size > 0U) {
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)snprintf(error, error_size, "%s", message);
    }
    return rc;
}

int salt_cli_flush_stream(FILE *stream, char *error, size_t error_size, const char *message) {
    if (fflush(stream) != 0) { /* GCOVR_EXCL_BR_LINE */
        return salt_cli_set_error(error, error_size, message, SALT_ERR_IO);
    }
    return SALT_OK;
}

int salt_cli_write_stream(FILE *stream, char *error, size_t error_size, const char *message,
                          const char *fmt, ...) {
    va_list args;
    int written;

    va_start(args, fmt);
    /* Annex K vfprintf_s is unavailable on glibc; va_list is initialized by va_start above. */
    /* NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    /* NOLINTBEGIN(clang-analyzer-valist.Uninitialized) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    written = vfprintf(stream, fmt, args);
#pragma GCC diagnostic pop
    /* NOLINTEND(clang-analyzer-valist.Uninitialized) */
    /* NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    va_end(args);
    if (written < 0) {
        return salt_cli_set_error(error, error_size, message, SALT_ERR_IO);
    }
    return salt_cli_flush_stream(stream, error, error_size, message);
}

int salt_cli_report_stream_status(FILE *stream, char *error, size_t error_size, int rc,
                                  const char *fmt, ...) {
    va_list args;
    int written;

    va_start(args, fmt);
    /* Annex K vfprintf_s is unavailable on glibc; va_list is initialized by va_start above. */
    /* NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    /* NOLINTBEGIN(clang-analyzer-valist.Uninitialized) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    written = vfprintf(stream, fmt, args);
#pragma GCC diagnostic pop
    /* NOLINTEND(clang-analyzer-valist.Uninitialized) */
    /* NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    va_end(args);
    if (written < 0) {
        return salt_cli_set_error(error, error_size, "failed to write diagnostic", SALT_ERR_IO);
    }

    if (salt_cli_flush_stream(stream, error, error_size, "failed to write diagnostic") !=
        SALT_OK) { /* GCOVR_EXCL_BR_LINE */
        return SALT_ERR_IO;
    }
    return rc;
}
