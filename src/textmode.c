/* This file is in the public domain. */

/*
 * Text mode.  For prose rather than code: turns on fill mode, so
 * that typing past the fill column wraps the line, the way text-mode
 * with turn-on-auto-fill does in GNU Emacs.  Plain text has no
 * syntax rules, so nothing is colored.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) textmodemap = {
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
textmode(int f, int n)
{
	/*
	 * Turn fill on before the text map is added, so a
	 * local-set-key in a text buffer lands in that map and not
	 * in the fill map every filling buffer shares.
	 */
	if (!buf_hasmode(curbp, "text") && ((f & FFARG) == 0 || n > 0))
		(void)fillmode(FFARG, 1);
	return (changemode(f, n, "text"));
}

void
textmode_init(void)
{
	funmap_add(textmode, "text-mode", 0);
	maps_add((KEYMAP *)&textmodemap, "text");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.txt", "text-mode");
	(void)add_autoexec("*.text", "text-mode");
	/* the files a source tree keeps without a suffix */
	(void)add_autoexec("AUTHORS", "text-mode");
	(void)add_autoexec("COPYING", "text-mode");
	(void)add_autoexec("INSTALL", "text-mode");
	(void)add_autoexec("LICENSE", "text-mode");
	(void)add_autoexec("NEWS", "text-mode");
	(void)add_autoexec("README", "text-mode");
	(void)add_autoexec("THANKS", "text-mode");
	(void)add_autoexec("TODO", "text-mode");
	/* what mutt calls the file it hands its editor */
	(void)add_autoexec("mutt-*", "text-mode");
#endif
}
