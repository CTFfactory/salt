---
description: 'Conventions for maintaining GNU Autotools and GNU Build System inputs in this repository'
applyTo: '**/configure.ac,**/Makefile.am,**/bootstrap'
---

# GNU Autotools Instructions

Follow these rules when editing Autoconf/Automake inputs and bootstrap flow.

## Core Rules

- Keep `configure.ac` and `Makefile.am` as the source of truth; do not hardcode generated-file edits.
- Keep configure checks explicit and fail-fast using `AC_MSG_ERROR` for required dependencies.
- Keep feature toggles deterministic via `AC_ARG_ENABLE` plus explicit defaults.
- Keep all externally consumed variables (`AC_SUBST`) and conditionals (`AM_CONDITIONAL`) synchronized with `Makefile.am` usage.
- Keep bootstrap minimal and deterministic (`autoreconf -fi` only unless a required, documented step is added).
- Keep compile and test flags centralized in configure substitutions rather than duplicating hardcoded flag blocks.

## Dependency Detection

- Prefer `pkg-config` checks first when available, then fall back to direct header/library checks.
- For required dependencies (libsodium, cmocka when tests are enabled), fail during configure rather than deferring failures to compile or link time.
- Keep optional tooling checks (for example valgrind) explicit and surfaced via Automake conditionals.

## Automake Layout

- Keep source lists grouped and reused through variables (for example `SALT_SRCS`, `TEST_SRCS`) instead of duplicating lists per target.
- Keep non-installed/internal headers in `noinst_HEADERS`; keep only public headers in `include_HEADERS`.
- Keep repository scripts needed by distribution archives listed in `EXTRA_DIST`.
- Keep custom test targets deterministic and explicit about required binaries, flags, and exit behavior.

## Example Patterns

```m4
AC_ARG_ENABLE([tests],
  [AS_HELP_STRING([--disable-tests], [skip cmocka-based unit test checks])],
  [],
  [enable_tests=yes])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" = "xyes"])
```

```m4
AS_IF(
  [test -n "$PKG_CONFIG" && $PKG_CONFIG --exists libsodium],
  [SODIUM_LIBS=`$PKG_CONFIG --libs libsodium`],
  [AC_CHECK_LIB([sodium], [sodium_init], [SODIUM_LIBS="-lsodium"], [AC_MSG_ERROR([libsodium library is required])])]
)
```

```makefile
if ENABLE_TESTS
check_PROGRAMS = test_salt
TESTS = test_salt
endif
```
