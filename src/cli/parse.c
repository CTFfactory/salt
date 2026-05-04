/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI key parsing and validation helpers.
 *
 * This file implements strict key-input parsing (base64 or JSON object)
 * with deterministic validation and whitespace handling.
 */

#include "salt.h"
#include "internal.h"

#include <stdarg.h>
#include <string.h>

/*
 * Explicit ASCII whitespace test. Avoid <ctype.h> isspace() which is
 * locale-dependent: under non-C locales, additional code points may be
 * classified as whitespace, which would silently trim or skip bytes the
 * cryptographic key parser must treat as significant.
 */
static int cli_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

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

static bool json_field_equals(const char *field, const char *expected) {
    /* JSON field names are not secret material; a plain strcmp is sufficient. */
    return strcmp(field, expected) == 0;
}

void salt_cli_trim_whitespace_inplace(char *value, size_t value_size) {
    const char *nul;
    size_t len;
    size_t start = 0U;
    size_t end;

    if (value == NULL || value_size == 0U) {
        return;
    }

    nul = (const char *)memchr(value, '\0', value_size);
    if (nul == NULL) {
        value[value_size - 1U] = '\0';
        len = value_size - 1U;
    } else {
        len = (size_t)(nul - value);
    }
    while (start < len && cli_is_ascii_ws((unsigned char)value[start]) != 0) {
        ++start;
    }
    if (start > 0U) {
        size_t i;
        for (i = 0U; i <= (len - start); ++i) {
            value[i] = value[start + i];
        }
    }

    len -= start;
    end = len;
    while (end > 0U && cli_is_ascii_ws((unsigned char)value[end - 1U]) != 0) {
        --end;
    }
    value[end] = '\0';
}

static void json_skip_ws(const char **cursor) {
    while (**cursor != '\0' && cli_is_ascii_ws((unsigned char)**cursor) != 0) {
        ++(*cursor);
    }
}

static int json_invalid_key_object(char *error, size_t error_size) {
    cli_set_error(error, error_size, "invalid JSON key object");
    return SALT_ERR_INPUT;
}

struct json_key_fields {
    char *key_b64;
    size_t key_b64_size;
    char *key_id;
    size_t key_id_size;
    bool key_set;
    bool key_id_set;
};

static bool json_parse_string_token(const char **cursor, char *out, size_t out_size) {
    size_t i = 0U;

    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0U) {
        return false;
    }

    if (**cursor != '"') {
        return false;
    }
    ++(*cursor);

    while (**cursor != '\0' && **cursor != '"') {
        /* Keep accepted escapes small and deterministic for CLI-safe parsing. */
        if (**cursor == '\\') {
            ++(*cursor);
            if (**cursor == '\0') {
                return false;
            }
            if (i + 1U >= out_size) {
                return false;
            }
            if (**cursor == 'n') {
                out[i++] = '\n';
            } else if (**cursor == 'r') {
                out[i++] = '\r';
            } else if (**cursor == 't') {
                out[i++] = '\t';
            } else if (**cursor == '"') {
                out[i++] = '"';
            } else if (**cursor == '\\') {
                out[i++] = '\\';
            } else if (**cursor == '/') {
                out[i++] = '/';
            } else {
                /* Reject unsupported escapes (\u, \b, \f, etc.) explicitly. */
                return false;
            }
            ++(*cursor);
            continue;
        }
        /*
         * RFC 8259 §7 forbids unescaped control characters (U+0000 through
         * U+001F) inside JSON strings. Reject them so malformed key_id or
         * field names cannot smuggle control bytes into stderr or the JSON
         * output envelope.
         */
        if ((unsigned char)**cursor < 0x20U) {
            return false;
        }
        if (i + 1U >= out_size) {
            return false;
        }
        out[i++] = **cursor;
        ++(*cursor);
    }

    if (**cursor != '"') {
        return false;
    }

    out[i] = '\0';
    ++(*cursor);
    return true;
}

static int json_parse_key_field(const char **cursor, char *key_b64, size_t key_b64_size,
                                char *error, size_t error_size) {
    if (!json_parse_string_token(cursor, key_b64, key_b64_size)) {
        cli_set_error(error, error_size, "JSON key object must include string field 'key'");
        return SALT_ERR_INPUT;
    }

    return SALT_OK;
}

static int json_parse_key_id_field(const char **cursor, char *key_id, size_t key_id_size,
                                   char *error, size_t error_size) {
    if (key_id == NULL || key_id_size == 0U ||
        !json_parse_string_token(cursor, key_id, key_id_size)) {
        cli_set_error(error, error_size, "JSON key object field 'key_id' must be a string");
        return SALT_ERR_INPUT;
    }

    return SALT_OK;
}

static int json_parse_field_name(const char **cursor, char *field, size_t field_size, char *error,
                                 size_t error_size) {
    json_skip_ws(cursor);
    if (!json_parse_string_token(cursor, field, field_size)) {
        return json_invalid_key_object(error, error_size);
    }

    json_skip_ws(cursor);
    if (**cursor != ':') {
        return json_invalid_key_object(error, error_size);
    }
    ++(*cursor);
    json_skip_ws(cursor);
    return SALT_OK;
}

static int json_parse_known_field(const char *field, const char **cursor,
                                  struct json_key_fields *fields, char *error, size_t error_size) {
    if (json_field_equals(field, "key")) {
        if (fields->key_set) {
            cli_set_error(error, error_size, "JSON key object has duplicate field 'key'");
            return SALT_ERR_INPUT;
        }
        if (json_parse_key_field(cursor, fields->key_b64, fields->key_b64_size, error,
                                 error_size) != SALT_OK) {
            return SALT_ERR_INPUT;
        }
        fields->key_set = true;
        return SALT_OK;
    }

    if (json_field_equals(field, "key_id")) {
        if (fields->key_id_set) {
            cli_set_error(error, error_size, "JSON key object has duplicate field 'key_id'");
            return SALT_ERR_INPUT;
        }
        if (json_parse_key_id_field(cursor, fields->key_id, fields->key_id_size, error,
                                    error_size) != SALT_OK) {
            return SALT_ERR_INPUT;
        }
        fields->key_id_set = true;
        return SALT_OK;
    }

    cli_set_error(error, error_size, "JSON key object contains unsupported field '%s'", field);
    return SALT_ERR_INPUT;
}

static int json_finish_key_fields(struct json_key_fields *fields, bool *has_key_id, char *error,
                                  size_t error_size) {
    if (!fields->key_set) {
        cli_set_error(error, error_size, "JSON key object must include string field 'key'");
        return SALT_ERR_INPUT;
    }

    salt_cli_trim_whitespace_inplace(fields->key_b64, fields->key_b64_size);
    if (fields->key_b64[0] == '\0') {
        cli_set_error(error, error_size, "JSON key field 'key' must not be empty");
        return SALT_ERR_INPUT;
    }

    if (fields->key_id_set) {
        salt_cli_trim_whitespace_inplace(fields->key_id, fields->key_id_size);
        if (fields->key_id[0] != '\0') {
            *has_key_id = true;
        }
    }

    return SALT_OK;
}

int salt_cli_parse_key_json_object_strict(const char *json, char *key_b64, size_t key_b64_size,
                                          char *key_id, size_t key_id_size, bool *has_key_id,
                                          char *error, size_t error_size) {
    const char *cursor = json;
    struct json_key_fields fields = {
        .key_b64 = key_b64,
        .key_b64_size = key_b64_size,
        .key_id = key_id,
        .key_id_size = key_id_size,
        .key_set = false,
        .key_id_set = false,
    };

    if (json == NULL || key_b64 == NULL || key_b64_size == 0U || has_key_id == NULL) {
        cli_set_error(error, error_size, "invalid JSON key parser arguments");
        return SALT_ERR_INPUT;
    }

    *has_key_id = false;
    if (key_id != NULL && key_id_size > 0U) {
        key_id[0] = '\0';
    }

    json_skip_ws(&cursor);
    if (*cursor != '{') {
        return json_invalid_key_object(error, error_size);
    }
    ++cursor;

    json_skip_ws(&cursor);
    if (*cursor == '}') {
        cli_set_error(error, error_size, "JSON key object must include string field 'key'");
        return SALT_ERR_INPUT;
    }

    /* Accept only known fields so caller intent cannot be ambiguous. */
    for (;;) {
        char field[32];

        if (json_parse_field_name(&cursor, field, sizeof(field), error, error_size) != SALT_OK) {
            return SALT_ERR_INPUT;
        }
        if (json_parse_known_field(field, &cursor, &fields, error, error_size) != SALT_OK) {
            return SALT_ERR_INPUT;
        }

        json_skip_ws(&cursor);
        if (*cursor == ',') {
            ++cursor;
            continue;
        }
        if (*cursor == '}') {
            ++cursor;
            break;
        }

        return json_invalid_key_object(error, error_size);
    }

    json_skip_ws(&cursor);
    if (*cursor != '\0') {
        return json_invalid_key_object(error, error_size);
    }

    return json_finish_key_fields(&fields, has_key_id, error, error_size);
}

static const char *skip_leading_ascii_ws(const char *value) {
    while (*value != '\0' && cli_is_ascii_ws((unsigned char)*value) != 0) {
        ++value;
    }
    return value;
}

static bool salt_cli_should_parse_key_as_json(enum salt_cli_key_format key_format,
                                              const char *cursor) {
    return key_format == SALT_CLI_KEY_FORMAT_JSON ||
           (key_format == SALT_CLI_KEY_FORMAT_AUTO && *cursor == '{');
}

static int salt_cli_copy_base64_key_input(const char *cursor, char *key_b64, size_t key_b64_size,
                                          char *error, size_t error_size) {
    size_t key_len = strlen(cursor);

    if (key_len >= key_b64_size) {
        cli_set_error(error, error_size, "public key buffer too small");
        return SALT_ERR_INPUT;
    }

    {
        size_t i;
        for (i = 0U; i <= key_len; ++i) {
            key_b64[i] = cursor[i];
        }
    }
    salt_cli_trim_whitespace_inplace(key_b64, key_b64_size);
    if (key_b64[0] == '\0') {
        cli_set_error(error, error_size, "public key must not be empty");
        return SALT_ERR_INPUT;
    }

    return SALT_OK;
}

int salt_cli_parse_key_input(const char *key_input, enum salt_cli_key_format key_format,
                             char *key_b64, size_t key_b64_size, char *key_id, size_t key_id_size,
                             bool *has_key_id, char *error, size_t error_size) {
    const char *cursor;

    if (key_input == NULL || key_b64 == NULL || key_b64_size == 0U || has_key_id == NULL) {
        cli_set_error(error, error_size, "invalid key input arguments");
        return SALT_ERR_INPUT;
    }

    *has_key_id = false;
    if (key_id != NULL && key_id_size > 0U) {
        key_id[0] = '\0';
    }

    cursor = skip_leading_ascii_ws(key_input);
    if (!salt_cli_should_parse_key_as_json(key_format, cursor)) {
        return salt_cli_copy_base64_key_input(cursor, key_b64, key_b64_size, error, error_size);
    }

    return salt_cli_parse_key_json_object_strict(cursor, key_b64, key_b64_size, key_id, key_id_size,
                                                 has_key_id, error, error_size);
}
