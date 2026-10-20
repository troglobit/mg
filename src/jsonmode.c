/* This file is in the public domain. */

/*
 * JSON mode.  Tags the buffer so that syntax highlighting picks the
 * json rules, and sets it up the way the files are written: a two
 * column indent step in spaces, and RET keeps the indent.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) jsonmodemap = {
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
jsonmode(int f, int n)
{
	/*
	 * Turn these on before the json mode is added, so a
	 * local-set-key in a json buffer lands in that mode's map and
	 * not in one they all share.
	 */
	if (!buf_hasmode(curbp, "json") && ((f & FFARG) == 0 || n > 0)) {
		modetabw(2);
		(void)notabmode(FFARG, 1);
		(void)indentmode(FFARG, 1);
	}
	return (changemode(f, n, "json"));
}

void
jsonmode_init(void)
{
	funmap_add(jsonmode, "json-mode", 0);
	maps_add((KEYMAP *)&jsonmodemap, "json");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.json", "json-mode");
	(void)add_autoexec("*.jsonc", "json-mode");
#endif
}
