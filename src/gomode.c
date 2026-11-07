/* This file is in the public domain. */

/*
 * Go mode.  Tags the buffer so that syntax highlighting picks the Go
 * rules, and sets it up the way gofmt writes Go: hard tabs, eight
 * wide.  TAB indents the line by the brackets of the line before,
 * RET keeps the indent, and the go tools run on C-c: gofmt on the
 * buffer, build and vet on the module, test and run on the package.
 * Enabled automatically for *.go files.
 */

#include <sys/types.h>

#include <ctype.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static int	 go_brace(int, int);
static int	 go_build(int, int);
static int	 go_colon(int, int);
static int	 go_fmt(int, int);
static int	 go_indent(int, int);
static int	 go_lfindent(int, int);
static int	 go_run(int, int);
static int	 go_tab(int, int);
static int	 go_test(int, int);
static int	 go_vet(int, int);

static PF gomode_cb[] = {
	go_build,	/* C-c C-b */
};

static PF gomode_cf[] = {
	go_fmt,		/* C-c C-f */
};

static PF gomode_cr[] = {
	go_run,		/* C-c C-r */
	rescan,		/* C-c C-s */
	go_test,	/* C-c C-t */
};

static PF gomode_cv[] = {
	go_vet,		/* C-c C-v */
};

static struct KEYMAPE (4) gomode_cmap = {
	4,
	4,
	rescan,
	{
		{ CCHR('B'), CCHR('B'), gomode_cb, NULL },
		{ CCHR('F'), CCHR('F'), gomode_cf, NULL },
		{ CCHR('R'), CCHR('T'), gomode_cr, NULL },
		{ CCHR('V'), CCHR('V'), gomode_cv, NULL }
	}
};

static PF gomode_cc[] = {
	NULL,		/* ^C */
	rescan,		/* ^D */
	rescan,		/* ^E */
	rescan,		/* ^F */
	rescan,		/* ^G */
	rescan,		/* ^H */
	go_tab,		/* ^I */
	rescan,		/* ^J */
	rescan,		/* ^K */
	rescan,		/* ^L */
	go_lfindent,	/* ^M */
};

static PF gomode_colon[] = {
	go_colon,	/* : */
};

static PF gomode_brace[] = {
	go_brace,	/* } */
};

static struct KEYMAPE (3) gomodemap = {
	3,
	3,
	rescan,
	{
		{ CCHR('C'), CCHR('M'), gomode_cc, (KEYMAP *)&gomode_cmap },
		{ ':', ':', gomode_colon, NULL },
		{ '}', '}', gomode_brace, NULL }
	}
};

/*
 * Whether the text at offset i of lp starts a case or default
 * label, which sits one level out from the statements around it.
 */
static int
go_case(const struct line *lp, int i)
{
	int	 len = llength(lp);

	if (i + 4 < len && memcmp(ltext(lp) + i, "case", 4) == 0 &&
	    isspace(lgetc(lp, i + 4)))
		return (TRUE);
	return (i + 8 <= len && memcmp(ltext(lp) + i, "default:", 8) == 0);
}

/*
 * The natural indent of the current line: that of the previous
 * non-blank line, one level deeper when that line leaves a bracket
 * open or ends in the colon of a label, one level back when this
 * line closes a bracket or is a label.
 */
static int
go_target(void)
{
	struct line	*lp;
	int	 c, col, depth, i, last, q;

	col = prevlineindent(&lp);
	if (lp != curbp->b_headp) {
		depth = 0;
		last = '\0';
		q = 0;
		for (i = 0; i < llength(lp); i++) {
			c = lgetc(lp, i);
			if (q != 0) {
				if (c == '\\' && q != '`')
					i++;
				else if (c == q)
					q = 0;
			} else if (c == '"' || c == '\'' || c == '`')
				q = c;
			else if (c == '/' && i + 1 < llength(lp) &&
			    lgetc(lp, i + 1) == '/')
				break;
			else if (c == '(' || c == '[' || c == '{')
				depth++;
			else if (c == ')' || c == ']' || c == '}')
				depth--;
			if (!isspace(c))
				last = c;
		}
		if (depth > 0 || last == ':')
			col += curbp->b_tabw;
	}

	lineindent(curwp->w_dotp, &i);
	if (i < llength(curwp->w_dotp) &&
	    ((c = lgetc(curwp->w_dotp, i)) == ')' || c == ']' || c == '}' ||
	    go_case(curwp->w_dotp, i)))
		col -= curbp->b_tabw;
	return (col < 0 ? 0 : col);
}

static int
go_indent(int f, int n)
{
	return (setindent(go_target()));
}

/*
 * Indent the line, or all the lines in the region when the mark is
 * active.  Pressing TAB again adds a level, for the continuation of
 * a statement, which the brackets alone do not reveal.
 */
static int
go_tab(int f, int n)
{
	int	 col;

	if (curwp->w_markact)
		return (regionlines(go_indent));

	thisflag |= CFINDT;
	if ((lastflag & CFINDT) != 0)
		col = lineindent(curwp->w_dotp, NULL) + curbp->b_tabw;
	else
		col = go_target();
	return (setindent(col));
}

/*
 * Whether the text at offset o of the current line is data, inside
 * a string or a comment, where a brace or a colon must not indent
 * the line.
 */
static int
go_verbatim(int o)
{
	int	 c = syn_class(curbp, curwp->w_dotp, o);

	return (c == SYN_STRING || c == SYN_COMMENT);
}

/*
 * Insert the brace and indent the line to it, unless the brace is
 * text, in a string or a comment.
 */
static int
go_brace(int f, int n)
{
	if (n < 0)
		return (FALSE);
	if (showmatch(FFRAND, 1) == FALSE)
		return (FALSE);
	if (go_verbatim(curwp->w_doto - 1))
		return (TRUE);
	return (go_indent(FFRAND, n));
}

/*
 * Insert the colon and, when it ends a case or default label, move
 * the line out a level, where gofmt puts it.
 */
static int
go_colon(int f, int n)
{
	int	 i;

	if (n < 0)
		return (FALSE);
	if (selfinsert(FFRAND, n) == FALSE)
		return (FALSE);
	lineindent(curwp->w_dotp, &i);
	if (!go_case(curwp->w_dotp, i) || go_verbatim(curwp->w_doto - 1))
		return (TRUE);
	return (go_indent(FFRAND, 1));
}

/*
 * Newline and indent, dropping the trailing blanks of the line
 * left behind as gofmt would; a plain newline inside a raw string.
 */
static int
go_lfindent(int f, int n)
{
	if (n < 0)
		return (FALSE);
	if (syn_class(curbp, curwp->w_dotp, curwp->w_doto) == SYN_STRING)
		return (enewline(FFRAND, n));
	deltrailwhite(FFRAND, 1);
	if (enewline(FFRAND, 1) == FALSE)
		return (FALSE);
	return (go_indent(FFRAND, n));
}

/*
 * The lines gofmt makes of the buffer, NULL-terminated, with its
 * exit status in *status; NULL when it could not be run.  gofmt
 * reads the buffer from an unlinked temporary file.
 */
static char **
go_gofmt(int *status)
{
	char	*argv[] = { "gofmt", NULL };
	const char	*tmpdir;
	char	 tname[NFILEN], *buf = NULL, **out = NULL;
	size_t	 cap = 128, len = 0, n = 0;
	ssize_t	 llen;
	FILE	*fp;
	pid_t	 pid;
	int	 fd, nl = 0, s;

	if ((tmpdir = getenv("TMPDIR")) == NULL || *tmpdir == '\0')
		tmpdir = _PATH_TMP;
	if (snprintf(tname, sizeof(tname), "%s/mg.XXXXXX", tmpdir) >=
	    (int)sizeof(tname) || (fd = mkstemp(tname)) == -1) {
		dobeep();
		ewprintf("Can't create a file in %s", tmpdir);
		return (NULL);
	}
	(void)unlink(tname);
	fp = fdopen(dup(fd), "w");
	s = fp != NULL ? ffputbuf(fp, curbp, 0) : FIOERR;
	if (fp == NULL || fclose(fp) != 0 || s != FIOSUC ||
	    lseek(fd, 0, SEEK_SET) == -1 ||
	    (fp = runopenin(NULL, argv, &pid, RUN_STDERR, fd)) == NULL) {
		close(fd);
		dobeep();
		ewprintf("Can't run gofmt");
		return (NULL);
	}
	close(fd);

	out = reallocarray(NULL, cap, sizeof(*out));
	while (out != NULL && (llen = getline(&buf, &len, fp)) != -1) {
		if ((nl = llen > 0 && buf[llen - 1] == '\n'))
			buf[--llen] = '\0';
		if (n + 2 > cap) {
			cap *= 2;
			if ((out = reallocarray(out, cap, sizeof(*out))) == NULL)
				break;
		}
		if ((out[n] = strdup(buf)) == NULL)
			break;
		n++;
	}
	free(buf);
	*status = runclose(fp, pid);
	if (out == NULL || (nl && (out[n] = strdup("")) == NULL)) {
		dobeep();
		ewprintf("Cannot allocate memory.");
		while (out != NULL && n > 0)
			free(out[--n]);
		free(out);
		return (NULL);
	}
	if (nl)
		n++;
	out[n] = NULL;
	return (out);
}

/*
 * Show the first error gofmt found and move dot to it.  gofmt names
 * its input <standard input>, which is this buffer.
 */
static int
go_fmt_error(char **out, int status)
{
	const char	*msg;
	int	 col, line;

	if ((msg = out[0]) == NULL)
		msg = status == 127 ? "gofmt not found" : "gofmt failed";
	else if (strncmp(msg, "<standard input>:", 17) == 0) {
		msg += 17;
		if (sscanf(msg, "%d:%d:", &line, &col) == 2) {
			setlineno(line);
			if (col < 1)
				col = 1;
			if (col > llength(curwp->w_dotp) + 1)
				col = llength(curwp->w_dotp) + 1;
			curwp->w_doto = col - 1;
		}
	}
	dobeep();
	ewprintf("gofmt: %s", msg);
	return (FALSE);
}

/*
 * Where line number l is after the n lines from k became m: moved
 * along below them, kept within them when it was one of them.
 */
static int
go_shifted(int l, int k, int n, int m)
{
	if (l >= k + n)
		return (l + m - n);
	if (l >= k && (m == 0 || l > k + m - 1))
		return (m == 0 ? k : k + m - 1);
	return (l);
}

static int
go_lineeq(const struct line *lp, const char *s)
{
	size_t	 len = strlen(s);

	return (len == (size_t)llength(lp) &&
	    (len == 0 || memcmp(ltext(lp), s, len) == 0));
}

/*
 * Replace the lines of the buffer that differ from out, the lines
 * before and after that match left alone, so dot and the mark keep
 * their place when gofmt only touched something else.  One undo
 * record.
 */
static int
go_fmt_apply(char **out)
{
	struct regionpos	 pos;
	struct line	*lp;
	char	*text, *p;
	size_t	 l, len;
	RSIZE	 bytes;
	int	 i, k, m, n, nb, nout, pre, suf;

	for (nout = 0; out[nout] != NULL; nout++)
		;
	nb = curbp->b_lines;

	pre = 0;
	lp = bfirstlp(curbp);
	while (pre < nb && pre < nout && go_lineeq(lp, out[pre])) {
		lp = lforw(lp);
		pre++;
	}
	if (pre == nb && pre == nout) {
		ewprintf("gofmt: no change");
		return (TRUE);
	}
	suf = 0;
	lp = lback(curbp->b_headp);
	while (suf < nb - pre && suf < nout - pre &&
	    go_lineeq(lp, out[nout - 1 - suf])) {
		lp = lback(lp);
		suf++;
	}
	k = pre + 1;		/* the first line that changes */
	n = nb - pre - suf;	/* lines to take out */
	m = nout - pre - suf;	/* lines to put in */

	/*
	 * The new lines: ending in a newline when a kept line follows,
	 * starting with one when they go after the last line.
	 */
	len = 2;
	for (i = 0; i < m; i++)
		len += strlen(out[pre + i]) + 1;
	if ((text = malloc(len)) == NULL) {
		dobeep();
		ewprintf("Cannot allocate memory.");
		return (FALSE);
	}
	p = text;
	if (n == 0 && suf == 0)
		*p++ = '\n';
	for (i = 0; i < m; i++) {
		l = strlen(out[pre + i]);
		memcpy(p, out[pre + i], l);
		p += l;
		if (i + 1 < m || suf > 0)
			*p++ = '\n';
	}

	savepos(&pos);
	undo_boundary_enable(FFRAND, 0);
	if (n == 0 && suf == 0) {
		setlineno(nb);
		curwp->w_doto = llength(curwp->w_dotp);
	} else {
		setlineno(k);
		curwp->w_doto = 0;
		bytes = 0;
		for (i = 0, lp = curwp->w_dotp; i < n; i++, lp = lforw(lp))
			bytes += llength(lp) + 1;
		if (suf == 0)
			bytes--;	/* no newline after the last line */
		if (bytes > 0 && ldelete(bytes, KNONE) != TRUE) {
			undo_boundary_enable(FFRAND, 1);
			free(text);
			return (FALSE);
		}
	}
	region_put_data(text, p - text);
	undo_boundary_enable(FFRAND, 1);
	free(text);

	pos.dotline = go_shifted(pos.dotline, k, n, m);
	pos.markline = go_shifted(pos.markline, k, n, m);
	restorepos(&pos);

	i = n > m ? n : m;
	ewprintf("gofmt: %d line%s changed", i, i == 1 ? "" : "s");
	return (TRUE);
}

/*
 * Run gofmt over the buffer and put its changes in place.  On a
 * syntax error dot moves to the first one and the echo line shows
 * it.
 */
static int
go_fmt(int f, int n)
{
	char	**out;
	int	 i, s, status;

	if (secure)
		return (secure_denied());
	if ((s = editable()) != TRUE)
		return (s);
	if ((out = go_gofmt(&status)) == NULL)
		return (FALSE);
	if (status == 0)
		s = go_fmt_apply(out);
	else
		s = go_fmt_error(out, status);
	for (i = 0; out[i] != NULL; i++)
		free(out[i]);
	free(out);
	return (s);
}

/*
 * The directory of the go.mod above the buffer's file, with the
 * trailing slash of b_cwd, or the buffer's own outside a module.
 */
static int
go_root(char *root, size_t len)
{
	char	 path[NFILEN];
	size_t	 n;

	if (getbufcwd(root, len) != TRUE)
		return (FALSE);
	n = strlen(root);
	if (n == 0 || root[n - 1] != '/') {
		if (strlcat(root, "/", len) >= len)
			return (FALSE);
		n++;
	}
	while (n > 1) {
		if (snprintf(path, sizeof(path), "%.*sgo.mod", (int)n, root) <
		    (int)sizeof(path) && access(path, F_OK) == 0) {
			root[n] = '\0';
			break;
		}
		for (n--; n > 0 && root[n - 1] != '/'; n--)
			;
	}
	return (TRUE);
}

/*
 * Run a go command, from the module root or the package directory,
 * in *compile* where next-error finds what it reports.  With an
 * argument the command is offered for editing first.
 */
static int
go_tool(int f, const char *cmd, int module)
{
	char	 dir[NFILEN], buf[NFILEN], *bufp;

	if ((module ? go_root(dir, sizeof(dir)) :
	    getbufcwd(dir, sizeof(dir))) != TRUE)
		return (FALSE);
	if (strlcpy(buf, cmd, sizeof(buf)) >= sizeof(buf))
		return (FALSE);
	if (f & FFARG) {
		if ((bufp = eread("Go command: ", buf, sizeof(buf),
		    EFDEF | EFNEW | EFCR)) == NULL)
			return (ABORT);
		else if (bufp[0] == '\0')
			return (FALSE);
	}
	return (compile_command(f, buf, dir));
}

static int
go_build(int f, int n)
{
	return (go_tool(f, "go build ./...", TRUE));
}

static int
go_vet(int f, int n)
{
	return (go_tool(f, "go vet ./...", TRUE));
}

static int
go_test(int f, int n)
{
	return (go_tool(f, "go test .", FALSE));
}

static int
go_run(int f, int n)
{
	return (go_tool(f, "go run . </dev/null", FALSE));
}

static int
gomode(int f, int n)
{
	int	 s;

	if ((s = changemode(f, n, "go")) != TRUE)
		return (s);
	if (!buf_hasmode(curbp, "go"))
		return (TRUE);	/* mode was toggled off */

	/* gofmt indents with tabs, shown eight wide */
	modetabw(8);
	return (notabmode(FFARG, 0));
}

void
gomode_init(void)
{
	funmap_add(gomode, "go-mode", 0);
	funmap_add(go_brace, "go-brace", 0);
	funmap_add(go_build, "go-build", 0);
	funmap_add(go_colon, "go-colon", 0);
	funmap_add(go_fmt, "go-fmt", 0);
	funmap_add(go_indent, "go-indent", 0);
	funmap_add(go_lfindent, "go-indent-and-newline", 0);
	funmap_add(go_run, "go-run", 0);
	funmap_add(go_tab, "go-tab-or-indent", 0);
	funmap_add(go_test, "go-test", 0);
	funmap_add(go_vet, "go-vet", 0);
	delsel_add(go_brace, FALSE);
	delsel_add(go_colon, FALSE);
	delsel_add(go_lfindent, FALSE);
	maps_add((KEYMAP *)&gomodemap, "go");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.go", "go-mode");
#endif
}
