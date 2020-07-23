/*      $OpenBSD: log.h,v 1.5 2019/07/18 10:50:24 lum Exp $   */

/* This file is in the public domain. */
#include <stdarg.h>

/*
 * Specifically for mg logging functionality.
 *
 */
int	 mglog(PF, void *);
int	 mgloginit(void);
int	 mglog_execbuf(	const char* const,
			const char* const,
			const char* const,
			const char* const,
	     		const int,
			const int,
			const char* const,
			const char* const,
			const char* const
			);

int	 mglog_isvar(	const char* const,
			const char* const,
			const int
			);
int	 mglog_misc(	const char *, ...);

extern char 		*mglogpath_lines;
extern char 		*mglogpath_undo;
extern char 		*mglogpath_window;
extern char 		*mglogpath_key;
extern char		*mglogpath_interpreter;
extern char		*mglogpath_misc;
