/* This file is in the public domain. */

/*
 * Version control on C-x v, as in GNU Emacs, for git only: the diff,
 * log and annotation of the file in the buffer, or of the whole
 * repository.  git runs without a shell, in the directory of the file.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/*
 * The directory and name of the file in the buffer, for the commands
 * on one file, or with root only the directory of the buffer, for the
 * commands on the repository.
 */
static int
vcfile(char *dir, char *name, int root)
{
	char	*p;

	if (root && curbp->b_fname[0] == '\0') {
		if (getbufcwd(dir, NFILEN) != TRUE)
			return (dobeep_msg("No directory for this buffer"));
		return (TRUE);
	}
	if (curbp->b_fname[0] == '\0' ||
	    (p = strrchr(curbp->b_fname, '/')) == NULL)
		return (dobeep_msg("Buffer is not visiting a file"));
	(void)strlcpy(name, p + 1, NFILEN);
	(void)strlcpy(dir, curbp->b_fname, NFILEN);
	dir[p + 1 - curbp->b_fname] = '\0';
	return (TRUE);
}

/*
 * Whether dir is in a git work tree.  The commands cannot leave that
 * to git: outside one, git diff compares plain files instead.
 */
static int
vcgit(const char *dir)
{
	char	*argv[] = { "git", "rev-parse", "--is-inside-work-tree", NULL };

	return (runbuf(dir, argv, NULL) == 0);
}

/*
 * Drop bp, which git with exit status s filled, and say why: the first
 * line it printed when it failed, else empty.
 */
static int
vcfail(struct buffer *bp, int s, const char *empty)
{
	char	*msg = NULL;

	if (s != 0 && bfirstlp(bp) != bp->b_headp)
		msg = linetostr(bfirstlp(bp));
	(void)killbuffer(bp);
	if (s != 0)
		dobeep();
	ewprintf("%s", msg != NULL ? msg : s == 0 ? empty :
	    s == -1 ? "Cannot run git" : "git failed");
	free(msg);
	return (FALSE);
}

/*
 * Run git in dir and show what it prints in the pop-up buffer name,
 * in diff-mode when diff is TRUE.  q closes it.  empty is the message
 * when git prints nothing.
 */
static int
vcshow(const char *dir, char *argv[], const char *name, int diff,
    const char *empty)
{
	struct buffer	*bp;
	int	 s;

	if (secure)
		return (secure_denied());
	if (!vcgit(dir))
		return (dobeep_msg("Not in a git repository"));
	if ((bp = popbuffer(name, popmode("view"))) == NULL)
		return (FALSE);
	if ((s = runbuf(dir, argv, bp)) != 0 || bfirstlp(bp) == bp->b_headp)
		return (vcfail(bp, s, empty));
	if (popselect(bp) != TRUE)
		return (FALSE);
	if (diff) {
		(void)changemode(FFARG, 1, "diff");
		modetabw(8);
	}
	ewprintf("q to close");
	return (TRUE);
}

/* Offer to save the buffer, so that git sees what is on screen. */
static int
vcsave(void)
{
	char	 prompt[NFILEN + 11];
	int	 s;

	if (!(curbp->b_flag & BFCHG))
		return (TRUE);
	(void)snprintf(prompt, sizeof(prompt), "Save file %s", curbp->b_fname);
	if ((s = eyorn(prompt)) != TRUE)
		return (s == ABORT ? ABORT : TRUE);
	return (filesave(FFRAND, 1));
}

/* How the file, or with root the repository, differs from HEAD. */
static int
vcdiffs(int root)
{
	char	 dir[NFILEN], name[NFILEN];
	char	*argv[] = { "git", "diff", "--no-color", "HEAD", "--", name,
		    NULL };
	int	 s;

	if ((s = vcfile(dir, name, root)) != TRUE || (s = vcsave()) != TRUE)
		return (s);
	if (root)
		argv[4] = NULL;
	return (vcshow(dir, argv, "*vc-diff*", TRUE, "No changes"));
}

/* The commits of the file, or with root the repository, newest first. */
static int
vclogs(int root)
{
	char	 dir[NFILEN], name[NFILEN];
	char	*argv[] = { "git", "log", "--no-color", "--max-count=2000",
		    "--", name, NULL };
	int	 s;

	if ((s = vcfile(dir, name, root)) != TRUE)
		return (s);
	if (root)
		argv[4] = NULL;
	return (vcshow(dir, argv, "*vc-change-log*", FALSE, "No commits"));
}

/* C-x v =: how the file differs from its last commit. */
static int
vcdiff(int f, int n)
{
	return (vcdiffs(FALSE));
}

/* C-x v D: how the repository differs from its last commit. */
static int
vcrootdiff(int f, int n)
{
	return (vcdiffs(TRUE));
}

/* C-x v l: the commits that changed the file. */
static int
vclog(int f, int n)
{
	return (vclogs(FALSE));
}

/* C-x v L: the commits of the repository. */
static int
vcrootlog(int f, int n)
{
	return (vclogs(TRUE));
}

/*
 * C-x v g: each line of the file with the commit that last changed
 * it, dot on the line it was on in the file.
 */
static int
vcannotate(int f, int n)
{
	char	 dir[NFILEN], name[NFILEN];
	char	*argv[] = { "git", "blame", "--", name, NULL };
	int	 s, line = curwp->w_dotline;

	if ((s = vcfile(dir, name, FALSE)) != TRUE || (s = vcsave()) != TRUE)
		return (s);
	if ((s = vcshow(dir, argv, "*vc-annotate*", FALSE, "No lines")) != TRUE)
		return (s);
	return (setlineno(line));
}

/*
 * C-x v ~: the file as it was in a revision, in a read-only pop-up
 * named file.~rev~, in the modes of a file of that name.
 */
static int
vcrevision(int f, int n)
{
	struct buffer	*bp;
	char	 dir[NFILEN], name[NFILEN], rev[NFILEN], spec[NFILEN];
	char	 bname[NBUFN];
	char	*argv[] = { "git", "show", spec, NULL };
	int	 s, len;

	if (secure)
		return (secure_denied());
	if ((s = vcfile(dir, name, FALSE)) != TRUE)
		return (s);
	if (!vcgit(dir))
		return (dobeep_msg("Not in a git repository"));
	if (eread("Revision (default HEAD): ", rev, sizeof(rev),
	    EFNUL | EFNEW) == NULL)
		return (ABORT);
	if (rev[0] == '\0')
		(void)strlcpy(rev, "HEAD", sizeof(rev));
	len = snprintf(spec, sizeof(spec), "%s:./%s", rev, name);
	if (len < 0 || len >= (int)sizeof(spec))
		return (dobeep_msg("Revision name too long"));
	len = snprintf(bname, sizeof(bname), "%s.~%s~", name, rev);
	if (len < 0 || len >= (int)sizeof(bname))
		return (dobeep_msg("Revision name too long"));

	if ((bp = popbuffer(bname, popmode("view"))) == NULL)
		return (FALSE);
	if ((s = runbuf(dir, argv, bp)) != 0)
		return (vcfail(bp, s, NULL));
	(void)strlcpy(bp->b_cwd, dir, sizeof(bp->b_cwd));
	if (popselect(bp) != TRUE)
		return (FALSE);
	automode(name);
	return (TRUE);
}

/*
 * The first line of the regular file at path, without its newline.
 * Opening does not wait, so a FIFO planted in its place cannot hang.
 */
static int
vcline(const char *path, char *line, size_t len)
{
	struct stat	 st;
	FILE	*fp;
	char	*p;
	int	 fd;

	if ((fd = open(path, O_RDONLY | O_NONBLOCK)) == -1)
		return (FALSE);
	if (fstat(fd, &st) == -1 || !S_ISREG(st.st_mode) ||
	    (fp = fdopen(fd, "r")) == NULL) {
		close(fd);
		return (FALSE);
	}
	p = fgets(line, len, fp);
	fclose(fp);
	if (p == NULL)
		return (FALSE);
	line[strcspn(line, "\n")] = '\0';
	return (TRUE);
}

/*
 * The branch checked out in the repository the file at path is in, or
 * the start of the commit when none is, read from HEAD in its git
 * directory: .git, or where a .git file points, as in a worktree.
 * Like git, the search stops at a directory owned by someone else.
 */
static int
vchead(const char *path, char *head, size_t len)
{
	struct stat	 st;
	char	 dir[NFILEN], git[NFILEN], line[NFILEN], *p;

	(void)strlcpy(dir, path, sizeof(dir));
	for (;;) {
		if ((p = strrchr(dir, '/')) == NULL)
			return (FALSE);
		*p = '\0';
		if (stat(dir[0] != '\0' ? dir : "/", &st) == -1 ||
		    st.st_uid != getuid())
			return (FALSE);
		if (snprintf(git, sizeof(git), "%s/.git", dir) >= (int)sizeof(git))
			return (FALSE);
		if (stat(git, &st) == 0)
			break;
	}
	if (S_ISREG(st.st_mode)) {
		if (!vcline(git, line, sizeof(line)) ||
		    strncmp(line, "gitdir: ", 8) != 0)
			return (FALSE);
		if (line[8] == '/')
			(void)strlcpy(git, line + 8, sizeof(git));
		else if (snprintf(git, sizeof(git), "%s/%s", dir, line + 8) >=
		    (int)sizeof(git))
			return (FALSE);
	} else if (!S_ISDIR(st.st_mode))
		return (FALSE);
	if (strlcat(git, "/HEAD", sizeof(git)) >= sizeof(git) ||
	    !vcline(git, line, sizeof(line)))
		return (FALSE);
	if (strncmp(line, "ref: refs/heads/", 16) == 0)
		p = line + 16;
	else if (strncmp(line, "ref: ", 5) == 0)
		p = line + 5;
	else {
		p = line;	/* detached: the commit, shortened */
		line[strnlen(line, 7)] = '\0';
	}
	(void)strlcpy(head, p, len);
	return (TRUE);
}

/*
 * Set the version control part of the mode line of bp: "Git-main" for
 * a file as committed on branch main, "Git:main" once it has changes,
 * nothing for a file git does not track.  git only runs for a file in
 * a repository, as GNU Emacs does, and so trusts the repository's own
 * config: fsmonitor is turned off, but a filter driver it names still
 * runs when git checks the file.  --no-optional-locks keeps git from
 * rewriting the index.
 */
void
vcstate(struct buffer *bp)
{
	struct stat	 st;
	FILE	*fp;
	pid_t	 pid;
	char	 dir[NFILEN], head[sizeof(bp->b_vc) - 4], *p;
	char	*argv[] = { "git", "--no-optional-locks", "-c",
		    "core.fsmonitor=false", "status", "--porcelain", "--ignored",
		    "--", NULL, NULL };
	int	 c, state;

	bp->b_vc[0] = '\0';
	/* a new file is not on disk yet */
	if (secure || bp->b_fname[0] == '\0' || stat(bp->b_fname, &st) != 0 ||
	    !S_ISREG(st.st_mode) || !vchead(bp->b_fname, head, sizeof(head)))
		return;
	(void)strlcpy(dir, bp->b_fname, sizeof(dir));
	if ((p = strrchr(dir, '/')) == NULL)
		return;
	*p = '\0';
	argv[8] = bp->b_fname + (p + 1 - dir);
	if ((fp = runopen(dir, argv, &pid, 0)) == NULL)
		return;
	/* nothing for a file as committed, else two status letters */
	if ((c = fgetc(fp)) == EOF)
		state = '-';
	else
		state = c == '?' || c == '!' ? 0 : ':';
	while (c != EOF)
		c = fgetc(fp);
	if (runclose(fp, pid) != 0 || state == 0)
		return;
	(void)snprintf(bp->b_vc, sizeof(bp->b_vc), "Git%c%s", state, head);
}

void
vc_init(void)
{
	funmap_add(vcannotate, "vc-annotate", 0);
	funmap_add(vcdiff, "vc-diff", 0);
	funmap_add(vclog, "vc-print-log", 0);
	funmap_add(vcrootlog, "vc-print-root-log", 0);
	funmap_add(vcrevision, "vc-revision-other-window", 0);
	funmap_add(vcrootdiff, "vc-root-diff", 0);

	dobindkey(fundamental_map, "vc-diff", "^Xv=");
	dobindkey(fundamental_map, "vc-root-diff", "^XvD");
	dobindkey(fundamental_map, "vc-print-root-log", "^XvL");
	dobindkey(fundamental_map, "vc-annotate", "^Xvg");
	dobindkey(fundamental_map, "vc-print-log", "^Xvl");
	dobindkey(fundamental_map, "vc-revision-other-window", "^Xv~");
}
