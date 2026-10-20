/* This file is in the public domain. */

/*
 * Diff mode.  Tags the buffer so that syntax highlighting picks the
 * diff rules: file headers, hunk headers, and the removed and added
 * lines within a hunk.  A cover letter from git format-patch opens
 * with its subject placeholder selected, and C-c C-n selects the
 * next one, so that typing replaces it.
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static int	 diff_nextplaceholder(int, int);

static PF diffmode_cn[] = {
	diff_nextplaceholder	/* ^N */
};

static struct KEYMAPE (1) diffcmap = {
	1,
	1,
	rescan,
	{
		{
			CCHR('N'), CCHR('N'), diffmode_cn, NULL
		}
	}
};

static PF diffmode_c[] = {
	NULL			/* ^C, the prefix itself */
};

static struct KEYMAPE (1) diffmodemap = {
	1,
	1,
	rescan,
	{
		{
			CCHR('C'), CCHR('C'), diffmode_c, (KEYMAP *) &diffcmap
		}
	}
};

/*
 * Select the first placeholder at or after byte o of line lp, the
 * line'th of the buffer: mark at its start, dot at its end, so that
 * typing replaces it.  FALSE, with dot where it was, when there is
 * none before the diff begins.
 */
static int
placeholder(struct line *lp, int o, int line)
{
	static const char *const marks[] = { MAIL_SUBJECT, MAIL_BLURB, NULL };
	const char *const *m;
	const char	*p;
	size_t	 len;

	for (; lp != curbp->b_headp; lp = lforw(lp), line++, o = 0) {
		if (llength(lp) >= 5 && (memcmp(ltext(lp), "diff ", 5) == 0 ||
		    memcmp(ltext(lp), "@@", 2) == 0))
			break;
		for (m = marks; *m != NULL; m++) {
			len = strlen(*m);
			p = memmem(ltext(lp) + o, llength(lp) - o, *m, len);
			if (p == NULL)
				continue;
			curwp->w_dotp = lp;
			curwp->w_dotline = line;
			curwp->w_doto = p - ltext(lp);
			isetmark();
			curwp->w_doto += len;
			curwp->w_markact = TRUE;
			curwp->w_rflag |= WFMOVE | WFFULL;
			return (TRUE);
		}
	}
	return (FALSE);
}

static int
diff_nextplaceholder(int f, int n)
{
	if (placeholder(curwp->w_dotp, curwp->w_doto, curwp->w_dotline) != TRUE)
		return (dobeep_msg("No placeholder left"));
	return (TRUE);
}

static int
diffmode(int f, int n)
{
	int	 s, was = buf_hasmode(curbp, "diff");

	if ((s = changemode(f, n, "diff")) != TRUE)
		return (s);
	if (!buf_hasmode(curbp, "diff"))
		return (TRUE);	/* mode was toggled off */

	/* a diff quotes its source byte for byte, at the canonical stop */
	modetabw(8);

	/* the mode just came on, as when a cover letter opens */
	if (!inrc && !was)
		(void)placeholder(bfirstlp(curbp), 0, 1);
	return (TRUE);
}

void
diffmode_init(void)
{
	funmap_add(diffmode, "diff-mode", 0);
	funmap_add(diff_nextplaceholder, "diff-next-placeholder", 0);
	maps_add((KEYMAP *)&diffmodemap, "diff");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.diff", "diff-mode");
	(void)add_autoexec("*.patch", "diff-mode");
	(void)add_autoexec("*.rej", "diff-mode");
#endif
}
