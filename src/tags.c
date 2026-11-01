/*	$OpenBSD: tags.c,v 1.27 2023/03/29 19:09:04 op Exp $	*/

/*
 * This file is in the public domain.
 *
 * Author: Sunil Nimmagadda <sunil@openbsd.org>
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "kbd.h"

#ifdef REGEX
#include <regex.h>
#endif

struct ctag;
struct tagpos;

static int               addctag(char *);
static int               addetag(const char *, size_t, const char *,
                             const char *, size_t, int);
static int               atbow(void);
void                     closetags(void);
static int               ctagcmp(struct ctag *, struct ctag *);
static int               findtag1(int, int, int);
static int               gototag(char *, int);
static int               inserttag(struct ctag *);
static int               loadbuffer(char *, int);
static int               loadetags(FILE *, const char *, int);
static int               loadtags(const char *, int);
static void              freepos(struct tagpos *);
static struct tagpos     *markpos(void);
static int               nexttag(int);
static int               pushtag(struct ctag *, int);
static int               showloc(const char *, char *, int, const char *, int);
static int               searchpat(char *, int);
static struct ctag       *searchtag(char *);
static char              *strip(char *, size_t);
static void              unloadtags(void);
static int               xrefgoto(int, int);
static int               xrefgotoquit(int, int);
static int               xrefnext(int, int);
static int               xrefnextfile(int, int);
static int               xrefprev(int, int);
static int               xrefprevfile(int, int);
static int               xrefquit(int, int);
static int               xrefvisit(int);
static struct mgwin      *xrefwin(void);

#define DEFAULTFN "tags"
#define ETAGSFN   "TAGS"

/*
 * ctags(1) and etags(1) entries are kept in a tree with one node per
 * name; other definitions of the name are chained to it in the order
 * read.  The pattern is the start of the line the tag is defined on;
 * etags also gives its line number.
 */
struct ctag {
	RB_ENTRY(ctag) entry;
	struct ctag *next;	/* another definition of the tag	*/
	struct ctag *last;	/* in the tree, the last of the chain	*/
	char *tag;
	char *fname;
	char *pat;
	int   line;		/* 0 when not known			*/
};
RB_HEAD(tagtree, ctag) tags = RB_INITIALIZER(&tags);
RB_GENERATE(tagtree, ctag, entry, ctagcmp);

struct tagpos {
	SLIST_ENTRY(tagpos) entry;
	int    doto;
	int    dotline;
	char   *bname;		/* the file, or a buffer with none	*/
	int    isfile;
};
SLIST_HEAD(tagstack, tagpos) shead = SLIST_HEAD_INITIALIZER(shead);

/* The files read, so each is loaded only once until the tags are unloaded. */
static struct {
	dev_t	 dev;
	ino_t	 ino;
} seen[64];
static int	 nseen;

/*
 * The *xref* list of locations, the definitions of a tag or what a
 * cscope search found, one line for each in xref[]: n and p show one,
 * N and P the first of the next or previous file, RET goes there, TAB
 * too and closes the list, and q only closes it.  Going to one pushes
 * where the list was asked from on the tag stack.  A location is found
 * by the start of its line, pat, near its line number, or by the line
 * number alone when pat is empty.
 */
struct xent {
	char	*fname;
	char	*pat;
	char	*text;			/* shown in the list		*/
	char	*name;			/* for messages			*/
	int	 line;
};
static struct xent	*xref;
static int		 nxref, maxxref;
static int		 xrefcur;	/* the entry gone to last, or -1	*/
static int		 xrefsame;	/* the list is of lasttag		*/

/* The tag M-. went to last, and which of its definitions. */
static char		 lasttag[MAX_TOKEN];
static int		 lastidx;
static struct mgwin	*xreffrom;	/* the window the list was asked from */
static struct tagpos	*xrefpos;	/* where, until pushed		*/

static PF xref_ci[] = {
	xrefgotoquit,		/* ^I */
	rescan,			/* ^J */
	rescan,			/* ^K */
	rescan,			/* ^L */
	xrefgoto		/* ^M */
};

static PF xref_NP[] = {
	xrefnextfile,		/* N */
	rescan,			/* O */
	xrefprevfile		/* P */
};

static PF xref_nq[] = {
	xrefnext,		/* n */
	rescan,			/* o */
	xrefprev,		/* p */
	xrefquit		/* q */
};

static struct KEYMAPE (3) xrefmap = {
	3,
	3,
	rescan,
	{
		{ CCHR('I'), CCHR('M'), xref_ci, NULL },
		{ 'N', 'P', xref_NP, NULL },
		{ 'n', 'q', xref_nq, NULL }
	}
};

int
ctagcmp(struct ctag *s, struct ctag *t)
{
	return strcmp(s->tag, t->tag);
}

/*
 * Load a tags file.  If a tags file is already loaded, ask the user to
 * retain loaded tags (i any) and unload them if the user chooses not to.
 */
int
tagsvisit(int f, int n)
{
	char fname[NFILEN], *bufp, *temp;
	const char *dfn = DEFAULTFN;
	size_t len;
	
	if (getbufcwd(fname, sizeof(fname)) == FALSE)
		fname[0] = '\0';
	len = strlen(fname);
	
	if (strlcat(fname, DEFAULTFN, sizeof(fname)) >= sizeof(fname)) {
		dobeep();
		ewprintf("Filename too long");
		return (FALSE);
	}
	/* no ctags(1) file, but an etags(1) one: offer that instead */
	if (access(fname, F_OK) == -1) {
		(void)strlcpy(fname + len, ETAGSFN, sizeof(fname) - len);
		if (access(fname, F_OK) == 0)
			dfn = ETAGSFN;
		else
			(void)strlcpy(fname + len, DEFAULTFN,
			    sizeof(fname) - len);
	}
	
	bufp = eread("Visit tags table (default %s): ", fname,
	    NFILEN, EFFILE | EFCR | EFNEW | EFDEF, dfn);
	if (bufp == NULL)
		return (ABORT);

	if (!RB_EMPTY(&tags)) {
		if (eyorn("Keep current list of tags table also") == FALSE) {
			ewprintf("Starting a new list of tags table");
			unloadtags();
		}
	}

	temp = bufp;
	if (temp[0] == '\0')
		temp = fname;
	if ((temp = adjustname(temp, TRUE)) == NULL)
		return (FALSE);

	return (loadtags(temp, 0));
}

/*
 * Ask user for a tag while treating word at dot as default. Visit tags
 * file if not yet done, load tags and jump to definition of the tag.
 */
int
findtag(int f, int n)
{
	return (findtag1(f, n, FALSE));
}

/*
 * C-x 4 .: find-tag, going to the definition in another window.
 */
int
findtagother(int f, int n)
{
	return (findtag1(f, n, TRUE));
}

static int
findtag1(int f, int n, int other)
{
	char utok[MAX_TOKEN], dtok[MAX_TOKEN];
	char *tok, *bufp;
	int  ret;

	/* C-u M-. for the next definition, C-u - M-. to go back */
	if (f & FFARG)
		return (n < 0 ? poptag(FFRAND, 1) : nexttag(other));

	/* first, so TAB has the names to complete */
	if (RB_EMPTY(&tags))
		if ((ret = tagsvisit(f, n)) != TRUE)
			return (ret);

	if (curtoken(f, n, dtok) == FALSE) {
		dtok[0] = '\0';
		bufp = eread("Find tag: ", utok, MAX_TOKEN,
		    EFNUL | EFNEW | EFTAG);
	} else
		bufp = eread("Find tag (default %s): ", utok, MAX_TOKEN,
		    EFNUL | EFNEW | EFTAG, dtok);

	if (bufp == NULL)
		return (ABORT);
	else if	(bufp[0] == '\0')
		tok = dtok;
	else
		tok = utok;
	
	if (tok[0] == '\0') {
		dobeep();
		ewprintf("There is no default tag");
		return (FALSE);
	}

	return (gototag(tok, other));
}

/*
 * The names of the tags starting with prefix, for completion.
 */
struct list *
complete_tag_list(const char *prefix)
{
	struct ctag t, *p;
	struct list *head = NULL, *el;
	size_t len = strlen(prefix);

	t.tag = (char *)prefix;
	for (p = RB_NFIND(tagtree, &tags, &t);
	    p != NULL && strncmp(p->tag, prefix, len) == 0;
	    p = RB_NEXT(tagtree, &tags, p)) {
		if ((el = malloc(sizeof(*el))) == NULL ||
		    (el->l_name = strdup(p->tag)) == NULL) {
			free(el);
			free_file_list(head);
			return (NULL);
		}
		el->l_next = head;
		head = el;
	}
	return (head);
}

/*
 * Go to the definition of tok, in another window if other is set, or
 * list them in *xref* when there are more than one.
 */
static int
gototag(char *tok, int other)
{
	char msg[NFILEN];
	struct ctag *res, *t;
	int n;

	if ((res = searchtag(tok)) == NULL)
		return (FALSE);
	(void)strlcpy(lasttag, tok, sizeof(lasttag));
	if (res->next == NULL) {
		lastidx = 0;
		return (pushtag(res, other));
	}
	/* none gone to yet, so C-u M-. starts at the first */
	lastidx = -1;
	xrefclear();
	for (n = 0, t = res; t != NULL; t = t->next, n++)
		if (xrefadd(t->fname, t->pat, t->line, t->tag, t->pat) == FALSE)
			return (FALSE);
	xrefsame = TRUE;
	(void)snprintf(msg, sizeof(msg), "%d definitions of %s", n, tok);
	return (xreflist(NULL, msg));
}

#ifdef REGEX
extern int	 casefoldsearch;

/*
 * List the definitions of the tags whose names match a regular
 * expression in *xref*, like xref-find-apropos in Emacs.
 */
int
tagsapropos(int f, int n)
{
	char	 re[NPAT], msg[NFILEN];
	regex_t	 rx;
	struct ctag *p, *t;
	int	 cnt = 0, ret;

	if (RB_EMPTY(&tags) && (ret = tagsvisit(f, n)) != TRUE)
		return (ret);
	if (eread("Find tags matching regexp: ", re, sizeof(re),
	    EFNEW | EFCR) == NULL)
		return (ABORT);
	if (regcomp(&rx, re, REG_EXTENDED | REG_NOSUB |
	    (casefoldsearch ? REG_ICASE : 0)) != 0) {
		dobeep();
		ewprintf("Bad regexp: %s", re);
		return (FALSE);
	}
	xrefclear();
	RB_FOREACH(p, tagtree, &tags) {
		if (regexec(&rx, p->tag, 0, NULL, 0) != 0)
			continue;
		for (t = p; t != NULL; t = t->next, cnt++)
			if (xrefadd(t->fname, t->pat, t->line, t->tag,
			    t->pat) == FALSE) {
				regfree(&rx);
				return (FALSE);
			}
	}
	regfree(&rx);
	if (cnt == 0) {
		dobeep();
		ewprintf("No tags matching %s", re);
		return (FALSE);
	}
	(void)snprintf(msg, sizeof(msg), "%d definitions matching %s", cnt,
	    re);
	return (xreflist(NULL, msg));
}
#endif

/*
 * Go to the next definition of the tag M-. went to last, like C-u M-.
 * in Emacs.
 */
static int
nexttag(int other)
{
	struct ctag t, *res;
	int i;

	t.tag = lasttag;
	if (lasttag[0] == '\0' ||
	    (res = RB_FIND(tagtree, &tags, &t)) == NULL) {
		dobeep();
		ewprintf("No previous tag");
		return (FALSE);
	}
	for (i = 0; i <= lastidx && res != NULL; i++)
		res = res->next;
	if (res == NULL) {
		dobeep();
		ewprintf("No more definitions of %s", lasttag);
		return (FALSE);
	}
	lastidx = i;
	return (pushtag(res, other));
}

/*
 * Free tags tree.
 */
void
unloadtags(void)
{
	struct ctag *var, *nxt, *t;
	
	nseen = 0;
	for (var = RB_MIN(tagtree, &tags); var != NULL; var = nxt) {
		nxt = RB_NEXT(tagtree, &tags, var);
		RB_REMOVE(tagtree, &tags, var);
		while ((t = var) != NULL) {
			var = t->next;
			/* line parsed with fparseln needs to be freed */
			free(t->tag);
			free(t);
		}
	}
}

/*
 * The location of dot, for the tag stack, or NULL when out of memory.
 */
static struct tagpos *
markpos(void)
{
	struct tagpos *s;
	int isfile;

	/* where to return: the file, or the name of a buffer with none */
	isfile = curbp->b_fname[0] != '\0';
	if ((s = malloc(sizeof(struct tagpos))) == NULL ||
	    (s->bname = strdup(isfile ? curbp->b_fname : curbp->b_bname)) ==
	    NULL) {
		free(s);
		(void)dobeep_msg("Out of memory");
		return (NULL);
	}
	s->isfile = isfile;
	s->doto = curwp->w_doto;
	s->dotline = curwp->w_dotline;
	return (s);
}

/*
 * Free an entry for the tag stack.
 */
static void
freepos(struct tagpos *s)
{
	free(s->bname);
	free(s);
}

/*
 * Load fname into the current window, or another one if other is set,
 * and put dot on the line starting with pat, near line, or on line when
 * pat is empty.  name is for the message when it is not there.
 */
static int
showloc(const char *fname, char *pat, int line, const char *name, int other)
{
	if (loadbuffer((char *)fname, other) == FALSE)
		return (FALSE);
	if (pat[0] == '\0')
		(void)setlineno(line);
	else if (searchpat(pat, line) == FALSE) {
		dobeep();
		ewprintf("%s: pattern not found", name);
		return (FALSE);
	}
	curwp->w_rflag |= WFMOVE;
	return (TRUE);
}

/*
 * Go to the definition res, in another window if other is set, pushing
 * where dot was onto the tag stack.
 */
static int
pushtag(struct ctag *res, int other)
{
	struct tagpos *s;

	if ((s = markpos()) == NULL)
		return (FALSE);
	if (showloc(res->fname, res->pat, res->line, res->tag, other) ==
	    FALSE) {
		freepos(s);
		return (FALSE);
	}
	SLIST_INSERT_HEAD(&shead, s, entry);
	return (TRUE);
}

/*
 * Empty the list of locations, for a new one.
 */
void
xrefclear(void)
{
	while (nxref > 0)
		free(xref[--nxref].fname);
	xrefcur = -1;
	xrefsame = FALSE;
}

/*
 * Add a location to the list, see struct xent; the list keeps copies.
 */
int
xrefadd(const char *fname, const char *pat, int line, const char *name,
    const char *text)
{
	struct xent *e;

	if (nxref == maxxref) {
		if ((e = reallocarray(xref, maxxref ? maxxref * 2 : 16,
		    sizeof(*e))) == NULL)
			return (dobeep_msg("Out of memory"));
		xref = e;
		maxxref = maxxref ? maxxref * 2 : 16;
	}
	/* one allocation for the strings, freed through fname */
	e = &xref[nxref];
	if (asprintf(&e->fname, "%s%c%s%c%s%c%s", fname, '\0', pat, '\0',
	    text, '\0', name) == -1)
		return (dobeep_msg("Out of memory"));
	e->pat = e->fname + strlen(fname) + 1;
	e->text = e->pat + strlen(pat) + 1;
	e->name = e->text + strlen(text) + 1;
	e->line = line;
	nxref++;
	return (TRUE);
}

/*
 * Show the list in the *xref* pop-up and select it, with msg on the
 * echo line.  File names under base, or the directory of the buffer
 * when it is NULL, are shown relative to it.
 */
int
xreflist(const char *base, const char *msg)
{
	static int	 initialized;
	struct buffer	*bp;
	struct mgwin	*wp;
	char		 cwd[NFILEN];
	const char	*fn;
	size_t		 clen;
	int		 i;

	if (!initialized) {
		maps_add((KEYMAP *)&xrefmap, "xref");
		initialized = 1;
	}
	if (xrefpos != NULL)
		freepos(xrefpos);
	if ((xrefpos = markpos()) == NULL)
		return (FALSE);
	xreffrom = curwp;

	if ((bp = popbuffer("*xref*", name_mode("xref"))) == NULL)
		return (FALSE);
#ifdef ENABLE_COMPILE_GREP
	next_error_buf(bp);
#endif
	if (base != NULL)
		(void)snprintf(cwd, sizeof(cwd), "%s/", base);
	else if (getbufcwd(cwd, sizeof(cwd)) == FALSE)
		cwd[0] = '\0';
	clen = strlen(cwd);
	for (i = 0; i < nxref; i++) {
		fn = xref[i].fname;
		if (clen > 0 && strncmp(fn, cwd, clen) == 0)
			fn += clen;
		if (xref[i].line > 0)
			addlinef(bp, "%s:%d: %s", fn, xref[i].line,
			    xref[i].text);
		else
			addlinef(bp, "%s: %s", fn, xref[i].text);
	}
	if ((wp = popshow(bp)) == NULL)
		return (FALSE);
	curwp = wp;
	curbp = bp;
	ewprintf("%s: n/p show, RET go, TAB go and close, q close", msg);
	return (TRUE);
}

/*
 * The window the *xref* list was asked from, if it is still there.
 */
static struct mgwin *
xrefwin(void)
{
	struct mgwin *wp;

	for (wp = wheadp; wp != NULL && wp != xreffrom; wp = wp->w_wndp)
		;
	return (wp);
}

/*
 * Go to entry i of the list in the current window, and push where the
 * list was asked from on the tag stack, once.
 */
static int
xrefgo(int i)
{
	struct xent *e = &xref[i];

	if (showloc(e->fname, e->pat, e->line, e->name, FALSE) != TRUE)
		return (FALSE);
	xrefcur = i;
	if (xrefsame)
		lastidx = i;
	if (xrefpos != NULL) {
		SLIST_INSERT_HEAD(&shead, xrefpos, entry);
		xrefpos = NULL;
	}
	return (TRUE);
}

/*
 * Show the location on the current line of the list in the window it
 * was asked from, or another one, and select that window if go is set.
 */
static int
xrefvisit(int go)
{
	struct mgwin *xwp = curwp, *wp;
	int i = curwp->w_dotline - 1, s;

	if (i < 0 || i >= nxref) {
		dobeep();
		ewprintf("No location here");
		return (FALSE);
	}
	if ((wp = xrefwin()) == NULL || wp == xwp)
		for (wp = wheadp; wp != NULL && wp == xwp; wp = wp->w_wndp)
			;
	if (wp == NULL && (wp = splitpop(WNONE)) == NULL)
		return (FALSE);
	curwp = wp;
	curbp = wp->w_bufp;
	if (go)
		s = xrefgo(i);
	else
		s = showloc(xref[i].fname, xref[i].pat, xref[i].line,
		    xref[i].name, FALSE);
	if (s != TRUE || !go) {
		curwp = xwp;
		curbp = xwp->w_bufp;
	}
	return (s);
}

/*
 * The entry after i, or before it when dir is -1, or with byfile the
 * first entry of the next or previous file; -1 when there is none.
 */
static int
xrefnth(int i, int dir, int byfile)
{
	const char *fn = i >= 0 && i < nxref ? xref[i].fname : NULL;

	i += dir;
	if (byfile && fn != NULL) {
		while (i >= 0 && i < nxref && strcmp(xref[i].fname, fn) == 0)
			i += dir;
		/* back to the first entry of that file */
		if (dir < 0)
			while (i > 0 &&
			    strcmp(xref[i - 1].fname, xref[i].fname) == 0)
				i--;
	}
	return (i >= 0 && i < nxref ? i : -1);
}

/*
 * Go to the next location of the last list, or the previous one when
 * dir is -1, or with byfile to the next or previous file, in the
 * current window.
 */
int
xrefstep(int dir, int byfile)
{
	int i;

	if (nxref == 0)
		return (dobeep_msg("No list of locations"));
	if (xrefcur < 0 && dir > 0)
		i = 0;
	else if ((i = xrefnth(xrefcur, dir, byfile)) == -1)
		return (dobeep_msg(dir > 0 ? "No more locations" :
		    "No earlier locations"));
	/* in the list itself, as RET on that line, not over the list */
	if (curbp->b_modes[1] == name_mode("xref")) {
		(void)setlineno(i + 1);
		return (xrefvisit(TRUE));
	}
	return (xrefgo(i));
}

/* RET in *xref*: go to the location on this line. */
static int
xrefgoto(int f, int n)
{
	return (xrefvisit(TRUE));
}

/* TAB in *xref*: go to the location on this line and close the list. */
static int
xrefgotoquit(int f, int n)
{
	struct buffer *bp = curbp;

	if (xrefvisit(TRUE) != TRUE)
		return (FALSE);
	return (popclose(bp, curwp));
}

/*
 * Move dot in *xref* to the next location, or the previous one when
 * dir is -1, or with byfile to the next or previous file, and show it.
 */
static int
xrefmove(int dir, int byfile)
{
	int i;

	if ((i = xrefnth(curwp->w_dotline - 1, dir, byfile)) == -1)
		return (dobeep_msg(dir > 0 ? "No more locations" :
		    "No earlier locations"));
	(void)setlineno(i + 1);
	return (xrefvisit(FALSE));
}

/* n, p, N and P in *xref*. */
static int
xrefnext(int f, int n)
{
	return (xrefmove(1, FALSE));
}

static int
xrefprev(int f, int n)
{
	return (xrefmove(-1, FALSE));
}

static int
xrefnextfile(int f, int n)
{
	return (xrefmove(1, TRUE));
}

static int
xrefprevfile(int f, int n)
{
	return (xrefmove(-1, TRUE));
}

/* q in *xref*: close the list, back in the window it was asked from. */
static int
xrefquit(int f, int n)
{
	return (popclose(curbp, xrefwin()));
}

/*
 * If tag stack is not empty pop stack and jump to recorded buffer, dot.
 */
int
poptag(int f, int n)
{
	struct line *dotp;
	struct tagpos *s;
	struct buffer *bp;
	int ok;
	
	if (SLIST_EMPTY(&shead)) {
		dobeep();
		ewprintf("No previous location for find-tag invocation");
		return (FALSE);
	}
	s = SLIST_FIRST(&shead);
	SLIST_REMOVE_HEAD(&shead, entry);
	if (s->isfile)
		ok = loadbuffer(s->bname, FALSE);
	else if ((ok = (bp = bfind(s->bname, FALSE)) != NULL) == TRUE) {
		curbp = bp;
		ok = showbuffer(bp, curwp, WFFULL);
	} else
		dobeep_msgs("No buffer", s->bname);
	if (ok == FALSE) {
		free(s->bname);
		free(s);
		return (FALSE);
	}
	curwp->w_dotline = s->dotline;
	curwp->w_doto = s->doto;
	
	/* storing of dotp in tagpos wouldn't work out in cases when
	 * that buffer is killed by user(dangling pointer). Explicitly
	 * traverse till dotline for correct handling. 
	 */
	dotp = curwp->w_bufp->b_headp;
	while (s->dotline--)
		dotp = dotp->l_fp;
	
	curwp->w_dotp = dotp;
	free(s->bname);
	free(s);
	return (TRUE);
}

/*
 * Parse the tags file and construct the tags tree. Remove escape 
 * characters while parsing the file.  An etags(1) file, which starts
 * with a form feed, goes to loadetags().
 */
int
loadtags(const char *fn, int depth)
{
	struct stat sb;
	char *l;
	FILE *fd;
	int c, i, s;
	
	if (!secure_allowed(fn))
		return (FALSE);
	/* before opening, which would block on a FIFO */
	if (stat(fn, &sb) == 0) {
		if (!S_ISREG(sb.st_mode)) {
			dobeep();
			ewprintf("Not a regular file");
			return (FALSE);
		}
		for (i = 0; i < nseen; i++)
			if (seen[i].dev == sb.st_dev &&
			    seen[i].ino == sb.st_ino)
				return (TRUE);
	}
	if ((fd = fopen(fn, "r")) == NULL) {
		dobeep();
		ewprintf("Unable to open tags file: %s", fn);
		return (FALSE);
	}
	if (fstat(fileno(fd), &sb) == -1) {
		dobeep();
		ewprintf("fstat: %s", strerror(errno));
		fclose(fd);
		return (FALSE);
	}
	if (!S_ISREG(sb.st_mode)) {
		dobeep();
		ewprintf("Not a regular file");
		fclose(fd);
		return (FALSE);
	}
	if (nseen == NELEMS(seen)) {
		dobeep();
		ewprintf("Too many tags files");
		fclose(fd);
		return (FALSE);
	}
	seen[nseen].dev = sb.st_dev;
	seen[nseen++].ino = sb.st_ino;
	c = getc(fd);
	(void)ungetc(c, fd);
	if (c == '\f') {
		s = loadetags(fd, fn, depth);
		fclose(fd);
		return (s);
	}
	while ((l = fparseln(fd, NULL, NULL, "\\\\\0",
	    FPARSELN_UNESCCONT | FPARSELN_UNESCREST)) != NULL) {
		if (addctag(l) == FALSE) {
			fclose(fd);
			return (FALSE);
		}
	}
	fclose(fd);
	return (TRUE);
}

/*
 * The characters Emacs allows in a tag name in an etags(1) file.
 */
static int
isetag(int c)
{
	return (c != '\0' && (isalnum(c) || strchr("-_+*$?:", c) != NULL));
}

/*
 * Parse an etags(1) file the way Emacs does.  Each source file has a
 * section "\f\n<file>,<size>\n" of lines
 * "<text>\177[<name>\001]<line>,<offset>", where <text> is the start
 * of the line the tag is on and, when <name> is left out, ends with
 * it.  A section "<file>,include" names another tags file, read at
 * depth + 1.  File names are relative to fn, the tags file.
 */
static int
loadetags(FILE *fd, const char *fn, int depth)
{
	char	 fname[NFILEN], *l = NULL, *d, *name, *num, *p;
	size_t	 cap = 0, nlen;
	ssize_t	 len;
	int	 dlen, s = TRUE;

	dlen = (p = strrchr(fn, '/')) != NULL ? p - fn + 1 : 0;
	fname[0] = '\0';
	while (s == TRUE && (len = getline(&l, &cap, fd)) != -1) {
		if (len > 0 && l[len - 1] == '\n')
			l[--len] = '\0';
		if (l[0] == '\f')
			continue;
		if ((d = strchr(l, '\177')) == NULL) {
			/* a section header */
			if ((p = strrchr(l, ',')) == NULL)
				continue;
			*p++ = '\0';
			(void)snprintf(fname, sizeof(fname), "%.*s%s",
			    l[0] == '/' ? 0 : dlen, fn, l);
			if (strcmp(p, "include") == 0) {
				if (depth >= 8) {
					dobeep();
					ewprintf("Tags files nested too deep");
					s = FALSE;
				} else
					s = loadtags(fname, depth + 1);
				fname[0] = '\0';
			}
			continue;
		}
		if (fname[0] == '\0')
			continue;
		*d = '\0';
		if ((num = strchr(d + 1, '\001')) != NULL) {
			name = d + 1;
			nlen = num++ - name;
		} else {
			/* the last name in the text, before what follows it */
			for (p = d; p > l && !isetag((unsigned char)p[-1]); p--)
				;
			for (name = p; name > l &&
			    isetag((unsigned char)name[-1]); name--)
				;
			nlen = p - name;
			num = d + 1;
		}
		if (nlen == 0)
			continue;
		s = addetag(name, nlen, fname, l, d - l, atoi(num));
	}
	free(l);
	return (s);
}

/*
 * Add an etags(1) tag, its name, file and pattern copied into one
 * allocation that unloadtags() frees through the name.
 */
static int
addetag(const char *name, size_t nlen, const char *fname, const char *pat,
    size_t plen, int line)
{
	struct ctag *t;
	size_t flen = strlen(fname);
	char *s;

	if ((t = malloc(sizeof(struct ctag))) == NULL ||
	    (s = malloc(nlen + flen + plen + 3)) == NULL) {
		free(t);
		dobeep();
		ewprintf("Out of memory");
		return (FALSE);
	}
	t->tag = s;
	memcpy(s, name, nlen);
	s[nlen] = '\0';
	t->fname = s + nlen + 1;
	memcpy(t->fname, fname, flen + 1);
	t->pat = t->fname + flen + 1;
	memcpy(t->pat, pat, plen);
	t->pat[plen] = '\0';
	t->line = line;
	if (inserttag(t) == FALSE) {
		free(s);
		free(t);
	}
	return (TRUE);
}

/*
 * Add t to the tree, after the other definitions of its name.  FALSE,
 * and t is not used, when it repeats the last of them.
 */
static int
inserttag(struct ctag *t)
{
	struct ctag *o, *l;

	t->next = NULL;
	t->last = t;
	if ((o = RB_INSERT(tagtree, &tags, t)) == NULL)
		return (TRUE);
	l = o->last;
	if (l->line == t->line && strcmp(l->pat, t->pat) == 0 &&
	    strcmp(l->fname, t->fname) == 0)
		return (FALSE);
	l->next = t;
	o->last = t;
	return (TRUE);
}

/*
 * Cleanup and destroy tree and stack.
 */
void
closetags(void)
{
	struct tagpos *s;	
	
	while (!SLIST_EMPTY(&shead)) {
		s = SLIST_FIRST(&shead);
		SLIST_REMOVE_HEAD(&shead, entry);
		free(s->bname);
		free(s);
	}
	unloadtags();
}

/*
 * Strip away any special characters in pattern.
 * The pattern in ctags isn't a true regular expression. Its of the form
 * /^xxx$/ or ?^xxx$? and in some cases the "$" would be missing. Strip 
 * the leading and trailing special characters so the pattern matching
 * would be a simple string compare. Escape character is taken care by 
 * fparseln.
 */
char *
strip(char *s, size_t len)
{
	/* first strip trailing special chars */	
	s[len - 1] = '\0';
	if (s[len - 2] == '$')
		s[len - 2] = '\0';
	
	/* then strip leading special chars */
	s++;
	if (*s == '^')
		s++;
	
	return s;
}

/*
 * tags line is of the format "<tag>\t<filename>\t<pattern>". Split them
 * by replacing '\t' with '\0'. This wouldn't alter the size of malloc'ed
 * l, and can be freed during cleanup.
 */
int
addctag(char *s)
{
	struct ctag *t;
	char *l, *m;
	
	if ((t = malloc(sizeof(struct ctag))) == NULL) {
		dobeep();
		ewprintf("Out of memory");
		free(s);
		return (FALSE);
	}
	t->tag = s;
	t->line = 0;
	if ((l = strchr(s, '\t')) == NULL)
		goto cleanup;
	*l++ = '\0';
	t->fname = l;
	if ((l = strchr(l, '\t')) == NULL)
		goto cleanup;
	*l++ = '\0';
	if (*l == '\0')
		goto cleanup;

	/* compat with newer exuberant ctags format */
	if ((m = strstr(l, ";\"")))
		*m = '\0';

	t->pat = strip(l, strlen(l));
	if (inserttag(t) == FALSE) {
		free(t);
		free(s);
	}
	return (TRUE);
cleanup:
	free(t);
	free(s);
	return (FALSE);
}

/*
 * Search through each line of buffer for one starting with pattern,
 * the one on line hint if that does, else the first.
 */
int
searchpat(char *s_pat, int hint)
{
	struct line *lp;
	int dotline, found = FALSE;
	size_t plen;

	plen = strlen(s_pat);
	dotline = 1;
	for (lp = lforw(curbp->b_headp); lp != curbp->b_headp;
	    lp = lforw(lp), dotline++) {
		if (ltext(lp) == NULL || (int)plen > llength(lp) ||
		    strncmp(s_pat, ltext(lp), plen) != 0)
			continue;
		if (!found || dotline == hint) {
			curwp->w_dotp = lp;
			curwp->w_dotline = dotline;
			found = TRUE;
		}
		if (hint == 0 || dotline == hint)
			break;
	}
	if (found)
		curwp->w_doto = 0;
	return (found);
}

/*
 * Return TRUE if dot is at beginning of a word or at beginning 
 * of line, else FALSE.
 */
int
atbow(void)
{
	struct line	*lp = curwp->w_dotp;
	int		 o = curwp->w_doto;

	if (o == 0)
		return (TRUE);
	if (!inword())
		return (FALSE);
	/* the character before dot, which may take more than a byte */
	while (--o > 0 && utf8_mode && utf8_iscont(lgetc(lp, o)))
		;
	return (!utf8_isword(&lp->l_text[o], llength(lp) - o));
}

/*
 * Extract the word at dot without changing dot position.
 */
int
curtoken(int f, int n, char *token)
{
	struct line *odotp, *lp;
	int odoto, tdoto, odotline, size, r;
	char c;
	
	/* Underscore character is to be treated as "inword" while
	 * processing tokens unlike mg's default word traversal. Save
	 * and restore its cinfo value so that tag matching works for
	 * identifier with underscore.
	 */
	c = cinfo['_'];
	cinfo['_'] = _MG_W;
	
	odotp = curwp->w_dotp;
	odoto = curwp->w_doto;
	odotline = curwp->w_dotline;
	
	/* Move backward unless we are at the beginning of a word or at
	 * beginning of line.
	 */
	if (!atbow())
		if ((r = backword(f, n)) == FALSE)
			goto cleanup;

	if ((r = forwword(f, n)) == FALSE)
		goto cleanup;

	/*
	 * Back to the start of the word just passed, leaving out what
	 * came before it on the line, like a comment start.
	 */
	lp = curwp->w_dotp;
	tdoto = curwp->w_doto;
	if ((r = backword(FFRAND, 1)) == FALSE)
		goto cleanup;

	size = tdoto - curwp->w_doto;
	if (curwp->w_dotp != lp || size <= 0 || size >= MAX_TOKEN) {
		r = FALSE;
		goto cleanup;
	}
	strncpy(token, ltext(lp) + curwp->w_doto, size);
	token[size] = '\0';
	r = TRUE;
	
cleanup:
	cinfo['_'] = c;
	curwp->w_dotp = odotp;
	curwp->w_doto = odoto;
	curwp->w_dotline = odotline;
	return (r);
}

/*
 * Search tagstree for a given token.
 */
struct ctag *
searchtag(char *tok)
{
	struct ctag t, *res;

	t.tag = tok;
	if ((res = RB_FIND(tagtree, &tags, &t)) == NULL) {
		dobeep();
		ewprintf("No tag containing %s", tok);
		return (NULL);
	}
	return res;
}

/*
 * This is equivalent to filevisit from file.c.
 * Look around to see if we can find the file in another buffer; if we
 * can't find it, create a new buffer, read in the text, and switch to
 * the new buffer, in another window if other is set.
 */
int
loadbuffer(char *bname, int other)
{
	struct buffer *bufp;
	struct mgwin *wp;
	char *adjf;

	if ((adjf = adjustname(bname, TRUE)) == NULL)
		return (FALSE);
	if ((bufp = findbuffer(adjf)) == NULL)
		return (FALSE);
	wp = curwp;
	if (other) {
		/* popbuf() would find the current window showing it */
		if (bufp != curbp)
			wp = popbuf(bufp, WNONE);
		else if (wheadp->w_wndp == NULL)
			wp = splitpop(WNONE);
		else
			for (wp = wheadp; wp == curwp; wp = wp->w_wndp)
				;
		if (wp == NULL)
			return (FALSE);
	}
	curwp = wp;
	curbp = bufp;
	if (showbuffer(bufp, curwp, WFFULL) != TRUE)
		return (FALSE);
	if (bufp->b_fname[0] == '\0') {
		if (readin(adjf) != TRUE) {
			killbuffer(bufp);
			return (FALSE);
		}
	}
	return (TRUE);
}
