/* This file is in the public domain. */

/*
 * m4 mode, for configure.ac and the other autoconf and m4 sources.
 * Tags the buffer so that syntax highlighting picks the m4 rules.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) m4modemap = {
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
m4mode(int f, int n)
{
	return (changemode(f, n, "m4"));
}

void
m4mode_init(void)
{
	funmap_add(m4mode, "m4-mode", 0);
	maps_add((KEYMAP *)&m4modemap, "m4");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.m4", "m4-mode");
	(void)add_autoexec("configure.ac", "m4-mode");
	(void)add_autoexec("configure.in", "m4-mode");
#endif
}
