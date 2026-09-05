/*	$OpenBSD: modes.c,v 1.22 2023/04/17 09:49:04 op Exp $	*/

/* This file is in the public domain. */

/*
 * Commands to toggle modes.   Without an argument, these functions will
 * toggle the given mode.  A negative or zero argument will turn the mode
 * off.  A positive argument will turn the mode on.
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "def.h"
#include "kbd.h"

int	changemode(int, int, char *);

int	 defb_nmodes = 0;
struct maps_s	*defb_modes[PBMODES] = { &fundamental_mode };
int	 defb_flag = 0;

/*
 * True when the buffer has the named mode enabled.
 */
int
buf_hasmode(struct buffer *bp, const char *name)
{
	struct maps_s	*m;
	int	 i;

	if ((m = name_mode(name)) == NULL)
		return (FALSE);
	for (i = 0; i <= bp->b_nmodes; i++)
		if (bp->b_modes[i] == m)
			return (TRUE);
	return (FALSE);
}

/*
 * The mode that names the buffer: the last one that is not merely a
 * qualifier, or fundamental when there is no other.
 */
struct maps_s *
buf_major(struct buffer *bp)
{
	struct maps_s	*m = bp->b_modes[0];
	int		 i;

	for (i = 1; i <= bp->b_nmodes; i++)
		if (!bp->b_modes[i]->p_minor)
			m = bp->b_modes[i];
	return (m);
}

/*
 * Set, clear, or toggle a buffer flag, on the defaults while the
 * startup file is being read and on the current buffer otherwise.
 */
static void
modeflag(int bit, int on)
{
	int	 flag = inrc ? defb_flag : curbp->b_flag;

	if (on < 0)
		flag ^= bit;
	else if (on)
		flag |= bit;
	else
		flag &= ~bit;

	if (inrc)
		defb_flag = flag;
	else
		curbp->b_flag = flag;
}

/*
 * Set the tab width a mode wants, on the defaults while the startup
 * file is being read and on the current buffer otherwise.
 */
void
modetabw(int n)
{
	if (inrc)
		defb_tabw = n;
	else
		curbp->b_tabw = n;
}

/*
 * A mode named in the startup file is meant for the files opened
 * afterwards, not for *scratch*, which is the only buffer there is
 * at the time, so it goes on the defaults instead.
 */
int
changemode(int f, int n, char *newmode)
{
	int	 i, nmodes;
	struct maps_s	*m, **modes;
	struct mgwin	*wp;

	if ((m = name_mode(newmode)) == NULL) {
		dobeep();
		ewprintf("Can't find mode %s", newmode);
		return (FALSE);
	}
	if (inrc) {
		modes = defb_modes;
		nmodes = defb_nmodes;
	} else {
		modes = curbp->b_modes;
		nmodes = curbp->b_nmodes;
	}
	if (!(f & FFARG)) {
		for (i = 0; i <= nmodes; i++)
			if (modes[i] == m) {
				/* mode already set */
				n = 0;
				break;
			}
	}
	if (n > 0) {
		for (i = 0; i <= nmodes; i++)
			if (modes[i] == m)
				/* mode already set */
				return (TRUE);
		if (nmodes >= PBMODES - 1) {
			dobeep();
			ewprintf("Too many modes");
			return (FALSE);
		}
		modes[++nmodes] = m;
	} else {
		/* fundamental is modes[0] and can't be unset */
		for (i = 1; i <= nmodes && m != modes[i]; i++)
			;
		if (i > nmodes)
			return (TRUE);	/* mode wasn't set */
		for (; i < nmodes; i++)
			modes[i] = modes[i + 1];
		nmodes--;
	}
	if (inrc)
		defb_nmodes = nmodes;
	else
		curbp->b_nmodes = nmodes;
	/* the modes decide the syntax highlighting, redraw */
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
		if (wp->w_bufp == curbp)
			wp->w_rflag |= WFMODE | WFFULL;
	return (TRUE);
}

int
indentmode(int f, int n)
{
	return (changemode(f, n, "indent"));
}

int
fillmode(int f, int n)
{
	return (changemode(f, n, "fill"));
}

/*
 * Wrap a line too long for the window onto the lines below it,
 * rather than truncating it at the right edge.
 */
int
wrapmode(int f, int n)
{
	struct mgwin	*wp;

	if (changemode(f, n, "wrap") != TRUE)
		return (FALSE);
	for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
		if (wp->w_bufp == curbp)
			wp->w_rflag |= WFFRAME | WFFULL;
	return (TRUE);
}

int
notabmode(int f, int n)
{
	if (changemode(f, n, "notab") == FALSE)
		return (FALSE);
	modeflag(BFNOTAB, (f & FFARG) ? n > 0 : -1);
	return (TRUE);
}

int
overwrite_mode(int f, int n)
{
	if (changemode(f, n, "overwrite") == FALSE)
		return (FALSE);
	modeflag(BFOVERWRITE, (f & FFARG) ? n > 0 : -1);
	return (TRUE);
}

int
set_default_mode(int f, int n)
{
	int	 i;
	struct maps_s	*m;
	char	 modebuf[32], *bufp;

	if ((bufp = eread("Set Default Mode: ", modebuf, sizeof(modebuf),
	    EFNEW)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if ((m = name_mode(modebuf)) == NULL) {
		dobeep();
		ewprintf("can't find mode %s", modebuf);
		return (FALSE);
	}
	if (!(f & FFARG)) {
		for (i = 0; i <= defb_nmodes; i++)
			if (defb_modes[i] == m) {
				/* mode already set */
				n = 0;
				break;
			}
	}
	if (n > 0) {
		for (i = 0; i <= defb_nmodes; i++)
			if (defb_modes[i] == m)
				/* mode already set */
				return (TRUE);
		if (defb_nmodes >= PBMODES - 1) {
			dobeep();
			ewprintf("Too many modes");
			return (FALSE);
		}
		defb_modes[++defb_nmodes] = m;
	} else {
		/* fundamental is defb_modes[0] and can't be unset */
		for (i = 1; i <= defb_nmodes && m != defb_modes[i]; i++);
		if (i > defb_nmodes)
			/* mode was not set */
			return (TRUE);
		for (; i < defb_nmodes; i++)
			defb_modes[i] = defb_modes[i + 1];
		defb_nmodes--;
	}
	if (strcmp(modebuf, "overwrite") == 0) {
		if (n <= 0)
			defb_flag &= ~BFOVERWRITE;
		else
			defb_flag |= BFOVERWRITE;
	}
	if (strcmp(modebuf, "notab") == 0) {
		if (n <= 0)
			defb_flag &= ~BFNOTAB;
		else
			defb_flag |= BFNOTAB;
	}
	return (TRUE);
}
