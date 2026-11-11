/* This file is in the public domain. */

/*
 * Projects, on C-c p.  The project of a buffer is the nearest
 * directory up from its own with a .git or a go.mod.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/*
 * The names argv prints in root, each ended by sep, without a leading
 * "./".
 */
static struct list *
project_run(const char *root, char *const argv[], int sep)
{
	struct list	*lh = NULL;
	FILE	*fp;
	pid_t	 pid;
	char	*line = NULL, *name;
	size_t	 cap = 0;
	ssize_t	 len;

	if ((fp = runopen(root, argv, &pid, 0)) == NULL)
		return (NULL);
	while ((len = getdelim(&line, &cap, sep, fp)) != -1) {
		if (len > 0 && line[len - 1] == sep)
			line[len - 1] = '\0';
		name = line;
		if (strncmp(name, "./", 2) == 0)
			name += 2;
		if (list_push(&lh, name) == FALSE)
			break;
	}
	free(line);
	(void)runclose(fp, pid);
	return (lh);
}

/*
 * The files of the project at root, relative to it: those git tracks
 * or would, or outside git every file below root.
 */
static struct list *
project_list(const char *root)
{
	char	*git[] = { "git", "ls-files", "-z", "--cached", "--others",
		    "--exclude-standard", NULL };
	char	*find[] = { "find", ".", "-type", "f", NULL };
	struct list	*lh;

	if ((lh = project_run(root, git, '\0')) == NULL)
		lh = project_run(root, find, '\n');
	return (lh);
}

/* The files project-find-file offers, see project_files(). */
static struct list	*pfiles;

/* The names in pfiles starting with prefix, see eread_list(). */
static struct list *
project_files(const char *prefix)
{
	struct list	*lp, *lh = NULL;
	size_t	 n = strlen(prefix);

	for (lp = pfiles; lp != NULL; lp = lp->l_next)
		if (strncmp(lp->l_name, prefix, n) == 0 &&
		    list_push(&lh, lp->l_name) == FALSE)
			break;
	return (lh);
}

/* Visit a file of the project, completing its name from the root. */
static int
project_find_file(int f, int n)
{
	char	 root[NFILEN], name[NFILEN], path[NFILEN], *bufp, *adjf;

	/* it lists names, as filename completion does, and runs git */
	if (secure)
		return (secure_denied());
	if (project_root(root, sizeof(root)) != TRUE || !secure_allowed(root))
		return (FALSE);
	if ((pfiles = project_list(root)) == NULL)
		return (dobeep_msg("No files in the project"));
	eread_list(project_files);
	bufp = eread("Find file in %s: ", name, sizeof(name),
	    EFNEW | EFCR | EFLIST, root);
	free_file_list(pfiles);
	pfiles = NULL;
	if (bufp == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		return (FALSE);
	if (snprintf(path, sizeof(path), "%s%s", root, bufp) >=
	    (int)sizeof(path) || (adjf = adjustname(path, TRUE)) == NULL)
		return (FALSE);
	return (visitfile(adjf));
}

void
project_init(void)
{
	funmap_add(project_compile, "project-compile", 0);
	funmap_add(project_find_file, "project-find-file", 0);
	dobindkey(fundamental_map, "project-compile", "^Cpc");
	dobindkey(fundamental_map, "project-find-file", "^Cpf");
}
