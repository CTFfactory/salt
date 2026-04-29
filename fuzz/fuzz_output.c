/*
 * SPDX-License-Identifier: Unlicense
 *
 * libFuzzer harness for text/JSON output emission and escaping behavior.
 */

/* NOLINTBEGIN(bugprone-reserved-identifier) -- _POSIX_C_SOURCE required for open_memstream */
#define _POSIX_C_SOURCE 200809L
/* NOLINTEND(bugprone-reserved-identifier) */

#include "../src/cli_output.h"
#include "fuzz_common.h"

#include <sodium.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_MAX_FIELD (64U * 1024U)
#define FUZZ_OUT_BUF (256U * 1024U)
#define FUZZ_MAX_ERROR 256U

static int fuzz_output_initialized;

static void fuzz_output_ensure_init(void) {
    if (fuzz_output_initialized != 0) {
        return;
    }
    if (sodium_init() < 0) {
        abort();
    }
    fuzz_output_initialized = 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char encrypted_value[FUZZ_MAX_FIELD + 1U];
    char key_id[FUZZ_MAX_FIELD + 1U];
    char error[FUZZ_MAX_ERROR];
    /*
     * fmemopen against an in-memory buffer is roughly two orders of
     * magnitude faster than tmpfile() (no filesystem syscalls), which
     * directly translates into more execs/sec and wider coverage per
     * libFuzzer second.
     */
    static char out_buf[FUZZ_OUT_BUF];
    FILE *out_stream;
    size_t split = 0U;
    enum salt_cli_output_mode output_mode = SALT_CLI_OUTPUT_TEXT;
    const char *effective_key_id = key_id;

    fuzz_output_ensure_init();

    if (size == 0U) {
        return 0;
    }

    output_mode = ((data[0] & 0x1U) == 0U) ? SALT_CLI_OUTPUT_TEXT : SALT_CLI_OUTPUT_JSON;

    if (size > 2U) {
        split = (size_t)data[1] % (size - 1U);
    }

    {
        const uint8_t *src_ptr = (size > 2U) ? (data + 2U) : NULL;
        size_t available = (size > 2U) ? (size - 2U) : 0U;
        size_t copy_len = (split < available) ? split : available;
        (void)fuzz_copy_cstr(encrypted_value, sizeof(encrypted_value), src_ptr, copy_len);
    }

    (void)fuzz_copy_cstr(key_id, sizeof(key_id), (size > 2U + split) ? (data + 2U + split) : NULL,
                         (size > 2U + split) ? (size - (2U + split)) : 0U);

    if ((data[0] & 0x6U) == 0x2U) {
        effective_key_id = "";
    } else if ((data[0] & 0x6U) == 0x4U) {
        effective_key_id = NULL;
    }

    out_stream = fmemopen(out_buf, sizeof(out_buf), "w");
    if (out_stream == NULL) {
        return 0;
    }

    /* Annex K memset_s is unavailable on glibc; size bounded by sizeof(error). */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(error, 0, sizeof(error));
    (void)salt_cli_emit_output(out_stream, output_mode, encrypted_value, effective_key_id, error,
                               sizeof(error));

    (void)fclose(out_stream);
    return 0;
}
