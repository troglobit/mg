/* This file is in the public domain. */

/*
 * Shell script mode.  Tags the buffer so that syntax highlighting
 * picks the shell rules, and sets the buffer up for shell scripts:
 * tab width eight, hard tabs, and RET keeps the indent.  Enable per
 * file type in ~/.mg with, e.g.:
 *
 *	auto-execute *.sh shell-script-mode
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static int	 sh_indent(int, int);
static int	 sh_tab(int, int);

static PF shmode_tab[] = {
	sh_tab		/* ^I */
};

static struct KEYMAPE (1) shmodemap = {
	1,
	1,
	rescan,
	{
		{
			CCHR('I'), CCHR('I'), shmode_tab, NULL
		}
	}
};

/*
 * Indent the current line like the previous non-blank line.
 */
static int
sh_indent(int f, int n)
{
	struct line	*lp;
	int	 c, col, i, s;

	col = 0;
	for (lp = lback(curwp->w_dotp); lp != curbp->b_headp;
	     lp = lback(lp)) {
		for (i = 0; i < llength(lp); i++) {
			c = lgetc(lp, i);
			if (c == ' ')
				col++;
			else if (c == '\t')
				col = ntabstop(col, curbp->b_tabw);
			else
				break;
		}
		if (i < llength(lp))
			break;		/* non-blank line found */
		col = 0;
	}

	/* one undo record for the delete and insert of whitespace */
	undo_boundary_enable(FFRAND, 0);
	s = indent(FFARG, col);
	undo_boundary_enable(FFRAND, 1);

	return (s);
}

/*
 * On a blank line insert a literal tab, for here documents;
 * otherwise indent the line, or all the lines in the region when
 * the mark is active.  Like c-tab-or-indent.
 */
static int
sh_tab(int f, int n)
{
	int	 i;

	if (curwp->w_markact)
		return (regionlines(sh_indent));

	for (i = 0; i < llength(curwp->w_dotp); i++)
		if (!isspace(lgetc(curwp->w_dotp, i)))
			return (sh_indent(FFRAND, 1));
	return (selfinsert(f, n));
}

static int
shmode(int f, int n)
{
	struct maps_s	*m;
	int		 i, s;

	if ((s = changemode(f, n, "shell-script")) != TRUE)
		return (s);

	m = name_mode("shell-script");
	for (i = 0; i <= curbp->b_nmodes; i++)
		if (curbp->b_modes[i] == m)
			break;
	if (i > curbp->b_nmodes)
		return (TRUE);	/* mode was toggled off */

	/*
	 * Tab width eight and hard tabs; here documents with <<-
	 * only strip real tabs.  The indent mode makes RET keep the
	 * indentation of continuation lines.
	 */
	curbp->b_tabw = 8;
	(void)notabmode(FFARG, 0);
	return (indentmode(FFARG, 1));
}

void
shmode_init(void)
{
	funmap_add(shmode, "shell-script-mode", 0);
	funmap_add(sh_indent, "sh-indent", 0);
	funmap_add(sh_tab, "sh-tab-or-indent", 0);
	maps_add((KEYMAP *)&shmodemap, "shell-script");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.sh", "shell-script-mode");
#endif
}
