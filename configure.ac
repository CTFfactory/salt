AC_PREREQ([2.69])
AC_INIT([salt], [0.0.0], [https://github.com/CTFfactory/salt/issues], [salt], [https://github.com/CTFfactory/salt])
AC_CONFIG_SRCDIR([src/salt.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])

# Squash the implicit -g -O2 CFLAGS default BEFORE AM_INIT_AUTOMAKE.
# AM_INIT_AUTOMAKE's dependency-tracking test fires AC_PROG_CC via
# AC_REQUIRE, which runs before any code placed after AM_INIT_AUTOMAKE
# in this file. Setting CFLAGS="" here (only when the caller has not
# explicitly set CFLAGS) causes AC_PROG_CC to see CFLAGS as "already
# set" and skip the -g/-O2 defaults; BASE_CFLAGS owns all flag policy.
AS_IF([test "${CFLAGS+set}" != set], [CFLAGS=""])

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

# Detect target host for cross-compilation support
AC_CANONICAL_HOST

AC_PROG_CC
dnl Autoconf does not provide AC_PROG_CC_C17 here, so fail early if the
dnl repository-wide -std=c17 flag would be rejected by the selected compiler.
AC_CACHE_CHECK([whether $CC supports -std=c17], [salt_cv_prog_cc_std_c17],
  [salt_save_CFLAGS=$CFLAGS
   CFLAGS="$CFLAGS -std=c17"
   AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM([], [[]])],
     [salt_cv_prog_cc_std_c17=yes],
     [salt_cv_prog_cc_std_c17=no])
   CFLAGS=$salt_save_CFLAGS])
AS_IF(
  [test "x$salt_cv_prog_cc_std_c17" != "xyes"],
  [AC_MSG_ERROR([the selected C compiler must support -std=c17])])
AM_PROG_AR
AC_PROG_INSTALL
AC_PATH_PROG([STRIP], [strip], [:])
AC_PROG_MAKE_SET
AC_MSG_CHECKING([for GNU make])
AS_IF([${MAKE-make} --version 2>/dev/null | grep -q 'GNU Make'],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_ERROR([GNU Make is required to build this project])])
PKG_PROG_PKG_CONFIG

AC_ARG_VAR([SALT_VERSION], [Version string compiled into the binary (default: PACKAGE_VERSION)])
AS_IF([test "x$SALT_VERSION" = "x"], [SALT_VERSION="$PACKAGE_VERSION"])
AC_SUBST([SALT_VERSION])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [build with debug-friendly flags (-g -O0) [default=no]])],
  [],
  [enable_debug=no])
AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--disable-hardening], [disable hardening compile/link flags])],
  [],
  [enable_hardening=yes])
AC_ARG_ENABLE([tests],
  [AS_HELP_STRING([--disable-tests], [skip cmocka-based unit test checks])],
  [],
  [enable_tests=yes])
AM_CONDITIONAL([ENABLE_DEBUG], [test "x$enable_debug" = "xyes"])

# ============================================================================
# Centralized Build Flags (DEBUG, VALGRIND, COVERAGE, OPTIMIZATION)
# ============================================================================

# Base compilation flags for all targets
BASE_CFLAGS="-std=c17 -Wall -Wextra -Wpedantic"

# Debug flags: disable optimizations for debugging
DEBUG_CFLAGS="-g -O0 -DDEBUG"

# Valgrind flags for memory profiling
VALGRIND_CFLAGS="-g -fno-inline"

# Coverage flags for code coverage analysis
COVERAGE_CFLAGS="-fprofile-arcs -ftest-coverage"

# Optimization level (depends on debug flag)
AS_IF(
  [test "x$enable_debug" = "xyes"],
  [BASE_CFLAGS="$BASE_CFLAGS $DEBUG_CFLAGS"],
  [BASE_CFLAGS="$BASE_CFLAGS -O3"]
)

# Target-specific flags for cross-compilation support
case "$host_os" in
  linux*)
    # Linux: Use Position Independent Code for shared libraries
    TARGET_CFLAGS="-fPIC"
    TARGET_LDFLAGS=""
    ;;
  darwin*)
    # macOS/Darwin: Compatible with both Intel and ARM (Apple Silicon)
    TARGET_CFLAGS="-fPIC"
    TARGET_LDFLAGS=""
    ;;
  mingw32 | mingw64 | cygwin*)
    # Windows (MinGW cross-compile): Set Windows-specific flags.
    # _WIN32_WINNT=0x0601 corresponds to Windows 7.
    TARGET_CFLAGS="-D_WIN32_WINNT=0x0601"
    TARGET_LDFLAGS=""
    ;;
  *)
    # Default for other platforms
    TARGET_CFLAGS="-fPIC"
    TARGET_LDFLAGS=""
    ;;
esac

AC_SUBST([DEBUG_CFLAGS])
AC_SUBST([VALGRIND_CFLAGS])
AC_SUBST([COVERAGE_CFLAGS])
AC_SUBST([BASE_CFLAGS])
AC_SUBST([TARGET_CFLAGS])
AC_SUBST([TARGET_LDFLAGS])

HARDEN_CFLAGS=""
HARDEN_LDFLAGS=""
AS_IF(
  [test "x$enable_hardening" = "xyes"],
  [
    HARDEN_CFLAGS="-fstack-protector-strong -Wformat=2 -Wformat-security"
    AS_IF(
      [test "x$enable_debug" != "xyes"],
      [HARDEN_CFLAGS="-D_FORTIFY_SOURCE=2 $HARDEN_CFLAGS"]
    )
    case "$host_os" in
      linux*)
        HARDEN_LDFLAGS="-Wl,-z,relro,-z,now"
        ;;
      *)
        HARDEN_LDFLAGS=""
        ;;
    esac
  ]
)
AC_SUBST([HARDEN_CFLAGS])
AC_SUBST([HARDEN_LDFLAGS])

SODIUM_CFLAGS=""
SODIUM_LIBS=""
AS_IF(
  [test -n "$PKG_CONFIG" && $PKG_CONFIG --exists libsodium],
  [
    # Check libsodium version constraint (>= 1.0.18)
    PKG_CHECK_MODULES([SODIUM], [libsodium >= 1.0.18], [],
      [
        AC_MSG_ERROR([libsodium >= 1.0.18 is required. Found version: $($PKG_CONFIG --modversion libsodium 2>/dev/null || echo 'unknown')])
      ]
    )
  ],
  [
    AC_CHECK_HEADERS([sodium.h], [], [AC_MSG_ERROR([libsodium headers are required (set CPPFLAGS/LDFLAGS if installed in a non-default prefix)])])
    AC_CHECK_LIB([sodium], [sodium_init], [SODIUM_LIBS="-lsodium"], [AC_MSG_ERROR([libsodium library is required (set CPPFLAGS/LDFLAGS if installed in a non-default prefix)])])
    dnl Enforce >= 1.0.18 via compile-time version macros. In libsodium,
    dnl SODIUM_LIBRARY_VERSION_MAJOR encodes the "1.0" line as 10, and
    dnl SODIUM_LIBRARY_VERSION_MINOR tracks the patch level for that line
    dnl (so 1.0.18 corresponds to MAJOR=10, MINOR=3). Reject anything below
    dnl that threshold. This catches the case where pkg-config is absent.
    AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[#include <sodium.h>]],
        [[
#if SODIUM_LIBRARY_VERSION_MAJOR < 10 || \
    (SODIUM_LIBRARY_VERSION_MAJOR == 10 && SODIUM_LIBRARY_VERSION_MINOR < 3)
# error "libsodium < 1.0.18"
#endif
        ]])],
      [],
      [AC_MSG_ERROR([libsodium >= 1.0.18 is required but an older version was detected (set CPPFLAGS/LDFLAGS if multiple versions are installed)])])
  ]
)
AC_SUBST([SODIUM_CFLAGS])
AC_SUBST([SODIUM_LIBS])

CMOCKA_CFLAGS=""
CMOCKA_LIBS=""
AS_IF(
  [test "x$enable_tests" = "xyes"],
  [
    AS_IF(
      [test -n "$PKG_CONFIG" && $PKG_CONFIG --exists cmocka],
      [
        # Check cmocka version constraint (>= 1.1.5)
        PKG_CHECK_MODULES([CMOCKA], [cmocka >= 1.1.5], [],
          [
            AC_MSG_ERROR([cmocka >= 1.1.5 is required for tests. Found version: $($PKG_CONFIG --modversion cmocka 2>/dev/null || echo 'unknown')])
          ]
        )
      ],
      [
        dnl cmocka.h requires <stdarg.h>, <stddef.h>, and <setjmp.h> to be
        dnl pre-included (it uses those types without pulling them in itself,
        dnl matching its documented include order).
        dnl Keep these pre-includes centralized so they are not accidentally
        dnl dropped during maintenance; omitting them can break header checks.
        CMOCKA_HEADER_PREINCLUDES='
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
'
        AC_CHECK_HEADERS([cmocka.h], [], [AC_MSG_ERROR([cmocka headers are required for tests (set CPPFLAGS/LDFLAGS if installed in a non-default prefix)])],
          [$CMOCKA_HEADER_PREINCLUDES])
        AC_CHECK_LIB([cmocka], [_cmocka_run_group_tests], [CMOCKA_LIBS="-lcmocka"], [AC_MSG_ERROR([cmocka library is required for tests (set CPPFLAGS/LDFLAGS if installed in a non-default prefix)])])
        dnl Enforce >= 1.1.5 via symbol probe: cmocka_set_test_filter was added in 1.1.5.
        dnl This catches the case where pkg-config is absent.
        AC_CHECK_LIB([cmocka], [cmocka_set_test_filter], [],
          [AC_MSG_ERROR([cmocka >= 1.1.5 is required for tests: cmocka_set_test_filter not found (upgrade cmocka or set CPPFLAGS/LDFLAGS)])])
      ]
    )
  ]
)
AC_SUBST([CMOCKA_CFLAGS])
AC_SUBST([CMOCKA_LIBS])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" = "xyes"])

AC_PATH_PROG([VALGRIND], [valgrind], [no])
AM_CONDITIONAL([HAVE_VALGRIND], [test "x$VALGRIND" != "xno"])
AC_SUBST([VALGRIND])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
