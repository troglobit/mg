/* This file is in the public domain. */

/*
 * Diff mode.  Tags the buffer so that syntax highlighting picks the
 * diff rules: file headers, hunk headers, and the removed and added
 * lines within a hunk.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) diffmodemap = {
	0,
	1,		/* 1 to avoid 0 sized array */
	rescan,
	{
		/* unused dummy entry, see keymap.c */
		{
			(KCHAR)0, (KCHAR)0, NULL, NULL
		}
	}
};

static int
diffmode(int f, int n)
{
	int	 s;

	if ((s = changemode(f, n, "diff")) != TRUE)
		return (s);
	if (!buf_hasmode(curbp, "diff"))
		return (TRUE);	/* mode was toggled off */

	/* a diff quotes its source byte for byte, at the canonical stop */
	curbp->b_tabw = 8;
	return (TRUE);
}

void
diffmode_init(void)
{
	funmap_add(diffmode, "diff-mode", 0);
	maps_add((KEYMAP *)&diffmodemap, "diff");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.diff", "diff-mode");
	(void)add_autoexec("*.patch", "diff-mode");
	(void)add_autoexec("*.rej", "diff-mode");
#endif
}
