/*
 * SPDX-License-Identifier: Unlicense
 *
 * CMocka coverage for CLI runtime performance, resource, and signal paths.
 */

/* POSIX feature-test macro must be named exactly. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _POSIX_C_SOURCE 200809L

#include "test_cli_shared.h"
#include "test_suites.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

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
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
