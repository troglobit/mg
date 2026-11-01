/*	$OpenBSD: cscope.c,v 1.21 2022/05/24 16:42:19 op Exp $	*/

/*
 * This file is in the public domain.
 *
 * Author: Sunil Nimmagadda <sunil@openbsd.org>
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"

#define CSSYMBOL      0
#define CSDEFINITION  1
#define CSCALLEDFUNCS 2
#define CSCALLERFUNCS 3
#define CSTEXT        4
#define CSEGREP       6
#define CSFINDFILE    7
#define CSINCLUDES    8

struct cstokens {
	const char *fname;
	const char *function;
	const char *lineno;
	const char *pattern;
};

struct csmatch {
	TAILQ_ENTRY(csmatch) entry;
	int lineno;
};

struct csrecord {
	TAILQ_ENTRY(csrecord) entry;
	char *filename;
	TAILQ_HEAD(matches, csmatch) matches;
};

static TAILQ_HEAD(csrecords, csrecord) csrecords = TAILQ_HEAD_INITIALIZER(csrecords);
static char             csinitdir[NFILEN];	/* where to look, if set */
static char             csdir[NFILEN];		/* of the database searched */
static struct csrecord *addentryr;
static struct csrecord *currecord;
static struct csmatch  *curmatch;
static const char      *addentryfn;
static const char      *csprompt[] = {
	"Find this symbol: ",
	"Find this global definition: ",
	"Find functions called by this function: ",
	"Find functions calling this function: ",
	"Find this text string: ",
	"Change this text string: ",
	"Find this egrep pattern: ",
	"Find this file: ",
	"Find files #including this file: "
};

static int  addentry(struct buffer *, char *);
static int  csclose(FILE *, pid_t);
static int  csargs(const char *, char **, int);
static int  csdbdir(char *, size_t);
static void csflush(void);
static FILE *csopen(const char *, char *const [], pid_t *);
static int  do_cscope(int);
static int  csexists(const char *);
static int  getattr(char *, struct cstokens *);
static int  jumptomatch(void);
static void prettyprint(struct buffer *, struct cstokens *);
static const char *ltrim(const char *);

/*
 * Find this symbol. Bound to C-c s s
 */
int
cssymbol(int f, int n)
{
	return (do_cscope(CSSYMBOL));
}

/*
 * Find this global definition. Bound to C-c s d
 */
int
csdefinition(int f, int n)
{
	return (do_cscope(CSDEFINITION));
}

/*
 * Find functions called by this function. Bound to C-c s l
 */
int
csfuncalled(int f, int n)
{
	return (do_cscope(CSCALLEDFUNCS));
}

/*
 * Find functions calling this function. Bound to C-c s c
 */
int
cscallerfuncs(int f, int n)
{
	return (do_cscope(CSCALLERFUNCS));
}

/*
 * Find this text. Bound to C-c s t
 */
int
csfindtext(int f, int n)
{
	return (do_cscope(CSTEXT));
}

/*
 * Find this egrep pattern. Bound to C-c s e
 */
int
csegrep(int f, int n)
{
	return (do_cscope(CSEGREP));
}

/*
 * Find this file. Bound to C-c s f
 */
int
csfindfile(int f, int n)
{
	return (do_cscope(CSFINDFILE));
}

/*
 * Find files #including this file. Bound to C-c s i
 */
int
csfindinc(int f, int n)
{
	return (do_cscope(CSINCLUDES));
}

/*
 * Where to look for the cscope database, cscope.out, instead of the
 * directory of the buffer and up.  Bound to C-c s a.
 */
int
cssetdir(int f, int n)
{
	char dir[NFILEN], *bufp, *adjf;

	if (getbufcwd(dir, sizeof(dir)) == FALSE)
		dir[0] = '\0';
	bufp = eread("Cscope initial directory (empty for the buffer's): ",
	    dir, sizeof(dir), EFNEW | EFCR | EFDEF | EFNUL);
	if (bufp == NULL)
		return (ABORT);
	if (bufp[0] == '\0') {
		csinitdir[0] = '\0';
		return (TRUE);
	}
	if ((adjf = adjustname(bufp, TRUE)) == NULL)
		return (FALSE);
	if (fisdir(adjf) != TRUE)
		return (dobeep_msgs(adjf, "Not a directory"));
	(void)strlcpy(csinitdir, adjf, sizeof(csinitdir));
	return (TRUE);
}

/*
 * Put the directory of the cscope database in dir: the nearest one with
 * a cscope.out, going up from the initial directory or the buffer's.
 * Like git, it stops at a directory owned by someone else and skips a
 * cscope.out that is not the user's, since cscope would run with it and
 * update it.
 */
static int
csdbdir(char *dir, size_t n)
{
	struct stat sb;
	char path[NFILEN], *p;
	size_t len;

	if (csinitdir[0] != '\0')
		(void)strlcpy(dir, csinitdir, n);
	else if (getbufcwd(dir, n) == FALSE)
		return (FALSE);
	/* the root is the empty string here */
	while ((len = strlen(dir)) > 0 && dir[len - 1] == '/')
		dir[len - 1] = '\0';
	for (;;) {
		if (stat(dir[0] != '\0' ? dir : "/", &sb) == -1 ||
		    sb.st_uid != getuid())
			return (FALSE);
		(void)snprintf(path, sizeof(path), "%s/cscope.out", dir);
		if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode) &&
		    sb.st_uid == getuid())
			break;
		if (dir[0] == '\0')
			return (FALSE);
		if ((p = strrchr(dir, '/')) == NULL)
			return (FALSE);
		*p = '\0';
	}
	if (dir[0] == '\0')
		(void)strlcpy(dir, "/", n);
	return (TRUE);
}

/*
 * Start argv with cscope and the options for the database in dir, the
 * same to build it as to search it, so a search updates the database
 * rather than rebuilds it: -q for the inverted index, and -R unless a
 * cscope.files lists the files.  A search only takes -q when there is
 * an inverted index already.  Returns the number of arguments.
 */
static int
csargs(const char *dir, char **argv, int build)
{
	char path[NFILEN];
	int ac = 0;

	argv[ac++] = "cscope";
	if (build)
		argv[ac++] = "-b";
	(void)snprintf(path, sizeof(path), "%s/cscope.in.out", dir);
	if (build || access(path, F_OK) == 0)
		argv[ac++] = "-q";
	(void)snprintf(path, sizeof(path), "%s/cscope.files", dir);
	if (access(path, F_OK) == -1)
		argv[ac++] = "-R";
	return (ac);
}

/*
 * Build the cscope database of a directory and those below it.  The
 * default is the directory of the database there is, else the top of
 * the git checkout, else that of the buffer.  Bound to C-c s I.
 */
int
csindex(int f, int n)
{
	char dir[NFILEN], found[NFILEN], path[NFILEN], *bufp, *adjf, *p;
	char *argv[6], buf[BUFSIZ];
	FILE *fp;
	pid_t pid;
	int s;

	if (csdbdir(dir, sizeof(dir)) == FALSE) {
		if (getbufcwd(dir, sizeof(dir)) == FALSE)
			dir[0] = '\0';
		(void)strlcpy(found, dir, sizeof(found));
		for (;;) {
			if (snprintf(path, sizeof(path), "%s/.git", found) <
			    (int)sizeof(path) && access(path, F_OK) == 0) {
				(void)strlcpy(dir, found, sizeof(dir));
				break;
			}
			if ((p = strrchr(found, '/')) == NULL || p == found)
				break;
			*p = '\0';
		}
	}
	bufp = eread("Index files in directory: ", dir, sizeof(dir),
	    EFNEW | EFCR | EFDEF);
	if (bufp == NULL)
		return (ABORT);
	if ((adjf = adjustname(bufp, TRUE)) == NULL)
		return (FALSE);
	if (fisdir(adjf) != TRUE)
		return (dobeep_msgs(adjf, "Not a directory"));
	(void)strlcpy(dir, adjf, sizeof(dir));
	if (secure)
		return (secure_denied());
	if (csexists("cscope") == FALSE)
		return(dobeep_msg("no such file or directory, cscope"));

	ewprintf("Indexing %s ...", dir);
	argv[csargs(dir, argv, TRUE)] = NULL;
	if ((fp = csopen(dir, argv, &pid)) == NULL)
		return(dobeep_msg("problem opening pipe"));
	while (fgets(buf, sizeof(buf), fp) != NULL)
		;
	if ((s = csclose(fp, pid)) != 0)
		return (dobeep_msgs("cscope failed in", dir));

	/* search there from here on, if it is not above the buffer */
	if (csdbdir(found, sizeof(found)) == FALSE ||
	    strcmp(found, dir) != 0)
		(void)strlcpy(csinitdir, dir, sizeof(csinitdir));
	ewprintf("Indexed %s", dir);
	return (TRUE);
}

/*
 * Create list of files to index in the given directory
 * using cscope-indexer.
 */
int
cscreatelist(int f, int n)
{
	struct buffer *bp;
	struct stat sb;
	FILE *fpipe;
	char dir[NFILEN], title[BUFSIZ], *line, *bufp;
	char *argv[] = { "cscope-indexer", "-v", dir, NULL };
	size_t sz;
	ssize_t len;
	pid_t pid;
	int clen;

	line = NULL;
	sz = 0;

	if (getbufcwd(dir, sizeof(dir)) == FALSE)
		dir[0] = '\0';

	bufp = eread("Index files in directory: ", dir,
	    sizeof(dir), EFCR | EFDEF | EFNEW | EFNUL);

	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	if (stat(dir, &sb) == -1)
		return(dobeep_msgs("stat:", strerror(errno)));
	else if (S_ISDIR(sb.st_mode) == 0)
		return(dobeep_msgs(dir, "Not a directory"));

	if (secure)
		return (secure_denied());
	if (csexists("cscope-indexer") == FALSE)
		return(dobeep_msg("no such file or directory, cscope-indexer"));

	if ((fpipe = csopen(NULL, argv, &pid)) == NULL)
		return(dobeep_msg("problem opening pipe"));

	bp = bfind("*cscope*", TRUE);
	if (bclear(bp) != TRUE) {
		csclose(fpipe, pid);
		return (FALSE);
	}
	bp->b_flag |= BFREADONLY;

	clen = snprintf(title, sizeof(title), "%s%s",
	    "Creating cscope file list 'cscope.files' in: ", dir);
	if (clen < 0 || clen >= (int)sizeof(title)) {
		csclose(fpipe, pid);
		return (FALSE);
	}
	addline(bp, title);
	addline(bp, "");
	while ((len = getline(&line, &sz, fpipe)) != -1) {
		if (line[len - 1] == *bp->b_nlchr)
			line[len - 1] = '\0';
		addline(bp, line);
	}
	free(line);
	if (ferror(fpipe))
		ewprintf("Problem reading pipe");
	csclose(fpipe, pid);
	return (popbuftop(bp, WNONE));
}

/*
 * Next Symbol. Bound to C-c s n
 */
int
csnextmatch(int f, int n)
{
	struct csrecord *r;
	struct csmatch *m;

	if (curmatch == NULL) {
		if ((r = TAILQ_FIRST(&csrecords)) == NULL)
			return(dobeep_msg("The *cscope* buffer does "
			    "not exist yet"));

		currecord = r;
		curmatch = TAILQ_FIRST(&r->matches);
	} else {
		m = TAILQ_NEXT(curmatch, entry);
		if (m == NULL) {
			r = TAILQ_NEXT(currecord, entry);
			if (r == NULL) {
				return(dobeep_msg("The end of *cscope* buffer "
				    "has been reached"));
			} else {
				currecord = r;
				curmatch = TAILQ_FIRST(&currecord->matches);
			}
		} else
			curmatch = m;
	}
	return (jumptomatch());
}

/*
 * Previous Symbol. Bound to C-c s p
 */
int
csprevmatch(int f, int n)
{
	struct csmatch *m;
	struct csrecord *r;

	if (curmatch == NULL)
		return (FALSE);
	else {
		m  = TAILQ_PREV(curmatch, matches, entry);
		if (m)
			curmatch = m;
		else {
			r = TAILQ_PREV(currecord, csrecords, entry);
			if (r == NULL) {
				return(dobeep_msg("The beginning of *cscope* "
				    "buffer has been reached"));
			} else {
				currecord = r;
				curmatch = TAILQ_LAST(&currecord->matches,
				    matches);
			}
		}
	}
	return (jumptomatch());
}

/*
 * Next file.
 */
int
csnextfile(int f, int n)
{
	struct csrecord *r;

	if (curmatch == NULL) {
		if ((r = TAILQ_FIRST(&csrecords)) == NULL)
			return(dobeep_msg("The *cscope* buffer does not "
			    "exist yet"));
	} else {
		if ((r = TAILQ_NEXT(currecord, entry)) == NULL)
			return(dobeep_msg("The end of *cscope* buffer has "
			    "been reached"));
	}
	currecord = r;
	curmatch = TAILQ_FIRST(&currecord->matches);
	return (jumptomatch());
}

/*
 * Previous file.
 */
int
csprevfile(int f, int n)
{
	struct csrecord *r;

	if (curmatch == NULL) {
		if ((r = TAILQ_FIRST(&csrecords)) == NULL)
			return(dobeep_msg("The *cscope* buffer does not"
			    "exist yet"));
	} else {
		if ((r = TAILQ_PREV(currecord, csrecords, entry)) == NULL)
			return(dobeep_msg("The beginning of *cscope* buffer "
			    "has been reached"));
	}
	currecord = r;
	curmatch = TAILQ_FIRST(&currecord->matches);
	return (jumptomatch());
}

/*
 * The current symbol location is extracted from currecord->filename and
 * curmatch->lineno. Load the file similar to filevisit and goto the
 * lineno recorded.
 */
int
jumptomatch(void)
{
	struct buffer *bp;
	char *adjf;

	if (curmatch == NULL || currecord == NULL)
		return (FALSE);
	adjf = adjustname(currecord->filename, TRUE);
	if (adjf == NULL)
		return (FALSE);
	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFFULL) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] == '\0') {
		if (readin(adjf) != TRUE)
			killbuffer(bp);
	}
	gotoline(FFARG, curmatch->lineno);
	return (TRUE);
}

/*
 * Ask for the symbol, construct cscope commandline with the symbol
 * and passed in index. Popen cscope, read the output into *cscope*
 * buffer and pop it.
 */
int
do_cscope(int i)
{
	struct buffer *bp;
	FILE *fpipe;
	char pattern[NFILEN], query[3], title[BUFSIZ];
	char *argv[8], *p, *buf;
	int clen, nores = 0, ac = 0;
	size_t sz;
	ssize_t len;
	pid_t pid;

	buf = NULL;
	sz = 0;

	/* the word at dot to start with, if any */
	if (curtoken(0, 1, pattern) == FALSE)
		pattern[0] = '\0';
	p = eread("%s", pattern, sizeof(pattern), EFNEW | EFCR | EFDEF,
	    csprompt[i]);
	if (p == NULL)
		return (ABORT);
	else if (p[0] == '\0')
		return (FALSE);

	if (secure)
		return (secure_denied());
	if (csexists("cscope") == FALSE)
		return(dobeep_msg("no such file or directory, cscope"));
	if (csdbdir(csdir, sizeof(csdir)) == FALSE)
		return(dobeep_msg("No cscope database here or above, "
		    "C-c s I to index"));

	csflush();
	ac = csargs(csdir, argv, FALSE);
	(void)snprintf(query, sizeof(query), "-%d", i);
	argv[ac++] = "-L";
	argv[ac++] = query;
	argv[ac++] = pattern;
	argv[ac] = NULL;
	if ((fpipe = csopen(csdir, argv, &pid)) == NULL)
		return(dobeep_msg("problem opening pipe"));

	bp = bfind("*cscope*", TRUE);
	if (bclear(bp) != TRUE) {
		csclose(fpipe, pid);
		return (FALSE);
	}
	bp->b_flag |= BFREADONLY;

	clen = snprintf(title, sizeof(title), "%s%s", csprompt[i], pattern);
	if (clen < 0 || clen >= (int)sizeof(title)) {
		csclose(fpipe, pid);
		return (FALSE);
	}
	addline(bp, title);
	addline(bp, "");
	addline(bp, "-------------------------------------------------------------------------------");
	while ((len = getline(&buf, &sz, fpipe)) != -1) {
		if (buf[len - 1] == *bp->b_nlchr)
			buf[len - 1] = '\0';
		if (addentry(bp, buf) != TRUE) {
			free(buf);
			csclose(fpipe, pid);
			return (FALSE);
		}
		nores = 1;
	}
	free(buf);
	if (ferror(fpipe))
		ewprintf("Problem reading pipe");
	csclose(fpipe, pid);
	addline(bp, "-------------------------------------------------------------------------------");
	if (nores == 0)
		ewprintf("No matches were found.");
	return (popbuftop(bp, WNONE));
}

/*
 * For each line read from cscope output, extract the tokens,
 * add them to list and pretty print a line in *cscope* buffer.
 */
int
addentry(struct buffer *bp, char *csline)
{
	struct csrecord *r;
	struct csmatch *m;
	struct cstokens t;
	int lineno;
	char buf[BUFSIZ];
	const char *errstr;

	r = NULL;
	if (getattr(csline, &t) == FALSE)
		return (FALSE);

	lineno = strtonum(t.lineno, INT_MIN, INT_MAX, &errstr);
	if (errstr)
		return (FALSE);

	if (addentryfn == NULL || strcmp(addentryfn, t.fname) != 0) {
		if ((r = malloc(sizeof(struct csrecord))) == NULL)
			return (FALSE);
		addentryr = r;
		/* names are relative to the database */
		if (t.fname[0] == '/')
			r->filename = strdup(t.fname);
		else if (asprintf(&r->filename, "%s/%s", csdir, t.fname) == -1)
			r->filename = NULL;
		if (r->filename == NULL)
			goto cleanup;
		/* the name as cscope gives it, for the next line */
		addentryfn = r->filename + strlen(r->filename) - strlen(t.fname);
		TAILQ_INIT(&r->matches);
		if ((m = malloc(sizeof(struct csmatch))) == NULL)
			goto cleanup;
		m->lineno = lineno;
		TAILQ_INSERT_TAIL(&r->matches, m, entry);
		TAILQ_INSERT_TAIL(&csrecords, r, entry);
		addline(bp, "");
		if (snprintf(buf, sizeof(buf), "*** %s", t.fname) < 0)
			goto cleanup;
		addline(bp, buf);
	} else {
		if ((m = malloc(sizeof(struct csmatch))) == NULL)
			goto cleanup;
		m->lineno = lineno;
		TAILQ_INSERT_TAIL(&addentryr->matches, m, entry);
	}
	prettyprint(bp, &t);
	return (TRUE);
cleanup:
	free(r);
	return (FALSE);
}

/*
 * Cscope line: <filename> <function> <lineno> <pattern>
 */
int
getattr(char *line, struct cstokens *t)
{
	char *p;

	if ((p = strchr(line, ' ')) == NULL)
		return (FALSE);
	*p++ = '\0';
	t->fname = line;
	line = p;

	if ((p = strchr(line, ' ')) == NULL)
		return (FALSE);
	*p++ = '\0';
	t->function = line;
	line = p;

	if ((p = strchr(line, ' ')) == NULL)
		return (FALSE);
	*p++ = '\0';
	t->lineno = line;

	if (*p == '\0')
		return (FALSE);
	t->pattern = p;

	return (TRUE);
}

void
prettyprint(struct buffer *bp, struct cstokens *t)
{
	char buf[BUFSIZ];

	if (snprintf(buf, sizeof(buf), "%s[%s]\t\t%s",
	    t->function, t->lineno, ltrim(t->pattern)) < 0)
		return;
	addline(bp, buf);
}

const char *
ltrim(const char *s)
{
	while (isblank((unsigned char)*s))
		s++;
	return s;
}

void
csflush(void)
{
	struct csrecord *r;
	struct csmatch *m;

	while ((r = TAILQ_FIRST(&csrecords)) != NULL) {
		free(r->filename);
		while ((m = TAILQ_FIRST(&r->matches)) != NULL) {
			TAILQ_REMOVE(&r->matches, m, entry);
			free(m);
		}
		TAILQ_REMOVE(&csrecords, r, entry);
		free(r);
	}
	addentryr = NULL;
	addentryfn = NULL;
	currecord = NULL;
	curmatch = NULL;
}

/*
 * Run argv in dir, or here when NULL, without a shell, and return its
 * standard output to read.  It gets /dev/null for input and errors,
 * which would otherwise mess up the screen.  Close with csclose().
 */
static FILE *
csopen(const char *dir, char *const argv[], pid_t *pid)
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
 * Close a stream from csopen() and reap the program, returning its
 * exit status, or -1 when it did not exit normally.
 */
static int
csclose(FILE *fp, pid_t pid)
{
	int status;

	fclose(fp);
	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR)
			return (-1);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

/*
 * Check if the cmd exists in $PATH. Split on ":" and iterate through
 * all paths in $PATH.
 */
int
csexists(const char *cmd)
{
	char fname[NFILEN], *dir, *path, *pathc, *tmp;
	int  len, dlen;

	/* Special case if prog contains '/' */
	if (strchr(cmd, '/')) {
		if (access(cmd, F_OK) == -1)
			return (FALSE);
		else
			return (TRUE);
	}
	if ((tmp = getenv("PATH")) == NULL)
		return (FALSE);
	if ((pathc = path = strndup(tmp, NFILEN)) == NULL)
		return(dobeep_msg("out of memory"));

	while ((dir = strsep(&path, ":")) != NULL) {
		if (*dir == '\0')
			continue;

		dlen = strlen(dir);
		while (dlen > 0 && dir[dlen-1] == '/')
			dir[--dlen] = '\0';     /* strip trailing '/' */

		len = snprintf(fname, sizeof(fname), "%s/%s", dir, cmd);
		if (len < 0 || len >= (int)sizeof(fname)) {
			dobeep_msg("path too long");
			goto cleanup;
		}
		if(access(fname, F_OK) == 0) {
			free(pathc);
			return (TRUE);
		}
	}
cleanup:
	free(pathc);
	return (FALSE);
}
