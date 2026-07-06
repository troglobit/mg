/* This file is in the public domain. */

/*
 * Shell script mode.  Tags the buffer so that syntax highlighting
 * picks the shell rules, and sets the buffer up for shell scripts:
 * tab width eight, hard tabs, and RET keeps the indent.  Enable per
 * file type in ~/.mg with, e.g.:
 *
 *	auto-execute *.sh shell-script-mode
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) shmodemap = {
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
shmode(int f, int n)
{
	struct maps_s	*m;
	int		 i, s;

	if ((s = changemode(f, n, "shell-script")) != TRUE)
		return (s);

	m = name_mode("shell-script");
	for (i = 0; i <= curbp->b_nmodes; i++)
		if (curbp->b_modes[i] == m)
			break;
	if (i > curbp->b_nmodes)
		return (TRUE);	/* mode was toggled off */

	/*
	 * Tab width eight and hard tabs; here documents with <<-
	 * only strip real tabs.  The indent mode makes RET keep the
	 * indentation of continuation lines.
	 */
	curbp->b_tabw = 8;
	(void)notabmode(FFARG, 0);
	return (indentmode(FFARG, 1));
}

void
shmode_init(void)
{
	funmap_add(shmode, "shell-script-mode", 0);
	maps_add((KEYMAP *)&shmodemap, "shell-script");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.sh", "shell-script-mode");
#endif
}
