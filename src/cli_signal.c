/*
 * SPDX-License-Identifier: Unlicense
 *
 * CLI signal handling implementation.
 *
 * This file implements process-wide signal disposition setup for graceful
 * interruption of long-running stdin reads.
 */

/* POSIX feature-test macro must be named exactly. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier) */
#define _POSIX_C_SOURCE 200809L

#include "cli_signal.h"
#include "cli_stream.h"
#include "salt.h"

#include <signal.h>

/*
 * Signal disposition is process-wide, so this interruption flag remains
 * process-scoped as well. The CLI is therefore not reentrant for concurrent
 * embedded callers even though the test-hook override state is thread-local.
 */
static volatile sig_atomic_t salt_cli_interrupted = 0;

static void salt_cli_signal_handler(int signal_number) {
    (void)signal_number;
    salt_cli_interrupted = 1;
}

void salt_cli_ensure_signal_handlers(void) {
    struct sigaction action = {0};

    /* The CLI intentionally owns SIGINT/SIGTERM handling for the process. */
    action.sa_handler = salt_cli_signal_handler;
    (void)sigemptyset(&action.sa_mask);

    (void)sigaction(SIGINT, &action, NULL);
    (void)sigaction(SIGTERM, &action, NULL);
}

int salt_cli_check_interrupted(char *error, size_t error_size) {
    if (salt_cli_interrupted != 0) {
        return salt_cli_set_error(error, error_size, "operation interrupted", SALT_ERR_SIGNAL);
    }
    return SALT_OK;
}

void salt_cli_reset_interrupted(void) {
    salt_cli_interrupted = 0;
}

#ifdef SALT_NO_MAIN
void salt_cli_test_set_interrupted_flag(int value) {
    salt_cli_interrupted = value;
}
#endif
