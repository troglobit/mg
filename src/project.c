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

/*
 * s quoted for the shell in buf, between single quotes.  Returns FALSE
 * when it does not fit.
 */
static int
shquote(char *buf, size_t len, const char *s)
{
	size_t	 i = 0;

	buf[i++] = '\'';
	for (; *s != '\0'; s++) {
		/* room for an escaped quote, the closing one and the NUL */
		if (i + 6 > len)
			return (FALSE);
		if (*s == '\'') {
			/* close, an escaped quote, open again */
			memcpy(buf + i, "'\\''", 4);
			i += 4;
		} else
			buf[i++] = *s;
	}
	buf[i++] = '\'';
	buf[i] = '\0';
	return (TRUE);
}

/*
 * Search the files of the project for a regexp, with git grep, or
 * grep outside git, listing the matches in *grep* for next-error.
 */
static int
project_find_regexp(int f, int n)
{
	char	 root[NFILEN], top[NFILEN], re[NFILEN], q[NFILEN * 2];
	char	 cmd[NFILEN * 2 + 32], *bufp;
	int	 usegit;

	if (project_root(root, sizeof(root)) != TRUE)
		return (FALSE);
	if ((bufp = eread("Find regexp: ", re, sizeof(re),
	    EFNEW | EFCR)) == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		return (FALSE);
	if (shquote(q, sizeof(q), bufp) != TRUE)
		return (dobeep_msg("Regexp too long"));
	usegit = findup(root, (const char *const[]){ ".git", NULL }, top,
	    sizeof(top)) != NULL;
	(void)snprintf(cmd, sizeof(cmd), "%s -e %s%s", usegit ?
	    "git grep -n -I --untracked" : "grep -rnI", q, usegit ? "" : " .");
	return (compile_run("*grep*", cmd, root));
}

/*
 * Whether the file, or else the directory, of bp is in the project at
 * root.
 */
static int
project_has(const struct buffer *bp, const char *root)
{
	const char	*p = bp->b_fname[0] != '\0' ? bp->b_fname : bp->b_cwd;

	return (p[0] != '\0' && strncmp(p, root, strlen(root)) == 0);
}

/* The root of the project project_buffers() completes for. */
static char	proot[NFILEN];

/*
 * The other buffers of the project starting with prefix, see
 * eread_list().
 */
static struct list *
project_buffers(const char *prefix)
{
	struct buffer	*bp;
	struct list	*lh = NULL;
	size_t	 n = strlen(prefix);

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp)
		if (bp != curbp && project_has(bp, proot) &&
		    strncmp(bp->b_bname, prefix, n) == 0 &&
		    list_push(&lh, bp->b_bname) == FALSE)
			break;
	return (lh);
}

/* Switch to another buffer of the project. */
static int
project_switch_to_buffer(int f, int n)
{
	char	 name[NBUFN], *bufp;

	if (project_root(proot, sizeof(proot)) != TRUE)
		return (FALSE);
	eread_list(project_buffers);
	if ((bufp = eread("Switch to buffer in %s: ", name, sizeof(name),
	    EFNEW | EFCR | EFLIST, proot)) == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		return (FALSE);
	if (bfind(bufp, FALSE) == NULL)
		return (dobeep_msgs("No buffer", bufp));
	return (usebufname(bufp));
}

/*
 * Kill the buffers of the project, after asking, each modified one
 * asking again.
 */
static int
project_kill_buffers(int f, int n)
{
	struct buffer	*bp, **bps;
	char	 root[NFILEN], msg[NFILEN + 32];
	int	 i, s = TRUE, nbp = 0;

	if (project_root(root, sizeof(root)) != TRUE)
		return (FALSE);
	for (bp = bheadp; bp != NULL; bp = bp->b_bufp)
		if (project_has(bp, root))
			nbp++;
	if (nbp == 0)
		return (dobeep_msg("No buffers in the project"));
	(void)snprintf(msg, sizeof(msg), "Kill %d buffer%s in %s", nbp,
	    nbp == 1 ? "" : "s", root);
	if (eyesno(msg) != TRUE)
		return (FALSE);
	/* a kill can move the buffer shown instead to the front: gather */
	if ((bps = calloc(nbp, sizeof(*bps))) == NULL)
		return (FALSE);
	for (i = 0, bp = bheadp; bp != NULL && i < nbp; bp = bp->b_bufp)
		if (project_has(bp, root))
			bps[i++] = bp;
	for (i = 0; i < nbp && s != ABORT; i++)
		for (bp = bheadp; bp != NULL; bp = bp->b_bufp)
			if (bp == bps[i]) {
				s = killbuffer(bp);
				break;
			}
	free(bps);
	return (s == ABORT ? ABORT : TRUE);
}

void
project_init(void)
{
	funmap_add(project_compile, "project-compile", 0);
	funmap_add(project_find_file, "project-find-file", 0);
	funmap_add(project_find_regexp, "project-find-regexp", 0);
	funmap_add(project_kill_buffers, "project-kill-buffers", 0);
	funmap_add(project_switch_to_buffer, "project-switch-to-buffer", 0);
	dobindkey(fundamental_map, "project-compile", "^Cpc");
	dobindkey(fundamental_map, "project-find-file", "^Cpf");
	dobindkey(fundamental_map, "project-find-regexp", "^Cpg");
	dobindkey(fundamental_map, "project-kill-buffers", "^Cpk");
	dobindkey(fundamental_map, "project-switch-to-buffer", "^Cpb");
}
