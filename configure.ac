AC_INIT([Mg], [3.0-beta1], [https://github.com/troglobit/mg/issues], [mg])
AM_INIT_AUTOMAKE([1.11 foreign])

AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

AC_ARG_ENABLE(autoexec, AS_HELP_STRING([--disable-autoexec], [Disable auto-execute support]))
AC_ARG_ENABLE(cmode,  AS_HELP_STRING([--disable-cmode],  [Disable C-mode support]))
AC_ARG_ENABLE(compile,AS_HELP_STRING([--disable-compile],[Disable C compile & grep mode, used by C-mode]))
AC_ARG_ENABLE(cscope, AS_HELP_STRING([--disable-cscope], [Disable Cscope support]))
AC_ARG_ENABLE(ctags,  AS_HELP_STRING([--disable-ctags],  [Disable ctags(1) support, required by Cscope]))
AC_ARG_ENABLE(dired,  AS_HELP_STRING([--disable-dired],  [Disable directory editor]))

AS_IF([test "x$enable_autoexec" != "xno"], enable_autoexec="yes"
	AC_DEFINE(ENABLE_AUTOEXEC, 1, [Enable auto-exec support]))

AS_IF([test "x$enable_cmode" != "xno"], enable_cmode="yes"
	AC_DEFINE(ENABLE_CMODE, 1, [Enable C programming mode, OpenBSD KNF]))

AS_IF([test "x$enable_compile" != "xno"], enable_compile="yes"
	AC_DEFINE(ENABLE_COMPILE_GREP, 1, [Enable C compile mode, M-x compile]))

AS_IF([test "x$enable_cscope" != "xno"], enable_cscope="yes"
	enable_ctags="yes"
	AC_DEFINE(CSCOPE, 1, [Enable Cscope programming tool]))

AS_IF([test "x$enable_ctags" != "xno"], enable_ctags="yes"
	AC_DEFINE(ENABLE_CTAGS, 1, [Enable ctags(1) support]))

AS_IF([test "x$enable_dired" != "xno"], enable_dired="yes"
	AC_DEFINE(ENABLE_DIRED, 1, [Enable dired, the directory editor]))

case "$host_os" in
	openbsd*)
		enable_theo="yes"
		AC_DEFINE(ENABLE_THEO, 1, [Enable M-x theo analyzer on OpenBSD])
		;;
	*)
		;;
esac

AM_CONDITIONAL(AUTOEXEC, [test "x$enable_autoexec" = "xyes"])
AM_CONDITIONAL(CMODE,  [test "x$enable_cmode" = "xyes"])
AM_CONDITIONAL(COMPILE,[test "x$enable_compile" = "xyes"])
AM_CONDITIONAL(CSCOPE, [test "x$enable_cscope" = "xyes"])
AM_CONDITIONAL(CTAGS,  [test "x$enable_ctags" = "xyes"])
AM_CONDITIONAL(DIRED,  [test "x$enable_dired" = "xyes"])
AM_CONDITIONAL(THEO,   [test "x$enable_theo" = "xyes"])

AC_OUTPUT
