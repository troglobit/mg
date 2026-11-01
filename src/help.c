/*	$OpenBSD: help.c,v 1.35 2015/03/19 21:22:15 bcallah Exp $	*/

/* This file is in the public domain. */

/*
 * Help functions for Mg 2
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"
#include "key.h"
#include "macro.h"

#define KEYNAMESZ	6 /* max is "C-SPC" */

const char *hlp = "C-h q  quick help | C-h t  tutorial | C-h b  key bindings | C = Ctrl | M = Alt";

int		 helpsh = TRUE;		/* help-text in echo buffer */
static int	 helpset = FALSE;	/* user set it, keep hands off */

static int	showall(struct buffer *, KEYMAP *, char *);
static PF	keyfunc(const KCHAR *, int, int *);

/*
 * Toggle permanent display of short help text in echo buffer
 */
int
helptoggle(int f, int n)
{
	if (f & FFARG)
		helpsh = n > 0;
	else
		helpsh = !helpsh;
	helpset = TRUE;

	sgarbf = TRUE;

	return (TRUE);
}

/*
 * The startup help is a courtesy to beginners: dismiss it on the
 * first key press, or after ten seconds, unless the user asked to
 * keep it with display-help-mode.  Anything already in the echo line
 * outranks it.
 */
void
helpstartup(void)
{
	if (epresf != FALSE) {
		if (helpset == FALSE)
			helpsh = FALSE;
	} else if (helpsh == TRUE) {
		ewprintf(" %s", hlp);
		update(CMODE);	/* park the cursor in the buffer */
		if (helpset == FALSE) {
			(void)ttwait(10000);
			helpsh = FALSE;
			eerase();
		}
	}
}

/*
 * Read a key from the keyboard, and look it up in the keymap.
 * Returns the function bound to it, rescan when there is none.
 */
static PF
readkey(const char *prompt)
{
	size_t	 num;
	PF	 funct;
	int	 m;
	char	*pep;
	char	 dprompt[80];

	num = strlcpy(dprompt, prompt, sizeof(dprompt));
	if (num >= sizeof(dprompt))
		num = sizeof(dprompt) - 1;
	pep = dprompt + num;
	key.k_count = 0;
	for (;;) {
		ewprintf("%s", dprompt);
		pep[-1] = ' ';
		pep = getkeyname(pep, sizeof(dprompt) - (pep - dprompt),
		    key.k_chars[key.k_count++] = getkey(FALSE));
		if ((funct = keyfunc(key.k_chars, key.k_count, &m)) != NULL)
			return (funct);
		if (key.k_count == MAXKEY)
			return (rescan);
		*pep++ = '-';
		*pep = '\0';
	}
}

/*
 * Display the name of the function bound to a key.
 */
int
desckey(int f, int n)
{
	PF		 funct;
	const char	*name;

	if (inmacro)
		return (TRUE);	/* ignore inside keyboard macro */

	funct = readkey("Describe key briefly: ");
	if (funct == rescan || funct == selfinsert)
		ewprintf("%k is not bound to any function");
	else if ((name = function_name(funct)) != NULL)
		ewprintf("%k runs the command %s", name);
	else
		ewprintf("%k is bound to an unnamed function");
	return (TRUE);
}

/*
 * Show the command bound to a key, and every key it is on, in the
 * *help* buffer.
 */
int
describekey(int f, int n)
{
	struct buffer	*bp;
	PF		 funct;
	const char	*name;
	char		 kname[MAXKEY * KEYNAMESZ + 1];
	char		 keys[256];

	if (inmacro)
		return (TRUE);

	if ((funct = readkey("Describe key: ")) == rescan) {
		ewprintf("%k is undefined");
		return (TRUE);
	}
	if ((name = function_name(funct)) == NULL) {
		ewprintf("%k is bound to an unnamed function");
		return (TRUE);
	}
	keyseqname(kname, sizeof(kname), key.k_chars, key.k_count);

	if ((bp = popbuffer("*help*", popmode("view"))) == NULL)
		return (FALSE);
	if (addlinef(bp, "%s runs the command %s.", kname, name) == FALSE ||
	    addline(bp, "") == FALSE)
		return (FALSE);
	if (funct == selfinsert)
		(void)strlcpy(keys, "many ordinary text characters", sizeof(keys));
	else
		whereis(funct, keys, sizeof(keys));
	if (keys[0] && (addlinef(bp, "It is bound to %s.", keys) == FALSE ||
	    addline(bp, "") == FALSE))
		return (FALSE);
	if (addline(bp, "See the mg(1) manual for what it does.") == FALSE)
		return (FALSE);
	eerase();
	return (popselect(bp));
}

/*
 * Show the keys a command is on.
 */
int
whereiscmd(int f, int n)
{
	PF	 funct;
	char	 fname[NXNAME], keys[256], *bufp;

	if ((bufp = eread("Where is command: ", fname, sizeof(fname),
	    EFNEW | EFFUNC)) == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		return (FALSE);
	if ((funct = name_function(bufp)) == NULL)
		return (dobeep_msgs("No such command:", bufp));
	whereis(funct, keys, sizeof(keys));
	if (keys[0] == '\0')
		ewprintf("%s is not on any key", function_name(funct));
	else
		ewprintf("%s is on %s", function_name(funct), keys);
	return (TRUE);
}

/*
 * The function the n keys in keys run in the current buffer, the way
 * the command loop finds it: the top mode can bind a prefix of them,
 * the modes below only the whole sequence, and an unbound upper case
 * key falls back to its lower case one.  NULL when the keys are a
 * prefix, rescan when they are unbound.  *mode is set to the mode
 * that binds them.
 */
static PF
keyfunc(const KCHAR *keys, int n, int *mode)
{
	KEYMAP	*map, *pre;
	PF	 fp = NULL;
	int	 i;

	for (*mode = curbp->b_nmodes; *mode >= 0; --*mode) {
		map = curbp->b_modes[*mode]->p_map;
		for (i = 0; i < n; i++) {
			pre = map;
			if ((fp = doscan(map, keys[i], &map)) != NULL)
				break;
		}
		if (fp == rescan && i == n - 1 && ISUPPER(keys[i]))
			fp = doscan(pre, TOLOWER(keys[i]), &map);
		if (fp == NULL)
			return (NULL);
		if (fp != rescan && (i == n - 1 || *mode == curbp->b_nmodes))
			return (fp);
	}
	return (rescan);
}

#define NWHERE	32		/* key sequences where-is keeps */

struct where {
	PF	 fun;
	int	 mode;		/* the mode whose map is searched */
	KCHAR	 keys[MAXKEY];
	int	 n;
	struct {
		KCHAR	 keys[MAXKEY];
		int	 len;
	} found[NWHERE];
};

/*
 * Collect the sequences in map, after the depth keys in w->keys, that
 * run w->fun in the current buffer.
 */
static void
wherekeys(KEYMAP *map, int depth, struct where *w)
{
	KEYMAP	*next;
	PF	 fp;
	int	 c, mode;

	for (c = 0; c < 256 && w->n < NWHERE; c++) {
		w->keys[depth] = c;
		if ((fp = doscan(map, c, &next)) == NULL) {
			if (depth + 1 < MAXKEY)
				wherekeys(next, depth + 1, w);
		} else if (fp == w->fun &&
		    keyfunc(w->keys, depth + 1, &mode) == fp &&
		    mode == w->mode) {
			memcpy(w->found[w->n].keys, w->keys, sizeof(w->keys));
			w->found[w->n++].len = depth + 1;
		}
	}
}

/*
 * Put the keys fun is on in the current buffer in buf, shortest first
 * and as many as fit.
 */
void
whereis(PF fun, char *buf, size_t len)
{
	struct where	 w = { .fun = fun };
	char		 kname[MAXKEY * KEYNAMESZ + 1];
	int		 i, n;

	buf[0] = '\0';
	for (w.mode = curbp->b_nmodes; w.mode >= 0; w.mode--)
		wherekeys(curbp->b_modes[w.mode]->p_map, 0, &w);
	for (n = 1; n <= MAXKEY; n++)
		for (i = 0; i < w.n; i++) {
			if (w.found[i].len != n)
				continue;
			keyseqname(kname, sizeof(kname), w.found[i].keys, n);
			if (strlen(buf) + strlen(kname) + 3 > len)
				return;
			if (buf[0])
				(void)strlcat(buf, ", ", len);
			(void)strlcat(buf, kname, len);
		}
}

/*
 * This function creates a table, listing all of the command
 * keys and their current bindings, and stores the table in the
 * *help* pop-up buffer.  This lets Mg produce its own wall chart.
 */
int
wallchart(int f, int n)
{
	int		 m;
	struct buffer		*bp;

	if ((bp = popbuffer("*help*", popmode("view"))) == NULL)
		return (FALSE);
	for (m = curbp->b_nmodes; m > 0; m--) {
		if ((addlinef(bp, "Local keybindings for mode %s:",
				curbp->b_modes[m]->p_name) == FALSE) ||
		    (showall(bp, curbp->b_modes[m]->p_map, "") == FALSE) ||
		    (addline(bp, "") == FALSE))
			return (FALSE);
	}
	if ((addline(bp, "Global bindings:") == FALSE) ||
	    (showall(bp, fundamental_map, "") == FALSE))
		return (FALSE);
	return (popselect(bp));
}

static int
showall(struct buffer *bp, KEYMAP *map, char *prefix)
{
	KEYMAP	*newmap;
	char	 buf[MAXKEY*KEYNAMESZ + 1], keybuf[KEYNAMESZ];
	PF	 fun;
	int	 c;

	if (addline(bp, "") == FALSE)
		return (FALSE);

	/* XXX - 256 ? */
	for (c = 0; c < 256; c++) {
		fun = doscan(map, c, &newmap);
		if (fun == rescan || fun == selfinsert)
			continue;
		getkeyname(keybuf, sizeof(keybuf), c);
		(void)snprintf(buf, sizeof(buf), "%s%s ", prefix, keybuf);
		if (fun == NULL) {
			if (showall(bp, newmap, buf) == FALSE)
				return (FALSE);
		} else {
			if (addlinef(bp, "%-16s%s", buf,
				    function_name(fun)) == FALSE)
				return (FALSE);
		}
	}
	return (TRUE);
}

int
help_help(int f, int n)
{
	KEYMAP	*kp;
	PF	 funct;

	if ((kp = name_map("help")) == NULL)
		return (FALSE);
	ewprintf("a b c t q: ");
	do {
		funct = doscan(kp, getkey(FALSE), NULL);
	} while (funct == NULL || funct == help_help);

	if (macrodef && macrocount < MAXMACRO)
		macro[macrocount - 1].m_funct = funct;

	return ((*funct)(f, n));
}

int
apropos_command(int f, int n)
{
	struct buffer		*bp;
	struct list		*fnames, *el;
	char		 string[32];

	if (eread("apropos: ", string, sizeof(string), EFNUL | EFNEW) == NULL)
		return (ABORT);
	/* FALSE means we got a 0 character string, which is fine */
	if ((bp = popbuffer("*help*", popmode("view"))) == NULL)
		return (FALSE);

	fnames = complete_function_list("");
	for (el = fnames; el != NULL; el = el->l_next) {
		char buf[48];

		if (strstr(el->l_name, string) == NULL)
			continue;

		whereis(name_function(el->l_name), buf, sizeof(buf));

		if (addlinef(bp, "%-32s%s", el->l_name,  buf) == FALSE) {
			free_file_list(fnames);
			return (FALSE);
		}
	}
	free_file_list(fnames);
	return (popselect(bp));
}

/*
 * Show quick help to get started with Mg
 */
int
quickhelp(int f, int n)
{
	struct buffer	*bp;

	/* If already displayed, toggle off */
	if ((bp = bfind("*quick*", FALSE)) != NULL)
		return (popdone(bp));

	/* Create new */
	if ((bp = popbuffer("*quick*", popmode("quick"))) == NULL)
		return (FALSE);

	addline(bp, "FILE             BUFFER          WINDOW           MARK/KILL       MISC");
	addline(bp, "C-x C-c exit     C-x b   switch  C-x 2/3 split    C-SPC mark      C-_ undo");
	addline(bp, "C-x C-f find     C-x k   close   C-x 0/1 del/only C-w   kill-reg. C-s search");
	addline(bp, "C-x C-s save     C-x C-b list    C-x +   balance  C-k   kill-line C-r r-search");
	if (utf8_mode) {
		addline(bp, "C-x s   save-all C-x h   mark    M-\xe2\x86\x94/\xe2\x86\x95   move     C-y     yank    M-% replace");
		addline(bp, "C-x i   insert   C-x g   goto-ln M-S-\xe2\x86\x94/\xe2\x86\x95 resize   C-x C-x swap    M-q reformat");
	} else {
		addline(bp, "C-x s   save-all C-x h   mark    M-arrows move    C-y     yank    M-% replace");
		addline(bp, "C-x i   insert   C-x g   goto-ln M-S-arrows size  C-x C-x swap    M-q reformat");
	}

	if (popshow(bp) == NULL)
		return (FALSE);
	quickresize();
	return (TRUE);
}

/*
 * Shrink the quick help pop-up back to the height of its text, by
 * moving its top seam, or at the top of the screen its bottom one.
 * Also called after balance-windows, which would otherwise resize
 * it like any other window.
 */
void
quickresize(void)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	int	 delta;

	if ((bp = bfind("*quick*", FALSE)) == NULL)
		return;
	if ((wp = bufwind(bp)) == NULL || (delta = wp->w_ntrows - 6) <= 0)
		return;
	if (!moveseam(wp->w_toprow - 1, delta, FALSE))
		(void)moveseam(wp->w_toprow + wp->w_ntrows, -delta, FALSE);
	wp->w_linep = lforw(bp->b_headp);
}

/*
 * This function tries to locate the 'tutorial' file, opens a buffer
 * and displays it in a pop-up buffer.
 */
int
tutorial(int f, int n)
{
	static char *variants[] = {
		DATADIR "/tutorial.md",
		DATADIR "/tutorial.md.gz",
		DATADIR "/tutorial.gz",
		DOCDIR "/tutorial.md",
		DOCDIR "/tutorial.md.gz",
		DOCDIR "/tutorial.gz",
	};
	struct buffer	*bp, *oldbp = curbp;
	char		*fn = NULL;
	size_t		 i;

	bp = bfind("*tutorial*", TRUE);
	if (bclear(bp) != TRUE) {
		return (FALSE);
	}

	curbp = bp;
	if (showbuffer(bp, curwp, WFFULL) != TRUE) {
	fail:
		killbuffer(bp);
		curbp = oldbp;
		return (FALSE);
	}

	for (i = 0; i < NELEMS(variants); i++) {
		fn = variants[i];
		if (access(fn, R_OK))
			fn = NULL;
		else
			break;
	}

	if (!fn) {
		ewprintf("Sorry, cannot find the tutorial on this system.");
		goto fail;
	}

	if (readin(fn) != TRUE)
		goto fail;

	/*
	 * The tutorial is markdown, show it with the colors on.  It
	 * is the reader's own copy: editable, for the exercises, and
	 * detached from the installed file so it cannot be saved
	 * back over it.
	 */
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_nmodes = 0;
	if ((bp->b_modes[1] = name_mode("markdown")) != NULL)
		bp->b_nmodes = 1;
	bp->b_flag &= ~BFREADONLY;
	bp->b_fname[0] = '\0';
	curwp->w_rflag |= WFFULL;

	return (TRUE);
}
