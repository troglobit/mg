# Written by Joachim Nilsson <troglobit@gmail.com> and put in the public domain.
AC_INIT([Mg], [3.2-dev], [https://github.com/troglobit/mg/issues], [mg])
AM_INIT_AUTOMAKE([1.11 foreign])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

# Check for library functions
AC_CHECK_FUNCS([futimens])

# Check for usually missing API's, which we can replace
AC_REPLACE_FUNCS([fparseln reallocarray strlcpy strlcat strtonum])
AC_CONFIG_LIBOBJ_DIR([lib])

# Check for a termcap compatible library
AC_CHECK_LIB(termcap, tgoto, , [
	AC_CHECK_LIB(tinfo, tgoto, , [
		AC_CHECK_LIB(curses, tgoto, , [
			AC_CHECK_LIB(ncurses, tgoto, , [
				AC_MSG_ERROR([Cannot find a termcap capable library, try installing Ncurses.])])
			])
		])
	])

# We want term.h, not termcap.h, because cookies ...
AC_CHECK_HEADERS(term.h, , [
	AC_MSG_ERROR([Cannot find required header file term.h, try installing Ncurses.])])

# Check for configured features
AC_ARG_ENABLE(size-optimizations,
	AS_HELP_STRING([--enable-size-optimizations], [Optimize for size, try real hard]))

AC_ARG_ENABLE(autoexec, AS_HELP_STRING([--disable-autoexec], [Disable auto-execute support]))
AC_ARG_ENABLE(cmode,    AS_HELP_STRING([--disable-cmode],    [Disable C-mode support]))
AC_ARG_ENABLE(compile,  AS_HELP_STRING([--disable-compile],  [Disable C compile & grep mode, used by C-mode]))
AC_ARG_ENABLE(cscope,   AS_HELP_STRING([--disable-cscope],   [Disable Cscope support]))
AC_ARG_ENABLE(ctags,    AS_HELP_STRING([--disable-ctags],    [Disable ctags(1) support, required by Cscope]))
AC_ARG_ENABLE(dired,    AS_HELP_STRING([--disable-dired],    [Disable directory editor]))
AC_ARG_ENABLE(notab,    AS_HELP_STRING([--disable-notab],    [Disable notab mode support]))
AC_ARG_ENABLE(regexp,   AS_HELP_STRING([--disable-regexp],   [Disable full regexp search]))
AC_ARG_ENABLE(all,      AS_HELP_STRING([--disable-all],      [Disable all optional features]))

AC_ARG_WITH(startup,
        AS_HELP_STRING([--with-startup=FILE], [Init file to run at startup if ~/.mg is missing]),
	[startup=$withval], [startup='no'])

# If all features are to be disabled, do that here
AS_IF([test "x$enable_all" = "xno"],
	enable_autoexec="no"
	enable_cmode="no"
	enable_compile="no"
	enable_cscope="no"
	enable_ctags="no"
	enable_dired="no"
	enable_notab="no"
	enable_regexp="no"
	with_startup="no")

# Enable features
AS_IF([test "x$enable_size_optimizations" = "xyes"], enable_tiny="yes"
	CFLAGS="-g -Os"
	AC_DEFINE(TINY, 1, [Enable extreme size optimizations]),
	enable_tiny="no")

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

AS_IF([test "x$enable_notab" != "xno"], enable_notab="yes"
	AC_DEFINE(NOTAB, 1, [Enable notab mode, disabled in OpenBSD]))

AS_IF([test "x$enable_regexp" != "xno"], enable_regexp="yes"
	AC_DEFINE(REGEX, 1, [Enable regexp search support]))

AS_IF([test "x$with_startup" != "xno"],[
	AS_IF([test "x$startup" = "xyes"],[
		AC_MSG_ERROR([Must supply argument])])
	]
	AC_DEFINE_UNQUOTED(STARTUPFILE, "$startup", [Init file to run if ~/.mg is missing]))

# Control build with automake flags
AM_CONDITIONAL(REGEX,    [test "x$enable_regexp"   = "xyes"])
AM_CONDITIONAL(AUTOEXEC, [test "x$enable_autoexec" = "xyes"])
AM_CONDITIONAL(CMODE,    [test "x$enable_cmode"    = "xyes"])
AM_CONDITIONAL(COMPILE,  [test "x$enable_compile"  = "xyes"])
AM_CONDITIONAL(CSCOPE,   [test "x$enable_cscope"   = "xyes"])
AM_CONDITIONAL(CTAGS,    [test "x$enable_ctags"    = "xyes"])
AM_CONDITIONAL(DIRED,    [test "x$enable_dired"    = "xyes"])
AM_CONDITIONAL(TINY,     [test "x$enable_tiny"     = "xyes"])

# Generate all files
AC_OUTPUT
