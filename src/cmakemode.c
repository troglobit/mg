/* This file is in the public domain. */

/*
 * CMake mode.  Tags the buffer so that syntax highlighting picks the
 * cmake rules.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) cmakemodemap = {
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
cmakemode(int f, int n)
{
	return (changemode(f, n, "cmake"));
}

void
cmakemode_init(void)
{
	funmap_add(cmakemode, "cmake-mode", 0);
	maps_add((KEYMAP *)&cmakemodemap, "cmake");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("CMakeLists.txt", "cmake-mode");
	(void)add_autoexec("*.cmake", "cmake-mode");
#endif
}
