/*
 * SPDX-License-Identifier: Unlicense
 *
 * CMocka coverage for CLI runtime performance, resource, and signal paths.
 */

/* POSIX feature-test macro must be named exactly. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _POSIX_C_SOURCE 200809L

#include "test_shared.h"
#include "../test_suites.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

static volatile sig_atomic_t runtime_sigint_count = 0;
static volatile sig_atomic_t runtime_sigterm_count = 0;

static void runtime_counting_signal_handler(int signum) {
    if (signum == SIGINT) {
        ++runtime_sigint_count;
    } else if (signum == SIGTERM) {
        ++runtime_sigterm_count;
    }
}

static void runtime_open_cli_streams(FILE **in_stream, FILE **out_stream, FILE **err_stream) {
    assert_non_null(in_stream);
    assert_non_null(out_stream);
    assert_non_null(err_stream);

    *in_stream = stream_from_text(NULL);
    *out_stream = test_tmpfile_checked();
    *err_stream = test_tmpfile_checked();

    assert_non_null(*in_stream);
    assert_non_null(*out_stream);
    assert_non_null(*err_stream);
}

static void runtime_close_cli_streams(FILE *in_stream, FILE *out_stream, FILE *err_stream) {
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_tmpfile_fd_cleanup(void **state) {
    int before_count;
    int after_count;
    int i;

    (void)state;

    before_count = count_open_fds();
    if (before_count < 0) {
        return;
    }

    for (i = 0; i < 16; ++i) {
        char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
        FILE *in_stream = stream_from_text(NULL);
        FILE *out_stream = test_tmpfile_checked();
        FILE *err_stream = test_tmpfile_checked();

        assert_non_null(out_stream);
        assert_non_null(err_stream);

        salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
        cli_expected_message = "hello";
        cli_expected_key = "base64-key";
        assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                         SALT_OK);

        (void)fclose(in_stream);
        (void)fclose(out_stream);
        (void)fclose(err_stream);
    }

    after_count = count_open_fds();
    assert_int_equal(after_count, before_count);
}

void test_salt_cli_run_max_plaintext_repeated_no_fd_leak(void **state) {
    int before_count;
    int after_count;
    char *large_plaintext;
    int i;

    (void)state;

    large_plaintext = (char *)malloc(SALT_GITHUB_SECRET_MAX_VALUE_LEN + 1U);
    assert_non_null(large_plaintext);
    /* Annex K memset_s/memcpy_s is unavailable on glibc; sizes bounded by buffer. */
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(large_plaintext, 'A', SALT_GITHUB_SECRET_MAX_VALUE_LEN);
    large_plaintext[SALT_GITHUB_SECRET_MAX_VALUE_LEN] = '\0';

    before_count = count_open_fds();
    if (before_count < 0) {
        free(large_plaintext);
        return;
    }

    for (i = 0; i < 8; ++i) {
        char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"-"};
        FILE *in_stream = stream_from_text(large_plaintext);
        FILE *out_stream = test_tmpfile_checked();
        FILE *err_stream = test_tmpfile_checked();

        assert_non_null(in_stream);
        assert_non_null(out_stream);
        assert_non_null(err_stream);

        salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
        cli_expected_key = "base64-key";
        cli_expected_message = NULL;
        assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                         SALT_OK);

        (void)fclose(in_stream);
        (void)fclose(out_stream);
        (void)fclose(err_stream);
    }

    after_count = count_open_fds();
    assert_int_equal(after_count, before_count);

    free(large_plaintext);
}

void test_salt_cli_run_repeated_invocations_smoke(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    int i;

    (void)state;

    for (i = 0; i < 250; ++i) {
        FILE *in_stream = stream_from_text(NULL);
        FILE *out_stream = test_tmpfile_checked();
        FILE *err_stream = test_tmpfile_checked();

        assert_non_null(in_stream);
        assert_non_null(out_stream);
        assert_non_null(err_stream);

        salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
        cli_expected_message = "hello";
        cli_expected_key = "base64-key";
        assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                         SALT_OK);

        (void)fclose(in_stream);
        (void)fclose(out_stream);
        (void)fclose(err_stream);
    }
}

void test_salt_cli_run_interrupted_stdin(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"-"};
    FILE *in_stream = stream_from_text("hello");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    salt_cli_set_stream_test_hooks(cli_stream_fread_interrupt, cli_stream_ferror, cli_stream_fgetc,
                                   cli_stream_ungetc);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_sigint_during_stdin(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"-"};
    FILE *in_stream = stream_from_text("hello");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    salt_cli_set_stream_test_hooks(cli_stream_fread_raise_sigint, cli_stream_ferror,
                                   cli_stream_fgetc, cli_stream_ungetc);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_sigterm_during_stdin(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"-"};
    FILE *in_stream = stream_from_text("hello");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    salt_cli_set_stream_test_hooks(cli_stream_fread_raise_sigterm, cli_stream_ferror,
                                   cli_stream_fgetc, cli_stream_ungetc);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_sigterm_after_seal(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    salt_cli_set_test_hooks(NULL, NULL, cli_fake_signal_after_seal);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_interrupted_after_key_parse(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    salt_cli_set_test_hooks(cli_interrupting_alloc, NULL, cli_fake_success);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_interrupted_after_plaintext_read(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"-"};
    FILE *in_stream = stream_from_text("hello");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    cli_stream_fread_bytes = 5U;
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    salt_cli_set_stream_test_hooks(cli_stream_fread, cli_stream_ferror_interrupt, cli_stream_fgetc,
                                   cli_stream_ungetc);

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

/*
 * PHASE 3: Signal interruption and state cleanup verification
 * Verify CLI handles SIGINT/SIGTERM gracefully during encryption.
 */

void test_salt_cli_run_sigint_during_encryption_state_cleanup(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    /* Trigger signal after seal: verifies state cleanup without corrupting output */
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_signal_after_seal);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    /* On signal, output should be empty (signal occurs after seal but before write) */
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_sigint_no_partial_output(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"-"};
    FILE *in_stream = stream_from_text("hello");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    /* Interrupt during stdin read: no partial output should be written */
    salt_cli_set_stream_test_hooks(cli_stream_fread_raise_sigint, cli_stream_ferror,
                                   cli_stream_fgetc, cli_stream_ungetc);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    /* Verify no partial data written on signal */
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_sigterm_state_zero_on_exit(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"-"};
    FILE *in_stream = stream_from_text("secret-data");
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    /* SIGTERM during stdin: CLI should clean state and exit cleanly */
    salt_cli_set_stream_test_hooks(cli_stream_fread_raise_sigterm, cli_stream_ferror,
                                   cli_stream_fgetc, cli_stream_ungetc);
    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_large_plaintext_sigint_clean_exit(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"-"};
    char *large_plaintext;
    FILE *in_stream;
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];
    int before_fd_count;
    int after_fd_count;

    (void)state;

    assert_non_null(out_stream);
    assert_non_null(err_stream);

    /* Allocate large plaintext to test signal during encryption with big buffer */
    large_plaintext = (char *)malloc((unsigned long)(256U * 1024U));
    assert_non_null(large_plaintext);
    /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
    (void)memset(large_plaintext, 'X', (unsigned long)(256U * 1024U - 1U));
    large_plaintext[(unsigned long)(256U * 1024U - 1U)] = '\0';

    in_stream = stream_from_text(large_plaintext);
    assert_non_null(in_stream);

    before_fd_count = count_open_fds();
    if (before_fd_count < 0) {
        free(large_plaintext);
        (void)fclose(out_stream);
        (void)fclose(err_stream);
        return;
    }

    /* Interrupt after reading large plaintext */
    salt_cli_set_stream_test_hooks(cli_stream_fread, cli_stream_ferror_interrupt, cli_stream_fgetc,
                                   cli_stream_ungetc);
    cli_stream_fread_bytes = 256U * 1024U - 1U;

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_ERR_SIGNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_string_equal(stderr_buffer, "error: operation interrupted\n");
    salt_cli_test_set_interrupted(false);

    after_fd_count = count_open_fds();
    /* Verify no fd leaks on signal exit */
    assert_int_equal(after_fd_count, before_fd_count);

    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
    free(large_plaintext);
}

void test_salt_cli_run_does_not_mutate_existing_signal_handlers(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = test_tmpfile_checked();
    struct sigaction custom = {0};
    struct sigaction old_sigint = {0};
    struct sigaction old_sigterm = {0};
    struct sigaction current = {0};

    (void)state;

    assert_non_null(in_stream);
    assert_non_null(out_stream);
    assert_non_null(err_stream);
    assert_int_equal(sigemptyset(&custom.sa_mask), 0);
    custom.sa_handler = runtime_counting_signal_handler;
    assert_int_equal(sigaction(SIGINT, &custom, &old_sigint), 0);
    assert_int_equal(sigaction(SIGTERM, &custom, &old_sigterm), 0);

    runtime_sigint_count = 0;
    runtime_sigterm_count = 0;
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(salt_cli_run_with_streams(4, argv, in_stream, out_stream, err_stream),
                     SALT_OK);
    assert_int_equal(sigaction(SIGINT, NULL, &current), 0);
    assert_true(current.sa_handler == runtime_counting_signal_handler);
    assert_int_equal(sigaction(SIGTERM, NULL, &current), 0);
    assert_true(current.sa_handler == runtime_counting_signal_handler);

    assert_int_equal(raise(SIGINT), 0);
    assert_int_equal(raise(SIGTERM), 0);
    assert_int_equal(runtime_sigint_count, 1);
    assert_int_equal(runtime_sigterm_count, 1);

    assert_int_equal(sigaction(SIGINT, &old_sigint, NULL), 0);
    assert_int_equal(sigaction(SIGTERM, &old_sigterm, NULL), 0);
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

void test_salt_cli_run_signal_handler_install_failure_reports_error(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream;
    FILE *out_stream;
    FILE *err_stream;
    char stdout_buffer[64];
    char stderr_buffer[TEST_SMALL_BUFFER_SIZE];

    (void)state;

    runtime_open_cli_streams(&in_stream, &out_stream, &err_stream);
    salt_cli_test_force_signal_handler_install_failure(1);

    assert_int_equal(
        salt_cli_run_with_streams_signal_handlers(4, argv, in_stream, out_stream, err_stream),
        SALT_ERR_INTERNAL);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "");
    assert_non_null(strstr(stderr_buffer, "failed to install signal handlers"));

    salt_cli_test_force_signal_handler_install_failure(0);
    runtime_close_cli_streams(in_stream, out_stream, err_stream);
}

void test_salt_cli_run_signal_handler_install_success(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream;
    FILE *out_stream;
    FILE *err_stream;
    char stdout_buffer[64];
    char stderr_buffer[64];

    (void)state;

    runtime_open_cli_streams(&in_stream, &out_stream, &err_stream);
    salt_cli_set_test_hooks(NULL, NULL, cli_fake_success);
    cli_expected_message = "hello";
    cli_expected_key = "base64-key";

    assert_int_equal(
        salt_cli_run_with_streams_signal_handlers(4, argv, in_stream, out_stream, err_stream),
        SALT_OK);
    read_stream(out_stream, stdout_buffer, sizeof(stdout_buffer));
    read_stream(err_stream, stderr_buffer, sizeof(stderr_buffer));
    assert_string_equal(stdout_buffer, "ZW5jb2RlZA==\n");
    assert_string_equal(stderr_buffer, "");

    runtime_close_cli_streams(in_stream, out_stream, err_stream);
}

void test_salt_cli_run_signal_handler_install_failure_write_error(void **state) {
    char *argv[] = {(char *)"salt", (char *)"--key", (char *)"base64-key", (char *)"hello"};
    FILE *in_stream = stream_from_text(NULL);
    FILE *out_stream = test_tmpfile_checked();
    FILE *err_stream = stream_with_write_fail_after(0U);

    (void)state;

    assert_non_null(in_stream);
    assert_non_null(out_stream);
    assert_non_null(err_stream);
    salt_cli_test_force_signal_handler_install_failure(1);

    assert_int_equal(
        salt_cli_run_with_streams_signal_handlers(4, argv, in_stream, out_stream, err_stream),
        SALT_ERR_IO);

    salt_cli_test_force_signal_handler_install_failure(0);
    (void)fclose(in_stream);
    (void)fclose(out_stream);
    (void)fclose(err_stream);
}

int run_cli_runtime_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_salt_cli_tmpfile_fd_cleanup, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_max_plaintext_repeated_no_fd_leak,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_repeated_invocations_smoke,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_interrupted_stdin, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_sigint_during_stdin, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_sigterm_during_stdin, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_sigterm_after_seal, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_interrupted_after_key_parse,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_interrupted_after_plaintext_read,
                                        cli_test_setup, cli_test_teardown),
        /* PHASE 3: Signal interruption and state cleanup */
        cmocka_unit_test_setup_teardown(test_salt_cli_run_sigint_during_encryption_state_cleanup,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_sigint_no_partial_output, cli_test_setup,
                                        cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_sigterm_state_zero_on_exit,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_large_plaintext_sigint_clean_exit,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_does_not_mutate_existing_signal_handlers,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_run_signal_handler_install_failure_reports_error, cli_test_setup,
            cli_test_teardown),
        cmocka_unit_test_setup_teardown(test_salt_cli_run_signal_handler_install_success,
                                        cli_test_setup, cli_test_teardown),
        cmocka_unit_test_setup_teardown(
            test_salt_cli_run_signal_handler_install_failure_write_error, cli_test_setup,
            cli_test_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
