/*	$OpenBSD: grep.c,v 1.49 2021/03/01 10:51:14 lum Exp $	*/

/* This file is in the public domain */

#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
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

static char compile_last_command[NFILEN] = "make ";

/*
 * The list next-error steps through: the last grep or compile buffer,
 * or *xref*, whichever was shown last.  See compile_buf() before
 * using it.
 */
static struct buffer	*compile_buffer;

/* The location next-error found last in compile_buffer. */
static struct line	*compile_last;

/*
 * The commands running for compile buffers, their output added as it
 * comes, see compile_output().
 */
static struct job {
	struct buffer	*bp;
	FILE		*fp;
	pid_t		 pid;
} jobs[COMPILE_JOBS];

static struct job *compile_job(struct buffer *);

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

	(void)strlcpy(cprompt, "grep -n ", sizeof(cprompt));
	if ((bufp = eread("Run grep: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (strlcat(cprompt, " /dev/null", sizeof(cprompt)) >= sizeof(cprompt))
		return (FALSE);

	return (compile_run("*grep*", cprompt, NULL));
}

int
compile(int f, int n)
{
	return (compile_prompt(f, NULL));
}

/* Prompt for a compile command and run it in dir. */
int
compile_prompt(int f, const char *dir)
{
	char	 cprompt[NFILEN], *bufp;

	(void)strlcpy(cprompt, compile_last_command, sizeof(cprompt));
	if ((bufp = eread("Compile command: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	(void)strlcpy(compile_last_command, bufp, sizeof(compile_last_command));

	return (compile_command(f, bufp, dir));
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
	if (savebuffers(f, 1) == ABORT)
		return (ABORT);
	return (compile_run("*compile*", command, dir));
}

/*
 * Run command in dir into the buffer name, see compile_mode(), and pop
 * it up selected, its window following the output from the end.
 */
int
compile_run(const char *name, const char *command, const char *dir)
{
	struct buffer	*bp;

	if ((bp = compile_mode(name, command, dir)) == NULL)
		return (FALSE);
	if (popselect(bp) != TRUE)
		return (FALSE);
	wineob(curwp);
	return (TRUE);
}

/* id-utils foo. */
static int
gid(int f, int n)
{
	char	 command[NFILEN];
	char	 cprompt[NFILEN], *bufp;
	int	c;
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

	return (compile_run("*gid*", command, NULL));
}

/*
 * Run command in directory dir, the current buffer's when NULL, in the
 * background, its output going to the buffer name, in compile mode, as
 * it comes, see compile_output().
 */
static struct buffer *
compile_mode(const char *name, const char *command, const char *dir)
{
	struct buffer	*bp;
	struct job	*jp;
	char	*argv[] = { "/bin/sh", "-c", NULL, NULL };
	int	 fd;

	if (secure) {
		secure_denied();
		return (NULL);
	}

	bp = bfind(name, TRUE);
	if (compile_job(bp) != NULL) {
		if (eyorn("A command is running, kill it") != TRUE)
			return (NULL);
		compile_kill(bp);
	}
	if ((jp = compile_job(NULL)) == NULL) {
		(void)dobeep_msg("Too many commands running");
		return (NULL);
	}
	if (bclear(bp) != TRUE)
		return (NULL);

	if (dir != NULL)
		(void)strlcpy(bp->b_cwd, dir, sizeof(bp->b_cwd));
	else if (getbufcwd(bp->b_cwd, sizeof(bp->b_cwd)) != TRUE)
		return (NULL);
	addlinef(bp, "cd %s", bp->b_cwd);
	addline(bp, command);
	addline(bp, "");
	/* the output goes on from this line */
	addline(bp, "");

	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("compile");
	bp->b_nmodes = 1;
	compile_buffer = bp;
	compile_last = NULL;

	argv[2] = (char *)command;
	if ((jp->fp = runopen(bp->b_cwd, argv, &jp->pid,
	    RUN_STDERR | RUN_GROUP)) == NULL) {
		(void)dobeep_msg("Can't run the command");
		return (NULL);
	}
	fd = fileno(jp->fp);
	(void)fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	jp->bp = bp;
	return (bp);
}

/* The command running for bp, or with bp NULL a free slot for one. */
static struct job *
compile_job(struct buffer *bp)
{
	struct job	*jp;

	for (jp = jobs; jp < &jobs[COMPILE_JOBS]; jp++)
		if (jp->bp == bp)
			return (jp);
	return (NULL);
}

/*
 * Reap the command of jp, returning its wait status, which runclose()
 * would not give, for the signal that killed it.
 */
static int
compile_reap(struct job *jp)
{
	int	 status = 0;

	(void)fclose(jp->fp);
	while (waitpid(jp->pid, &status, 0) == -1 && errno == EINTR)
		;
	jp->bp = NULL;
	return (status);
}

/*
 * Stop the command running for bp, if any, as when bp is killed.
 */
void
compile_kill(struct buffer *bp)
{
	struct job	*jp;
	int	 i;

	if (bp == NULL || (jp = compile_job(bp)) == NULL)
		return;
	(void)kill(-jp->pid, SIGTERM);
	/* a moment to clean up, then for sure */
	for (i = 0; i < 10 && waitpid(jp->pid, NULL, WNOHANG) == 0; i++)
		(void)poll(NULL, 0, 50);
	if (i == 10)
		(void)kill(-jp->pid, SIGKILL);
	(void)compile_reap(jp);
}

/* Stop the commands still running, as mg exits. */
void
compile_killall(void)
{
	struct job	*jp;

	for (jp = jobs; jp < &jobs[COMPILE_JOBS]; jp++)
		compile_kill(jp->bp);
}

/*
 * The descriptors of the running commands, up to COMPILE_JOBS, for
 * idle() to wait on.  Returns how many.
 */
int
compile_fds(struct pollfd *pfd)
{
	struct job	*jp;
	int	 i = 0;

	for (jp = jobs; jp < &jobs[COMPILE_JOBS]; jp++) {
		if (jp->bp == NULL)
			continue;
		pfd[i].fd = fileno(jp->fp);
		pfd[i].events = POLLIN;
		pfd[i++].revents = 0;
	}
	return (i);
}

/*
 * Reap the command of jp, and add how it ended to its buffer, after a
 * blank line.
 */
static void
compile_done(struct job *jp)
{
	struct buffer	*bp = jp->bp;
	char	 how[64], line[NFILEN], timestr[NTIME];
	time_t	 t;
	int	 status, n;

	status = compile_reap(jp);
	if (!WIFEXITED(status))
		(void)snprintf(how, sizeof(how), "Subshell killed by signal %d",
		    WTERMSIG(status));
	else if (WEXITSTATUS(status) != 0)
		(void)snprintf(how, sizeof(how), "Command exited abnormally "
		    "with code %d", WEXITSTATUS(status));
	else
		(void)strlcpy(how, "Command finished", sizeof(how));
	t = time(NULL);
	strftime(timestr, sizeof(timestr), "%a %b %e %T %Y", localtime(&t));
	n = snprintf(line, sizeof(line), "\n%s at %s", how, timestr);
	if (llength(blastlp(bp)) > 0)
		(void)bufappend(bp, "\n", 1);
	(void)bufappend(bp, line, n);
}

/*
 * Add what the running commands printed since to their buffers, see
 * idle().  Returns TRUE if a buffer changed.
 */
int
compile_output(void)
{
	struct job	*jp;
	char	 buf[BUFSIZ];
	ssize_t	 n;
	int	 i, changed = FALSE;

	for (jp = jobs; jp < &jobs[COMPILE_JOBS]; jp++) {
		if (jp->bp == NULL)
			continue;
		/* a few reads at a time, for keys to get through a flood */
		for (i = 0; i < 16 &&
		    (n = read(fileno(jp->fp), buf, sizeof(buf))) > 0; i++) {
			(void)bufappend(jp->bp, buf, n);
			changed = TRUE;
		}
		if (n == 0 || (n == -1 && errno != EAGAIN && errno != EINTR)) {
			compile_done(jp);
			changed = TRUE;
		}
	}
	return (changed);
}

/*
 * Whether lp, a line of a compile buffer, is a location: "file:line:"
 * with the file relative to the buffer's directory, indented or not,
 * as go test indents its lines.  The file is from *fs to *fe, the
 * line number, with a ":column" after it, up to *ne.
 */
int
compile_span(const struct line *lp, int *fs, int *fe, int *ne)
{
	int	 i, len = llength(lp);

	for (i = 0; i < len && isblank(lgetc(lp, i)); i++)
		;
	for (*fs = i; i < len && lgetc(lp, i) != ':'; i++)
		;
	if (i == *fs || i + 1 >= len || !isdigit(lgetc(lp, i + 1)))
		return (FALSE);
	*fe = i;
	for (i++; i < len && isdigit(lgetc(lp, i)); i++)
		;
	if (i < len && lgetc(lp, i) != ':')
		return (FALSE);
	if (i + 1 < len && isdigit(lgetc(lp, i + 1)))
		for (i++; i < len && isdigit(lgetc(lp, i)); i++)
			;
	*ne = i;
	return (TRUE);
}

/*
 * The file and line number of the location on lp, see compile_span(),
 * the file made a path.
 */
static int
compile_loc(struct line *lp, char *path, size_t len, int *lineno)
{
	char	 fname[NFILEN], *adjf;
	int	 fs, fe, ne, i;

	if (!compile_span(lp, &fs, &fe, &ne) || fe - fs >= (int)sizeof(fname))
		return (FALSE);
	memcpy(fname, ltext(lp) + fs, fe - fs);
	fname[fe - fs] = '\0';
	for (*lineno = 0, i = fe + 1; i < ne && isdigit(lgetc(lp, i)); i++) {
		if (*lineno > INT_MAX / 10 - 1)
			return (FALSE);
		*lineno = *lineno * 10 + lgetc(lp, i) - '0';
	}
	if (fname[0] != '/')
		return (getbufcwd(path, len) == TRUE &&
		    strlcat(path, fname, len) < len);
	return ((adjf = adjustname(fname, TRUE)) != NULL &&
	    strlcpy(path, adjf, len) < len);
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
	compile_last = lp;
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
	struct line	*lp;
	int	 line, dir = n < 0 ? -1 : 1;

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
	/* on from the location found last, if it is still there */
	for (lp = bfirstlp(bp), line = 1;
	    lp != bp->b_headp && lp != compile_last; lp = lforw(lp))
		line++;
	if (lp == bp->b_headp)
		return (compile_find(dir < 0 ? lp : bfirstlp(bp),
		    dir < 0 ? 0 : 1, dir));
	return (compile_find(dir < 0 ? lback(lp) : lforw(lp), line + dir,
	    dir));
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
