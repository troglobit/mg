/* $OpenBSD$ */
/*
 * This file is in the public domain.
 *
 * Author: Omar Polo <op@openbsd.org>
 */

#include <sys/queue.h>
#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"
#include "key.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

int	pairp(int, int);
int	epclosep(void);

/* Keymaps */

static PF ele_cmap[] = {
	epfdel,			/* ^D */
	rescan,			/* ^E */
	rescan,			/* ^F */
	rescan,			/* ^G */
	rescan,			/* ^H */
	rescan,			/* ^I */
	rescan,			/* ^J */
	rescan,			/* ^K */
	rescan,			/* ^L */
	rescan,			/* ^M */
	//epnewline,		/* ^M */
};

static PF ele_quote[] = {
	epinsert,		/* " */
};

static PF ele_apostrophe[] = {
	epinsert,		/* ' */
};

static PF ele_paren[] = {
	epinsert,		/* ( */
	epskip,			/* ) */
};

static PF ele_bracket[] = {
	epinsert,		/* [ */
	rescan,			/* \ */
	epskip,			/* ] */
};

static PF ele_backtick[] = {
	epinsert,		/* ` */
};

static PF ele_high[] = {
	epinsert,		/* { */
	rescan,			/* | */
	epskip,			/* } */
	rescan,			/* ~ */
	epbdel,			/* DEL */
};

static struct KEYMAPE (7) epmodemap = {
	7,
	7,
	rescan,
	{
		{ CCHR('D'), CCHR('M'),	ele_cmap, NULL },
		{ '"', '"',		ele_quote, NULL },
		{ '\'', '\'',		ele_apostrophe, NULL },
		{ '(', ')',		ele_paren, NULL },
		{ '[', ']',		ele_bracket, NULL },
		{ '`', '`',		ele_backtick, NULL },
		{ '{', CCHR('?'),	ele_high, NULL },
	}
};

/* Function, Mode hooks */

void
epmode_init(void)
{
	funmap_add(epmode, "electric-pair-mode", 0);
	maps_add((KEYMAP *)&epmodemap, "ep");
}

/*
 * Enable/toggle electric-pair-mode.
 */
int
epmode(int f, int n)
{
	return (changemode(f, n, "ep"));
}

/* Do o and c form a pair? */
int
pairp(int o, int c)
{
	switch (o) {
	case '"':
	case '\'':
	case '`':
		return (c == o);
	case '(':
		return (c == ')');
	case '[':
		return (c == ']');
	case '{':
		return (c == '}');
	}
	return (FALSE);
}

/* Can we skip over the character? */
int
epclosep(void)
{
	int c;

	c = key.k_chars[key.k_count - 1];
	if (curwp->w_doto < llength(curwp->w_dotp))
		return (c == lgetc(curwp->w_dotp, curwp->w_doto));
	return (FALSE);
}

/*
 * Handle pair character - selfinsert then selfinsert.
 */
int
epinsert(int f, int n)
{
	int	s, c;

	if (n < 0)
		return (FALSE);

	if (n == 0)
		return (TRUE);

	if (n == 1 && epclosep())
		return (forwchar(FFRAND, 1));

	c = key.k_chars[key.k_count - 1];
	if ((s = selfinsert(FFRAND, n)) != TRUE)
		return (s);

	switch (c) {
	case '"':
	case '\'':
	case '`':
		s = linsert(n, c);
		break;
	case '(':
		s = linsert(n, ')');
		break;
	case '[':
		s = linsert(n, ']');
		break;
	case '{':
		s = linsert(n, '}');
		break;
	}

	if (s != TRUE)
		return (s);

	return (backchar(FFRAND, n));
}

/*
 * Do forwchar if trying to insert a character equal to the next one.
 */
int
epskip(int f, int n)
{
	if (n == 1 && epclosep())
		return (forwchar(FFRAND, 1));
	return (selfinsert(f, n));
}

/*
 * Handle deletion of a character trying to keep pairs balanced.
 */
int
epbdel(int f, int n)
{
	int	s, o, c;

	if (n < 0)
		return (epfdel(f | FFRAND, -n));

	while (n--) {
		o = '\0';
		c = '\0';

		/* peek at the character to delete */
		if (curwp->w_doto > 0)
			o = lgetc(curwp->w_dotp, curwp->w_doto - 1);

		/* do the delete */
		if ((s = backdel(FFRAND, 1)) != TRUE)
			return (s);

		/* peek at the next character */
		if (curwp->w_doto < llength(curwp->w_dotp))
			c = lgetc(curwp->w_dotp, curwp->w_doto);

		if (!pairp(o, c))
			continue;
		if ((s = forwdel(FFRAND, 1)) != TRUE)
			return (s);
	}

	return (TRUE);
}

/*
 * Handle deletion of a character trying to keep pairs balanced.
 */
int
epfdel(int f, int n)
{
	int	s, o, c;

	if (n < 0)
		return (epbdel(f | FFRAND, -n));

	while (n--) {
		o = '\0';
		c = '\0';

		/* peek at the character to delete */
		if (curwp->w_doto < llength(curwp->w_dotp))
			c = lgetc(curwp->w_dotp, curwp->w_doto);

		/* do the delete */
		if ((s = forwdel(FFRAND, 1)) != TRUE)
			return (s);

		/* peek at the prev character */
		if (curwp->w_doto > 0)
			o = lgetc(curwp->w_dotp, curwp->w_doto - 1);

		if (!pairp(o, c))
			continue;
		if ((s = backdel(FFRAND, 1)) != TRUE)
			return (s);
	}

	return (TRUE);
}

int
epnewline(int f, int n)
{
	int	s, o, c;

	if (n != 1 || curwp->w_doto == 0 ||
	    curwp->w_doto == llength(curwp->w_dotp))
		return (lfindent(f, n));

	o = lgetc(curwp->w_dotp, curwp->w_doto - 1);
	c = lgetc(curwp->w_dotp, curwp->w_doto);
	if (!pairp(o, c))
		return (lfindent(f, n));

	if ((s = lfindent(FFRAND, 2)) != TRUE ||
	    (s = backline(FFRAND, 1)) != TRUE ||
	    (s = gotoeol(FFRAND, 1)) != TRUE)
		return (s);
	return (TRUE);
}
