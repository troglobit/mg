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

struct ctag;

static int               addctag(char *);
static int               addetag(const char *, size_t, const char *,
                             const char *, size_t, int);
static int               atbow(void);
void                     closetags(void);
static int               ctagcmp(struct ctag *, struct ctag *);
static int               loadbuffer(char *);
static int               loadetags(FILE *, const char *, int);
static int               loadtags(const char *, int);
static int               pushtag(char *);
static int               searchpat(char *, int);
static struct ctag       *searchtag(char *);
static char              *strip(char *, size_t);
static void              unloadtags(void);

#define DEFAULTFN "tags"
#define ETAGSFN   "TAGS"

/*
 * ctags(1) and etags(1) entries are parsed and maintained in a tree.
 * The pattern is the start of the line the tag is defined on; etags
 * also gives its line number.
 */
struct ctag {
	RB_ENTRY(ctag) entry;
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
	char utok[MAX_TOKEN], dtok[MAX_TOKEN];
	char *tok, *bufp;
	int  ret;

	if (curtoken(f, n, dtok) == FALSE) {
		dtok[0] = '\0';
		bufp = eread("Find tag: ", utok, MAX_TOKEN, EFNUL | EFNEW);
	} else
		bufp = eread("Find tag (default %s): ", utok, MAX_TOKEN,
		    EFNUL | EFNEW, dtok);

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

	if (RB_EMPTY(&tags))
		if ((ret = tagsvisit(f, n)) != TRUE)
			return (ret);
	return pushtag(tok);
}

/*
 * Free tags tree.
 */
void
unloadtags(void)
{
	struct ctag *var, *nxt;
	
	for (var = RB_MIN(tagtree, &tags); var != NULL; var = nxt) {
		nxt = RB_NEXT(tagtree, &tags, var);
		RB_REMOVE(tagtree, &tags, var);
		/* line parsed with fparseln needs to be freed */
		free(var->tag);
		free(var);
	}
}

/*
 * Lookup tag passed in tree and if found, push current location and 
 * buffername onto stack, load the file with tag definition into a new
 * buffer and position dot at the pattern.
 */
/*ARGSUSED */
int
pushtag(char *tok)
{
	struct ctag *res;
	struct tagpos *s;
	char *bname;
	int doto, dotline, isfile;
	
	if ((res = searchtag(tok)) == NULL)
		return (FALSE);
		
	doto = curwp->w_doto;
	dotline = curwp->w_dotline;
	/* where to return: the file, or the name of a buffer with none */
	isfile = curbp->b_fname[0] != '\0';
	if ((bname = strdup(isfile ? curbp->b_fname : curbp->b_bname)) == NULL)
		return (dobeep_msg("Out of memory"));

	if (loadbuffer(res->fname) == FALSE) {
		free(bname);
		return (FALSE);
	}
	
	if (searchpat(res->pat, res->line) == TRUE) {
		if ((s = malloc(sizeof(struct tagpos))) == NULL) {
			free(bname);
			return (dobeep_msg("Out of memory"));
		}
		s->bname = bname;
		s->isfile = isfile;
		s->doto = doto;
		s->dotline = dotline;
		SLIST_INSERT_HEAD(&shead, s, entry);
		return (TRUE);
	} else {
		free(bname);
		dobeep();
		ewprintf("%s: pattern not found", res->tag);
		return (FALSE);
	}
	/* NOTREACHED */
	return (FALSE);
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
		ok = loadbuffer(s->bname);
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
	int c, s;
	
	if (!secure_allowed(fn))
		return (FALSE);
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
	if (RB_INSERT(tagtree, &tags, t) != NULL) {
		/* ignore duplicates */
		free(s);
		free(t);
	}
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
	if (RB_INSERT(tagtree, &tags, t) != NULL) {
		/* ignore duplicates */
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
	struct line *odotp;
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
		
	tdoto = curwp->w_doto;

	if ((r = forwword(f, n)) == FALSE)
		goto cleanup;
	
	/* strip away leading whitespace if any like emacs. */
	while (ltext(curwp->w_dotp) && 
	    isspace(lgetc(curwp->w_dotp, tdoto)))
		tdoto++;

	size = curwp->w_doto - tdoto;
	if (size <= 0 || size >= MAX_TOKEN || 
	    ltext(curwp->w_dotp) == NULL) {
		r = FALSE;
		goto cleanup;
	}    
	strncpy(token, ltext(curwp->w_dotp) + tdoto, size);
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
 * the new buffer.
 */
int
loadbuffer(char *bname)
{
	struct buffer *bufp;
	char *adjf;

	if ((adjf = adjustname(bname, TRUE)) == NULL)
		return (FALSE);
	if ((bufp = findbuffer(adjf)) == NULL)
		return (FALSE);
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
