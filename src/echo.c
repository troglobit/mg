/*	$OpenBSD: echo.c,v 1.69 2022/10/15 17:01:14 op Exp $	*/

/* This file is in the public domain. */

/*
 *	Echo line reading and writing.
 *
 * Common routines for reading and writing characters in the echo line area
 * of the display screen. Used by the entire known universe.
 */

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ttydef.h"
#include "def.h"
#include "funmap.h"
#include "kbd.h"
#include "key.h"
#include "macro.h"

static char	*veread(const char *, char *, size_t, int, va_list)
			__attribute__((__format__ (printf, 1, 0)));
static int	 complt(int, int, char *, size_t, int, int *);
static int	 complt_list(int, char *, int);
static int	 complt_show(struct list *);
static void	 eformat(const char *, va_list)
			__attribute__((__format__ (printf, 1, 0)));
static void	 eputi(int, int);
static void	 eputl(long, int);
static void	 eputs(const char *);
static void	 eputc(char);
static struct list	*copy_list(struct list *);
static int	 list_push(struct list **, const char *);

int		epresf = FALSE;		/* stuff in echo line flag */

static int	shortanswers = TRUE;	/* y/n answers yes/no prompts */

/*
 * Erase the echo line.
 */
void
eerase(void)
{
	ttcolor(CTEXT);
	ttmove(nrow - 1, 0);
	tteeol();
	ttflush();
	epresf = FALSE;
#ifdef ENABLE_HELP
	if (helpsh)
		ewprintf(" %s", hlp);
#endif
}

/*
 * Ask a "yes" or "no" question.  Return ABORT if the user answers the
 * question with the abort ("^G") character.  Return FALSE for "no" and
 * TRUE for "yes".  No formatting services are available.  No newline
 * required.
 */
int
eyorn(const char *sp)
{
	int	 s;

	if (inmacro)
		return (TRUE);

	ewprintf("%s? (y or n) ", sp);
	for (;;) {
		s = getkey(FALSE);
		if (s == 'y' || s == 'Y' || s == ' ') {
			eerase();
			return (TRUE);
		}
		if (s == 'n' || s == 'N' || s == CCHR('M')) {
			eerase();
			return (FALSE);
		}
		if (s == CCHR('G')) {
			eerase();
			return (ctrlg(FFRAND, 1));
		}
		ewprintf("Please answer y or n.  %s? (y or n) ", sp);
	}
	/* NOTREACHED */
}

/*
 * Ask a "yes", "no" or "revert" question.  Return ABORT if the user answers
 * the question with the abort ("^G") character.  Return FALSE for "no",
 * TRUE for "yes" and REVERT for "revert". No formatting services are
 * available.  No newline required.
 */
int
eynorr(const char *sp)
{
	int	 s;

	if (inmacro)
		return (TRUE);

	ewprintf("%s? (y, n or r) ", sp);
	for (;;) {
		s = getkey(FALSE);
		if (s == 'y' || s == 'Y' || s == ' ') {
			eerase();
			return (TRUE);
		}
		if (s == 'n' || s == 'N' || s == CCHR('M')) {
			eerase();
			return (FALSE);
		}
		if (s == 'r' || s == 'R') {
			eerase();
			return (REVERT);
		}
		if (s == CCHR('G')) {
			eerase();
			return (ctrlg(FFRAND, 1));
		}
		ewprintf("Please answer y, n or r.");
	}
	/* NOTREACHED */
}

/*
 * Like eyorn, but for more important questions.  User must type all of
 * "yes" or "no" and the trailing newline.
 */
int
eyesno(const char *sp)
{
	char	 buf[64], *rep;

	if (inmacro)
		return (TRUE);
	if (shortanswers)
		return (eyorn(sp));

	rep = eread("%s? (yes or no) ", buf, sizeof(buf),
	    EFNUL | EFNEW | EFCR, sp);
	for (;;) {
		if (rep == NULL) {
			eerase();
			return (ABORT);
		}
		if (rep[0] != '\0') {
			if (macrodef) {
				struct line	*lp = maclcur;

				maclcur = lp->l_bp;
				maclcur->l_fp = lp->l_fp;
				free(lp);
			}
			if (strcasecmp(rep, "yes") == 0) {
				eerase();
				return (TRUE);
			}
			if (strcasecmp(rep, "no") == 0) {
				eerase();
				return (FALSE);
			}
		}
		rep = eread("Please answer yes or no.  %s? (yes or no) ",
		    buf, sizeof(buf), EFNUL | EFNEW | EFCR, sp);
	}
	/* NOTREACHED */
}

/*
 * Toggle short answers to yes-or-no prompts, like use-short-answers
 * in GNU Emacs: a single y or n instead of spelling it out.
 */
int
useshortanswers(int f, int n)
{
	if (f & FFARG)
		shortanswers = n > 0;
	else
		shortanswers = !shortanswers;
	ewprintf("Short answers %sabled", shortanswers ? "en" : "dis");

	return (TRUE);
}

/*
 * This is the general "read input from the echo line" routine.  The basic
 * idea is that the prompt string "prompt" is written to the echo line, and
 * a one line reply is read back into the supplied "buf" (with maximum
 * length "len").
 * XXX: When checking for an empty return value, always check rep, *not* buf
 * as buf may be freed in pathological cases.
 */
char *
eread(const char *fmt, char *buf, size_t nbuf, int flag, ...)
{
	va_list	 ap;
	char	*rep;

	va_start(ap, flag);
	rep = veread(fmt, buf, nbuf, flag, ap);
	va_end(ap);
	return (rep);
}

/*
 * The line being read in the echo area.  veread() is not reentrant,
 * so the helpers below work on this one instance.
 */
static struct {
	char	*buf;
	size_t	 nbuf;
	int	 cpos, epos;		/* cursor and end position in buf */
	int	 dynbuf;		/* buf is ours to grow */
	int	 lo;			/* where the editable text starts */
	int	 ido;			/* ido-mode is on at this prompt */
	int	 stale;			/* the matches shown are out of date */
} mb;

/*
 * Columns c takes on the echo line, as eputc() draws it: none for
 * the continuation byte of a UTF-8 sequence, two for ^X.
 */
static int
mbwidth(int c)
{
	if (utf8_mode && utf8_iscont(c))
		return (0);
	return (ISCTRL(c) ? 2 : 1);
}

/*
 * Redraw the line from the cursor on, and put the cursor back.
 */
static void
mbredraw(void)
{
	int	 i, rr, cc;

	rr = ttrow;
	cc = ttcol;
	tteeol();
	for (i = mb.cpos; i < mb.epos; i++)
		eputc(mb.buf[i]);
	ttmove(rr, cc);
	mb.stale = TRUE;
}

/*
 * Where the character before, or after, byte i of the line starts,
 * and whether the one at i is part of a word.
 */
static int
mbprevchar(int i)
{
	while (--i > mb.lo && mbwidth(mb.buf[i]) == 0)
		;
	return (i);
}

static int
mbnextchar(int i)
{
	while (++i < mb.epos && mbwidth(mb.buf[i]) == 0)
		;
	return (i);
}

static int
mbisword(int i)
{
	return (utf8_isword(mb.buf + i, mb.epos - i));
}

/*
 * Move the cursor one character left or right.
 */
static void
mbleft(void)
{
	int	 w;

	if (mb.cpos == mb.lo)
		return;
	mb.cpos = mbprevchar(mb.cpos);
	w = mbwidth(mb.buf[mb.cpos]);
	while (w-- > 0) {
		ttputc('\b');
		--ttcol;
	}
}

/*
 * Bytes in the character under the cursor.
 */
static int
mbcharlen(void)
{
	return (mb.cpos < mb.epos ? mbnextchar(mb.cpos) - mb.cpos : 0);
}

static void
mbright(void)
{
	int	 n;

	for (n = mbcharlen(); n > 0; n--)
		eputc(mb.buf[mb.cpos++]);
}

static void
mbgoto(int pos)
{
	while (mb.cpos > pos)
		mbleft();
	while (mb.cpos < pos)
		mbright();
}

/*
 * Where the word before the cursor starts, and the one after it ends.
 */
static int
mbprevword(void)
{
	int	 i = mb.cpos, j;

	while (i > mb.lo && !mbisword(j = mbprevchar(i)))
		i = j;
	while (i > mb.lo && mbisword(j = mbprevchar(i)))
		i = j;
	return (i);
}

static int
mbnextword(void)
{
	int	 i = mb.cpos;

	while (i < mb.epos && !mbisword(i))
		i = mbnextchar(i);
	while (i < mb.epos && mbisword(i))
		i = mbnextchar(i);
	return (i);
}

/*
 * Delete n bytes at the cursor.
 */
static void
mbdelete(int n)
{
	memmove(mb.buf + mb.cpos, mb.buf + mb.cpos + n, mb.epos - mb.cpos - n);
	mb.epos -= n;
	mbredraw();
}

/*
 * Delete between the cursor and pos, on either side of it.
 */
static void
mbdelto(int pos)
{
	int	 n = mb.cpos - pos;

	if (n < 0) {
		mbdelete(-n);
		return;
	}
	mbgoto(pos);
	mbdelete(n);
}

/*
 * Insert the n bytes at s at the cursor: FALSE when they do not fit
 * the line, ABORT when out of memory.
 */
static int
mbinsert(const char *s, int n)
{
	int	 i;

	if (mb.buf == NULL || (size_t)mb.epos + n >= mb.nbuf) {
		void	*newp;
		size_t	 newsize = mb.epos + mb.epos + n + 16;

		if (!mb.dynbuf)
			return (FALSE);
		if ((newp = realloc(mb.buf, newsize)) == NULL)
			return (ABORT);
		mb.buf = newp;
		mb.nbuf = newsize;
	}
	memmove(mb.buf + mb.cpos + n, mb.buf + mb.cpos, mb.epos - mb.cpos);
	memcpy(mb.buf + mb.cpos, s, n);
	mb.epos += n;
	for (i = 0; i < n; i++)
		eputc(mb.buf[mb.cpos++]);
	mbredraw();
	return (TRUE);
}

static int
mbinsertc(int c)
{
	char	 ch = c;

	return (mbinsert(&ch, 1));
}

/*
 * Replace the text with s.  Returns what mbinsert() does.
 */
static int
mbset(const char *s)
{
	mbgoto(mb.lo);
	mbdelto(mb.epos);
	return (mbinsert(s, strlen(s)));
}

#ifdef ENABLE_IDO
/*
 * The text after the directory of a file prompt, NUL terminated.
 */
static char *
mbtext(void)
{
	mb.buf[mb.epos] = '\0';
	return (mb.buf + mb.lo);
}

/*
 * The column after the text, and drawing pre, emph marked, and post
 * there with the cursor put back.
 */
static int
mbendcol(void)
{
	int	 i, w = ttcol;

	for (i = mb.cpos; i < mb.epos; i++)
		w += mbwidth(mb.buf[i]);
	return (w);
}

static void
mbtail(const char *pre, const char *emph, const char *post)
{
	int	 rr = ttrow, cc = ttcol;

	ttmove(rr, mbendcol());
	tteeol();
	if (pre != NULL)
		eputs(pre);
	if (emph != NULL) {
		ttemph(TRUE);
		eputs(emph);
		ttemph(FALSE);
	}
	if (post != NULL)
		eputs(post);
	ttmove(rr, cc);
}

/*
 * In ido-mode, match the text after the directory and draw the
 * matches after it, the selected one marked, when they changed.
 */
static void
mbido(void)
{
	char	 list[ncol + 1];
	const char	*sel;
	int	 col;

	if (ido_match(mbtext()) == TRUE)
		mb.stale = TRUE;
	if (!mb.stale)
		return;
	mb.stale = FALSE;
	if ((sel = ido_selected()) == NULL) {
		mbtail(" [No match]", NULL, NULL);
		return;
	}
	col = mbendcol() + 2 + strlen(sel);
	ido_list(list, ncol > col ? ncol - col : 1);
	mbtail(ido_nmatch() == 1 ? " [" : " {", sel, list);
}

static void
mbrotate(int dir)
{
	ido_rotate(dir);
	mb.stale = TRUE;
}

/*
 * Replace the directory a file prompt shows before the text, and the
 * text.  Returns what mbinsert() does.
 */
static int
mbsetdir(const char *dir, const char *text)
{
	int	 y;

	mb.lo = 0;
	if ((y = mbset(dir)) != TRUE)
		return (y);
	mb.lo = mb.epos;
	return (mbinsert(text, strlen(text)));
}

/*
 * After a key in ido-mode: a slash in the text of a file prompt
 * moves it to that directory, then the matches are redrawn.  Returns
 * what mbinsert() does.
 */
static int
mbidoupdate(int flag)
{
	char	 text[NFILEN];
	int	 y = TRUE;

	if ((flag & EFFILE) != 0 &&
	    strlcpy(text, mbtext(), sizeof(text)) < sizeof(text) &&
	    ido_slash(text) == TRUE)
		y = mbsetdir(ido_dir(), text);
	if (y == TRUE)
		mbido();
	return (y);
}

/* The matches as a list, for the *Completions* window. */
static struct list *
mbidolist(void)
{
	struct list	*lh = NULL;
	int		 i;

	for (i = 0; i < ido_nmatch(); i++)
		if (list_push(&lh, ido_matchname(i)) == FALSE)
			break;
	return (lh);
}

/*
 * The keys ido-mode takes at a prompt.  Returns what mbinsert() does,
 * or MBPASS for a key the editor gets.
 */
#define MBPASS	(-1)

static int
mbidokey(int *c, int flag, int *cwin)
{
	const char	*sel;
	char	 name[NFILEN];
	int	 y;

	switch (*c) {
	case CCHR('S'):
	case CCHR('R'):
		mbrotate(*c == CCHR('S') ? 1 : -1);
		return (TRUE);
	case CCHR('D'):			/* nothing typed: the directory itself */
		if ((flag & EFFILE) != 0 && mb.epos == mb.lo)
			*c = CCHR('M');
		return (MBPASS);
	case CCHR('F'):			/* at the end: plain completion */
		if (mb.cpos < mb.epos)
			return (MBPASS);
		mb.ido = FALSE;
		mb.lo = 0;
		ido_stop();
		mbtail(NULL, NULL, NULL);
		return (TRUE);
	case CCHR('M'):
		if ((sel = ido_selected()) == NULL ||
		    (mb.epos == mb.lo && !ido_rotated() && (flag & EFFILE) == 0))
			return (MBPASS);
		if ((flag & EFFILE) != 0 && ido_enter(sel) == TRUE)
			return (mbsetdir(ido_dir(), ""));
		if ((y = mbset(sel)) != TRUE)
			return (y);
		return (MBPASS);	/* RET takes the match as typed */
	case CCHR('I'):
		if (ido_nmatch() == 1)
			return (mbset(ido_selected()));
		if (ido_common(mbtext(), name, sizeof(name)))
			return (mbset(name));
		if (ido_nmatch() > 1) {
			complt_show(mbidolist());
			*cwin = TRUE;
		}
		return (TRUE);
	case CCHR('H'):
	case CCHR('?'):
		if ((flag & EFFILE) == 0 || mb.epos > mb.lo)
			return (MBPASS);
		if (ido_updir() == TRUE)
			return (mbsetdir(ido_dir(), ""));
		dobeep();
		return (TRUE);
	}
	return (MBPASS);
}
#endif /* ENABLE_IDO */

/*
 * What was typed at earlier prompts, newest last, one list per kind
 * of prompt: commands, buffers and files by their flag, any other
 * prompt by its text.
 */
#define HISTLEN	32
struct hist {
	const char	*key;
	char		*line[HISTLEN];
	int		 n;
	struct hist	*next;
};
static struct hist *hists;

/*
 * The list being walked at the prompt: pos is the line shown, n one
 * past the newest, where cur holds what was typed before walking off.
 */
static struct {
	struct hist	*h;
	int		 pos;
	char		*cur;
} mbh;

/*
 * The list for a prompt.  A prompt that names a default, as in
 * "Find tag (default %s): ", shares the list of "Find tag: ".
 */
static struct hist *
histfind(const char *fp, int flag)
{
	struct hist	*h;
	const char	*key, *dflt;
	size_t		 len;

	if (flag & EFFUNC)
		key = "M-x";
	else if (flag & EFBUF)
		key = "buffer";
	else if (flag & EFFILE)
		key = "file";
	else
		key = fp;
	if ((dflt = strstr(key, " (default")) != NULL)
		len = dflt - key;
	else if ((len = strlen(key)) > 2 && strcmp(key + len - 2, ": ") == 0)
		len -= 2;
	for (h = hists; h != NULL; h = h->next)
		if (strncmp(h->key, key, len) == 0 && h->key[len] == '\0')
			return (h);
	if ((h = calloc(1, sizeof(*h))) == NULL ||
	    (h->key = strndup(key, len)) == NULL) {
		free(h);
		return (NULL);
	}
	h->next = hists;
	hists = h;
	return (h);
}

/*
 * Remember s as the newest line of h, unless it is empty or the same
 * as the one before.
 */
static void
histadd(struct hist *h, const char *s)
{
	if (h == NULL || *s == '\0' ||
	    (h->n > 0 && strcmp(h->line[h->n - 1], s) == 0))
		return;
	if (h->n == HISTLEN) {
		free(h->line[0]);
		memmove(h->line, h->line + 1, --h->n * sizeof(*h->line));
	}
	if ((h->line[h->n] = strdup(s)) != NULL)
		h->n++;
}

/*
 * Show the line dir (-1 or 1) steps away in the history, keeping
 * what was typed so walking back down restores it.  Returns what
 * mbinsert() does.
 */
static int
mbhist(int dir)
{
	int	 pos = mbh.pos + dir;

	if (mbh.h == NULL || pos < 0 || pos > mbh.h->n) {
		dobeep();
		return (TRUE);
	}
	if (mbh.pos == mbh.h->n) {
		free(mbh.cur);
		if ((mbh.cur = strndup(mb.buf + mb.lo, mb.epos - mb.lo)) == NULL)
			return (ABORT);
	}
	mbh.pos = pos;
	return (mbset(pos == mbh.h->n ? mbh.cur : mbh.h->line[pos]));
}

/*
 * The history commands: they have an effect at a prompt only, where
 * mbcommand() sees them by name.
 */
int
prevhist(int f, int n)
{
	dobeep();
	return (FALSE);
}

int
nexthist(int f, int n)
{
	dobeep();
	return (FALSE);
}

/*
 * Insert the first line of the kill buffer at the cursor, gathered
 * to be drawn once.  Returns what mbinsert() does.
 */
static int
mbyank(void)
{
	int	 c, n;

	for (n = 0; (c = kremove(n)) >= 0 && c != *curbp->b_nlchr; n++)
		;
	{
		char	 kill[n + 1];

		for (c = 0; c < n; c++)
			kill[c] = kremove(c);
		return (mbinsert(kill, n));
	}
}

/*
 * Read the key sequence c begins through the fundamental map and
 * return the command it is bound to, rescan for none; c is left at
 * the last byte read.  This is how the terminal's arrow, Home, End
 * and Delete keys, and whatever the user bound, reach the echo line.
 */
static PF
mbkey(int *c)
{
	KEYMAP	*map = fundamental_map;
	PF	 funct;
	int	 esc, csi = 0;

	esc = (*c == CCHR('['));
	while ((funct = doscan(map, *c, &map)) == NULL) {
		*c = getkey(FALSE);
		if (esc)
			csi = (*c == '[');
		esc = 0;
	}
	/* swallow the rest of a CSI sequence nothing is bound to */
	if (funct == rescan && csi)
		while (*c < 0x40 || *c > 0x7e)
			*c = getkey(FALSE);
	return (funct);
}

/*
 * Do on the echo line what the editor command funct, reached by the
 * key c, does in a buffer.  Returns what mbinsert() does.
 */
static int
mbcommand(PF funct, int c)
{
	if (funct == selfinsert)
		return (mbinsertc(c));
	if (funct == yank)
		return (mbyank());
	if (funct == backchar)
		mbleft();
	else if (funct == forwchar)
		mbright();
	else if (funct == gotobol)
		mbgoto(mb.lo);
	else if (funct == gotoeol)
		mbgoto(mb.epos);
	else if (funct == backword)
		mbgoto(mbprevword());
	else if (funct == forwword)
		mbgoto(mbnextword());
	else if (funct == backdel) {
		if (mb.cpos > mb.lo) {
			mbleft();
			mbdelete(mbcharlen());
		}
	} else if (funct == forwdel)
		mbdelete(mbcharlen());
	else if (funct == delbword)
		mbdelto(mbprevword());
	else if (funct == delfword)
		mbdelto(mbnextword());
	else if (funct == backline || funct == prevhist)
		return (mbhist(-1));
	else if (funct == forwline || funct == nexthist)
		return (mbhist(1));
	else
		dobeep();
	return (TRUE);
}

static char *
veread(const char *fp, char *buf, size_t nbuf, int flag, va_list ap)
{
	int	 c, i, y;
	int	 cplflag;		/* display completion list */
	PF	 funct;
	int	 cwin = FALSE;		/* completion list created */
	struct buffer	*bp;			/* completion list buffer */
	struct mgwin	*wp;			/* window for compl list */
	char	*ret;			/* return value */
#ifdef ENABLE_IDO
	char	 text[NFILEN];
	int	 esc;
#endif

	static char emptyval[] = "";	/* XXX hackish way to return err msg*/

	if (inmacro) {
		if (buf == NULL) {
			if ((buf = malloc(maclcur->l_used + 1)) == NULL)
				return (NULL);
		} else if ((size_t)maclcur->l_used >= nbuf)
			return (NULL);
		bcopy(maclcur->l_text, buf, maclcur->l_used);
		buf[maclcur->l_used] = '\0';
		maclcur = maclcur->l_fp;
		return (buf);
	}
	mb.buf = buf;
	mb.nbuf = nbuf;
	mb.dynbuf = (buf == NULL);
	mb.epos = mb.cpos = 0;
	mbh.h = histfind(fp, flag);
	mbh.pos = mbh.h != NULL ? mbh.h->n : 0;
	free(mbh.cur);
	mbh.cur = NULL;
	cplflag = FALSE;

	if ((flag & EFNEW) != 0 || ttrow != nrow - 1) {
		ttcolor(CTEXT);
		ttmove(nrow - 1, 0);
		epresf = TRUE;
	} else
		eputc(' ');
	eformat(fp, ap);
	mb.lo = 0;
	mb.ido = FALSE;
#ifdef ENABLE_IDO
	mb.ido = ido_mode && (flag & EFAUTO) != 0 && buf != NULL;
	if (mb.ido) {
		strlcpy(text, (flag & EFDEF) != 0 ? buf : "", sizeof(text));
		if (ido_start(flag, text) != TRUE)
			mb.ido = FALSE;
	}
#endif
	if ((flag & EFDEF) != 0 && !mb.ido) {
		if (buf == NULL)
			return (NULL);
		eputs(buf);
		mb.epos = mb.cpos += strlen(buf);
	}
	tteeol();
#ifdef ENABLE_IDO
	if (mb.ido) {
		/* the directory of a file prompt first, read only */
		if (mbsetdir(ido_dir(), text) == TRUE)
			mbido();
		else {
			mb.ido = FALSE;
			ido_stop();
		}
	}
#endif
	ttflush();
	for (;;) {
		c = getkey(FALSE);
#ifdef ENABLE_IDO
		if (mb.ido && (y = mbidokey(&c, flag, &cwin)) != MBPASS)
			goto next;
#endif
		y = TRUE;
		if ((flag & EFAUTO) != 0 && c == CCHR('I')) {
			if (mb.buf == NULL)
				goto memfail;

			if (cplflag == TRUE) {
				complt_list(flag, mb.buf, mb.cpos);
				cwin = TRUE;
			} else if (complt(flag, c, mb.buf, mb.nbuf, mb.epos,
			    &i) == TRUE) {
				cplflag = TRUE;
				mb.epos += i;
				mb.cpos = mb.epos;
			}
			continue;
		}
		cplflag = FALSE;

		switch (c) {
		case CCHR('K'):			/* copy here-EOL to kill buffer */
			kdelete();
			kchunk(mb.buf + mb.cpos, mb.epos - mb.cpos, KFORW);
			mbdelto(mb.epos);
			break;

		case CCHR('J'):
			c = CCHR('M');
			/* fallthrough */

		case CCHR('M'):			/* return, done */
			/* if there's nothing in the minibuffer, abort */
			if (mb.epos == 0 && !(flag & EFNUL)) {
				(void)ctrlg(FFRAND, 0);
				ttflush();
				if (mb.dynbuf)
					free(mb.buf);
				return (NULL);
			}
			if ((flag & EFFUNC) != 0) {
				if (mb.buf == NULL)
					goto memfail;
				if (complt(flag, c, mb.buf, mb.nbuf, mb.epos, &i)
				    == FALSE)
					continue;
				if (i > 0)
					mb.epos += i;
			}
			if (mb.buf != NULL) {
				mb.buf[mb.epos] = '\0';
				histadd(mbh.h, mb.buf);
			}
			if ((flag & EFCR) != 0) {
				ttputc(CCHR('M'));
				ttflush();
			}
			if (macrodef) {
				struct line	*lp;

				if ((lp = lalloc(mb.cpos)) == NULL)
					goto memfail;
				lp->l_fp = maclcur->l_fp;
				maclcur->l_fp = lp;
				lp->l_bp = maclcur;
				maclcur = lp;
				bcopy(mb.buf, lp->l_text, mb.cpos);
			}
			ret = mb.buf;
			goto done;

		case CCHR('G'):			/* bell, abort */
			eputc(CCHR('G'));
			(void)ctrlg(FFRAND, 0);
			ttflush();
			ret = NULL;
			goto done;

		case CCHR('H'):			/* rubout, erase */
			y = mbcommand(backdel, c);
			break;

		case CCHR('X'):			/* kill line */
			/* fallthrough */
		case CCHR('U'):
			mbdelto(mb.lo);
			break;

		case CCHR('W'):			/* kill to beginning of word */
			mbdelto(mbprevword());
			break;

		case CCHR('\\'):
			/* fallthrough */
		case CCHR('Q'):			/* quote next */
			y = mbinsertc(getkey(FALSE));
			break;

		default:			/* as bound in the editor */
#ifdef ENABLE_IDO
			esc = (c == CCHR('['));
#endif
			funct = mbkey(&c);
#ifdef ENABLE_IDO
			/* the arrow keys step through the matches in ido-mode */
			if (mb.ido && esc &&
			    (funct == forwchar || funct == backchar)) {
				mbrotate(funct == forwchar ? 1 : -1);
				break;
			}
#endif
			y = mbcommand(funct, c);
		}
#ifdef ENABLE_IDO
next:
		if (y == TRUE && mb.ido)
			y = mbidoupdate(flag);
#endif
		if (y == ABORT)
			goto memfail;
		if (y == FALSE)
			goto toolong;
		ttflush();
		continue;
toolong:
		dobeep_msg("Line too long. Press Control-g to escape.");
	}
done:
#ifdef ENABLE_IDO
	ido_stop();
#endif
	if (cwin == TRUE) {
		/* blow away cpltion window */
		bp = bfind("*Completions*", TRUE);
		if ((wp = popbuf(bp, WEPHEM)) != NULL) {
			if (wp->w_flag & WEPHEM) {
				curwp = wp;
				delwind(FFRAND, 1);
			} else {
				killbuffer(bp);
			}
		}
	}
	return (ret);
memfail:
#ifdef ENABLE_IDO
	ido_stop();
#endif
	if (mb.dynbuf)
		free(mb.buf);
	dobeep();
	ewprintf("Out of memory");
	return (emptyval);
}

/*
 * Do completion on a list of objects.
 * c is SPACE, TAB, or CR
 * return TRUE if matched (or partially matched)
 * FALSE is result is ambiguous,
 * ABORT on error.
 */
static int
complt(int flags, int c, char *buf, size_t nbuf, int cpos, int *nx)
{
	struct list	*lh, *lh2;
	struct list	*wholelist = NULL;
	int	 i, nxtra, nhits, bxtra, msglen, nshown;
	int	 wflag = FALSE;
	char	*msg;

	lh = lh2 = NULL;

	if ((flags & EFFUNC) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_function_list(buf);
	} else if ((flags & EFBUF) != 0) {
		lh = &(bheadp->b_list);
	} else if ((flags & EFFILE) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = make_file_list(buf);
#ifdef ENABLE_CTAGS
	} else if ((flags & EFTAG) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_tag_list(buf);
#endif
	} else
		panic("broken complt call: flags");

	if (c == ' ')
		wflag = TRUE;
	else if (c != '\t' && c != CCHR('M'))
		panic("broken complt call: c");

	nhits = 0;
	nxtra = HUGE;

	for (; lh != NULL; lh = lh->l_next) {
		if (strncmp(buf, lh->l_name, cpos) != 0)
			continue;
		if (nhits == 0)
			lh2 = lh;
		++nhits;
		if (lh->l_name[cpos] == '\0')
			nxtra = -1; /* exact match */
		else {
			bxtra = getxtra(lh, lh2, cpos, wflag);
			if (bxtra < nxtra)
				nxtra = bxtra;
			lh2 = lh;
		}
	}
	if (nhits == 0)
		msg = " [No match]";
	else if (nhits > 1 && nxtra == 0)
		msg = " [Ambiguous. Ctrl-G to cancel]";
	else {
		/*
		 * Being lazy - ought to check length, but all things
		 * autocompleted have known types/lengths.
		 */
		if (nxtra < 0 && nhits > 1 && c == ' ')
			nxtra = 1; /* ??? */
		/* a completion too long for buf is cut, leaving the NUL room */
		for (i = 0; i < nxtra && (size_t)cpos + 1 < nbuf; ++i) {
			buf[cpos] = lh2->l_name[cpos];
			eputc(buf[cpos++]);
		}
		if (nxtra > 0)
			nxtra = i;
		ttflush();
		free_file_list(wholelist);
		*nx = nxtra;
		if (nxtra < 0 && c != CCHR('M')) /* exact */
			*nx = 0;
		return (TRUE);
	}

	/*
	 * wholelist is NULL if we are doing buffers.  Want to free lists
	 * that were created for us, but not the buffer list!
	 */
	free_file_list(wholelist);

	/* Set up backspaces, etc., being mindful of echo line limit. */
	msglen = strlen(msg);
	nshown = (ttcol + msglen + 2 > ncol) ?
		ncol - ttcol - 2 : msglen;
	eputs(msg);
	ttcol -= (i = nshown);	/* update ttcol!		 */
	while (i--)		/* move back before msg		 */
		ttputc('\b');
	ttflush();		/* display to user		 */
	i = nshown;
	while (i--)		/* blank out on next flush	 */
		eputc(' ');
	ttcol -= (i = nshown);	/* update ttcol on BS's		 */
	while (i--)
		ttputc('\b');	/* update ttcol again!		 */
	*nx = nxtra;
	return ((nhits > 0) ? TRUE : FALSE);
}

/*
 * Do completion on a list of objects, listing instead of completing.
 */
static int
complt_list(int flags, char *buf, int cpos)
{
	struct list	*lh = NULL, *ml = NULL;
	struct list	*wholelist = NULL;
	int	 preflen = 0;
	char	*cp;

	/*
	 * First get the list of objects.  This list may contain only
	 * the ones that complete what has been typed, or may be the
	 * whole list of all objects of this type.  They are filtered
	 * below in any case.  The buffer list is copied, since the
	 * sort would otherwise reorder the main buffer list.
	 */
	if ((flags & EFBUF) != 0)
		wholelist = lh = copy_list(&(bheadp->b_list));
	else if ((flags & EFFUNC) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_function_list(buf);
	} else if ((flags & EFFILE) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = make_file_list(buf);
		/*
		 * We don't want to display stuff up to the / for file
		 * names preflen is the list of a prefix of what the
		 * user typed that should not be displayed.
		 */
		cp = strrchr(buf, '/');
		if (cp)
			preflen = cp - buf + 1;
#ifdef ENABLE_CTAGS
	} else if ((flags & EFTAG) != 0) {
		buf[cpos] = '\0';
		wholelist = lh = complete_tag_list(buf);
#endif
	} else
		panic("broken complt call: flags");

	for (; lh != NULL; lh = lh->l_next) {
		if (strncmp(buf, lh->l_name, cpos) == 0 &&
		    list_push(&ml, lh->l_name + preflen) == FALSE)
			break;
	}
	free_file_list(wholelist);

	return (complt_show(ml));
}

/*
 * Show the names in lh, sorted, in columns, in a *Completions*
 * window, and free the list.
 */
static int
complt_show(struct list *lh)
{
	struct list	*lh2, *lh3;
	struct buffer	*bp;
	int	 maxwidth, width;
	int	 oldrow = ttrow;
	int	 oldcol = ttcol;
	int	 oldhue = tthue;
	char	 *linebuf;
	size_t	 linesize, len;
	char *cp;

	ttflush();

	/* The results are put into a completion buffer. */
	bp = bfind("*Completions*", TRUE);
	if (bclear(bp) == FALSE) {
		free_file_list(lh);
		return (FALSE);
	}
	bp->b_flag |= BFREADONLY;

	/*
	 * Sort the list, since users expect to see it in alphabetic
	 * order.
	 */
	lh2 = lh;
	while (lh2 != NULL) {
		lh3 = lh2->l_next;
		while (lh3 != NULL) {
			if (strcmp(lh2->l_name, lh3->l_name) > 0) {
				cp = lh2->l_name;
				lh2->l_name = lh3->l_name;
				lh3->l_name = cp;
			}
			lh3 = lh3->l_next;
		}
		lh2 = lh2->l_next;
	}

	/*
	 * First find max width of object to be displayed, so we can
	 * put several on a line.
	 */
	maxwidth = 0;
	for (lh2 = lh; lh2 != NULL; lh2 = lh2->l_next) {
		width = strlen(lh2->l_name);
		if (width > maxwidth)
			maxwidth = width;
	}
	maxwidth += 1;

	/*
	 * Now do the display.  Objects are written into linebuf until
	 * it fills, and then put into the help buffer.
	 */
	linesize = (ncol > maxwidth ? ncol : maxwidth) + 1;
	if ((linebuf = malloc(linesize)) == NULL) {
		free_file_list(lh);
		return (FALSE);
	}
	width = 0;

	/*
	 * We're going to strlcat() into the buffer, so it has to be
	 * NUL terminated.
	 */
	linebuf[0] = '\0';
	for (lh2 = lh; lh2 != NULL; lh2 = lh2->l_next) {
		/* if it wraps */
		if ((width + maxwidth) > ncol) {
			addline(bp, linebuf);
			linebuf[0] = '\0';
			width = 0;
		}
		len = strlcat(linebuf, lh2->l_name, linesize);
		width += maxwidth;
		if (len < (size_t)width && (size_t)width < linesize) {
			/* pad so the objects nicely line up */
			memset(linebuf + len, ' ',
			    maxwidth - strlen(lh2->l_name));
			linebuf[width] = '\0';
		}
	}
	if (width > 0)
		addline(bp, linebuf);
	free(linebuf);
	free_file_list(lh);

	popbuftop(bp, WEPHEM);	/* split the screen and put up the help
				 * buffer */
	update(CMODE);		/* needed to make the new stuff actually
				 * appear */
	ttmove(oldrow, oldcol);	/* update leaves cursor in arbitrary place */
	ttcolor(oldhue);	/* with arbitrary color */
	ttflush();
	return (0);
}

/*
 * The "lp1" and "lp2" point to list structures.  The "cpos" is a horizontal
 * position in the name.  Return the longest block of characters that can be
 * autocompleted at this point.  Sometimes the two symbols are the same, but
 * this is normal.
 */
int
getxtra(struct list *lp1, struct list *lp2, int cpos, int wflag)
{
	int	i;

	i = cpos;
	for (;;) {
		if (lp1->l_name[i] != lp2->l_name[i])
			break;
		if (lp1->l_name[i] == '\0')
			break;
		++i;
		if (wflag && !ISWORD(lp1->l_name[i - 1]))
			break;
	}
	return (i - cpos);
}

/*
 * Special "printf" for the echo line.  Each call to "ewprintf" starts a
 * new line in the echo area, and ends with an erase to end of the echo
 * line.  The formatting is done by a call to the standard formatting
 * routine.
 */
void
ewprintf(const char *fmt, ...)
{
	va_list	 ap;

	if (inmacro)
		return;

	va_start(ap, fmt);
	ttcolor(CTEXT);
	ttmove(nrow - 1, 0);
	eformat(fmt, ap);
	va_end(ap);
	tteeol();
	ttflush();
	epresf = TRUE;
}

/*
 * Printf style formatting. This is called by "ewprintf" to provide
 * formatting services to its clients.  The move to the start of the
 * echo line, and the erase to the end of the echo line, is done by
 * the caller. 
 * %c prints the "name" of the supplied character.
 * %k prints the name of the current key (and takes no arguments).
 * %d prints a decimal integer
 * %o prints an octal integer
 * %p prints a pointer
 * %s prints a string
 * %ld prints a long word
 * Anything else is echoed verbatim
 */
static void
eformat(const char *fp, va_list ap)
{
	char	kname[NKNAME], tmp[100];
	int	c;

	while ((c = *fp++) != '\0') {
		if (c != '%')
			eputc(c);
		else {
			c = *fp++;
			switch (c) {
			case 'c':
				getkeyname(kname, sizeof(kname),
				    va_arg(ap, int));
				eputs(kname);
				break;

			case 'k':
				keyseqname(kname, sizeof(kname), key.k_chars,
				    key.k_count);
				eputs(kname);
				break;

			case 'd':
				eputi(va_arg(ap, int), 10);
				break;

			case 'o':
				eputi(va_arg(ap, int), 8);
				break;

			case 'p':
				snprintf(tmp, sizeof(tmp), "%p",
				    va_arg(ap, void *));
				eputs(tmp);
				break;

			case 's':
				eputs(va_arg(ap, char *));
				break;

			case 'l':
				/* explicit longword */
				c = *fp++;
				switch (c) {
				case 'd':
					eputl(va_arg(ap, long), 10);
					break;
				default:
					eputc(c);
					break;
				}
				break;

			default:
				eputc(c);
			}
		}
	}
}

/*
 * Put integer, in radix "r".
 */
static void
eputi(int i, int r)
{
	int	 q;

	if (i < 0) {
		eputc('-');
		i = -i;
	}
	if ((q = i / r) != 0)
		eputi(q, r);
	eputc(i % r + '0');
}

/*
 * Put long, in radix "r".
 */
static void
eputl(long l, int r)
{
	long	 q;

	if (l < 0) {
		eputc('-');
		l = -l;
	}
	if ((q = l / r) != 0)
		eputl(q, r);
	eputc((int)(l % r) + '0');
}

/*
 * Put string.
 */
static void
eputs(const char *s)
{
	int	 c;

	while ((c = *s++) != '\0')
		eputc(c);
}

/*
 * Put character.  Watch for control characters, and for the line getting
 * too long.
 */
static void
eputc(char c)
{
	if (ttcol + 2 < ncol) {
		if (ISCTRL(c)) {
			eputc('^');
			c = CCHR(c);
		}
		ttputc(c);
		if (!utf8_mode || !utf8_iscont(c))
			++ttcol;
	}
}

void
free_file_list(struct list *lp)
{
	struct list	*next;

	while (lp) {
		next = lp->l_next;
		free(lp->l_name);
		free(lp);
		lp = next;
	}
}

/* Put a copy of name at the head of the list; FALSE when out of memory. */
static int
list_push(struct list **lh, const char *name)
{
	struct list	*lp;

	if ((lp = malloc(sizeof(*lp))) == NULL)
		return (FALSE);
	if ((lp->l_name = strdup(name)) == NULL) {
		free(lp);
		return (FALSE);
	}
	lp->l_next = *lh;
	*lh = lp;
	return (TRUE);
}

static struct list *
copy_list(struct list *lp)
{
	struct list	*last = NULL;

	for (; lp != NULL; lp = lp->l_next) {
		if (list_push(&last, lp->l_name) == FALSE) {
			free_file_list(last);
			return (NULL);
		}
	}
	return (last);
}
