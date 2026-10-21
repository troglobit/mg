/* This file is in the public domain. */

/*
 * Registers: a place or a piece of text kept under a single
 * character, on the C-x r keys.  point-to-register saves where dot
 * is, jump-to-register goes back there, copy-to-register saves the
 * region, and insert-register inserts it.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"

struct reg {
	struct buffer	*bp;		/* a place: its buffer,		*/
	struct line	*lp;		/* its line, while it lasts,	*/
	int		 lineno, doto;	/* else its number		*/
	char		*text;		/* or a piece of text		*/
	int		 len;
};

static struct reg	regs[256];

/*
 * A register holds a place or a piece of text, whichever came last.
 */
static void
regclear(struct reg *r)
{
	free(r->text);
	memset(r, 0, sizeof(*r));
}

/*
 * The register named by the key typed at the prompt, or NULL on C-g.
 */
static struct reg *
getreg(const char *prompt)
{
	int	 c;

	ewprintf("%s", prompt);
	c = getkey(FALSE);
	eerase();
	if (c == ABORT || c == CCHR('G'))
		return (NULL);
	return (&regs[CHARMASK(c)]);
}

int
pointtoreg(int f, int n)
{
	struct reg	*r;

	if ((r = getreg("Point to register: ")) == NULL)
		return (ABORT);
	regclear(r);
	r->bp = curbp;
	r->lp = curwp->w_dotp;
	r->lineno = curwp->w_dotline;
	r->doto = curwp->w_doto;
	return (TRUE);
}

/*
 * Go to the place in a register: its buffer in the current window,
 * then its line if that is still in the buffer, else the line of the
 * same number.
 */
int
jumptoreg(int f, int n)
{
	struct reg	*r;
	struct buffer	*bp;
	struct line	*lp;
	int	 lineno;

	if ((r = getreg("Jump to register: ")) == NULL)
		return (ABORT);
	if (r->bp == NULL)
		return (dobeep_msg("Register does not contain a place"));
	for (bp = bheadp; bp != NULL && bp != r->bp; bp = bp->b_bufp)
		;
	if (bp == NULL) {
		regclear(r);
		return (dobeep_msg("The buffer of the register is gone"));
	}
	curbp = bp;
	if (showbuffer(bp, curwp, WFFRAME | WFFULL) != TRUE)
		return (FALSE);
	for (lp = bfirstlp(bp), lineno = 1; lp != bp->b_headp && lp != r->lp;
	     lp = lforw(lp), lineno++)
		;
	if (lp != bp->b_headp) {
		curwp->w_dotp = lp;
		curwp->w_dotline = lineno;
		curwp->w_rflag |= WFMOVE;
	} else
		(void)setlineno(r->lineno);
	lp = curwp->w_dotp;
	curwp->w_doto = r->doto < llength(lp) ? r->doto : llength(lp);
	return (TRUE);
}

int
copytoreg(int f, int n)
{
	struct reg	*r;
	char	*text;
	int	 len;

	if ((r = getreg("Copy to register: ")) == NULL)
		return (ABORT);
	if ((text = regiontext(&len)) == NULL)
		return (FALSE);
	regclear(r);
	r->text = text;
	r->len = len;
	return (TRUE);
}

/*
 * Insert the text of a register at dot, which stays before it with
 * the mark after, or the other way round with an argument.  The mark
 * is not activated, so that typing goes on inserting.
 */
int
insertreg(int f, int n)
{
	struct reg	*r;
	int	 s = TRUE;

	if ((r = getreg("Insert register: ")) == NULL)
		return (ABORT);
	if (r->text == NULL)
		return (dobeep_msg("Register does not contain text"));
	if (curbp->b_flag & BFREADONLY)
		return (dobeep_msg("Buffer is read-only"));
	isetmark();
	undo_boundary_enable(FFRAND, 0);
	region_put_data(r->text, r->len);
	undo_boundary_enable(FFRAND, 1);
	if (!(f & FFARG)) {
		s = swapmark(FFARG, 0);
		mark_deactivate(curwp);
	}
	return (s);
}
