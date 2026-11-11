/*	$OpenBSD: grep.c,v 1.49 2021/03/01 10:51:14 lum Exp $	*/

/* This file is in the public domain */

#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

static int	 global_wd_mode = FALSE;
static int	 compile_goto_error(int, int);
int		 next_error(int, int);
static int	 grep(int, int);
static int	 gid(int, int);
static struct buffer	*compile_mode(const char *, const char *,
			     const char *);
void grep_init(void);

/* the lines compile_mode() puts before the output: cd, command, blank */
#define COMPILE_HEAD	3

static char compile_last_command[NFILEN] = "make ";

/*
 * The list next-error steps through: the last grep or compile buffer,
 * or *xref*, whichever was shown last.  See compile_buf() before
 * using it.
 */
static struct buffer	*compile_buffer;

static int	 compile_quit(int, int);

static PF compile_pf[] = {
	compile_goto_error
};

static PF compile_q[] = {
	compile_quit
};

static struct KEYMAPE (2) compilemap = {
	2,
	2,
	rescan,
	{
		{ CCHR('M'), CCHR('M'), compile_pf, NULL },
		{ 'q', 'q', compile_q, NULL }
	}
};

void
grep_init(void)
{
	funmap_add(compile_goto_error, "compile-goto-error", 0);
	funmap_add(next_error, "next-error", 0);
	funmap_add(previous_error, "previous-error", 0);
	funmap_add(grep, "grep", 1);
	funmap_add(compile, "compile", 0);
	funmap_add(gid, "gid", 1);
	maps_add((KEYMAP *)&compilemap, "compile");
}

static int
grep(int f, int n)
{
	char	 cprompt[NFILEN], *bufp;
	struct buffer	*bp;

	(void)strlcpy(cprompt, "grep -n ", sizeof(cprompt));
	if ((bufp = eread("Run grep: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (strlcat(cprompt, " /dev/null", sizeof(cprompt)) >= sizeof(cprompt))
		return (FALSE);

	if ((bp = compile_mode("*grep*", cprompt, NULL)) == NULL)
		return (FALSE);
	return (popselect(bp));
}

int
compile(int f, int n)
{
	char	 cprompt[NFILEN], *bufp;

	(void)strlcpy(cprompt, compile_last_command, sizeof(cprompt));
	if ((bufp = eread("Compile command: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	(void)strlcpy(compile_last_command, bufp, sizeof(compile_last_command));

	return (compile_command(f, bufp, NULL));
}

/*
 * Save the modified buffers, asking unless f says not to, run
 * command in directory dir, the buffer's own when NULL, and show
 * its output in *compile*, where next-error steps through the
 * errors.
 */
int
compile_command(int f, const char *command, const char *dir)
{
	struct buffer	*bp;

	if (savebuffers(f, 1) == ABORT)
		return (ABORT);
	if ((bp = compile_mode("*compile*", command, dir)) == NULL)
		return (FALSE);
	return (popselect(bp));
}

/* id-utils foo. */
static int
gid(int f, int n)
{
	char	 command[NFILEN];
	char	 cprompt[NFILEN], *bufp;
	int	c;
	struct buffer	*bp;
	int	 i, j, len;

	/* catch ([^\s(){}]+)[\s(){}]* */

	i = curwp->w_doto;
	/* Skip backwards over delimiters we are currently on */
	while (i > 0) {
		c = lgetc(curwp->w_dotp, i);
		if (isalnum(c) || c == '_')
			break;

		i--;
	}

	/* Skip the symbol itself */
	for (; i > 0; i--) {
		c = lgetc(curwp->w_dotp, i - 1);
		if (!isalnum(c) && c != '_')
			break;
	}
	/* Fill the symbol in cprompt[] */
	for (j = 0; j < (int)sizeof(cprompt) - 1 && i < llength(curwp->w_dotp);
	    j++, i++) {
		c = lgetc(curwp->w_dotp, i);
		if (!isalnum(c) && c != '_')
			break;
		cprompt[j] = c;
	}
	cprompt[j] = '\0';

	if ((bufp = eread("Run gid (with args): ", cprompt, NFILEN,
	    (j ? EFDEF : 0) | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	len = snprintf(command, sizeof(command), "gid %s", cprompt);
	if (len < 0 || len >= (int)sizeof(command))
		return (FALSE);

	if ((bp = compile_mode("*gid*", command, NULL)) == NULL)
		return (FALSE);
	return (popselect(bp));
}

/*
 * Run command in directory dir, the current buffer's when NULL, and
 * collect its output in the buffer name, in compile mode.
 */
struct buffer *
compile_mode(const char *name, const char *command, const char *dir)
{
	struct buffer	*bp;
	FILE	*fpipe;
	char	*buf = NULL;
	size_t	 len = 0;
	ssize_t	 llen;
	int	 ret, n, status;
	char	 cwd[NFILEN], qcmd[NFILEN];
	char	 timestr[NTIME];
	time_t	 t;

	if (secure) {
		secure_denied();
		return (NULL);
	}

	n = snprintf(qcmd, sizeof(qcmd), "%s 2>&1", command);
	if (n < 0 || n >= (int)sizeof(qcmd))
		return (NULL);

	bp = bfind(name, TRUE);
	if (bclear(bp) != TRUE)
		return (NULL);

	if (dir != NULL)
		(void)strlcpy(bp->b_cwd, dir, sizeof(bp->b_cwd));
	else if (getbufcwd(bp->b_cwd, sizeof(bp->b_cwd)) != TRUE)
		return (NULL);
	addlinef(bp, "cd %s", bp->b_cwd);
	addline(bp, qcmd);
	addline(bp, "");

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		panic("Can't get current directory!");
	if (chdir(bp->b_cwd) == -1) {
		dobeep();
		ewprintf("Can't change dir to %s", bp->b_cwd);
		return (NULL);
	}
	if ((fpipe = popen(qcmd, "r")) == NULL) {
		dobeep();
		ewprintf("Problem opening pipe");
		return (NULL);
	}

	while ((llen = getline(&buf, &len, fpipe)) != -1) {
		if (llen > 0 && buf[llen - 1] == *bp->b_nlchr)
			buf[llen - 1] = '\0';
		addline(bp, buf);
	}
	if (buf)
		free(buf);
	ret = pclose(fpipe);

	t = time(NULL);
	strftime(timestr, sizeof(timestr), "%a %b %e %T %Y", localtime(&t));
	addline(bp, "");

	if (WIFEXITED(ret)) {
		status = WEXITSTATUS(ret);
		if (status == 0)
			addlinef(bp, "Command finished at %s", timestr);
		else
			addlinef(bp, "Command exited abnormally with code %d "
			    "at %s", status, timestr);
	} else
		addlinef(bp, "Subshell killed by signal %d at %s",
		    WTERMSIG(ret), timestr);

	bp->b_dotp = bfirstlp(bp);
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("compile");
	bp->b_nmodes = 1;

	compile_buffer = bp;

	if (chdir(cwd) == -1) {
		dobeep();
		ewprintf("Can't change dir back to %s", cwd);
		return (NULL);
	}
	return (bp);
}

/*
 * Whether lp, a line of a compile buffer, is a location: "file:line:"
 * with the file relative to the buffer's directory, indented or not,
 * as go test indents its lines.
 */
static int
compile_loc(struct line *lp, char *path, size_t len, int *lineno)
{
	char	*line, *p, *fname, *ln, *adjf;
	const char *errstr;
	int	 s = FALSE;

	if ((line = linetostr(lp)) == NULL)
		return (FALSE);
	p = line + strspn(line, " \t");
	if ((fname = strsep(&p, ":")) == NULL || *fname == '\0')
		goto out;
	if ((ln = strsep(&p, ":")) == NULL || *ln == '\0')
		goto out;
	*lineno = (int)strtonum(ln, INT_MIN, INT_MAX, &errstr);
	if (errstr)
		goto out;
	if (fname[0] != '/') {
		if (getbufcwd(path, len) == FALSE ||
		    strlcat(path, fname, len) >= len)
			goto out;
	} else if ((adjf = adjustname(fname, TRUE)) == NULL ||
	    strlcpy(path, adjf, len) >= len)
		goto out;
	s = TRUE;
out:
	free(line);
	return (s);
}

/*
 * Go to the first location from lp, line dotline of the current compile
 * buffer, going in direction dir, and show it in another window.  The
 * lines compile_mode() adds around the output are left out: the
 * command, and the time it finished, can look like locations.
 */
static int
compile_find(struct line *lp, int dotline, int dir)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	char	 path[NFILEN];
	int	 lineno;

	for (; lp != curbp->b_headp && lp != blastlp(curbp);
	    lp = dir < 0 ? lback(lp) : lforw(lp), dotline += dir) {
		if (dotline <= COMPILE_HEAD) {
			if (dir < 0)
				break;
		} else if (compile_loc(lp, path, sizeof(path), &lineno))
			goto found;
	}
	return (dobeep_msg(dir < 0 ? "No earlier hits" : "No more hits"));
found:
	curwp->w_dotp = lp;
	curwp->w_doto = 0;
	curwp->w_dotline = dotline;
	curwp->w_rflag |= WFMOVE;

	if ((bp = findbuffer(path)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] == '\0')
		readin(path);
	return (setlineno(lineno));
}

/* q in a compile buffer: close its window, see pophide(). */
static int
compile_quit(int f, int n)
{
	return (pophide(curbp));
}

/*
 * RET in a compile buffer: go to the location on this line, or the
 * first one after it.
 */
static int
compile_goto_error(int f, int n)
{
	compile_buffer = curbp;
	return (compile_find(curwp->w_dotp, curwp->w_dotline, 1));
}

/*
 * The buffer for next-error, or NULL once it has been killed.  Its
 * memory may since have gone to another buffer, so that has to be in
 * compile or xref mode too.
 */
static struct buffer *
compile_buf(void)
{
	struct buffer	*bp;

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp)
		if (bp == compile_buffer)
			break;
	if (bp == NULL || bp->b_nmodes < 1)
		return (NULL);
	if (bp->b_modes[1] == name_mode("compile"))
		return (bp);
#ifdef ENABLE_CTAGS
	if (bp->b_modes[1] == name_mode("xref"))
		return (bp);
#endif
	return (NULL);
}

/* Make bp, a list like *xref*, the one next-error steps through. */
void
next_error_buf(struct buffer *bp)
{
	compile_buffer = bp;
}

/*
 * Go to the next location in the list shown last, or with a negative
 * argument the one before.
 */
int
next_error(int f, int n)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	int	 dir = n < 0 ? -1 : 1;

	if ((bp = compile_buf()) == NULL)
		return (dobeep_msg("No compilation active"));
#ifdef ENABLE_CTAGS
	if (bp->b_modes[1] != name_mode("compile"))
		return (xrefstep(dir, FALSE));
#endif
	if ((wp = popbuf(bp, WEPHEM)) == NULL)
		return (FALSE);
	curwp = wp;
	curbp = bp;
	return (compile_find(dir < 0 ? lback(curwp->w_dotp) :
	    lforw(curwp->w_dotp), curwp->w_dotline + dir, dir));
}

/* Go to the location before the last one, see next_error(). */
int
previous_error(int f, int n)
{
	return (next_error(f, -n));
}

/*
 * Since we don't have variables (we probably should) these are command
 * processors for changing the values of mode flags.
 */
int
globalwdtoggle(int f, int n)
{
	if (f & FFARG)
		global_wd_mode = n > 0;
	else
		global_wd_mode = !global_wd_mode;

	sgarbf = TRUE;

	return (TRUE);
}

/*
 * Is working directory global?
 */
int
globalwd(void)
{
	return global_wd_mode;
}
