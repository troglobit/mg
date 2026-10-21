/* This file is in the public domain. */

/*
 * Rectangle commands: the region between mark and dot read as the
 * columns between them on each of their lines.  The C-x r keys act
 * on that rectangle as it is; rectangle-mark-mode on C-x SPC draws it
 * and has C-w, M-w, C-d, DEL, C-o and C-t act on it, and C-y yank
 * back what a rectangle kill took.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"

struct rect {
	struct line	*lp;		/* top line			*/
	int		 lineno;	/* its number			*/
	int		 nlines;
	int		 c1, c2;	/* the columns, c1 before c2	*/
};

static char	**rows;		/* the rectangle last killed or copied	*/
static int	  nrows;
static int	  pending;	/* and the one C-y yanks		*/

/*
 * The rectangle between mark and dot, in a buffer that can be
 * written when write is set.
 */
static int
rect_get(struct rect *r, int write)
{
	int	 cd, cm;

	if (curwp->w_markp == NULL)
		return (dobeep_msg("No mark set in this window"));
	if (write && (curbp->b_flag & BFREADONLY))
		return (dobeep_msg("Buffer is read-only"));
	cd = bytecol(curwp->w_dotp, curwp->w_doto, curbp->b_tabw);
	cm = bytecol(curwp->w_markp, curwp->w_marko, curbp->b_tabw);
	if (curwp->w_markline <= curwp->w_dotline) {
		r->lp = curwp->w_markp;
		r->lineno = curwp->w_markline;
		r->nlines = curwp->w_dotline - curwp->w_markline + 1;
	} else {
		r->lp = curwp->w_dotp;
		r->lineno = curwp->w_dotline;
		r->nlines = curwp->w_markline - curwp->w_dotline + 1;
	}
	r->c1 = cd < cm ? cd : cm;
	r->c2 = cd < cm ? cm : cd;
	return (TRUE);
}

static void
setdot(struct line *lp, int o, int lineno)
{
	curwp->w_dotp = lp;
	curwp->w_doto = o;
	curwp->w_dotline = lineno;
	curwp->w_rflag |= WFMOVE;
}

/*
 * The text of lp between columns c1 and c2, padded with spaces to
 * that width where the line ends before c2 or a tab or wide
 * character crosses an edge.  Malloced.
 */
static char *
rowtext(const struct line *lp, int c1, int c2)
{
	char	*row;
	int	 c, i, l, n, pad, w;

	if ((row = malloc(llength(lp) + c2 - c1 + 1)) == NULL)
		return (NULL);
	n = 0;
	for (i = c = 0; i < llength(lp) && c < c2; i += l, c += w) {
		w = charcols(lp, i, c, curbp->b_tabw, &l);
		if (c + w <= c1)
			continue;
		if (c >= c1 && c + w <= c2 && lgetc(lp, i) != '\t') {
			memcpy(row + n, ltext(lp) + i, l);
			n += l;
			continue;
		}
		/* a tab, or something crossing an edge, as spaces */
		pad = (c + w < c2 ? c + w : c2) - (c > c1 ? c : c1);
		memset(row + n, ' ', pad);
		n += pad;
	}
	pad = c2 - (c > c1 ? (c < c2 ? c : c2) : c1);
	memset(row + n, ' ', pad);
	row[n + pad] = '\0';
	return (row);
}

/*
 * Put dot at column col of its line, a tab that spans the column
 * turned into spaces first, and a line that ends before it padded
 * with spaces when pad is set.  The column reached: less than col
 * when the line is shorter, or a wide character spans it.
 */
static int
tocol(int col, int pad)
{
	struct line	*lp = curwp->w_dotp;
	int	 c, i, len, w;

	i = colbyte(lp, col, curbp->b_tabw, &c);
	if (c < col && i < llength(lp) && lgetc(lp, i) == '\t') {
		w = charcols(lp, i, c, curbp->b_tabw, &len);
		curwp->w_doto = i;
		if (ldelete(1, KNONE) != TRUE || linsert(w, ' ') != TRUE)
			return (c);
		i += col - c;
		c = col;
	}
	curwp->w_doto = i;
	if (pad && c < col && i == llength(lp)) {
		if (linsert(col - c, ' ') != TRUE)
			return (c);
		c = col;
	}
	return (c);
}

/*
 * Put dot at the left edge of the rectangle on line lp, number ln,
 * and return the number of bytes to its right edge; -1 when the line
 * ends before the rectangle and pad is not set.
 */
static int
span(struct line *lp, int ln, struct rect *r, int pad)
{
	int	 e, s;

	setdot(lp, 0, ln);
	if (tocol(r->c1, pad) < r->c1 && curwp->w_doto == llength(lp))
		return (-1);
	s = curwp->w_doto;
	(void)tocol(r->c2, 0);
	e = curwp->w_doto;
	setdot(lp, s, ln);
	return (e - s);
}

/*
 * Leave dot at the top left corner, the mark cleared, the way the
 * region commands do.
 */
static int
rect_done(struct rect *r)
{
	setdot(r->lp, 0, r->lineno);
	(void)tocol(r->c1, 0);
	(void)clearmark(FFARG, 0);
	undo_boundary_enable(FFRAND, 1);
	curwp->w_rflag |= WFFULL;
	return (TRUE);
}

/*
 * Save the rectangle, delete it, or both.
 */
static int
rect_cut(int save, int delete)
{
	struct rect	 r;
	struct line	*lp;
	int	 i, n;

	if (rect_get(&r, delete) != TRUE)
		return (FALSE);
	if (save) {
		for (i = 0; i < nrows; i++)
			free(rows[i]);
		free(rows);
		nrows = 0;
		if ((rows = calloc(r.nlines, sizeof(*rows))) == NULL)
			return (dobeep_msg("Out of memory"));
		pending = TRUE;
	}
	undo_boundary_enable(FFRAND, 0);
	for (i = 0, lp = r.lp; i < r.nlines; i++, lp = lforw(lp)) {
		if (save && (rows[nrows++] = rowtext(lp, r.c1, r.c2)) == NULL)
			break;
		if (delete && (n = span(lp, r.lineno + i, &r, 0)) > 0 &&
		    ldelete(n, KNONE) != TRUE)
			break;
	}
	return (rect_done(&r));
}

int
killrect(int f, int n)
{
	return (rect_cut(TRUE, TRUE));
}

int
delrect(int f, int n)
{
	return (rect_cut(FALSE, TRUE));
}

int
copyrect(int f, int n)
{
	return (rect_cut(TRUE, FALSE));
}

/*
 * Insert the saved rectangle with its top left corner at dot, a row
 * to a line, lines padded or added as needed.  The mark goes to the
 * corner, dot after the last row.
 */
int
yankrect(int f, int n)
{
	struct line	*lp;
	int	 col, i, lineno;

	if (nrows == 0)
		return (dobeep_msg("No rectangle to yank"));
	if (curbp->b_flag & BFREADONLY)
		return (dobeep_msg("Buffer is read-only"));
	col = bytecol(curwp->w_dotp, curwp->w_doto, curbp->b_tabw);
	lp = curwp->w_dotp;
	lineno = curwp->w_dotline;
	isetmark();
	undo_boundary_enable(FFRAND, 0);
	for (i = 0; i < nrows; i++) {
		setdot(lp, 0, lineno + i);
		(void)tocol(col, 1);
		if (insstr(rows[i]) != TRUE)
			break;
		if (i + 1 == nrows)
			break;
		if (lforw(lp) == curbp->b_headp) {
			(void)gotoeol(FFRAND, 1);
			if (lnewline() != TRUE)
				break;
		}
		lp = lforw(lp);
	}
	undo_boundary_enable(FFRAND, 1);
	curwp->w_rflag |= WFFULL;
	return (TRUE);
}

/*
 * Push the rectangle right by its width in spaces, leaving it blank.
 */
int
openrect(int f, int n)
{
	struct rect	 r;
	struct line	*lp;
	int	 i;

	if (rect_get(&r, TRUE) != TRUE)
		return (FALSE);
	undo_boundary_enable(FFRAND, 0);
	for (i = 0, lp = r.lp; i < r.nlines; i++, lp = lforw(lp)) {
		setdot(lp, 0, r.lineno + i);
		if (tocol(r.c1, 0) < r.c1)
			continue;
		if (linsert(r.c2 - r.c1, ' ') != TRUE)
			break;
	}
	return (rect_done(&r));
}

/*
 * Blank the rectangle out, keeping its place.  A line that ends
 * inside it loses its tail instead.
 */
int
clearrect(int f, int n)
{
	struct rect	 r;
	struct line	*lp;
	int	 i, w;

	if (rect_get(&r, TRUE) != TRUE)
		return (FALSE);
	undo_boundary_enable(FFRAND, 0);
	for (i = 0, lp = r.lp; i < r.nlines; i++, lp = lforw(lp)) {
		if ((w = span(lp, r.lineno + i, &r, 0)) < 0)
			continue;
		if (ldelete(w, KNONE) != TRUE)
			break;
		if (curwp->w_doto < llength(lp) &&
		    linsert(r.c2 - r.c1, ' ') != TRUE)
			break;
	}
	return (rect_done(&r));
}

/*
 * Replace each row of the rectangle with a string, lines that end
 * before it padded to reach it.
 */
int
stringrect(int f, int n)
{
	struct rect	 r;
	struct line	*lp;
	char	 buf[NLINE], *s;
	int	 i, w;

	if (rect_get(&r, TRUE) != TRUE)
		return (FALSE);
	if ((s = eread("String rectangle: ", buf, sizeof(buf),
	    EFNEW | EFCR)) == NULL)
		return (ABORT);
	undo_boundary_enable(FFRAND, 0);
	for (i = 0, lp = r.lp; i < r.nlines; i++, lp = lforw(lp)) {
		w = span(lp, r.lineno + i, &r, 1);
		if (ldelete(w, KNONE) != TRUE || insstr(s) != TRUE)
			break;
	}
	return (rect_done(&r));
}

/*
 * Toggle rectangle-mark-mode in the current window: the region is
 * drawn and worked on as a rectangle, with the mark set at dot if
 * it was not active.
 */
int
rectmode(int f, int n)
{
	int	 on;

	on = (f & FFARG) ? n > 0 : !curwp->w_rect;
	if (on && (!curwp->w_markact || curwp->w_markp == NULL)) {
		isetmark();
		curwp->w_markact = TRUE;
	}
	curwp->w_rect = on;
	thisflag |= CFMARK;
	curwp->w_rflag |= WFFULL;
	ewprintf("Rectangle mark mode %sabled", on ? "en" : "dis");
	return (TRUE);
}

/*
 * Whether C-y yanks the rectangle: the last kill was one, and no
 * ordinary kill has started a kill buffer since.
 */
int
rect_yankpending(void)
{
	return (pending && nrows > 0);
}

void
rect_forget(void)
{
	pending = FALSE;
}
