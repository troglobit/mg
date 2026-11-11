/* This file is in the public domain. */

/*
 * Projects, on C-c p.  The project of a buffer is the nearest
 * directory up from its own with a .git or a go.mod.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"

static const char *const markers[] = { ".git", "go.mod", NULL };

/*
 * Put the root of the current buffer's project in root, with a
 * trailing '/'.
 */
static int
project_root(char *root, size_t len)
{
	char	 dir[NFILEN];

	if (getbufcwd(dir, sizeof(dir)) != TRUE)
		return (FALSE);
	if (findup(dir, markers, root, len) == NULL)
		return (dobeep_msg("Not in a project"));
	return (TRUE);
}

/* Run compile from the root of the project. */
static int
project_compile(int f, int n)
{
	char	 root[NFILEN];

	if (project_root(root, sizeof(root)) != TRUE)
		return (FALSE);
	return (compile_prompt(f, root));
}

void
project_init(void)
{
	funmap_add(project_compile, "project-compile", 0);
	dobindkey(fundamental_map, "project-compile", "^Cpc");
}
