/* This file is in the public domain. */

/*
 * Makefile mode.  Tags the buffer so that syntax highlighting picks
 * the makefile rules, and sets the buffer up for make: tab width
 * eight and hard tabs, which recipe lines must start with.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) mkmodemap = {
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
mkmode(int f, int n)
{
	int	 s;

	if ((s = changemode(f, n, "makefile")) != TRUE)
		return (s);
	if (!buf_hasmode(curbp, "makefile"))
		return (TRUE);	/* mode was toggled off */

	curbp->b_tabw = 8;
	return (notabmode(FFARG, 0));
}

void
mkmode_init(void)
{
	funmap_add(mkmode, "makefile-mode", 0);
	maps_add((KEYMAP *)&mkmodemap, "makefile");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*[Mm]akefile", "makefile-mode");
	(void)add_autoexec("Makefile.*", "makefile-mode");
	(void)add_autoexec("*.mk", "makefile-mode");
	(void)add_autoexec("*.make", "makefile-mode");
#endif
}
