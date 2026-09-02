/* This file is in the public domain. */

/*
 * YAML mode.  Tags the buffer so that syntax highlighting picks the
 * yaml rules, and sets it up for a format where whitespace is
 * structure: a two column indent step, spaces only since a tab is a
 * syntax error, and RET keeps the indent.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) yamlmodemap = {
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
yamlmode(int f, int n)
{
	/*
	 * Turn these on before the yaml mode is added, so a
	 * local-set-key in a yaml buffer lands in that mode's map and
	 * not in one they all share.
	 */
	if (!buf_hasmode(curbp, "yaml") && ((f & FFARG) == 0 || n > 0)) {
		modetabw(2);
		(void)notabmode(FFARG, 1);
		(void)indentmode(FFARG, 1);
	}
	return (changemode(f, n, "yaml"));
}

void
yamlmode_init(void)
{
	funmap_add(yamlmode, "yaml-mode", 0);
	maps_add((KEYMAP *)&yamlmodemap, "yaml");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.yml", "yaml-mode");
	(void)add_autoexec("*.yaml", "yaml-mode");
	/* clang and yamllint keep theirs without a suffix */
	(void)add_autoexec(".clang-format", "yaml-mode");
	(void)add_autoexec(".yamllint", "yaml-mode");
#endif
}
