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
        unsigned char b = bytes[i];

        /* 1-byte sequence: 0x00..0x7F */
        if (b <= 0x7FU) {
            i++;
            continue;
        }

        /* 2-byte sequence: 0xC2..0xDF 0x80..0xBF */
        if (b >= 0xC2U && b <= 0xDFU) {
            if ((len - i) < 2U) {
                return false;
            }
            if (bytes[i + 1U] < 0x80U || bytes[i + 1U] > 0xBFU) {
                return false;
            }
            i += 2U;
            continue;
        }

        /* 3-byte sequences: 0xE0..0xEF */
        if (b >= 0xE0U && b <= 0xEFU) {
            unsigned char b2;
            unsigned char b3;

            if ((len - i) < 3U) {
                return false;
            }
            if (bytes[i + 1U] < 0x80U || bytes[i + 2U] < 0x80U) {
                return false;
            }
            b2 = bytes[i + 1U];
            b3 = bytes[i + 2U];
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
            /* Third byte must always be 0x80..0xBF */
            if (b3 < 0x80U || b3 > 0xBFU) {
                return false;
            }
            i += 3U;
            continue;
        }

        /* 4-byte sequences: 0xF0..0xF4 */
        if (b >= 0xF0U && b <= 0xF4U) {
            unsigned char b2;
            unsigned char b3;
            unsigned char b4;

            if ((len - i) < 4U) {
                return false;
            }
            if (bytes[i + 1U] < 0x80U || bytes[i + 2U] < 0x80U || bytes[i + 3U] < 0x80U) {
                return false;
            }
            b2 = bytes[i + 1U];
            b3 = bytes[i + 2U];
            b4 = bytes[i + 3U];
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
            /* Third and fourth bytes must be 0x80..0xBF */
            if (b3 < 0x80U || b3 > 0xBFU || b4 < 0x80U || b4 > 0xBFU) {
                return false;
            }
            i += 4U;
            continue;
        }

        /* Invalid start byte or incomplete sequence */
        return false;
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
