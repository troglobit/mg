AC_INIT([Mg], [3.0-beta1], [https://github.com/troglobit/mg/issues], [mg])
AM_INIT_AUTOMAKE([1.11 foreign])

AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

AC_ARG_ENABLE(cmode,  AS_HELP_STRING([--disable-cmode],  [Disable C-mode support]))
AC_ARG_ENABLE(cscope, AS_HELP_STRING([--disable-cscope], [Disable Cscope support]))
AC_ARG_ENABLE(dired,  AS_HELP_STRING([--disable-dired],  [Disable directory editor]))

AS_IF([test "x$enable_cmode" != "xno"], enable_cmode="yes"
	AC_DEFINE(ENABLE_CMODE, 1, [Enable C programming mode, OpenBSD KNF]))

AS_IF([test "x$enable_cscope" != "xno"], enable_cscope="yes"
	AC_DEFINE(CSCOPE, 1, [Enable Cscope programming tool]))

AS_IF([test "x$enable_dired" != "xno"], enable_dired="yes"
	AC_DEFINE(ENABLE_DIRED, 1, [Enable dired, the directory editor]))

AM_CONDITIONAL(CMODE,  [test "x$enable_cmode" = "xyes"])
AM_CONDITIONAL(CSCOPE, [test "x$enable_cscope" = "xyes"])
AM_CONDITIONAL(DIRED,  [test "x$enable_dired" = "xyes"])

AC_OUTPUT
