/* This file is in the public domain. */

/*
 * Python mode.  Tags the buffer so that syntax highlighting picks
 * the python rules, and sets the buffer up for python: four column
 * indent steps, spaces only, and RET keeps the indent.  Enabled
 * automatically for *.py files and #! lines naming python.
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static int	 py_indent(int, int);
static int	 py_tab(int, int);

static PF pymode_tab[] = {
	py_tab		/* ^I */
};

static struct KEYMAPE (1) pymodemap = {
	1,
	1,
	rescan,
	{
		{
			CCHR('I'), CCHR('I'), pymode_tab, NULL
		}
	}
};

/*
 * The natural indent of the current line: that of the previous
 * non-blank line, one level deeper when it opens a block with a
 * colon.
 */
static int
py_target(void)
{
	struct line	*lp;
	int	 c, col, depth, i, last, q;

	col = prevlineindent(&lp);
	if (lp != curbp->b_headp) {
		/* a trailing colon opens a block, an unclosed bracket
		 * a continuation; # ends the line unless it is inside
		 * a string literal */
		depth = 0;
		last = '\0';
		q = 0;
		for (i = 0; i < llength(lp); i++) {
			c = lgetc(lp, i);
			if (q != 0) {
				if (c == '\\')
					i++;
				else if (c == q)
					q = 0;
			} else if (c == '"' || c == '\'')
				q = c;
			else if (c == '#')
				break;
			else if (c == '(' || c == '[' || c == '{')
				depth++;
			else if (c == ')' || c == ']' || c == '}')
				depth--;
			if (!isspace(c))
				last = c;
		}
		if (depth > 0 || last == ':')
			col += curbp->b_tabw;
	}

	/* a closing bracket steps back to the opening line */
	lineindent(curwp->w_dotp, &i);
	if (i < llength(curwp->w_dotp) &&
	    ((c = lgetc(curwp->w_dotp, i)) == ')' || c == ']' || c == '}')) {
		col -= curbp->b_tabw;
		if (col < 0)
			col = 0;
	}
	return (col);
}

/*
 * Indent the current line to the given column, as one undo record
 * for the delete and insert of whitespace.
 */
static int
py_setindent(int col)
{
	int	 s;

	undo_boundary_enable(FFRAND, 0);
	s = indent(FFARG, col);
	undo_boundary_enable(FFRAND, 1);

	return (s);
}

static int
py_indent(int f, int n)
{
	return (py_setindent(py_target()));
}

/*
 * Indent the line, or all the lines in the region when the mark is
 * active.  The natural indent is only a guess: after a block ends
 * the line may belong to any enclosing block, so a repeated TAB
 * steps the line back one level at a time, and wraps around.
 */
static int
py_tab(int f, int n)
{
	int	 col, target;

	if (curwp->w_markact)
		return (regionlines(py_indent));

	thisflag |= CFINDT;
	col = lineindent(curwp->w_dotp, NULL);
	if ((lastflag & CFINDT) != 0 && col > 0)
		target = ((col - 1) / curbp->b_tabw) * curbp->b_tabw;
	else
		target = py_target();
	return (py_setindent(target));
}

static int
pymode(int f, int n)
{
	/*
	 * Whitespace is structure in python: four column indent
	 * steps, spaces only since a stray tab is a syntax error,
	 * and RET keeps the indent.  Set the buffer up before the
	 * python map is added, so its indenting TAB binding stays
	 * in front of the plain one the notab mode brings.
	 */
	if (!buf_hasmode(curbp, "python") &&
	    ((f & FFARG) == 0 || n > 0)) {
		curbp->b_tabw = 4;
		(void)notabmode(FFARG, 1);
		(void)indentmode(FFARG, 1);
	}
	return (changemode(f, n, "python"));
}

void
pymode_init(void)
{
	funmap_add(pymode, "python-mode", 0);
	funmap_add(py_indent, "py-indent", 0);
	funmap_add(py_tab, "py-tab-or-indent", 0);
	maps_add((KEYMAP *)&pymodemap, "python");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.py", "python-mode");
#endif
}
