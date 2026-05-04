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

#include "signals.h"
#include "stream.h"
#include "salt.h"

#include <stdbool.h>
#include <signal.h>

/*
 * Signal disposition is process-wide, so this interruption flag remains
 * process-scoped as well. The CLI is therefore not reentrant for concurrent
 * embedded callers even though the test-hook override state is thread-local.
 */
static volatile sig_atomic_t salt_cli_interrupted = 0;
static struct sigaction salt_cli_saved_sigint_action;
static struct sigaction salt_cli_saved_sigterm_action;
static bool salt_cli_have_saved_sigint = false;
static bool salt_cli_have_saved_sigterm = false;
#ifdef SALT_NO_MAIN
static _Thread_local int salt_cli_force_signal_install_failure = 0;
#endif

static void salt_cli_signal_handler(int signal_number) {
    (void)signal_number;
    salt_cli_interrupted = 1;
}

int salt_cli_ensure_signal_handlers(void) {
    struct sigaction action = {0};

#ifdef SALT_NO_MAIN
    if (salt_cli_force_signal_install_failure != 0) {
        return SALT_ERR_INTERNAL;
    }
#endif

    action.sa_handler = salt_cli_signal_handler;
    if (sigemptyset(&action.sa_mask) != 0) {
        return SALT_ERR_INTERNAL;
    }

    if (sigaction(SIGINT, &action, &salt_cli_saved_sigint_action) != 0) {
        return SALT_ERR_INTERNAL;
    }
    salt_cli_have_saved_sigint = true;

    if (sigaction(SIGTERM, &action, &salt_cli_saved_sigterm_action) != 0) {
        (void)sigaction(SIGINT, &salt_cli_saved_sigint_action, NULL);
        salt_cli_have_saved_sigint = false;
        return SALT_ERR_INTERNAL;
    }
    salt_cli_have_saved_sigterm = true;

    return SALT_OK;
}

void salt_cli_restore_signal_handlers(void) {
    if (salt_cli_have_saved_sigint) {
        (void)sigaction(SIGINT, &salt_cli_saved_sigint_action, NULL);
        salt_cli_have_saved_sigint = false;
    }
    if (salt_cli_have_saved_sigterm) {
        (void)sigaction(SIGTERM, &salt_cli_saved_sigterm_action, NULL);
        salt_cli_have_saved_sigterm = false;
    }
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

void salt_cli_test_force_signal_handler_install_failure(int value) {
    salt_cli_force_signal_install_failure = (value != 0) ? 1 : 0;
}
#endif
