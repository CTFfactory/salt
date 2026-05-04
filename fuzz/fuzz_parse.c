/*
 * SPDX-License-Identifier: Unlicense
 *
 * libFuzzer harness for key-input parsing and strict JSON key-object handling.
 */

#include "../src/cli/internal.h"
#include "fuzz_common.h"

#include <sodium.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_MAX_INPUT (32U * 1024U)
#define FUZZ_MAX_KEY_B64 1024U
#define FUZZ_MAX_KEY_ID 1024U
#define FUZZ_MAX_ERROR 256U

static int fuzz_parse_initialized;

static void fuzz_parse_ensure_init(void) {
    if (fuzz_parse_initialized != 0) {
        return;
    }
    /*
     * salt_cli_parse_key_input decodes base64 via libsodium; libsodium
     * requires sodium_init() to be called once before any helper. Without
     * this the fuzzer would race the lazy init paths inconsistently and
     * produce flaky reproducers.
     */
    if (sodium_init() < 0) {
        abort();
    }
    fuzz_parse_initialized = 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char input[FUZZ_MAX_INPUT + 1U];
    char json_input[FUZZ_MAX_INPUT + 16U];
    char key_b64[FUZZ_MAX_KEY_B64];
    char key_id[FUZZ_MAX_KEY_ID];
    char error[FUZZ_MAX_ERROR];
    bool has_key_id = false;
    enum salt_cli_key_format key_format = SALT_CLI_KEY_FORMAT_AUTO;
    size_t key_b64_size = sizeof(key_b64);
    size_t key_id_size = sizeof(key_id);

    fuzz_parse_ensure_init();

    if (size == 0U) {
        return 0;
    }

    switch (data[0] % 3U) {
    case 0U:
        key_format = SALT_CLI_KEY_FORMAT_AUTO;
        break;
    case 1U:
        key_format = SALT_CLI_KEY_FORMAT_BASE64;
        break;
    default:
        key_format = SALT_CLI_KEY_FORMAT_JSON;
        break;
    }

    key_b64_size = 1U + ((size > 1U ? data[1] : 0U) % (sizeof(key_b64) - 1U));
    key_id_size = 1U + ((size > 2U ? data[2] : 0U) % (sizeof(key_id) - 1U));

    (void)fuzz_copy_cstr(input, sizeof(input), (size > 1U) ? (data + 1U) : NULL,
                         (size > 1U) ? (size - 1U) : 0U);

    /* Annex K memset_s is unavailable on glibc; size bounded by sizeof(error). */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(error, 0, sizeof(error));
    (void)salt_cli_parse_key_input(input, key_format, key_b64, key_b64_size, key_id, key_id_size,
                                   &has_key_id, error, sizeof(error));

    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(error, 0, sizeof(error));
    (void)salt_cli_parse_key_json_object_strict(input, key_b64, key_b64_size, key_id, key_id_size,
                                                &has_key_id, error, sizeof(error));

    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)snprintf(json_input, sizeof(json_input), " \t%s\n", input);
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(error, 0, sizeof(error));
    (void)salt_cli_parse_key_json_object_strict(json_input, key_b64, key_b64_size, key_id,
                                                key_id_size, &has_key_id, error, sizeof(error));

    salt_cli_trim_whitespace_inplace(key_b64, sizeof(key_b64));
    salt_cli_trim_whitespace_inplace(key_id, sizeof(key_id));

    return 0;
}
