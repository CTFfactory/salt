/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI output serialization interfaces.
 *
 * This header is internal to the salt implementation and should not be
 * installed or exposed to downstream consumers. It provides output
 * formatting functions used by src/main.c and tested by tests/ and fuzz/.
 */

#ifndef SALT_CLI_OUTPUT_H
#define SALT_CLI_OUTPUT_H

#include <stddef.h>
#include <stdio.h>

/*
 * Output format mode for CLI encryption results.
 * TEXT: Base64-encoded ciphertext (default).
 * JSON: JSON object with encrypted_value and key_id fields.
 */
enum salt_cli_output_mode { SALT_CLI_OUTPUT_TEXT = 0, SALT_CLI_OUTPUT_JSON = 1 };

/*
 * Emit encrypted output in text or JSON format.
 *
 * Writes the base64-encoded ciphertext to the output stream, optionally
 * wrapped in a JSON object with key_id for API workflows.
 *
 * Parameters:
 *   out_stream: Output file stream (usually stdout).
 *   output_mode: TEXT for plain base64, JSON for JSON envelope.
 *   encrypted_value: Base64-encoded ciphertext.
 *   key_id: Key identifier (required for JSON mode; ignored in TEXT mode).
 *   error: Optional error message buffer.
 *   error_size: Size of error buffer.
 *
 * Return Values:
 *   SALT_OK (0): Success; output written to out_stream.
 *   SALT_ERR_INPUT (1): JSON mode requested but key_id is NULL, empty, or
 *                       contains invalid UTF-8 (JSON requires valid UTF-8).
 *   SALT_ERR_IO (4): Output stream rejected or truncated the serialized value.
 *
 * Output Formats:
 *   TEXT: Writes "ciphertext\n" to stream.
 *   JSON: Writes {"encrypted_value": "ciphertext", "key_id": "id"}\n to stream.
 *
 * UTF-8 Validation:
 *   JSON output enforces RFC 8259 §8.1 UTF-8 requirement. If key_id contains
 *   invalid UTF-8, the function returns SALT_ERR_INPUT with a descriptive error.
 *   This ensures all JSON consumers (strict parsers, GitHub Actions, etc.) can
 *   successfully parse the output.
 *
 * Notes:
 *   - JSON output always includes both fields (no optional fields).
 *   - Special characters in encrypted_value and key_id are JSON-escaped.
 *   - Control characters are `\u`-escaped; the API is C-string based, so
 *     embedded NUL bytes are not representable inputs here.
 *
 * Example (TEXT):
 *   salt_cli_emit_output(stdout, SALT_CLI_OUTPUT_TEXT, "ciphertext...", NULL, error,
 * sizeof(error));
 *   // Outputs: ciphertext...\n
 *
 * Example (JSON):
 *   salt_cli_emit_output(stdout, SALT_CLI_OUTPUT_JSON, "ciphertext...", "k123", error,
 * sizeof(error));
 *   // Outputs: {"encrypted_value":"ciphertext...","key_id":"k123"}\n
 */
int salt_cli_emit_output(FILE *out_stream, enum salt_cli_output_mode output_mode,
                         const char *encrypted_value, const char *key_id, char *error,
                         size_t error_size);

#endif
