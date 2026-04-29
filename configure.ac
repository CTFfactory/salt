AC_PREREQ([2.69])
AC_INIT([salt], [0.0.0], [https://github.com/CTFfactory/salt/issues], [salt], [https://github.com/CTFfactory/salt])
AC_CONFIG_SRCDIR([src/salt.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

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

BASE_CFLAGS="-std=c11 -Wall -Wextra -Wpedantic"
AS_IF(
  [test "x$enable_debug" = "xyes"],
  [BASE_CFLAGS="$BASE_CFLAGS -g -O0"],
  [BASE_CFLAGS="$BASE_CFLAGS -O3"]
)
AC_SUBST([BASE_CFLAGS])

HARDEN_CFLAGS=""
HARDEN_LDFLAGS=""
AS_IF(
  [test "x$enable_hardening" = "xyes"],
  [
    HARDEN_CFLAGS="-D_FORTIFY_SOURCE=2 -fstack-protector-strong -Wformat=2 -Wformat-security"
    HARDEN_LDFLAGS="-Wl,-z,relro,-z,now"
  ]
)
AC_SUBST([HARDEN_CFLAGS])
AC_SUBST([HARDEN_LDFLAGS])

SODIUM_CFLAGS=""
SODIUM_LIBS=""
AS_IF(
  [test -n "$PKG_CONFIG" && $PKG_CONFIG --exists libsodium],
  [
    SODIUM_CFLAGS=`$PKG_CONFIG --cflags libsodium`
    SODIUM_LIBS=`$PKG_CONFIG --libs libsodium`
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
        CMOCKA_CFLAGS=`$PKG_CONFIG --cflags cmocka`
        CMOCKA_LIBS=`$PKG_CONFIG --libs cmocka`
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
