/* This file is in the public domain. */

/*
 * Commenting text out and back in, after comment-dwim and its
 * relatives in GNU Emacs.  The delimiters come from the buffer's
 * mode, through the syntax rules, which must also report what they
 * open as a comment.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "def.h"

#define COMMENTCOL	40		/* where a comment after code goes */

static const char	*cstart, *cend;	/* the buffer's delimiters */

/*
 * Set cstart and cend for the current buffer, or say why not.
 */
static int
csyntax(void)
{
	if (!syn_comment(curbp, &cstart, &cend))
		return (dobeep_msg("No comment syntax for this mode"));
	return (TRUE);
}

static int
insstr(const char *s)
{
	for (; *s != '\0'; s++)
		if (linsert(1, *s) != TRUE)
			return (FALSE);
	return (TRUE);
}

/*
 * The delimiters as written: cstart and a space at dot, and a space
 * and cend when the mode closes its comments.
 */
static int
opencomment(void)
{
	return (insstr(cstart) && insstr(" "));
}

static int
closecomment(void)
{
	return (*cend == '\0' || (insstr(" ") && insstr(cend)));
}

/*
 * Where the code on lp starts, past its indentation, or -1 for a
 * line with none.
 */
static int
codeat(const struct line *lp)
{
	int	 i;

	lineindent(lp, &i);
	return (i < llength(lp) ? i : -1);
}

/*
 * Where the comment on lp begins, by the mode's syntax rules, or -1.
 */
static int
commentpos(struct line *lp)
{
	const struct syntax	*sy = syntax_lookup(curbp);
	char	 attr[llength(lp) + 1];
	int	 i;

	syn_parse(sy, lp, syn_state(sy, curbp, lp), attr);
	for (i = 0; i < llength(lp); i++)
		if (attr[i] == SYN_COMMENT)
			return (i);
	return (-1);
}

/*
 * The offset of the delimiter that comments lp out as a whole, or -1
 * for a line of code or an empty one.
 */
static int
commented(const struct line *lp)
{
	int	 i;

	if ((i = codeat(lp)) < 0 || matchat(lp, i, cstart) == 0)
		return (-1);
	return (i);
}

/*
 * Comment out the line dot is on, at its indentation; an empty line
 * is left alone.
 */
static int
commentout(int f, int n)
{
	int	 i;

	if ((i = codeat(curwp->w_dotp)) < 0)
		return (TRUE);
	curwp->w_doto = i;
	if (!opencomment())
		return (FALSE);
	(void)gotoeol(FFRAND, 1);
	return (closecomment());
}

/*
 * Take the delimiters, and the space beside each, off the line dot
 * is on: the closing one first, so the opening one stays put.
 */
static int
commentin(int f, int n)
{
	struct line	*lp = curwp->w_dotp;
	int	 e, i, len;

	if ((i = commented(lp)) < 0)
		return (TRUE);
	len = strlen(cend);
	e = llength(lp);
	if (len > 0 && e - len > i && matchat(lp, e - len, cend) != 0) {
		if (lgetc(lp, e - len - 1) == ' ')
			len++;
		curwp->w_doto = e - len;
		if (ldelete(len, KNONE) != TRUE)
			return (FALSE);
	}
	len = strlen(cstart);
	if (i + len < llength(lp) && lgetc(lp, i + len) == ' ')
		len++;
	curwp->w_doto = i;
	return (ldelete(len, KNONE));
}

/*
 * For the region walk that asks whether every line of code in it
 * is commented out: FALSE stops the walk at the first that is not.
 */
static int
probe(int f, int n)
{
	return (codeat(curwp->w_dotp) < 0 || commented(curwp->w_dotp) >= 0);
}

/*
 * Run fn over the region's lines as one undo step.  More than one
 * line changes, so the window needs a full redraw.
 */
static int
overregion(int (*fn)(int, int))
{
	int	 s;

	undo_boundary_enable(FFRAND, 0);
	s = regionlines(fn);
	undo_boundary_enable(FFRAND, 1);
	curwp->w_rflag |= WFFULL;
	return (s);
}

/*
 * Comment out every line of code in the region.
 */
int
commentregion(int f, int n)
{
	if (!csyntax())
		return (FALSE);
	return (overregion(commentout));
}

/*
 * Take the comment delimiters off every line of the region.
 */
int
uncommentregion(int f, int n)
{
	if (!csyntax())
		return (FALSE);
	return (overregion(commentin));
}

/*
 * The region: comment it out, or back in when all of it is out.
 */
static int
toggleregion(void)
{
	return (overregion(regionlines(probe) == TRUE ? commentin : commentout));
}

/*
 * What a comment key should do here: with the mark set, comment the
 * region out or back in.  On a line with a comment, go to it, or with
 * an argument kill it.  Otherwise start a comment: after the code on
 * the line, at the comment column, or right here on an empty line.
 */
int
commentdwim(int f, int n)
{
	struct line	*lp;
	int	 col, doto, i, s;

	if (!csyntax())
		return (FALSE);
	if (curwp->w_markact && curwp->w_markp != NULL)
		return (toggleregion());

	lp = curwp->w_dotp;
	if ((i = commentpos(lp)) >= 0) {
		if (f & FFARG) {
			while (i > 0 && isblank(lgetc(lp, i - 1)))
				i--;
			curwp->w_doto = i;
			return (ldelete(llength(lp) - i, KFORW));
		}
		if (matchat(lp, i, cstart) != 0) {
			i += strlen(cstart);
			if (i < llength(lp) && lgetc(lp, i) == ' ')
				i++;
		}
		curwp->w_doto = i;
		curwp->w_rflag |= WFMOVE;
		return (TRUE);
	}
	undo_boundary_enable(FFRAND, 0);
	if (codeat(lp) >= 0) {
		(void)gotoeol(FFRAND, 1);
		(void)delwhite(FFRAND, 1);
		col = getcolpos(curwp);
		linsert(col < COMMENTCOL ? COMMENTCOL - col : 1, ' ');
	}
	s = opencomment();
	doto = curwp->w_doto;
	if (s)
		s = closecomment();
	undo_boundary_enable(FFRAND, 1);
	curwp->w_doto = doto;
	return (s);
}

/*
 * Comment the line out, or back in when it is out, and go to the
 * next line; with the mark set, the region instead.
 */
int
commentline(int f, int n)
{
	int	 s;

	if (!csyntax())
		return (FALSE);
	if (curwp->w_markact && curwp->w_markp != NULL)
		return (toggleregion());
	undo_boundary_enable(FFRAND, 0);
	s = commented(curwp->w_dotp) >= 0 ?
	    commentin(FFRAND, 1) : commentout(FFRAND, 1);
	undo_boundary_enable(FFRAND, 1);
	if (s == TRUE)
		s = forwline(FFRAND, 1);
	return (s);
}
