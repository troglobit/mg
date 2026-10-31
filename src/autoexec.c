/* $OpenBSD: autoexec.c,v 1.18 2021/04/21 14:45:28 lum Exp $ */
/* this file is in the public domain */
/* Author: Vincent Labrecque <vincent@openbsd.org>	April 2002 */

#include <ctype.h>
#include <fnmatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"

struct autoexec {
	SLIST_ENTRY(autoexec) next;	/* link in the linked list */
	const char	*pattern;	/* Pattern to match to filenames */
	PF		 fp;
	int		 builtin;	/* registered by a mode, not the user */
	int		 major;		/* runs a major mode command */
	int		 literal;	/* no wildcards: names a file */
};

static SLIST_HEAD(, autoexec)	 autos;
static int			 ready;

/*
 * Patterns match the whole path or just the basename.
 */
static int
matchname(const char *pattern, const char *fname)
{
	const char	*bname;

	if ((bname = strrchr(fname, '/')) != NULL)
		bname++;
	else
		bname = fname;
	return (fnmatch(pattern, fname, 0) == 0 ||
	    (bname != fname && fnmatch(pattern, bname, 0) == 0));
}

/*
 * Whether ae applies to fname, or to plain: fname without its .gz,
 * or NULL.
 */
static int
matches(const struct autoexec *ae, const char *fname, const char *plain)
{
	return (matchname(ae->pattern, fname) ||
	    (plain != NULL && matchname(ae->pattern, plain)));
}


#define AUTO_GROW 8
/*
 * Return a NULL terminated array of function pointers to be called
 * when we open a file that matches <fname>.  The list must be free(ed)
 * after use.
 */
PF *
find_autoexec(const char *fname, int *majors)
{
	PF		*pfl, *npfl;
	int		 have, used, exact, pass;
	char		 buf[NFILEN];
	const char	*plain;
	struct autoexec *ae;

	*majors = 0;
	if (!ready)
		return (NULL);

	/*
	 * A file read through gunzip, like foo.md.gz, also gets the
	 * modes of the name it has uncompressed.
	 */
	plain = NULL;
	if (!secure && isgzip(fname) &&
	    strlcpy(buf, fname, sizeof(buf)) < sizeof(buf)) {
		buf[strlen(buf) - 3] = '\0';
		plain = buf;
	}

	/*
	 * A built-in pattern that names the file, like CMakeLists.txt,
	 * outranks the built-in ones it only matches by wildcard, like
	 * *.txt; the user's patterns always apply.
	 */
	exact = 0;
	SLIST_FOREACH(ae, &autos, next)
		if (ae->builtin && ae->literal && matches(ae, fname, plain))
			exact = 1;

	/*
	 * The built-in hooks first, so a mode the user names comes last.
	 * *majors tells whose patterns name a major mode.
	 */
	pfl = NULL;
	have = 0;
	used = 0;
	for (pass = 1; pass >= 0; pass--) {
		SLIST_FOREACH(ae, &autos, next) {
			if (ae->builtin != pass)
				continue;
			if (exact && pass && !ae->literal)
				continue;
			if (!matches(ae, fname, plain))
				continue;
			if (used >= have) {
				npfl = reallocarray(pfl, have + AUTO_GROW + 1,
				    sizeof(PF));
				if (npfl == NULL)
					panic("out of memory");
				pfl = npfl;
				have += AUTO_GROW;
			}
			pfl[used++] = ae->fp;
			if (ae->major)
				*majors |= pass ? AUTO_NAMED : AUTO_USER;
		}
	}
	if (used)
		pfl[used] = NULL;

	return (pfl);
}

/*
 * Whether func turns on a major mode: foo-mode for a mode foo that is
 * not a minor one.
 */
static int
ismajorcmd(const char *func)
{
	struct maps_s	*m;
	char	 name[NFILEN];
	size_t	 len = strlen(func);

	if (len <= 5 || strcmp(func + len - 5, "-mode") != 0 ||
	    len - 5 >= sizeof(name))
		return (FALSE);
	memcpy(name, func, len - 5);
	name[len - 5] = '\0';
	return ((m = name_mode(name)) != NULL && !m->p_minor);
}

static int
addauto(const char *pattern, const char *func, int user)
{
	PF		 fp;
	struct autoexec *ae;

	if (!ready) {
		SLIST_INIT(&autos);
		ready = 1;
	}
	fp = name_function(func);
	if (fp == NULL)
		return (FALSE);
	/* ~/.mg may repeat a built-in entry, which is then the user's */
	SLIST_FOREACH(ae, &autos, next)
		if (ae->fp == fp && strcmp(ae->pattern, pattern) == 0) {
			if (user)
				ae->builtin = 0;
			return (TRUE);
		}
	ae = malloc(sizeof(*ae));
	if (ae == NULL)
		return (FALSE);
	ae->fp = fp;
	ae->builtin = !user;
	ae->major = ismajorcmd(func);
	ae->literal = strpbrk(pattern, "*?[") == NULL;
	ae->pattern = strdup(pattern);
	if (ae->pattern == NULL) {
		free(ae);
		return (FALSE);
	}
	SLIST_INSERT_HEAD(&autos, ae, next);

	return (TRUE);
}

/*
 * A mode's own pattern, registered at start up.
 */
int
add_autoexec(const char *pattern, const char *func)
{
	return (addauto(pattern, func, 0));
}

/*
 * Register an auto-execute hook; that is, specify a filename pattern
 * (conforming to the shell's filename globbing rules) and an associated
 * function to execute when a file matching the specified pattern
 * is read into a buffer.
*/
int
auto_execute(int f, int n)
{
	char	patbuf[NFILEN], funcbuf[BUFSIZE], *patp, *funcp;
	int	s;

	if ((patp = eread("Filename pattern: ", patbuf, sizeof(patbuf),
	    EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (patp[0] == '\0')
		return (FALSE);
	if ((funcp = eread("Execute: ", funcbuf, sizeof(funcbuf),
	    EFNEW | EFCR | EFFUNC)) == NULL)
		return (ABORT);
	else if (funcp[0] == '\0')
		return (FALSE);
	if ((s = addauto(patp, funcp, 1)) != TRUE)
		return (s);
	return (TRUE);
}

/*
 * Interpreters on a #! line mapped to the mode command to run when
 * a visited file names one, whatever the file is called.
 */
static const struct {
	const char	*interp;
	const char	*mode;
} shebangs[] = {
	{ "sh",		"shell-script-mode" },
	{ "ash",	"shell-script-mode" },
	{ "bash",	"shell-script-mode" },
	{ "dash",	"shell-script-mode" },
	{ "ksh",	"shell-script-mode" },
	{ "mksh",	"shell-script-mode" },
	{ "zsh",	"shell-script-mode" },
	{ "python",	"python-mode" },
	{ "python2",	"python-mode" },
	{ "python3",	"python-mode" },
	{ "make",	"makefile-mode" },
	{ "gmake",	"makefile-mode" },
	{ NULL,		NULL }
};

static int	 shebang_execute(void);

/*
 * The first character of the buffer that is not white space, from
 * line *lpp, byte *op on, both left there; -1 at the end.
 */
static int
nextnonblank(struct line **lpp, int *op)
{
	for (; *lpp != curbp->b_headp; *lpp = lforw(*lpp), *op = 0)
		for (; *op < llength(*lpp); (*op)++)
			if (!isspace(lgetc(*lpp, *op)))
				return (lgetc(*lpp, *op));
	return (-1);
}

/*
 * Whether the buffer holds JSON: an object, or an array whose first
 * element is not a word, which an INI [section] is.
 */
static int
isjson(void)
{
	struct line	*lp = bfirstlp(curbp);
	int		 c, o = 0;

	if ((c = nextnonblank(&lp, &o)) == '{')
		return (TRUE);
	if (c != '[')
		return (FALSE);
	o++;
	return ((c = nextnonblank(&lp, &o)) != -1 &&
	    strchr("{[\"-0123456789]", c) != NULL);
}

/*
 * Run a mode command picked by what the current buffer holds, for a
 * file its name tells nothing about, or the wrong thing: a JSON file
 * named *.cfg, a script with no suffix.  named is set when a
 * built-in pattern gave the file its major mode.
 */
void
content_execute(int named)
{
	PF		 fp;

	if (shebang_execute())
		return;

	/* JSON in a file named for something generic, or for nothing */
	if (named && buf_major(curbp) != name_mode("conf"))
		return;
	if (isjson() && (fp = name_function("json-mode")) != NULL)
		(void)(*fp)(FFARG, 1);
}

/*
 * Run the mode command matching the interpreter on the #! line of
 * the current buffer, if any.  A /usr/bin/env indirection is peeled
 * off first.
 */
static int
shebang_execute(void)
{
	struct line	*lp;
	PF		 fp;
	char		 buf[NFILEN], *tok, *end, *interp;
	int		 i, len;

	lp = bfirstlp(curbp);
	len = llength(lp);
	if (len < 3 || lgetc(lp, 0) != '#' || lgetc(lp, 1) != '!')
		return (FALSE);
	snprintf(buf, sizeof(buf), "%.*s", len, ltext(lp));

	/* first token, peeling off env indirection and options */
	end = buf + 2;
	do {
		tok = end + strspn(end, " \t");
		end = tok + strcspn(tok, " \t\r");
		if (*end != '\0')
			*end++ = '\0';
		if ((interp = strrchr(tok, '/')) != NULL)
			interp++;
		else
			interp = tok;
	} while (strcmp(interp, "env") == 0 || interp[0] == '-');
	if (*interp == '\0')
		return (FALSE);

	for (i = 0; shebangs[i].interp != NULL; i++) {
		if (strcmp(interp, shebangs[i].interp) != 0)
			continue;
		if ((fp = name_function(shebangs[i].mode)) != NULL)
			(void)(*fp)(FFARG, 1);
		return (TRUE);
	}
	return (FALSE);
}
