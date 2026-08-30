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
#include <string.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static int	 git_ack(int, int);
static int	 git_cancel(int, int);
static int	 git_finish(int, int);
static int	 git_review(int, int);
static int	 git_signoff(int, int);
static int	 git_test(int, int);

static PF gitmode_cc[] = {
	git_ack,		/* ^A */
	rescan,			/* ^B */
	git_finish,		/* ^C */
	rescan,			/* ^D */
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

	fp = popen("git var GIT_COMMITTER_IDENT 2>/dev/null", "r");
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

static int
gitmode(int f, int n)
{
	int	 s;

	if ((s = changemode(f, n, "git-commit")) != TRUE)
		return (s);
	if (!buf_hasmode(curbp, "git-commit"))
		return (TRUE);	/* mode was toggled off */

	/* the diff git appends quotes its source at the canonical stop */
	curbp->b_tabw = 8;
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
