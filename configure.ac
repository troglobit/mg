AC_INIT([mg], [3.0-beta1], [https://github.com/troglobit/mg/issues])
AM_INIT_AUTOMAKE([1.11 foreign])

AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

AC_ARG_ENABLE(cscope, AS_HELP_STRING([--disable-cscope], [Disable Cscope support]))

AS_IF([test "x$enable_cscope" != "xno"], enable_cscope="yes"
	AC_DEFINE(CSCOPE, 1, [Enable Cscope programming tool]))

AM_CONDITIONAL(CSCOPE, [test "x$enable_cscope" = "xyes"])

AC_OUTPUT
