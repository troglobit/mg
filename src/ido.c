/* This file is in the public domain. */

/*
 * ido-mode: the M-x, buffer and file prompts match what is typed as
 * a substring of the candidates and show the matches on the echo
 * line, like ido-mode in GNU Emacs.  The prompt loop in echo.c reads
 * the keys and draws; this keeps the candidates and the directory a
 * file prompt is in.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "def.h"
#include "funmap.h"

int	 ido_mode = TRUE;

static struct {
	int	 flag;			/* EFFUNC, EFBUF or EFFILE	*/
	char	 dir[NFILEN];		/* file prompt: its directory	*/
	char	**name;			/* the candidates		*/
	int	*match;			/* the matching ones, best first */
	int	 nname, nmatch;
	int	 sel;			/* the selected match		*/
	char	 text[NFILEN];		/* what the matches are for	*/
	int	 stale;			/* the candidates changed	*/
} ido;

static int	 ido_setdir(const char *);

int
idomode(int f, int n)
{
	if (f & FFARG)
		ido_mode = n > 0;
	else
		ido_mode = !ido_mode;
	ewprintf("Ido mode %sabled", ido_mode ? "en" : "dis");

	return (TRUE);
}

static void
ido_clear(void)
{
	int	 i;

	for (i = 0; i < ido.nname; i++)
		free(ido.name[i]);
	free(ido.name);
	free(ido.match);
	ido.name = NULL;
	ido.match = NULL;
	ido.nname = ido.nmatch = 0;
	ido.stale = TRUE;
}

static int
cmpname(const void *a, const void *b)
{
	return (strcmp(*(char * const *)a, *(char * const *)b));
}

/*
 * Gather the candidates: every command, sorted; the buffers as last
 * used, the current one last; or the entries of the directory, sorted,
 * with a slash after each subdirectory.
 */
static int
ido_gather(void)
{
	struct list	*lh = NULL, *lp;
	struct buffer	*bp;
	const char	*s;
	size_t		 dlen = strlen(ido.dir);
	int		 n = 0;

	ido_clear();
	if (ido.flag & EFFUNC)
		lh = complete_function_list("");
	else if (ido.flag & EFFILE)
		lh = make_file_list(ido.dir);
	for (lp = lh; lp != NULL; lp = lp->l_next)
		n++;
	if (ido.flag & EFBUF)
		for (bp = bheadp; bp != NULL; bp = bp->b_bufp)
			n++;
	if ((ido.name = calloc(n, sizeof(*ido.name))) == NULL ||
	    (ido.match = calloc(n, sizeof(*ido.match))) == NULL) {
		free_file_list(lh);
		ido_clear();
		return (FALSE);
	}
	for (lp = lh; lp != NULL; lp = lp->l_next) {
		s = lp->l_name + dlen;
		if (strcmp(s, "./") == 0 || strcmp(s, "../") == 0)
			continue;
		if ((ido.name[ido.nname] = strdup(s)) != NULL)
			ido.nname++;
	}
	free_file_list(lh);
	if (ido.flag & EFBUF) {
		for (bp = bheadp; bp != NULL; bp = bp->b_bufp)
			if (bp != curbp &&
			    (ido.name[ido.nname] = strdup(bp->b_bname)) != NULL)
				ido.nname++;
		if ((ido.name[ido.nname] = strdup(curbp->b_bname)) != NULL)
			ido.nname++;
	} else
		qsort(ido.name, ido.nname, sizeof(*ido.name), cmpname);

	return (TRUE);
}

/*
 * Set up for a prompt.  A file prompt starts in the directory of its
 * default, which is taken out of the text, or the buffer's.
 */
int
ido_start(int flag, char *text)
{
	char	 cwd[NFILEN];

	ido.flag = flag & EFAUTO;
	ido.dir[0] = '\0';
	if ((flag & EFFILE) == 0)
		return (ido_gather());

	if (text[0] != '\0' && ido_setdir(text) == TRUE) {
		text[0] = '\0';
		return (TRUE);
	}
	if (ido_slash(text) == TRUE)
		return (TRUE);
	if (getbufcwd(cwd, sizeof(cwd)) != TRUE)
		return (FALSE);
	return (ido_setdir(cwd));
}

void
ido_stop(void)
{
	ido_clear();
	ido.dir[0] = '\0';
}

/*
 * How well a name matches the text: 0 exactly, 1 as a prefix, 2 as
 * a substring, 3 not at all; case does not matter.
 */
static int
rank(const char *name, const char *text)
{
	size_t	 len = strlen(text);

	if (strncasecmp(name, text, len) == 0)
		return (name[len] == '\0' ? 0 : 1);
	for (; *name != '\0'; name++)
		if (strncasecmp(name, text, len) == 0)
			return (2);
	return (3);
}

/*
 * Find the names matching text, the best first and otherwise in the
 * order gathered, and select the first.  Nothing is done, and FALSE
 * returned, while the text is the same, so the selection stands.
 */
int
ido_match(const char *text)
{
	int	 i, r;

	if (!ido.stale && strcmp(text, ido.text) == 0)
		return (FALSE);
	strlcpy(ido.text, text, sizeof(ido.text));
	ido.stale = FALSE;
	ido.sel = 0;
	ido.nmatch = 0;
	for (r = 0; r < 3; r++)
		for (i = 0; i < ido.nname; i++)
			if (rank(ido.name[i], text) == r)
				ido.match[ido.nmatch++] = i;

	return (TRUE);
}

/* The matches from the selected one on, i steps after it. */
const char *
ido_matchname(int i)
{
	return (ido.name[ido.match[(ido.sel + i) % ido.nmatch]]);
}

const char *
ido_selected(void)
{
	return (ido.nmatch > 0 ? ido_matchname(0) : NULL);
}

/* Whether a match other than the first was selected for this text. */
int
ido_rotated(void)
{
	return (ido.sel != 0);
}

int
ido_nmatch(void)
{
	return (ido.nmatch);
}

/* Select the next (dir > 0) or previous match. */
void
ido_rotate(int dir)
{
	if (ido.nmatch > 1)
		ido.sel = (ido.sel + dir + ido.nmatch) % ido.nmatch;
}

/*
 * The prefix all matches share beyond text, into buf; FALSE when the
 * text is not a prefix of every match or nothing can be added.
 */
int
ido_common(const char *text, char *buf, size_t n)
{
	const char	*first, *name;
	size_t		 len, tlen = strlen(text), j;
	int		 i;

	if (ido.nmatch == 0)
		return (FALSE);
	first = ido_matchname(0);
	len = strlen(first);
	for (i = 1; i < ido.nmatch; i++) {
		name = ido_matchname(i);
		if (strncasecmp(name, text, tlen) != 0)
			return (FALSE);
		for (j = 0; j < len && name[j] == first[j]; j++)
			;
		len = j;
	}
	if (len <= tlen || len >= n)
		return (FALSE);
	memcpy(buf, first, len);
	buf[len] = '\0';

	return (TRUE);
}

/*
 * What follows the selected match on the echo line: "]" when it is
 * the only one, else " | second | third}", as many as fit in n.
 */
void
ido_list(char *buf, size_t n)
{
	const char	*name;
	size_t		 len = 0;
	int		 i;

	if (ido.nmatch == 1) {
		strlcpy(buf, "]", n);
		return;
	}
	buf[0] = '\0';
	for (i = 1; i < ido.nmatch; i++) {
		name = ido_matchname(i);
		if (len + strlen(" | ") + strlen(name) + sizeof(" | ...}") > n) {
			strlcat(buf, " | ...", n);
			break;
		}
		len += snprintf(buf + len, n - len, " | %s", name);
	}
	strlcat(buf, "}", n);
}

const char *
ido_dir(void)
{
	return (ido.dir);
}

static int
ido_isdir(const char *name)
{
	size_t	 len = strlen(name);

	return (len > 0 && name[len - 1] == '/');
}

/*
 * Move a file prompt to the directory path, shown with ~ for the home
 * directory and a slash at the end.
 */
static int
ido_setdir(const char *path)
{
	static char	 home[NFILEN];
	char	*adj;
	size_t	 len;

	if (home[0] == '\0' && (adj = adjustname("~", TRUE)) != NULL)
		strlcpy(home, adj, sizeof(home));
	if ((adj = adjustname(path, TRUE)) == NULL || fisdir(adj) != TRUE)
		return (FALSE);
	if ((len = strlen(home)) > 1 && strncmp(adj, home, len) == 0 &&
	    (adj[len] == '/' || adj[len] == '\0')) {
		strlcpy(ido.dir, "~", sizeof(ido.dir));
		strlcat(ido.dir, adj + len, sizeof(ido.dir));
	} else
		strlcpy(ido.dir, adj, sizeof(ido.dir));
	if (!ido_isdir(ido.dir))
		strlcat(ido.dir, "/", sizeof(ido.dir));

	return (ido_gather());
}

/* Enter the selected match, when it is a subdirectory. */
int
ido_enter(const char *sel)
{
	char	 path[NFILEN];

	if (!ido_isdir(sel))
		return (FALSE);
	strlcpy(path, ido.dir, sizeof(path));
	if (strlcat(path, sel, sizeof(path)) >= sizeof(path))
		return (FALSE);

	return (ido_setdir(path));
}

/* Move up to the parent directory; FALSE at the root. */
int
ido_updir(void)
{
	char	 path[NFILEN];

	if (strcmp(ido.dir, "/") == 0)
		return (FALSE);
	strlcpy(path, ido.dir, sizeof(path));
	if (strlcat(path, "..", sizeof(path)) >= sizeof(path))
		return (FALSE);

	return (ido_setdir(path));
}

/*
 * The text names a directory up to its last slash: move there and
 * leave the rest in text.  A text starting with / or ~ names an
 * absolute directory, otherwise it is below the current one.
 */
int
ido_slash(char *text)
{
	char	 path[NFILEN], *slash;

	if (strchr(text, '/') == NULL)
		return (FALSE);
	if (text[0] == '/' || text[0] == '~')
		strlcpy(path, text, sizeof(path));
	else
		snprintf(path, sizeof(path), "%s%s", ido.dir, text);
	if ((slash = strrchr(path, '/')) == NULL)
		return (FALSE);
	slash[1] = '\0';
	if (ido_setdir(path) != TRUE)
		return (FALSE);
	slash = strrchr(text, '/');
	memmove(text, slash + 1, strlen(slash + 1) + 1);

	return (TRUE);
}
