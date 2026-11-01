/* This file is in the public domain. */

/*
 * Git commit message mode.  Colors the message, the lines git
 * comments out below it, and the diff a verbose commit appends, and
 * binds the C-c keys that Magit's git-commit mode made standard:
 * finish the commit, cancel it, and add a trailer.
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static FILE	*git_popen(const char *);
static int	 git_ack(int, int);
static int	 git_cancel(int, int);
static int	 git_finish(int, int);
static int	 git_review(int, int);
static int	 git_showdiff(int, int);
static int	 git_signoff(int, int);
static int	 git_test(int, int);

static PF gitmode_cc[] = {
	git_ack,		/* ^A */
	rescan,			/* ^B */
	git_finish,		/* ^C */
	git_showdiff,		/* ^D */
	rescan,			/* ^E */
	rescan,			/* ^F */
	rescan,			/* ^G */
	rescan,			/* ^H */
	rescan,			/* ^I */
	rescan,			/* ^J */
	git_cancel,		/* ^K */
	rescan,			/* ^L */
	rescan,			/* ^M */
	rescan,			/* ^N */
	rescan,			/* ^O */
	rescan,			/* ^P */
	rescan,			/* ^Q */
	git_review,		/* ^R */
	git_signoff,		/* ^S */
	git_test		/* ^T */
};

static struct KEYMAPE (1) gitcmap = {
	1,
	1,
	rescan,
	{
		{
			CCHR('A'), CCHR('T'), gitmode_cc, NULL
		}
	}
};

static PF gitmode_c[] = {
	NULL			/* ^C, the prefix itself */
};

static struct KEYMAPE (1) gitmodemap = {
	1,
	1,
	rescan,
	{
		{
			CCHR('C'), CCHR('C'), gitmode_c, (KEYMAP *) &gitcmap
		}
	}
};

/*
 * The committer as git itself would write it, from
 * `git var GIT_COMMITTER_IDENT' with the timestamp cut off.
 */
static int
git_ident(char *buf, size_t len)
{
	FILE	*fp;
	char	*p;

	if (secure)
		return (FALSE);
	fp = git_popen("git var GIT_COMMITTER_IDENT 2>/dev/null");
	if (fp == NULL)
		return (FALSE);
	p = fgets(buf, len, fp);
	if (pclose(fp) != 0 || p == NULL)
		return (FALSE);
	if ((p = strrchr(buf, '>')) == NULL)
		return (FALSE);
	p[1] = '\0';
	return (TRUE);
}

/*
 * True when the line already carries a trailer, in which case a new
 * one joins it instead of opening a paragraph of its own.
 */
static int
git_istrailer(const struct line *lp)
{
	int	 c, i, len;

	len = llength(lp);
	for (i = 0; i < len; i++) {
		c = lgetc(lp, i);
		if (c == ':')
			return (i > 0 && i + 1 < len &&
			    lgetc(lp, i + 1) == ' ');
		if (!isalpha(c) && c != '-')
			break;
	}
	return (FALSE);
}

/*
 * Add a trailer below the message, above the lines git commented
 * out, one blank line under the body it belongs to.
 */
static int
git_trailer(const char *kind)
{
	struct line	*lp, *lastlp = NULL, *odotp;
	char	 buf[NLINE], ident[NLINE];
	int	 added, lastline = 0, lineno = 0, odoto, odotline, want;

	if (curbp->b_flag & BFREADONLY)
		return (dobeep_msg("Buffer is read-only"));
	if (git_ident(ident, sizeof(ident)) != TRUE)
		return (dobeep_msg("Cannot read the committer identity"));
	if (snprintf(buf, sizeof(buf), "%s: %s", kind, ident) >=
	    (int)sizeof(buf))
		return (dobeep_msg("Committer identity too long"));

	/* the message runs until the first line git commented out */
	for (lp = bfirstlp(curbp); lp != curbp->b_headp; lp = lforw(lp)) {
		if (llength(lp) > 0 && lgetc(lp, 0) == '#')
			break;
		lineno++;
		if (llength(lp) > 0) {
			lastline = lineno;
			lastlp = lp;
		}
	}
	if (lastlp == NULL)
		return (dobeep_msg("No message to add a trailer to"));

	/* the trailer goes below the message, dot stays where it was */
	odotp = curwp->w_dotp;
	odoto = curwp->w_doto;
	odotline = curwp->w_dotline;
	curwp->w_dotp = lastlp;
	curwp->w_doto = llength(lastlp);
	curwp->w_dotline = lastline;

	/* a trailer joins the block above it, prose does not */
	want = git_istrailer(lastlp) ? 1 : 2;
	undo_boundary_enable(FFRAND, 0);
	for (added = 0; added < want; added++)
		if (lnewline() != TRUE)
			break;
	if (added == want)
		region_put_data(buf, strlen(buf));
	undo_boundary_enable(FFRAND, 1);

	curwp->w_dotp = odotp;
	curwp->w_doto = odoto;
	/* only the lines below the trailer moved down */
	curwp->w_dotline = odotline > lastline ? odotline + added : odotline;
	curwp->w_rflag |= WFMOVE;

	return (added == want ? TRUE : FALSE);
}

static int
git_ack(int f, int n)
{
	return (git_trailer("Acked-by"));
}

static int
git_review(int f, int n)
{
	return (git_trailer("Reviewed-by"));
}

static int
git_signoff(int f, int n)
{
	return (git_trailer("Signed-off-by"));
}

static int
git_test(int f, int n)
{
	return (git_trailer("Tested-by"));
}

/*
 * Hand the message back to git: save and leave.
 */
static int
git_finish(int f, int n)
{
	if (filesave(FFRAND, 1) != TRUE)
		return (FALSE);
	return (quit(FFRAND, 1));
}

/*
 * Give git an empty message, which is how it is told to give up,
 * and leave.  A commit, a tag, and a rebase todo list all abort on
 * one.
 */
static int
git_cancel(int f, int n)
{
	struct line	*lp;
	int		 s;

	if ((s = eyesno("Abort")) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY)
		return (dobeep_msg("Buffer is read-only, cannot abort"));

	lp = bfirstlp(curbp);
	if (lforw(lp) != curbp->b_headp || llength(lp) > 0) {
		(void)markbuffer(FFRAND, 1);
		if (killregion(FFRAND, 1) != TRUE)
			return (FALSE);
	}
	if (filesave(FFRAND, 1) != TRUE)
		return (FALSE);
	return (quit(FFRAND, 1));
}

/*
 * Whether the buffer is the message of a commit or a merge, and holds
 * no diff of its own, as a verbose commit appends one.
 */
static int
git_wantsdiff(void)
{
	struct line	*lp;
	const char	*base;

	base = strrchr(curbp->b_fname, '/');
	base = base != NULL ? base + 1 : curbp->b_fname;
	if (strcmp(base, "COMMIT_EDITMSG") != 0 && strcmp(base, "MERGE_MSG") != 0)
		return (FALSE);
	for (lp = bfirstlp(curbp); lp != curbp->b_headp; lp = lforw(lp))
		if (llength(lp) >= 11 && memcmp(ltext(lp), "diff --git ", 11) == 0)
			return (FALSE);
	return (TRUE);
}

/*
 * Run a git command in the repository the message belongs to, its
 * directory being the git dir, so that a message opened from
 * elsewhere still reads its own.
 */
static FILE *
git_popen(const char *cmd)
{
	FILE	*fp;
	char	*old = NULL, dir[NFILEN], *p;

	(void)strlcpy(dir, curbp->b_fname, sizeof(dir));
	if ((p = strrchr(dir, '/')) == NULL)
		return (popen(cmd, "r"));
	*p = '\0';
	if ((old = getenv("GIT_DIR")) != NULL)
		old = strdup(old);
	(void)setenv("GIT_DIR", dir, 1);
	fp = popen(cmd, "r");
	if (old != NULL) {
		(void)setenv("GIT_DIR", old, 1);
		free(old);
	} else
		(void)unsetenv("GIT_DIR");
	return (fp);
}

/*
 * Whether the message is the one of HEAD, which is what git commit
 * --amend opens: the lines above the comments git added, line for
 * line what git log -1 prints, its terminating blank line included.
 */
static int
git_isamend(void)
{
	struct line	*lp;
	FILE	*fp;
	char	*line = NULL;
	size_t	 cap = 0;
	ssize_t	 len;
	int	 same = TRUE;

	/* a new message opens on a blank line or on the comments */
	lp = bfirstlp(curbp);
	if (llength(lp) == 0 || lgetc(lp, 0) == '#')
		return (FALSE);
	if ((fp = git_popen("git log -1 --format=%B 2>/dev/null")) == NULL)
		return (FALSE);
	while (same && (len = getline(&line, &cap, fp)) > 0) {
		if (line[len - 1] == '\n')
			line[--len] = '\0';
		same = lp != curbp->b_headp && llength(lp) == len &&
		    memcmp(ltext(lp), line, len) == 0;
		lp = lforw(lp);
	}
	free(line);
	(void)pclose(fp);
	for (; same && lp != curbp->b_headp && llength(lp) == 0; lp = lforw(lp))
		;
	return (same && (lp == curbp->b_headp || lgetc(lp, 0) == '#'));
}

/* told when the mode came on, before the message was edited */
static int	 git_amend;

/*
 * Show the staged changes, the diff the commit will make, in a window
 * of their own next to the message, split off like a pop-up: to the
 * right when there is room for both at 80 columns, else above or
 * below.  An amend shows the whole of the amended commit, against the
 * parent, or the empty tree under a root commit.  Run again after
 * staging more.
 */
static int
git_showdiff(int f, int n)
{
	struct buffer	*bp, *obp;
	struct mgwin	*wp;
	FILE	*fp;
	char	*line = NULL;
	size_t	 cap = 0;
	ssize_t	 len;

	if (secure)
		return (secure_denied());
	if (!(f & FFRAND) && !(curbp->b_flag & BFCHG))
		git_amend = git_isamend();
	/* the parent, or the empty tree under a root commit */
	fp = git_popen(git_amend ?
	    "git diff --cached --no-color $(git rev-parse -q --verify HEAD^ ||"
	    " git hash-object -t tree /dev/null) 2>/dev/null" :
	    "git diff --cached --no-color 2>/dev/null");
	if (fp == NULL)
		return (FALSE);
	if ((len = getline(&line, &cap, fp)) <= 0) {
		(void)pclose(fp);
		free(line);
		if (f & FFRAND)
			return (FALSE);
		return (dobeep_msg("Nothing staged"));
	}
	bp = bfind("*diff*", TRUE);
	bp->b_flag |= BFREADONLY;
	if (bclear(bp) != TRUE)
		return (FALSE);
	do {
		if (line[len - 1] == '\n')
			line[len - 1] = '\0';
		addline(bp, line);
	} while ((len = getline(&line, &cap, fp)) > 0);
	free(line);
	(void)pclose(fp);
	bp->b_dotp = bfirstlp(bp);

	obp = curbp;
	curbp = bp;
	(void)changemode(FFARG, 1, "diff");
	modetabw(8);
	curbp = obp;

	if (bp->b_nwnd == 0) {
		if ((wp = splitpop(WNONE)) == NULL)
			return (FALSE);
		(void)showbuffer(bp, wp, WFFULL);
	} else
		/* the windows on it still point at the lines bclear() freed */
		for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
			if (wp->w_bufp == bp) {
				wp->w_dotp = wp->w_linep = bfirstlp(bp);
				wp->w_doto = 0;
				wp->w_dotline = 1;
				wp->w_rflag |= WFFULL;
			}
	return (TRUE);
}

static int
gitmode(int f, int n)
{
	int	 s, was = buf_hasmode(curbp, "git-commit");

	if ((s = changemode(f, n, "git-commit")) != TRUE)
		return (s);
	if (!buf_hasmode(curbp, "git-commit"))
		return (TRUE);	/* mode was toggled off */

	/* the diff git appends quotes its source at the canonical stop */
	modetabw(8);
	/* git log indents the message by four, so 72 fits an 80 column terminal */
	modefillcol(72);
	/* the mode just came on, as when git opens the message */
	if (!inrc && !was) {
		if (!secure && git_wantsdiff()) {
			git_amend = git_isamend();
			(void)git_showdiff(FFRAND, 1);
		}
		ewprintf("C-c C-c to commit, C-c C-s to sign off, "
		    "C-c C-k to abort");
	}
	return (TRUE);
}

void
gitmode_init(void)
{
	funmap_add(gitmode, "git-commit-mode", 0);
	funmap_add(git_ack, "git-commit-ack", 0);
	funmap_add(git_cancel, "git-commit-cancel", 0);
	funmap_add(git_finish, "git-commit-finish", 0);
	funmap_add(git_review, "git-commit-review", 0);
	funmap_add(git_showdiff, "git-commit-show-diff", 0);
	funmap_add(git_signoff, "git-commit-signoff", 0);
	funmap_add(git_test, "git-commit-test", 0);
	maps_add((KEYMAP *)&gitmodemap, "git-commit");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("COMMIT_EDITMSG", "git-commit-mode");
	(void)add_autoexec("MERGE_MSG", "git-commit-mode");
	(void)add_autoexec("NOTES_EDITMSG", "git-commit-mode");
	(void)add_autoexec("TAG_EDITMSG", "git-commit-mode");
	(void)add_autoexec("git-rebase-todo", "git-commit-mode");
#endif
}
