/*
 * SPDX-License-Identifier: Unlicense
 *
 * Internal CLI parsing interfaces used by production code, tests, and fuzzers.
 *
 * This header is internal to the salt implementation and should not be
 * installed or exposed to downstream consumers. It provides key parsing
 * functions used by src/main.c and tested by tests/ and fuzz/.
 */

#ifndef SALT_CLI_INTERNAL_H
#define SALT_CLI_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Key input format for CLI.
 * AUTO: Detect based on presence of '{' (json if detected, base64 otherwise).
 * BASE64: Plain base64-encoded public key (no JSON parsing).
 * JSON: Strict JSON object with "key" field (and optional "key_id" field).
 */
enum salt_cli_key_format {
    SALT_CLI_KEY_FORMAT_AUTO = 0,
    SALT_CLI_KEY_FORMAT_BASE64 = 1,
    SALT_CLI_KEY_FORMAT_JSON = 2
};

/*
 * Trim leading and trailing whitespace from a string in-place.
 *
 * Parameters:
 *   value: Pointer to string buffer (modified in-place).
 *   value_size: Size of value buffer, including space for the trailing NUL.
 *
 * Behavior:
 *   - Removes leading/trailing ASCII space, tab, newline, carriage return,
 *     vertical tab, and form-feed bytes.
 *   - If NULL, returns without modification.
 *
 * Example:
 *   char key[] = "  base64key  ";
 *   salt_cli_trim_whitespace_inplace(key, sizeof(key));
 *   // key is now "base64key"
 */
void salt_cli_trim_whitespace_inplace(char *value, size_t value_size);

/*
 * Parse a strict JSON key object and extract base64 key and optional key_id.
 *
 * Expected JSON format: { "key": "...", "key_id": "..." }
 * - "key" field is mandatory and must be a string.
 * - "key_id" field is optional and must be a string if present.
 * - No unknown fields are allowed; parsing rejects extra fields.
 * - Supports a small deterministic escape set: `\\`, `\"`, `\/`, `\n`,
 *   `\r`, and `\t`.
 *
 * Parameters:
 *   json: JSON string (must be NUL-terminated).
 *   key_b64: Buffer to receive base64 key.
 *   key_b64_size: Size of key_b64 buffer.
 *   key_id: Buffer to receive key_id (may be NULL if not needed).
 *   key_id_size: Size of key_id buffer (ignored if key_id is NULL).
 *   has_key_id: Output flag (true if key_id was successfully parsed).
 *   error: Optional error message buffer.
 *   error_size: Size of error buffer.
 *
 * Return Values:
 *   SALT_OK (0): Success; key_b64 and optionally key_id populated.
 *   SALT_ERR_INPUT (1): JSON parsing failed or field missing/invalid.
 *
 * Notes:
 *   - Whitespace is trimmed from key and key_id values.
 *   - Rejects empty key field or missing key field.
 *   - Does NOT validate that key_b64 is valid base64 or correct length.
 *
 * Example:
 *   char key_b64[256], key_id[256];
 *   bool has_id = false;
 *   char error[256];
 *   int rc = salt_cli_parse_key_json_object_strict(
 *       "{\"key\":\"base64...\",\"key_id\":\"k1\"}",
 *       key_b64, sizeof(key_b64), key_id, sizeof(key_id), &has_id, error, sizeof(error));
 */
int salt_cli_parse_key_json_object_strict(const char *json, char *key_b64, size_t key_b64_size,
                                          char *key_id, size_t key_id_size, bool *has_key_id,
                                          char *error, size_t error_size);

/*
 * Parse key input in auto/base64/json formats.
 *
 * Handles flexible key input: either a plain base64 string or a JSON object.
 * Format auto-detection or explicit selection via key_format parameter.
 *
 * Parameters:
 *   key_input: Key string (base64 or JSON; must be NUL-terminated).
 *   key_format: Desired format (AUTO, BASE64, or JSON).
 *   key_b64: Buffer for base64 key output.
 *   key_b64_size: Size of key_b64 buffer.
 *   key_id: Buffer for key_id (may be NULL).
 *   key_id_size: Size of key_id buffer (0 if key_id is NULL).
 *   has_key_id: Output flag (true if key_id was extracted).
 *   error: Optional error message buffer.
 *   error_size: Size of error buffer.
 *
 * Return Values:
 *   SALT_OK (0): Success; key_b64 and optionally key_id populated.
 *   SALT_ERR_INPUT (1): Input parsing failed or malformed.
 *
 * Behavior:
 *   - If key_format == BASE64: treats input as plain base64 (no JSON parsing).
 *   - If key_format == JSON: expects JSON object (fails if not JSON).
 *   - If key_format == AUTO: detects JSON by checking for leading '{'.
 *   - Trims leading whitespace and field values.
 *
 * Notes:
 *   - Does NOT validate base64 encoding or resulting key length.
 *   - Rejects empty key inputs.
 *
 * Example:
 *   char key_b64[256], key_id[64];
 *   bool has_id = false;
 *   int rc = salt_cli_parse_key_input(
 *       "{\"key\":\"jQ7xQ+...\", \"key_id\":\"k123\"}",
 *       SALT_CLI_KEY_FORMAT_AUTO, key_b64, sizeof(key_b64),
 *       key_id, sizeof(key_id), &has_id, error, sizeof(error));
 */
int salt_cli_parse_key_input(const char *key_input, enum salt_cli_key_format key_format,
                             char *key_b64, size_t key_b64_size, char *key_id, size_t key_id_size,
                             bool *has_key_id, char *error, size_t error_size);

#endif
