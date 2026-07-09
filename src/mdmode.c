/* This file is in the public domain. */

/*
 * Markdown mode.  Tags the buffer so that syntax highlighting picks
 * the markdown rules.  Enabled automatically for *.md files.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) mdmodemap = {
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
mdmode(int f, int n)
{
	return (changemode(f, n, "markdown"));
}

void
mdmode_init(void)
{
	funmap_add(mdmode, "markdown-mode", 0);
	maps_add((KEYMAP *)&mdmodemap, "markdown");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.md", "markdown-mode");
	(void)add_autoexec("*.markdown", "markdown-mode");
#endif
}
