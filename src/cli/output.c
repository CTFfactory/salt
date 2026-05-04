/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI output serialization for text and JSON modes.
 *
 * This module handles deterministic output emission, JSON string escaping,
 * and UTF-8 validation to ensure the emitted JSON is RFC 8259 compliant.
 */

#include "output.h"
#include "salt.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*
 * Write formatted error message to optional error buffer.
 *
 * Wraps vsnprintf for consistent error reporting throughout the CLI module.
 * The return value is intentionally discarded because:
 * - vsnprintf returns the number of characters written (on success) or -1 (on error).
 * - For error buffer population, we accept either outcome; a truncated error message
 *   is still useful diagnostics (better than no message).
 * - This is a best-effort reporting path; failures should not cause secondary errors.
 *
 * The call is defensive: although vsnprintf should succeed with our format strings
 * and arguments, discarding the return value via cast-to-void (MISRA C:2012 Rule 17.7)
 * signals this is intentional and documents why we don't branch on the result.
 */
static void cli_set_error(char *error, size_t error_size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

static void cli_set_error(char *error, size_t error_size, const char *fmt, ...) {
    va_list args;

    if (error == NULL || error_size == 0U) {
        return;
    }

    va_start(args, fmt);
    /* Annex K vsnprintf_s is unavailable on glibc; va_list is initialized by va_start above. */
    /* NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    /* NOLINTBEGIN(clang-analyzer-valist.Uninitialized) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    (void)vsnprintf(error, error_size, fmt, args);
#pragma GCC diagnostic pop
    /* NOLINTEND(clang-analyzer-valist.Uninitialized) */
    /* NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    va_end(args);
}

static int cli_stream_write_error(char *error, size_t error_size, const char *message) {
    cli_set_error(error, error_size, "%s", message);
    return SALT_ERR_IO;
}

static int cli_stream_putc(FILE *stream, int ch, char *error, size_t error_size,
                           const char *message) {
    if (fputc(ch, stream) == EOF) {
        return cli_stream_write_error(error, error_size, message);
    }
    return SALT_OK;
}

static int cli_stream_puts(FILE *stream, const char *value, char *error, size_t error_size,
                           const char *message) {
    if (fputs(value, stream) < 0) {
        return cli_stream_write_error(error, error_size, message);
    }
    return SALT_OK;
}

/*
 * Formatted printf emission to stream with integrated error reporting.
 *
 * Wraps vfprintf for stream output with error message capture. The return value
 * check (< 0) is defensive:
 * - POSIX vfprintf() returns the number of characters transmitted (on success)
 *   or -1 (EOF or encoding error).
 * - We branch on negative return to signal I/O failure upstream, allowing the
 *   caller to report via the error buffer and bail out cleanly.
 * - The check is future-proofing: our format strings are constrained, so failures
 *   should not occur in normal operation.
 *
 * This pattern ensures that output failures are caught and propagated
 * rather than silently ignored.
 */
static int cli_stream_printf(FILE *stream, char *error, size_t error_size, const char *message,
                             const char *fmt, ...) __attribute__((format(printf, 5, 6)));

static int cli_stream_printf(FILE *stream, char *error, size_t error_size, const char *message,
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
        return cli_stream_write_error(error, error_size, message);
    }
    return SALT_OK;
}

static int cli_stream_flush(FILE *stream, char *error, size_t error_size, const char *message) {
    if (fflush(stream) != 0 || ferror(stream) != 0) {
        return cli_stream_write_error(error, error_size, message);
    }
    return SALT_OK;
}

/*
 * Validate a single UTF-8 sequence starting at ptr.
 *
 * This helper function validates one complete UTF-8 character starting at
 * the given pointer, checking:
 * - Correct continuation bytes (0x80..0xBF)
 * - Proper lead byte range rules
 * - No overlong encodings
 * - No UTF-16 surrogate pairs (U+D800..U+DFFF)
 * - No code points beyond U+10FFFF
 *
 * Returns true if valid; false if invalid or incomplete.
 * If valid, *out_len is set to the byte count of the sequence (1-4 bytes).
 *
 * This extraction improves testability (can unit-test UTF-8 decoder
 * separately) and reduces cognitive load in the main validation loop.
 */
static bool is_valid_utf8_sequence(const unsigned char *ptr, size_t remaining, size_t *out_len) {
    unsigned char b;
    unsigned char b2;
    unsigned char b3;
    unsigned char b4;

    if (ptr == NULL || out_len == NULL || remaining == 0U) {
        return false;
    }

    b = ptr[0];

    /* 1-byte sequence: 0x00..0x7F */
    if (b <= 0x7FU) {
        *out_len = 1U;
        return true;
    }

    /* 2-byte sequence: 0xC2..0xDF followed by 0x80..0xBF */
    if (b >= 0xC2U && b <= 0xDFU) {
        if (remaining < 2U) {
            return false;
        }
        b2 = ptr[1];
        if (b2 < 0x80U || b2 > 0xBFU) {
            return false;
        }
        *out_len = 2U;
        return true;
    }

    /* 3-byte sequences: 0xE0..0xEF */
    if (b >= 0xE0U && b <= 0xEFU) {
        if (remaining < 3U) {
            return false;
        }

        b2 = ptr[1];
        b3 = ptr[2];

        /* Both continuation bytes must be in valid range */
        if (b2 < 0x80U || b3 < 0x80U || b2 > 0xBFU || b3 > 0xBFU) {
            return false;
        }

        /* E0: second byte must be 0xA0..0xBF (avoid overlong encoding) */
        if (b == 0xE0U && (b2 < 0xA0U || b2 > 0xBFU)) {
            return false;
        }
        /* ED: second byte must be 0x80..0x9F (avoid UTF-16 surrogates U+D800..U+DFFF) */
        if (b == 0xEDU && (b2 < 0x80U || b2 > 0x9FU)) {
            return false;
        }
        /* E1..EC, EE..EF: second byte must be 0x80..0xBF */
        if (b != 0xE0U && b != 0xEDU && (b2 < 0x80U || b2 > 0xBFU)) {
            return false;
        }

        *out_len = 3U;
        return true;
    }

    /* 4-byte sequences: 0xF0..0xF4 */
    if (b >= 0xF0U && b <= 0xF4U) {
        if (remaining < 4U) {
            return false;
        }

        b2 = ptr[1];
        b3 = ptr[2];
        b4 = ptr[3];

        /* All continuation bytes must be in valid range */
        if (b2 < 0x80U || b3 < 0x80U || b4 < 0x80U || b2 > 0xBFU || b3 > 0xBFU || b4 > 0xBFU) {
            return false;
        }

        /* F0: second byte must be 0x90..0xBF (avoid overlong encoding) */
        if (b == 0xF0U && (b2 < 0x90U || b2 > 0xBFU)) {
            return false;
        }
        /* F4: second byte must be 0x80..0x8F (avoid code points > U+10FFFF) */
        if (b == 0xF4U && (b2 < 0x80U || b2 > 0x8FU)) {
            return false;
        }
        /* F1..F3: second byte must be 0x80..0xBF */
        if (b >= 0xF1U && b <= 0xF3U && (b2 < 0x80U || b2 > 0xBFU)) {
            return false;
        }

        *out_len = 4U;
        return true;
    }

    /* Invalid start byte */
    return false;
}

/*
 * Validate that a string contains only well-formed UTF-8.
 *
 * This function ensures RFC 3629 compliance by checking:
 * - Correct sequence lengths (1-4 bytes)
 * - No overlong encodings
 * - No UTF-16 surrogate pairs (U+D800..U+DFFF)
 * - No code points beyond U+10FFFF
 *
 * JSON output (RFC 8259 §8.1) requires strings to be valid Unicode encoded
 * in UTF-8. Rejecting invalid UTF-8 at the boundary prevents producing
 * malformed JSON that strict parsers cannot load.
 *
 * This function delegates to is_valid_utf8_sequence() for each character,
 * improving code clarity and testability by isolating sequence validation logic.
 */
static bool is_valid_utf8(const char *str) {
    const unsigned char *bytes;
    size_t len;
    size_t i = 0U;

    if (str == NULL) {
        return false;
    }

    bytes = (const unsigned char *)str;
    len = strlen(str);

    while (i < len) {
        size_t seq_len;

        /* Loop invariant: len > 0 ensures subtraction is safe (len - i produces valid size_t) */
        /* Validate the current UTF-8 sequence and get its byte count */
        if (!is_valid_utf8_sequence(&bytes[i], len - i, &seq_len)) {
            return false;
        }

        i += seq_len;
    }

    return true;
}

static int write_json_string(FILE *stream, const char *value, char *error, size_t error_size) {
    const unsigned char *cursor = (const unsigned char *)value;

    if (cli_stream_putc(stream, '"', error, error_size, "failed to write encrypted output") !=
        SALT_OK) {
        return SALT_ERR_IO;
    }
    while (*cursor != '\0') {
        if (*cursor == '"' || *cursor == '\\') {
            if (cli_stream_putc(stream, '\\', error, error_size,
                                "failed to write encrypted output") != SALT_OK ||
                cli_stream_putc(stream, (int)*cursor, error, error_size,
                                "failed to write encrypted output") != SALT_OK) {
                return SALT_ERR_IO;
            }
        } else if (*cursor == '\n') {
            if (cli_stream_puts(stream, "\\n", error, error_size,
                                "failed to write encrypted output") != SALT_OK) {
                return SALT_ERR_IO;
            }
        } else if (*cursor == '\r') {
            if (cli_stream_puts(stream, "\\r", error, error_size,
                                "failed to write encrypted output") != SALT_OK) {
                return SALT_ERR_IO;
            }
        } else if (*cursor == '\t') {
            if (cli_stream_puts(stream, "\\t", error, error_size,
                                "failed to write encrypted output") != SALT_OK) {
                return SALT_ERR_IO;
            }
        } else if (*cursor < 0x20U) {
            /* Control bytes are escaped to keep output valid JSON. */
            if (cli_stream_printf(stream, error, error_size, "failed to write encrypted output",
                                  "\\u%04x", *cursor) != SALT_OK) {
                return SALT_ERR_IO;
            }
        } else {
            if (cli_stream_putc(stream, (int)*cursor, error, error_size,
                                "failed to write encrypted output") != SALT_OK) {
                return SALT_ERR_IO;
            }
        }
        ++cursor;
    }
    return cli_stream_putc(stream, '"', error, error_size, "failed to write encrypted output");
}

int salt_cli_emit_output(FILE *out_stream, enum salt_cli_output_mode output_mode,
                         const char *encrypted_value, const char *key_id, char *error,
                         size_t error_size) {
    if (output_mode == SALT_CLI_OUTPUT_TEXT) {
        if (cli_stream_printf(out_stream, error, error_size, "failed to write encrypted output",
                              "%s\n", encrypted_value) != SALT_OK) {
            return SALT_ERR_IO;
        }
        return cli_stream_flush(out_stream, error, error_size, "failed to write encrypted output");
    }

    if (key_id == NULL || key_id[0] == '\0') {
        cli_set_error(error, error_size,
                      "JSON output requires key_id (provide JSON key_id or --key-id)");
        return SALT_ERR_INPUT;
    }

    /*
     * RFC 8259 §8.1: JSON text exchanged between systems that are not part of
     * a closed ecosystem MUST be encoded using UTF-8. Reject invalid UTF-8 in
     * key_id before emitting JSON to ensure all consumers can parse the output.
     */
    if (!is_valid_utf8(key_id)) {
        cli_set_error(error, error_size,
                      "key_id contains invalid UTF-8 (JSON requires valid UTF-8 strings)");
        return SALT_ERR_INPUT;
    }

    if (cli_stream_putc(out_stream, '{', error, error_size, "failed to write encrypted output") !=
            SALT_OK ||
        cli_stream_puts(out_stream, "\"encrypted_value\":", error, error_size,
                        "failed to write encrypted output") != SALT_OK ||
        write_json_string(out_stream, encrypted_value, error, error_size) != SALT_OK ||
        cli_stream_puts(out_stream, ",\"key_id\":", error, error_size,
                        "failed to write encrypted output") != SALT_OK ||
        write_json_string(out_stream, key_id, error, error_size) != SALT_OK ||
        cli_stream_puts(out_stream, "}\n", error, error_size, "failed to write encrypted output") !=
            SALT_OK) {
        return SALT_ERR_IO;
    }
    return cli_stream_flush(out_stream, error, error_size, "failed to write encrypted output");
}
