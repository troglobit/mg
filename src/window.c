/*	$OpenBSD: window.c,v 1.35 2015/10/29 20:20:49 lum Exp $	*/

/* This file is in the public domain. */

/*
 *		Window handling.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "def.h"

/*
 * True when the window lies inside the given edge rectangle: rows
 * top to bot and columns left to right, both bounds included, the
 * bottom and right edge being the mode line and divider.
 */
static int
balin(const struct mgwin *wp, int top, int bot, int left, int right)
{
	return (wp->w_toprow >= top && wp->w_toprow + wp->w_ntrows <= bot &&
	    wp->w_leftcol >= left && wp->w_leftcol + wp->w_ntcols <= right);
}

/*
 * Find the lowest cut through the rectangle: a mode line row (or,
 * for vertical cuts, a divider column) that no window straddles.
 * Returns 0 when there is none.
 */
static int
balcut(int top, int bot, int left, int right, int horiz)
{
	struct mgwin	*wp, *p;
	int	 e, best = 0;

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (!balin(wp, top, bot, left, right))
			continue;
		e = horiz ? wp->w_toprow + wp->w_ntrows :
		    wp->w_leftcol + wp->w_ntcols;
		if (e >= (horiz ? bot : right))
			continue;
		for (p = wheadp; p != NULL; p = p->w_wndp) {
			if (!balin(p, top, bot, left, right))
				continue;
			if (horiz ? p->w_toprow <= e &&
			    p->w_toprow + p->w_ntrows > e :
			    p->w_leftcol <= e &&
			    p->w_leftcol + p->w_ntcols > e)
				break;
		}
		if (p == NULL && (best == 0 || e < best))
			best = e;
	}
	return (best);
}

/*
 * The number of windows in the tallest stack in the rectangle,
 * each of which needs two rows to be shown at all.  Returns zero
 * for a layout the cuts cannot take apart.
 */
static int
balweight(int top, int bot, int left, int right)
{
	struct mgwin	*wp;
	int	 e, a, b;

	if ((e = balcut(top, bot, left, right, TRUE)) != 0) {
		a = balweight(top, e, left, right);
		b = balweight(e + 1, bot, left, right);
		return (a == 0 || b == 0 ? 0 : a + b);
	}
	if ((e = balcut(top, bot, left, right, FALSE)) != 0) {
		a = balweight(top, bot, left, e);
		b = balweight(top, bot, e + 1, right);
		return (a == 0 || b == 0 ? 0 : a > b ? a : b);
	}
	a = 0;
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
		if (balin(wp, top, bot, left, right))
			a++;
	return (a == 1);
}

/*
 * Retile the windows of the old rectangle into the new rows.  The
 * sections at a horizontal cut get room in proportion to their
 * tallest stack, so the window heights come out even; sections at
 * a vertical cut keep their columns.
 */
static void
balassign(int ot, int ob, int nt, int nb, int left, int right)
{
	struct mgwin	*wp;
	int	 e, w1, w2, c;

	if ((e = balcut(ot, ob, left, right, TRUE)) != 0) {
		w1 = balweight(ot, e, left, right);
		w2 = balweight(e + 1, ob, left, right);
		if (w1 <= 0 || w2 <= 0)
			return;		/* balancewind() refused these */
		c = nt - 1 + (nb - nt + 1) * w1 / (w1 + w2);
		balassign(ot, e, nt, c, left, right);
		balassign(e + 1, ob, c + 1, nb, left, right);
		return;
	}
	if ((e = balcut(ot, ob, left, right, FALSE)) != 0) {
		balassign(ot, ob, nt, nb, left, e);
		balassign(ot, ob, nt, nb, e + 1, right);
		return;
	}
	/*
	 * Park the new position as negative until every window has
	 * one: the cut and weight scans judge the windows still to
	 * be placed by their old rows, which a window moved into an
	 * unvisited rectangle would corrupt.
	 */
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
		if (balin(wp, ot, ob, left, right)) {
			wp->w_toprow = -nt - 1;
			wp->w_ntrows = nb - nt;
			wp->w_rflag |= WFFULL | WFMODE;
		}
}

/*
 * Even out the window heights, like balance-windows in GNU Emacs.
 * Bound to C-x +.
 */
int
balancewind(int f, int n)
{
	struct mgwin	*wp;
	int	 w;

	if (wheadp->w_wndp == NULL)
		return (TRUE);
	if ((w = balweight(0, nrow - 2, 0, ncol)) == 0)
		return (dobeep_msg("Cannot balance this window layout"));
	if (nrow - 1 < 2 * w)
		return (dobeep_msg("Not enough room to balance"));
	balassign(0, nrow - 2, 0, nrow - 2, 0, ncol);
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
		if (wp->w_toprow < 0)
			wp->w_toprow = -wp->w_toprow - 1;
	sgarbf = TRUE;
	return (TRUE);
}

/*
 * Shift every window edge at or below the given seam row by delta
 * rows, or, with horiz false, at or right of the seam column by
 * delta columns: windows on the seam grow or shrink, windows past
 * it move as they are, the rest stay put.  All edges move by the
 * same amount, so any tiling of the screen survives.  Refuses,
 * without changing anything, when a window would come out below
 * one text row or two columns.
 */
static int
shiftedges(int seam, int delta, int horiz)
{
	struct mgwin	*wp;
	int	*pos, *size;

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		pos = horiz ? &wp->w_toprow : &wp->w_leftcol;
		size = horiz ? &wp->w_ntrows : &wp->w_ntcols;
		if (*pos <= seam && *pos + *size >= seam &&
		    *size + delta < (horiz ? 1 : 2))
			return (FALSE);
	}

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		pos = horiz ? &wp->w_toprow : &wp->w_leftcol;
		size = horiz ? &wp->w_ntrows : &wp->w_ntcols;
		if (*pos > seam)
			*pos += delta;
		else if (*pos + *size >= seam)
			*size += delta;
		wp->w_rflag |= WFFULL | WFMODE;
	}
	return (TRUE);
}

struct mgwin *
new_window(struct buffer *bp)
{
	struct mgwin *wp;

	wp = calloc(1, sizeof(struct mgwin));
	if (wp == NULL)
		return (NULL);

	wp->w_bufp = bp;
	wp->w_dotp = NULL;
	wp->w_doto = 0;
	wp->w_markp = NULL;
	wp->w_marko = 0;
	wp->w_markact = FALSE;
	wp->w_rflag = 0;
	wp->w_frame = 0;
	wp->w_wrapline = NULL;
	wp->w_dotline = wp->w_markline = 1;
	wp->w_leftcol = 0;
	wp->w_ntcols = ncol;
	if (bp)
		bp->b_nwnd++;
	return (wp);
}

/*
 * The window vertically adjacent to wp in the same column strip,
 * above or below, or NULL.  With side by side windows the list
 * neighbor is not necessarily the neighbor on screen, and rows can
 * only move between windows of the same width without breaking the
 * neighbors of the wider one.
 */
static struct mgwin *
vneighbor(struct mgwin *wp)
{
	struct mgwin	*p;

	for (p = wheadp; p != NULL; p = p->w_wndp) {
		if (p->w_leftcol != wp->w_leftcol ||
		    p->w_ntcols != wp->w_ntcols)
			continue;
		if (p->w_toprow == wp->w_toprow + wp->w_ntrows + 1 ||
		    wp->w_toprow == p->w_toprow + p->w_ntrows + 1)
			return (p);
	}
	return (NULL);
}

/*
 * Reposition dot in the current window to line "n".  If the argument is
 * positive, it is that line.  If it is negative it is that line from the
 * bottom.  If it is 0 the window is centered (this is what the standard
 * redisplay code does).
 */
int
reposition(int f, int n)
{
	curwp->w_frame = (f & FFARG) ? (n >= 0 ? n + 1 : n) : 0;
	curwp->w_rflag |= WFFRAME;
	sgarbf = TRUE;
	return (TRUE);
}

/*
 * Refresh the display.  A call is made to the "ttresize" entry in the
 * terminal handler, which tries to reset "nrow" and "ncol".  They will,
 * however, never be set outside of the NROW or NCOL range.  If the display
 * changed size, arrange that everything is redone, then call "update" to
 * fix the display.  We do this so the new size can be displayed.  In the
 * normal case the call to "update" in "main.c" refreshes the screen, and
 * all of the windows need not be recomputed. This call includes a
 * 'force' parameter to ensure that the redraw is done, even after a
 * a suspend/continue (where the window size parameters will already
 * be updated). Note that when you get to the "display unusable"
 * message, the screen will be messed up. If you make the window bigger
 * again, and send another command, everything will get fixed!
 */
int
redraw(int f, int n)
{
	return (do_redraw(f, n, FALSE));
}

int
do_redraw(int f, int n, int force)
{
	struct mgwin	*wp;
	int		 oldnrow, oldncol, delta, bot, rgt;

	oldnrow = nrow;
	oldncol = ncol;
	ttresize();
	if (nrow != oldnrow || ncol != oldncol || force) {

		/*
		 * Each delta is measured against the actual tiling, not
		 * the old terminal size, so a refused resize heals on
		 * the next one.  The focused window absorbs the delta,
		 * else the windows on the far edge, else the layout
		 * collapses to a single window.
		 */
		rgt = 0;
		for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
			if (wp->w_leftcol + wp->w_ntcols > rgt)
				rgt = wp->w_leftcol + wp->w_ntcols;
		if ((delta = ncol - rgt) != 0) {
			if (!shiftedges(curwp->w_leftcol + curwp->w_ntcols,
			    delta, FALSE) && !shiftedges(rgt, delta, FALSE))
				(void)onlywind(FFRAND, 0);
		}

		bot = 0;
		for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
			if (wp->w_toprow + wp->w_ntrows > bot)
				bot = wp->w_toprow + wp->w_ntrows;
		if ((delta = nrow - 2 - bot) != 0) {
			if (!shiftedges(curwp->w_toprow + curwp->w_ntrows,
			    delta, TRUE) && !shiftedges(bot, delta, TRUE)) {
				if (nrow < 3) {
					dobeep();
					ewprintf("Display unusable");
					return (FALSE);
				}
				(void)onlywind(FFRAND, 0);
			}
		}
		sgarbf = TRUE;
		update(CMODE);
	} else
		sgarbf = TRUE;
	return (TRUE);
}

/*
 * Select the window beside the current one, like windmove in GNU
 * Emacs: the neighbor sharing the edge in the given direction,
 * with overlapping rows or columns.
 */
static int
gotowind(int dx, int dy)
{
	struct mgwin	*wp;

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp == curwp)
			continue;
		if (dx > 0 &&
		    wp->w_leftcol != curwp->w_leftcol + curwp->w_ntcols + 1)
			continue;
		if (dx < 0 &&
		    curwp->w_leftcol != wp->w_leftcol + wp->w_ntcols + 1)
			continue;
		if (dy > 0 &&
		    wp->w_toprow != curwp->w_toprow + curwp->w_ntrows + 1)
			continue;
		if (dy < 0 &&
		    curwp->w_toprow != wp->w_toprow + wp->w_ntrows + 1)
			continue;
		/* text rows or columns must overlap, edges do not count */
		if (dx != 0 &&
		    (wp->w_toprow >= curwp->w_toprow + curwp->w_ntrows ||
		    curwp->w_toprow >= wp->w_toprow + wp->w_ntrows))
			continue;
		if (dy != 0 &&
		    (wp->w_leftcol >= curwp->w_leftcol + curwp->w_ntcols ||
		    curwp->w_leftcol >= wp->w_leftcol + wp->w_ntcols))
			continue;
		curwp = wp;
		curbp = wp->w_bufp;
		return (TRUE);
	}
	return (dobeep_msg("No window there"));
}

int
windmoveleft(int f, int n)
{
	return (gotowind(-1, 0));
}

int
windmoveright(int f, int n)
{
	return (gotowind(1, 0));
}

int
windmoveup(int f, int n)
{
	return (gotowind(0, -1));
}

int
windmovedown(int f, int n)
{
	return (gotowind(0, 1));
}

/*
 * The command to make the next window (next => down the screen) the current
 * window. There are no real errors, although the command does nothing if
 * there is only 1 window on the screen.
 */
int
nextwind(int f, int n)
{
	struct mgwin	*wp;

	if ((wp = curwp->w_wndp) == NULL)
		wp = wheadp;
	curwp = wp;
	curbp = wp->w_bufp;
	return (TRUE);
}

/* not in GNU Emacs */
/*
 * This command makes the previous window (previous => up the screen) the
 * current window. There are no errors, although the command does not do
 * a lot if there is only 1 window.
 */
int
prevwind(int f, int n)
{
	struct mgwin	*wp1, *wp2;

	wp1 = wheadp;
	wp2 = curwp;
	if (wp1 == wp2)
		wp2 = NULL;
	while (wp1->w_wndp != wp2)
		wp1 = wp1->w_wndp;
	curwp = wp1;
	curbp = wp1->w_bufp;
	return (TRUE);
}

/*
 * This command makes the current window the only window on the screen.  Try
 * to set the framing so that "." does not have to move on the display.  Some
 * care has to be taken to keep the values of dot and mark in the buffer
 * structures right if the destruction of a window makes a buffer become
 * undisplayed.
 */
int
onlywind(int f, int n)
{
	struct mgwin	*wp;
	struct line	*lp;
	int		 i;

	while (wheadp != curwp) {
		wp = wheadp;
		wheadp = wp->w_wndp;
		if (--wp->w_bufp->b_nwnd == 0) {
			wp->w_bufp->b_dotp = wp->w_dotp;
			wp->w_bufp->b_doto = wp->w_doto;
			wp->w_bufp->b_markp = wp->w_markp;
			wp->w_bufp->b_marko = wp->w_marko;
			wp->w_bufp->b_dotline = wp->w_dotline;
			wp->w_bufp->b_markline = wp->w_markline;
		}
		free(wp);
	}
	while (curwp->w_wndp != NULL) {
		wp = curwp->w_wndp;
		curwp->w_wndp = wp->w_wndp;
		if (--wp->w_bufp->b_nwnd == 0) {
			wp->w_bufp->b_dotp = wp->w_dotp;
			wp->w_bufp->b_doto = wp->w_doto;
			wp->w_bufp->b_markp = wp->w_markp;
			wp->w_bufp->b_marko = wp->w_marko;
			wp->w_bufp->b_dotline = wp->w_dotline;
			wp->w_bufp->b_markline = wp->w_markline;
		}
		free(wp);
	}
	lp = curwp->w_linep;
	i = curwp->w_toprow;
	while (i != 0 && lback(lp) != curbp->b_headp) {
		--i;
		lp = lback(lp);
	}
	curwp->w_toprow = 0;

	/* 2 = mode, echo */
	curwp->w_ntrows = nrow - 2;
	curwp->w_leftcol = 0;
	curwp->w_ntcols = ncol;
	curwp->w_linep = lp;
	curwp->w_rflag |= WFMODE | WFFULL;
	return (TRUE);
}

/*
 * Allocate the second window of a split, showing the same buffer,
 * with the same dot and mark, in the same column strip as the
 * current window.
 */
static struct mgwin *
splitstart(void)
{
	struct mgwin	*wp;

	wp = new_window(curbp);
	if (wp == NULL) {
		dobeep();
		ewprintf("Unable to create a window");
		return (NULL);
	}

	/* use the current dot and mark */
	wp->w_dotp = curwp->w_dotp;
	wp->w_doto = curwp->w_doto;
	wp->w_markp = curwp->w_markp;
	wp->w_marko = curwp->w_marko;
	wp->w_dotline = curwp->w_dotline;
	wp->w_markline = curwp->w_markline;
	wp->w_leftcol = curwp->w_leftcol;
	wp->w_ntcols = curwp->w_ntcols;

	return (wp);
}

/*
 * Split the current window.  A window smaller than 3 lines cannot be split.
 * The only other error that is possible is a "malloc" failure allocating the
 * structure for the new window.
 * If called with a FFOTHARG, flags on the new window are set to 'n'.
 */
int
splitwind(int f, int n)
{
	struct mgwin	*wp, *wp1, *wp2;
	struct line	*lp;
	int		 ntru, ntrd, ntrl;

	if (curwp->w_ntrows < 3) {
		dobeep();
		ewprintf("Cannot split a %d line window", curwp->w_ntrows);
		return (FALSE);
	}
	if ((wp = splitstart()) == NULL)
		return (FALSE);

	/* figure out which half of the screen we're in */
	ntru = (curwp->w_ntrows - 1) / 2;	/* Upper size */
	ntrl = (curwp->w_ntrows - 1) - ntru;	/* Lower size */

	for (lp = curwp->w_linep, ntrd = 0; lp != curwp->w_dotp;
	    lp = lforw(lp))
		ntrd++;

	lp = curwp->w_linep;

	/* old is upper window */
	if (ntrd <= ntru) {
		/* hit mode line */
		if (ntrd == ntru)
			lp = lforw(lp);
		curwp->w_ntrows = ntru;
		wp->w_wndp = curwp->w_wndp;
		curwp->w_wndp = wp;
		wp->w_toprow = curwp->w_toprow + ntru + 1;
		wp->w_ntrows = ntrl;
	/* old is lower window */
	} else {
		wp1 = NULL;
		wp2 = wheadp;
		while (wp2 != curwp) {
			wp1 = wp2;
			wp2 = wp2->w_wndp;
		}
		if (wp1 == NULL)
			wheadp = wp;
		else
			wp1->w_wndp = wp;
		wp->w_wndp = curwp;
		wp->w_toprow = curwp->w_toprow;
		wp->w_ntrows = ntru;

		/* mode line */
		++ntru;
		curwp->w_toprow += ntru;
		curwp->w_ntrows = ntrl;
		while (ntru--)
			lp = lforw(lp);
	}

	/* adjust the top lines if necessary */
	curwp->w_linep = lp;
	wp->w_linep = lp;

	curwp->w_rflag |= WFMODE | WFFULL;
	wp->w_rflag |= WFMODE | WFFULL;
	/* if FFOTHARG, set flags) */
	if (f & FFOTHARG)
		wp->w_flag = n;

	return (TRUE);
}

/*
 * Split the current window side by side.  The current window becomes
 * the left half; the new window shows the same buffer to the right of
 * a one column divider.
 */
int
splitwindh(int f, int n)
{
	struct mgwin	*wp;
	int		 ntcl;

	if (curwp->w_ntcols < 8) {
		dobeep();
		ewprintf("Cannot split a %d column window",
		    curwp->w_ntcols);
		return (FALSE);
	}
	if ((wp = splitstart()) == NULL)
		return (FALSE);

	wp->w_toprow = curwp->w_toprow;
	wp->w_ntrows = curwp->w_ntrows;
	wp->w_linep = curwp->w_linep;

	ntcl = (curwp->w_ntcols - 1) / 2;
	wp->w_leftcol = curwp->w_leftcol + ntcl + 1;
	wp->w_ntcols = curwp->w_ntcols - 1 - ntcl;
	curwp->w_ntcols = ntcl;

	wp->w_wndp = curwp->w_wndp;
	curwp->w_wndp = wp;

	curwp->w_rflag |= WFMODE | WFFULL;
	wp->w_rflag |= WFMODE | WFFULL;
	if (f & FFOTHARG)
		wp->w_flag = n;

	return (TRUE);
}

/*
 * Enlarge the current window.  Find the window that loses space.  Make sure
 * it is big enough.  If so, hack the window descriptions, and ask redisplay
 * to do all the hard work.  You don't just set "force reframe" because dot
 * would move.
 */
int
enlargewind(int f, int n)
{
	struct mgwin	*adjwp;
	struct line	*lp;
	int		 i;

	if (n < 0)
		return (shrinkwind(f, -n));
	if (wheadp->w_wndp == NULL) {
		dobeep();
		ewprintf("Only one window");
		return (FALSE);
	}
	if ((adjwp = vneighbor(curwp)) == NULL) {
		dobeep();
		ewprintf("No window above or below to take space from");
		return (FALSE);
	}
	if (adjwp->w_ntrows <= n) {
		dobeep();
		ewprintf("Impossible change");
		return (FALSE);
	}

	/* shrink below */
	if (adjwp->w_toprow > curwp->w_toprow) {
		lp = adjwp->w_linep;
		for (i = 0; i < n && lp != adjwp->w_bufp->b_headp; ++i)
			lp = lforw(lp);
		adjwp->w_linep = lp;
		adjwp->w_toprow += n;
	/* shrink above */
	} else {
		lp = curwp->w_linep;
		for (i = 0; i < n && lback(lp) != curbp->b_headp; ++i)
			lp = lback(lp);
		curwp->w_linep = lp;
		curwp->w_toprow -= n;
	}
	curwp->w_ntrows += n;
	adjwp->w_ntrows -= n;
	curwp->w_rflag |= WFMODE | WFFULL;
	adjwp->w_rflag |= WFMODE | WFFULL;
	sgarbf = TRUE;
	return (TRUE);
}

/*
 * Shrink the current window.  Find the window that gains space.  Hack at the
 * window descriptions. Ask the redisplay to do all the hard work.
 */
int
shrinkwind(int f, int n)
{
	struct mgwin	*adjwp;
	struct line	*lp;
	int		 i;

	if (n < 0)
		return (enlargewind(f, -n));
	if (wheadp->w_wndp == NULL) {
		dobeep();
		ewprintf("Only one window");
		return (FALSE);
	}
	/*
	 * Bit of flakiness - FFRAND means it was an internal call, and
	 * to be trusted implicitly about sizes.
	 */
	if (!(f & FFRAND) && curwp->w_ntrows <= n) {
		dobeep();
		ewprintf("Impossible change");
		return (FALSE);
	}
	if ((adjwp = vneighbor(curwp)) == NULL) {
		if (!(f & FFRAND)) {
			dobeep();
			ewprintf("No window above or below to give "
			    "space to");
		}
		return (FALSE);
	}

	/* grow below */
	if (adjwp->w_toprow > curwp->w_toprow) {
		lp = adjwp->w_linep;
		for (i = 0; i < n && lback(lp) != adjwp->w_bufp->b_headp; ++i)
			lp = lback(lp);
		adjwp->w_linep = lp;
		adjwp->w_toprow -= n;
	/* grow above */
	} else {
		lp = curwp->w_linep;
		for (i = 0; i < n && lp != curbp->b_headp; ++i)
			lp = lforw(lp);
		curwp->w_linep = lp;
		curwp->w_toprow += n;
	}
	curwp->w_ntrows -= n;
	adjwp->w_ntrows += n;
	curwp->w_rflag |= WFMODE | WFFULL;
	adjwp->w_rflag |= WFMODE | WFFULL;
	sgarbf = TRUE;
	return (TRUE);
}

/*
 * Give the columns of wp, and the divider, to the column-adjacent
 * windows on one side.  Only safe when those windows tile wp's rows
 * exactly; a window sticking out above or below would end up
 * overlapping a third window, so that is checked first.
 */
static int
hmerge(struct mgwin *wp, int toleft)
{
	struct mgwin	*p;
	int		 rows = 0;

	for (p = wheadp; p != NULL; p = p->w_wndp) {
		if (toleft ? p->w_leftcol + p->w_ntcols + 1 != wp->w_leftcol
		    : wp->w_leftcol + wp->w_ntcols + 1 != p->w_leftcol)
			continue;
		/* rows not shared with wp are not affected */
		if (p->w_toprow >= wp->w_toprow + wp->w_ntrows + 1 ||
		    wp->w_toprow >= p->w_toprow + p->w_ntrows + 1)
			continue;
		if (p->w_toprow < wp->w_toprow ||
		    p->w_toprow + p->w_ntrows >
		    wp->w_toprow + wp->w_ntrows)
			return (FALSE);
		rows += p->w_ntrows + 1;
	}
	if (rows != wp->w_ntrows + 1)
		return (FALSE);
	for (p = wheadp; p != NULL; p = p->w_wndp) {
		if (toleft ? p->w_leftcol + p->w_ntcols + 1 != wp->w_leftcol
		    : wp->w_leftcol + wp->w_ntcols + 1 != p->w_leftcol)
			continue;
		if (p->w_toprow >= wp->w_toprow + wp->w_ntrows + 1 ||
		    wp->w_toprow >= p->w_toprow + p->w_ntrows + 1)
			continue;
		if (!toleft)
			p->w_leftcol = wp->w_leftcol;
		p->w_ntcols += wp->w_ntcols + 1;
		p->w_rflag |= WFMODE | WFFULL;
	}
	return (TRUE);
}

/*
 * Delete current window. Call shrink-window to do the screen updating, then
 * throw away the window.
 */
int
delwind(int f, int n)
{
	struct mgwin	*wp, *nwp;

	wp = curwp;		/* Cheap...		 */

	if (wheadp->w_wndp == NULL) {
		dobeep();
		ewprintf("Only one window");
		return (FALSE);
	}

	if (vneighbor(wp) != NULL) {
		if (shrinkwind(FFRAND, wp->w_ntrows + 1) == FALSE)
			return (FALSE);
	} else if (!hmerge(wp, TRUE) && !hmerge(wp, FALSE)) {
		dobeep();
		ewprintf("No window to give this window's space to, "
		    "try delete-other-windows");
		return (FALSE);
	}
	if (--wp->w_bufp->b_nwnd == 0) {
		wp->w_bufp->b_dotp = wp->w_dotp;
		wp->w_bufp->b_doto = wp->w_doto;
		wp->w_bufp->b_markp = wp->w_markp;
		wp->w_bufp->b_marko = wp->w_marko;
		wp->w_bufp->b_dotline = wp->w_dotline;
		wp->w_bufp->b_markline = wp->w_markline;
	}

	/* since shrinkwind did't crap out, we know we have a second window */
	if (wp == wheadp)
		wheadp = curwp = wp->w_wndp;
	else if ((curwp = wp->w_wndp) == NULL)
		curwp = wheadp;
	curbp = curwp->w_bufp;
	for (nwp = wheadp; nwp != NULL; nwp = nwp->w_wndp)
		if (nwp->w_wndp == wp) {
			nwp->w_wndp = wp->w_wndp;
			break;
		}
	free(wp);
	return (TRUE);
}
