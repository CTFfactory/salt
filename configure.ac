AC_PREREQ([2.69])
AC_INIT([salt], [0.0.0], [https://github.com/CTFfactory/salt/issues], [salt], [https://github.com/CTFfactory/salt])
AC_CONFIG_SRCDIR([src/salt.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

# Detect target host for cross-compilation support
AC_CANONICAL_HOST

AC_PROG_CC
AM_PROG_AR
AC_PROG_INSTALL
AC_PATH_PROG([STRIP], [strip], [:])
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
BASE_CFLAGS="-std=c11 -Wall -Wextra -Wpedantic"

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
    # Windows (MinGW cross-compile): Set Windows-specific flags
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
    PKG_CHECK_MODULES([SODIUM], [libsodium >= 1.0.18],
      [
        SODIUM_CFLAGS="$SODIUM_CFLAGS"
        SODIUM_LIBS="$SODIUM_LIBS"
      ],
      [
        AC_MSG_ERROR([libsodium >= 1.0.18 is required. Found version: $($PKG_CONFIG --modversion libsodium 2>/dev/null || echo 'unknown')])
      ]
    )
  ],
  [
    AC_CHECK_HEADERS([sodium.h], [], [AC_MSG_ERROR([libsodium headers are required])])
    AC_CHECK_LIB([sodium], [sodium_init], [SODIUM_LIBS="-lsodium"], [AC_MSG_ERROR([libsodium library is required])])
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
        PKG_CHECK_MODULES([CMOCKA], [cmocka >= 1.1.5],
          [
            CMOCKA_CFLAGS="$CMOCKA_CFLAGS"
            CMOCKA_LIBS="$CMOCKA_LIBS"
          ],
          [
            AC_MSG_ERROR([cmocka >= 1.1.5 is required for tests. Found version: $($PKG_CONFIG --modversion cmocka 2>/dev/null || echo 'unknown')])
          ]
        )
      ],
      [
        AC_CHECK_HEADERS([cmocka.h], [], [AC_MSG_ERROR([cmocka headers are required for tests])])
        AC_CHECK_LIB([cmocka], [_cmocka_run_group_tests], [CMOCKA_LIBS="-lcmocka"], [AC_MSG_ERROR([cmocka library is required for tests])])
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
