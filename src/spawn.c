/*	$OpenBSD: spawn.c,v 1.11 2006/08/01 22:16:03 jason Exp $	*/

/* This file is in the public domain. */

/*
 * Spawn: suspend Mg, or run a program and read its output.
 * Assumes POSIX job control.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "ttydef.h"
#include "def.h"

/*
 * This causes mg to send itself a stop signal.  It assumes the parent
 * shell supports POSIX job control.  If the terminal supports an alternate
 * screen, we will switch to it.
 */
int
spawncli(int f, int n)
{
	sigset_t	oset;

	if (secure)
		return (secure_denied());
	/* Very similar to what vttidy() does. */
	ttcolor(CTEXT);
	ttnowindow();
	ttmove(nrow - 1, 0);
	if (epresf != FALSE) {
		tteeol();
		epresf = FALSE;
	}
	if (ttcooked() == FALSE)
		return (FALSE);

	/* Exit application mode and tidy. */
	tttidy();
	ttflush();
	(void)sigprocmask(SIG_SETMASK, NULL, &oset);
	(void)kill(0, SIGTSTP);
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	ttreinit();

	/* Force repaint. */
	sgarbf = TRUE;
	return (ttraw());
}

/*
 * Run argv in dir, or here when NULL, without a shell, and return its
 * standard output to read.  It gets /dev/null for input and errors,
 * which would otherwise mess up the screen.  Close with runclose().
 */
FILE *
runopen(const char *dir, char *const argv[], pid_t *pid)
{
	FILE *fp;
	int p[2], fd;

	if (pipe(p) == -1)
		return (NULL);
	switch ((*pid = fork())) {
	case -1:
		close(p[0]);
		close(p[1]);
		return (NULL);
	case 0:
		close(p[0]);
		if (dup2(p[1], STDOUT_FILENO) == -1)
			_exit(1);
		close(p[1]);
		if ((fd = open("/dev/null", O_RDWR)) != -1) {
			(void)dup2(fd, STDIN_FILENO);
			(void)dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);
		}
		if (dir != NULL && chdir(dir) == -1)
			_exit(1);
		execvp(argv[0], argv);
		_exit(127);
	}
	close(p[1]);
	if ((fp = fdopen(p[0], "r")) == NULL) {
		close(p[0]);
		(void)waitpid(*pid, NULL, 0);
	}
	return (fp);
}

/*
 * Close a stream from runopen() and reap the program, returning its
 * exit status, or -1 when it did not exit normally.
 */
int
runclose(FILE *fp, pid_t pid)
{
	int status;

	fclose(fp);
	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR)
			return (-1);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}
