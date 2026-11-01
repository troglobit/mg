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

/* Whether git may run in dir, saying why not. */
static int
vcready(const char *dir)
{
	if (secure)
		return (secure_denied());
	if (!vcgit(dir))
		return (dobeep_msg("Not in a git repository"));
	return (TRUE);
}

/*
 * Drop bp, which git with exit status s filled, and pass on the first
 * line it printed, or empty when there is none.  TRUE when git did not
 * fail.
 */
static int
vcreport(struct buffer *bp, int s, const char *empty)
{
	char	*msg = NULL;

	if (bfirstlp(bp) != bp->b_headp)
		msg = linetostr(bfirstlp(bp));
	(void)killbuffer(bp);
	if (s != 0) {
		dobeep();
		if (msg == NULL)
			empty = s == -1 ? "Cannot run git" : "git failed";
	}
	if (msg != NULL || empty != NULL)
		ewprintf("%s", msg != NULL ? msg : empty);
	free(msg);
	return (s == 0);
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

	if ((s = vcready(dir)) != TRUE)
		return (s);
	if ((bp = popbuffer(name, popmode("view"))) == NULL)
		return (FALSE);
	if ((s = runbuf(dir, argv, bp)) != 0 || bfirstlp(bp) == bp->b_headp)
		return (vcreport(bp, s, empty));
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
	char	*argv[] = { "git", "--literal-pathspecs", "diff", "--no-color",
		    "HEAD", "--", name, NULL };
	int	 s;

	if ((s = vcfile(dir, name, root)) != TRUE || (s = vcsave()) != TRUE)
		return (s);
	if (root)
		argv[5] = NULL;
	return (vcshow(dir, argv, "*vc-diff*", TRUE, "No changes"));
}

/* The commits of the file, or with root the repository, newest first. */
static int
vclogs(int root)
{
	char	 dir[NFILEN], name[NFILEN];
	char	*argv[] = { "git", "--literal-pathspecs", "log", "--no-color",
		    "--max-count=2000", "--", name, NULL };
	int	 s;

	if ((s = vcfile(dir, name, root)) != TRUE)
		return (s);
	if (root)
		argv[5] = NULL;
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
	char	*argv[] = { "git", "--literal-pathspecs", "blame", "--", name,
		    NULL };
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

	if ((s = vcfile(dir, name, FALSE)) != TRUE || (s = vcready(dir)) != TRUE)
		return (s);
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
		return (vcreport(bp, s, NULL));
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
 * What git says of the file name in dir: '-' as committed, ':' with
 * changes, '?' untracked, '!' ignored, or 0 when git fails.  See
 * vcstate() for what running git here trusts.
 */
static int
vcstatus(const char *dir, const char *name)
{
	FILE	*fp;
	pid_t	 pid;
	char	*argv[] = { "git", "--literal-pathspecs", "--no-optional-locks",
		    "-c", "core.fsmonitor=false", "status", "--porcelain",
		    "--ignored", "--", (char *)name, NULL };
	int	 c, state;

	if ((fp = runopen(dir, argv, &pid, 0)) == NULL)
		return (0);
	/* nothing for a file as committed, else two status letters */
	if ((c = fgetc(fp)) == EOF)
		state = '-';
	else
		state = c == '?' || c == '!' ? c : ':';
	while (c != EOF)
		c = fgetc(fp);
	return (runclose(fp, pid) == 0 ? state : 0);
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
	char	 dir[NFILEN], head[sizeof(bp->b_vc) - 4], *p;
	int	 state;

	bp->b_vc[0] = '\0';
	/* a new file is not on disk yet */
	if (secure || bp->b_fname[0] == '\0' || stat(bp->b_fname, &st) != 0 ||
	    !S_ISREG(st.st_mode) || !vchead(bp->b_fname, head, sizeof(head)))
		return;
	(void)strlcpy(dir, bp->b_fname, sizeof(dir));
	if ((p = strrchr(dir, '/')) == NULL)
		return;
	*p = '\0';
	state = vcstatus(dir, bp->b_fname + (p + 1 - dir));
	if (state == '-' || state == ':')
		(void)snprintf(bp->b_vc, sizeof(bp->b_vc), "Git%c%s", state,
		    head);
}

/* Update the mode line of the buffers visiting dir/name. */
static void
vcrefresh(const char *dir, const char *name)
{
	struct buffer	*bp;
	char	 path[NFILEN];

	(void)snprintf(path, sizeof(path), "%s%s", dir, name);
	for (bp = bheadp; bp != NULL; bp = bp->b_bufp)
		if (strcmp(bp->b_fname, path) == 0) {
			vcstate(bp);
			upmodes(bp);
		}
}

/*
 * Run git in dir for a command that changes something, its first line
 * of output the message.  git may write to the terminal itself, as
 * pinentry does to sign a commit, so the screen is redrawn after.
 */
static int
vcdo(const char *dir, char *argv[])
{
	struct buffer	*bp;
	int	 s;

	if ((bp = bfind("*vc*", TRUE)) == NULL || bclear(bp) != TRUE)
		return (FALSE);
	s = runbuf(dir, argv, bp);
	sgarbf = TRUE;
	return (vcreport(bp, s, NULL));
}

/* Start tracking dir/name. */
static int
vcadd(const char *dir, char *name)
{
	char	*argv[] = { "git", "--literal-pathspecs", "add", "--", name,
		    NULL };
	int	 s;

	if ((s = vcdo(dir, argv)) != TRUE)
		return (s);
	vcrefresh(dir, name);
	ewprintf("Registered %s", name);
	return (TRUE);
}

/* C-x v i: start tracking the file, with git add. */
static int
vcregister(int f, int n)
{
	char	 dir[NFILEN], name[NFILEN];
	int	 s;

	if ((s = vcfile(dir, name, FALSE)) != TRUE ||
	    (s = vcready(dir)) != TRUE || (s = vcsave()) != TRUE)
		return (s);
	return (vcadd(dir, name));
}

/*
 * C-x v u: throw away the changes to the file, saved or not, back to
 * its last commit.
 */
static int
vcrevert(int f, int n)
{
	char	 dir[NFILEN], name[NFILEN], prompt[NFILEN + 32];
	char	*argv[] = { "git", "--literal-pathspecs", "checkout", "HEAD",
		    "--", name, NULL };
	int	 s, state;

	if (secure)
		return (secure_denied());
	if ((s = vcfile(dir, name, FALSE)) != TRUE)
		return (s);
	if ((state = vcstatus(dir, name)) == 0)
		return (dobeep_msg("Not in a git repository"));
	if (state != ':' && !(curbp->b_flag & BFCHG))
		return (dobeep_msg("No changes to revert"));
	(void)snprintf(prompt, sizeof(prompt), "Discard changes in %s", name);
	if ((s = eyesno(prompt)) != TRUE)
		return (s);
	if (state == ':' && (s = vcdo(dir, argv)) != TRUE)
		return (s);
	return (revertbp(curbp));
}

/*
 * The *vc-log* buffer: C-c C-c commits the file it was opened for,
 * C-c C-k gives up.
 */
static int	 vccommit(int, int);
static int	 vccancel(int, int);
static int	 vclogdiff(int, int);

static char	 vclogdir[NFILEN], vclogname[NFILEN];

static PF vclog_cc[] = {
	vccommit,		/* ^C */
	vclogdiff		/* ^D */
};

static PF vclog_ck[] = {
	vccancel		/* ^K */
};

static struct KEYMAPE (2) vclogcmap = {
	2,
	2,
	rescan,
	{
		{ CCHR('C'), CCHR('D'), vclog_cc, NULL },
		{ CCHR('K'), CCHR('K'), vclog_ck, NULL }
	}
};

static PF vclog_c[] = {
	NULL			/* ^C, the prefix itself */
};

static struct KEYMAPE (1) vclogmap = {
	1,
	1,
	rescan,
	{
		{ CCHR('C'), CCHR('C'), vclog_c, (KEYMAP *)&vclogcmap }
	}
};

/*
 * Open *vc-log* for the message of a commit of dir/name, in
 * git-commit-mode when there is one.  A message started for the same
 * file is kept; one for another file is only dropped when confirmed.
 */
static int
vclogedit(const char *dir, const char *name)
{
	struct buffer	*bp;
	PF	 gitmode;
	char	 prompt[NFILEN + 32];
	int	 s, same;

	same = strcmp(vclogdir, dir) == 0 && strcmp(vclogname, name) == 0;
	if ((bp = bfind("*vc-log*", FALSE)) != NULL && !same &&
	    bfirstlp(bp) != bp->b_headp) {
		(void)snprintf(prompt, sizeof(prompt),
		    "Drop the message for %s", vclogname);
		if ((s = eyesno(prompt)) != TRUE)
			return (s);
	}
	(void)strlcpy(vclogdir, dir, sizeof(vclogdir));
	(void)strlcpy(vclogname, name, sizeof(vclogname));
	if ((bp = bfind("*vc-log*", TRUE)) == NULL ||
	    (!same && bclear(bp) != TRUE) || popselect(bp) != TRUE)
		return (FALSE);
	if ((gitmode = name_function("git-commit-mode")) != NULL)
		(void)(*gitmode)(FFARG, 1);
	else
		ewprintf("C-c C-c to commit, C-c C-k to cancel");
	return (changemode(FFARG, 1, "vc-log"));
}

/* Whether the current buffer is *vc-log*, saying so when not. */
static int
vcinlog(void)
{
	if (strcmp(curbp->b_bname, "*vc-log*") != 0)
		return (dobeep_msg("Not in *vc-log*"));
	return (TRUE);
}

/*
 * C-c C-c in *vc-log*: commit the file with the message written.  Its
 * comment lines are left out, as git-commit-mode shows them.
 */
static int
vccommit(int f, int n)
{
	struct line	*lp;
	char	*msg, *p;
	char	*argv[] = { "git", "--literal-pathspecs", "commit",
		    "--cleanup=strip", "-m", NULL, "--", vclogname, NULL };
	size_t	 len = 1;
	int	 s;

	if ((s = vcinlog()) != TRUE)
		return (s);
	for (lp = bfirstlp(curbp); lp != curbp->b_headp; lp = lforw(lp))
		len += llength(lp) + 1;
	if ((msg = p = malloc(len)) == NULL)
		return (dobeep_msg("Out of memory"));
	for (lp = bfirstlp(curbp); lp != curbp->b_headp; lp = lforw(lp)) {
		memcpy(p, ltext(lp), llength(lp));
		p += llength(lp);
		*p++ = '\n';
	}
	*p = '\0';
	if (msg[strspn(msg, " \t\n")] == '\0') {
		free(msg);
		return (dobeep_msg("Empty message, C-c C-k to cancel"));
	}
	argv[5] = msg;
	s = vcdo(vclogdir, argv);
	free(msg);
	if (s != TRUE)
		return (s);
	(void)popdone(curbp);
	vcrefresh(vclogdir, vclogname);
	return (TRUE);
}

/* C-c C-k in *vc-log*: close it without committing. */
static int
vccancel(int f, int n)
{
	int	 s;

	if ((s = vcinlog()) != TRUE)
		return (s);
	(void)popdone(curbp);
	ewprintf("Commit canceled");
	return (TRUE);
}

/* C-c C-d in *vc-log*: what the commit will hold, the file against HEAD. */
static int
vclogdiff(int f, int n)
{
	char	*argv[] = { "git", "--literal-pathspecs", "diff", "--no-color",
		    "HEAD", "--", vclogname, NULL };
	int	 s;

	if ((s = vcinlog()) != TRUE)
		return (s);
	return (vcshow(vclogdir, argv, "*vc-diff*", TRUE, "No changes"));
}

/*
 * C-x v v: the next step for the file.  A file git does not track is
 * added, a changed one is committed, after saving it.
 */
static int
vcnext(int f, int n)
{
	char	 dir[NFILEN], name[NFILEN];
	int	 s;

	if (secure)
		return (secure_denied());
	if ((s = vcfile(dir, name, FALSE)) != TRUE || (s = vcsave()) != TRUE)
		return (s);
	switch (vcstatus(dir, name)) {
	case '?':
		return (vcadd(dir, name));
	case '!':
		return (dobeep_msg("The file is ignored by git"));
	case '-':
		ewprintf("No changes to commit");
		return (TRUE);
	case ':':
		return (vclogedit(dir, name));
	default:
		return (dobeep_msg("Not in a git repository"));
	}
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
	funmap_add(vcnext, "vc-next-action", 0);
	funmap_add(vcregister, "vc-register", 0);
	funmap_add(vcrevert, "vc-revert", 0);
	funmap_add(vccommit, "vc-log-commit", 0);
	funmap_add(vccancel, "vc-log-cancel", 0);
	funmap_add(vclogdiff, "vc-log-diff", 0);
	(void)minor_add((KEYMAP *)&vclogmap, "vc-log");

	dobindkey(fundamental_map, "vc-diff", "^Xv=");
	dobindkey(fundamental_map, "vc-root-diff", "^XvD");
	dobindkey(fundamental_map, "vc-print-root-log", "^XvL");
	dobindkey(fundamental_map, "vc-annotate", "^Xvg");
	dobindkey(fundamental_map, "vc-print-log", "^Xvl");
	dobindkey(fundamental_map, "vc-revision-other-window", "^Xv~");
	dobindkey(fundamental_map, "vc-register", "^Xvi");
	dobindkey(fundamental_map, "vc-revert", "^Xvu");
	dobindkey(fundamental_map, "vc-next-action", "^Xvv");
}
