/*
 * SPDX-License-Identifier: Unlicense
 *
 * Shared support for CLI-focused cmocka tests.
 */

/* GNU extensions are required for fopencookie(). */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _GNU_SOURCE
/* POSIX feature-test macro must be named exactly. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _POSIX_C_SOURCE 200809L

#include "test_shared.h"

#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cmocka.h>

static cli_test_context *g_cli_ctx = NULL;

struct failing_write_cookie {
    size_t writes_before_fail;
    size_t writes_seen;
};

FILE *test_tmpfile_checked(void) {
    static const char suffix[] = "salt-cli-test.XXXXXX";
    const char *tmpdir = getenv("TMPDIR");
    size_t tmpdir_len;
    size_t suffix_len = sizeof(suffix) - 1U;
    size_t path_len;
    char *path;
    int need_sep;
    int fd;
    FILE *stream;

    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = "/tmp";
    }

    tmpdir_len = strlen(tmpdir);
    need_sep = (tmpdir_len == 0U || tmpdir[tmpdir_len - 1U] != '/') ? 1 : 0;
    assert_true(tmpdir_len <= SIZE_MAX - suffix_len - (size_t)need_sep - 1U);
    path_len = tmpdir_len + (size_t)need_sep + suffix_len + 1U;
    path = (char *)malloc(path_len);
    assert_non_null(path);

    for (size_t i = 0U; i < tmpdir_len; ++i) {
        path[i] = tmpdir[i];
    }
    if (need_sep != 0) {
        path[tmpdir_len] = '/';
    }
    for (size_t i = 0U; i <= suffix_len; ++i) {
        path[tmpdir_len + (size_t)need_sep + i] = suffix[i];
    }

    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(unlink(path), 0);
    free(path);

    stream = fdopen(fd, "w+");
    if (stream == NULL) {
        (void)close(fd);
    }
    assert_non_null(stream);
    return stream;
}

FILE *test_fopen_checked(const char *path, const char *mode) {
    mode_t old_umask = 0;
    int mode_creates_file = 0;
    FILE *stream;

    assert_non_null(path);
    assert_non_null(mode);

    for (size_t i = 0U; mode[i] != '\0'; ++i) {
        if (mode[i] == 'w' || mode[i] == 'a' || mode[i] == 'x') {
            mode_creates_file = 1;
            break;
        }
    }

    if (mode_creates_file) {
        old_umask = umask(S_IRWXG | S_IRWXO);
    }

    stream = fopen(path, mode);
    if (mode_creates_file) {
        (void)umask(old_umask);
    }
    assert_non_null(stream);
    return stream;
}

cli_test_context *cli_get_ctx(void) {
    assert_non_null(g_cli_ctx);
    return g_cli_ctx;
}

void *cli_failing_alloc(size_t size) {
    (void)size;
    return NULL;
}

void *cli_fail_on_nth_alloc(size_t size) {
    ++cli_alloc_call_count;
    if (cli_fail_alloc_on_call != 0U && cli_alloc_call_count == cli_fail_alloc_on_call) {
        return NULL;
    }
    return malloc(size);
}

void *cli_interrupting_alloc(size_t size) {
    ++cli_alloc_call_count;
    if (cli_alloc_call_count == 1U) {
        salt_cli_test_set_interrupted(true);
    }
    return malloc(size);
}

void cli_passthrough_free(void *ptr) {
    free(ptr);
}

size_t cli_stream_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    size_t bytes_to_write = cli_stream_fread_bytes;

    (void)stream;
    if (bytes_to_write > total) {
        bytes_to_write = total;
    }
    if (bytes_to_write > 0U) {
        /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
        /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
        (void)memset(ptr, 'R', bytes_to_write);
    }
    if (size == 0U) {
        return 0U;
    }
    return bytes_to_write / size;
}

int cli_stream_ferror(FILE *stream) {
    (void)stream;
    return cli_stream_ferror_code;
}

int cli_stream_ferror_interrupt(FILE *stream) {
    (void)stream;
    salt_cli_test_set_interrupted(true);
    return 0;
}

int cli_stream_fgetc(FILE *stream) {
    (void)stream;
    return cli_stream_fgetc_code;
}

int cli_stream_ungetc(int c, FILE *stream) {
    (void)stream;
    cli_stream_fgetc_code = c;
    return c;
}

/* Simulate an interrupted stdin read without delivering a real signal. */
size_t cli_stream_fread_interrupt(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    salt_cli_test_set_interrupted(true);
    return 0U;
}

/* Simulate SIGINT while the CLI is blocked in stdin ingestion. */
size_t cli_stream_fread_raise_sigint(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    salt_cli_test_set_interrupted(true);
    return 0U;
}

/* Simulate SIGTERM while the CLI is blocked in stdin ingestion. */
size_t cli_stream_fread_raise_sigterm(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    salt_cli_test_set_interrupted(true);
    return 0U;
}

/* NOLINTBEGIN(readability-non-const-parameter) -- signature must match salt_seal_base64 ABI. */
int cli_fake_error(const unsigned char *message, size_t message_len, const char *public_key_b64,
                   char *output, size_t output_size, size_t *output_written, char *error,
                   size_t error_size) {
    (void)message;
    (void)message_len;
    (void)public_key_b64;
    (void)output;
    (void)output_size;
    (void)output_written;
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)snprintf(error, error_size, "simulated failure");
    return SALT_ERR_INPUT;
}
/* NOLINTEND(readability-non-const-parameter) */

/* NOLINTBEGIN(readability-non-const-parameter) -- signature must match salt_seal_base64 ABI. */
int cli_fake_zero_output(const unsigned char *message, size_t message_len,
                         const char *public_key_b64, char *output, size_t output_size,
                         size_t *output_written, char *error, size_t error_size) {
    (void)message;
    (void)message_len;
    (void)public_key_b64;
    (void)output_size;
    (void)error;
    (void)error_size;
    output[0] = '\0';
    *output_written = 0U;
    return SALT_OK;
}
/* NOLINTEND(readability-non-const-parameter) */

/* NOLINTBEGIN(readability-non-const-parameter) -- signature must match salt_seal_base64 ABI. */
int cli_fake_success(const unsigned char *message, size_t message_len, const char *public_key_b64,
                     char *output, size_t output_size, size_t *output_written, char *error,
                     size_t error_size) {
    const char *encoded = (cli_fake_encoded != NULL) ? cli_fake_encoded : "ZW5jb2RlZA==";

    if (cli_expected_message != NULL) {
        assert_int_equal((int)message_len, (int)strlen(cli_expected_message));
        assert_memory_equal(message, cli_expected_message, message_len);
    }
    if (cli_expected_key != NULL) {
        assert_string_equal(public_key_b64, cli_expected_key);
    }
    (void)error;
    (void)error_size;
    assert_true(output_size > strlen(encoded));
    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memcpy(output, encoded, strlen(encoded) + 1U);
    *output_written = strlen(encoded);
    return SALT_OK;
}
/* NOLINTEND(readability-non-const-parameter) */

int cli_fake_signal_after_seal(const unsigned char *message, size_t message_len,
                               const char *public_key_b64, char *output, size_t output_size,
                               size_t *output_written, char *error, size_t error_size) {
    int rc = cli_fake_success(message, message_len, public_key_b64, output, output_size,
                              output_written, error, error_size);

    salt_cli_test_set_interrupted(true);
    return rc;
}

int cli_test_setup(void **state) {
    cli_test_context *ctx = (cli_test_context *)calloc(1U, sizeof(*ctx));

    assert_non_null(state);
    assert_non_null(ctx);

    *state = ctx;
    g_cli_ctx = ctx;
    cli_expected_message = NULL;
    cli_expected_key = NULL;
    cli_fake_encoded = "ZW5jb2RlZA==";
    cli_alloc_call_count = 0U;
    cli_fail_alloc_on_call = 0U;
    cli_stream_fread_bytes = 0U;
    cli_stream_ferror_code = 0;
    cli_stream_fgetc_code = EOF;
    salt_cli_reset_test_hooks();
    salt_cli_test_force_signal_handler_install_failure(0);
    return 0;
}

int cli_test_teardown(void **state) {
    cli_test_context *ctx;

    assert_non_null(state);
    ctx = (cli_test_context *)(*state);
    assert_non_null(ctx);

    g_cli_ctx = ctx;
    cli_expected_message = NULL;
    cli_expected_key = NULL;
    cli_fake_encoded = "ZW5jb2RlZA==";
    cli_alloc_call_count = 0U;
    cli_fail_alloc_on_call = 0U;
    cli_stream_fread_bytes = 0U;
    cli_stream_ferror_code = 0;
    cli_stream_fgetc_code = EOF;
    salt_cli_reset_test_hooks();
    salt_cli_test_force_signal_handler_install_failure(0);

    g_cli_ctx = NULL;
    free(ctx);
    *state = NULL;
    return 0;
}

void read_stream(FILE *stream, char *buffer, size_t buffer_size) {
    size_t bytes_read;

    assert_non_null(stream);
    assert_true(buffer_size > 0U);
    (void)fflush(stream);
    assert_int_equal(fseek(stream, 0L, SEEK_SET), 0);
    bytes_read = fread(buffer, 1U, buffer_size - 1U, stream);
    buffer[bytes_read] = '\0';
}

FILE *stream_from_text(const char *text) {
    FILE *stream = test_tmpfile_checked();
    size_t written;

    assert_non_null(stream);
    if (text != NULL) {
        written = fwrite(text, 1U, strlen(text), stream);
        assert_int_equal((int)written, (int)strlen(text));
    }
    assert_int_equal(fseek(stream, 0L, SEEK_SET), 0);
    return stream;
}

void assert_string_starts_with(const char *actual, const char *prefix) {
    size_t prefix_len;

    assert_non_null(actual);
    assert_non_null(prefix);

    prefix_len = strlen(prefix);
    assert_true(strncmp(actual, prefix, prefix_len) == 0);
}

static ssize_t failing_stream_write(void *cookie, const char *buf, size_t size) {
    struct failing_write_cookie *ctx = (struct failing_write_cookie *)cookie;

    (void)buf;
    if (ctx == NULL) {
        errno = EIO;
        return -1;
    }
    if (ctx->writes_seen >= ctx->writes_before_fail) {
        errno = EIO;
        return -1;
    }
    ++ctx->writes_seen;
    return (ssize_t)size;
}

static int failing_stream_close(void *cookie) {
    free(cookie);
    return 0;
}

FILE *stream_with_write_fail_after(size_t writes_before_fail) {
    struct failing_write_cookie *ctx = (struct failing_write_cookie *)calloc(1U, sizeof(*ctx));
    cookie_io_functions_t io = {
        .read = NULL,
        .write = failing_stream_write,
        .seek = NULL,
        .close = failing_stream_close,
    };

    assert_non_null(ctx);
    ctx->writes_before_fail = writes_before_fail;
    return fopencookie(ctx, "w", io);
}

int count_open_fds(void) {
    DIR *dir = opendir("/proc/self/fd");
    struct dirent *entry;
    int count = 0;

    if (dir == NULL) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        ++count;
    }

    closedir(dir);
    return count;
}
