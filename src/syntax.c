/* This file is in the public domain. */

/*
 * Syntax highlighting.  A small line-based parser classifies the
 * bytes of a line into comment, string, keyword, type, number and
 * preprocessor.  Which rules apply is decided by the buffer's mode,
 * so c-mode buffers get C rules.  The display code asks for one
 * line at a time; the only state carried across lines is whether a
 * multiline comment is open.
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "def.h"
#include "kbd.h"

/*
 * Keywords are NULL-terminated lists; a trailing '|' marks the
 * second class, shown in the type color.
 */
static const char *c_keywords[] = {
	"auto", "break", "case", "continue", "default", "do", "else",
	"enum", "extern", "for", "goto", "if", "inline", "register",
	"restrict", "return", "sizeof", "static", "struct", "switch",
	"typedef", "union", "volatile", "while",
	"NULL", "TRUE", "FALSE",
	"bool|", "char|", "const|", "double|", "float|", "int|", "long|",
	"short|", "signed|", "size_t|", "ssize_t|", "unsigned|", "void|",
	"int8_t|", "int16_t|", "int32_t|", "int64_t|",
	"uint8_t|", "uint16_t|", "uint32_t|", "uint64_t|",
	NULL
};

struct syntax {
	const char	 *sy_mode;	/* buffer mode this applies to	*/
	const char	**sy_keywords;
	const char	 *sy_slcomm;	/* single line comment starter	*/
	const char	 *sy_mcs;	/* multiline comment start	*/
	const char	 *sy_mce;	/* multiline comment end	*/
	int		  sy_preproc;	/* #directive lines		*/
};

static const struct syntax syntab[] = {
	{ "c", c_keywords, "//", "/*", "*/", 1 },
	{ NULL, NULL, NULL, NULL, NULL, 0 }
};

/*
 * The syntax rules for a buffer, decided by its modes, or NULL.
 */
const struct syntax *
syntax_lookup(struct buffer *bp)
{
	struct maps_s	*m;
	int	 i, j;

	for (j = 0; syntab[j].sy_mode != NULL; j++) {
		if ((m = name_mode(syntab[j].sy_mode)) == NULL)
			continue;
		for (i = 0; i <= bp->b_nmodes; i++)
			if (bp->b_modes[i] == m)
				return (&syntab[j]);
	}
	return (NULL);
}

/*
 * True when an edit in bp can recolor the lines below it, in which
 * case a single line display update is not enough.
 */
int
syn_multiline(struct buffer *bp)
{
	const struct syntax	*sy;

	sy = syntax_lookup(bp);
	return (sy != NULL && sy->sy_mcs != NULL);
}

static int
issep(int c)
{
	return (c == '\0' || isspace(c) ||
	    strchr(",.()+-/*=~%<>[];{}!&^|?:", c) != NULL);
}

static void
setattr(char *attr, int i, int cls)
{
	if (attr != NULL)
		attr[i] = cls;
}

/*
 * Length of s when the line matches it at byte offset i, else 0.
 */
static int
matchat(const struct line *lp, int i, const char *s)
{
	int	 n = strlen(s);

	if (i + n > llength(lp))
		return (0);
	if (memcmp(ltext(lp) + i, s, n) != 0)
		return (0);
	return (n);
}

/*
 * Classify the bytes of one line.  incom is the open multiline
 * comment state at the start of the line; the state after the line
 * is returned.  attr, when not NULL, receives one SYN_* class per
 * byte and must hold llength(lp) bytes.  With a NULL attr only the
 * comment and string state is tracked, for syn_state().
 */
int
syn_parse(const struct syntax *sy, const struct line *lp, int incom,
    char *attr)
{
	const char	**kw;
	int	 c, end, i, j, n, len;
	int	 prev_sep = 1;
	int	 instr = 0;

	len = llength(lp);
	if (attr != NULL)
		memset(attr, SYN_NONE, len);

	i = 0;
	while (i < len) {
		c = lgetc(lp, i);
		if (incom) {
			if ((n = matchat(lp, i, sy->sy_mce)) != 0) {
				for (j = 0; j < n; j++)
					setattr(attr, i + j, SYN_COMMENT);
				i += n;
				incom = 0;
				prev_sep = 1;
			} else {
				setattr(attr, i, SYN_COMMENT);
				i++;
			}
			continue;
		}
		if (instr) {
			setattr(attr, i, SYN_STRING);
			if (c == '\\' && i + 1 < len) {
				setattr(attr, i + 1, SYN_STRING);
				i += 2;
				continue;
			}
			if (c == instr)
				instr = 0;
			i++;
			continue;
		}
		if (sy->sy_slcomm != NULL &&
		    matchat(lp, i, sy->sy_slcomm) != 0) {
			for (j = i; j < len; j++)
				setattr(attr, j, SYN_COMMENT);
			break;
		}
		if (sy->sy_mcs != NULL &&
		    (n = matchat(lp, i, sy->sy_mcs)) != 0) {
			for (j = 0; j < n; j++)
				setattr(attr, i + j, SYN_COMMENT);
			i += n;
			incom = 1;
			continue;
		}
		if (c == '"' || c == '\'') {
			setattr(attr, i, SYN_STRING);
			instr = c;
			i++;
			continue;
		}
		if (attr == NULL) {
			/* only comment and string state is wanted */
			i++;
			continue;
		}
		if (sy->sy_preproc && c == '#' && prev_sep) {
			setattr(attr, i, SYN_PREPROC);
			for (i++; i < len; i++) {
				c = lgetc(lp, i);
				if (!isalpha(c))
					break;
				setattr(attr, i, SYN_PREPROC);
			}
			prev_sep = 0;
			continue;
		}
		if (isdigit(c) && prev_sep) {
			setattr(attr, i, SYN_NUMBER);
			for (i++; i < len; i++) {
				c = lgetc(lp, i);
				if (!isxdigit(c) && c != '.' && c != 'x' &&
				    c != 'X')
					break;
				setattr(attr, i, SYN_NUMBER);
			}
			prev_sep = 0;
			continue;
		}
		if (prev_sep && sy->sy_keywords != NULL &&
		    (isalpha(c) || c == '_')) {
			for (end = i + 1; end < len; end++) {
				c = lgetc(lp, end);
				if (!isalnum(c) && c != '_')
					break;
			}
			for (kw = sy->sy_keywords; *kw != NULL; kw++) {
				j = strlen(*kw);
				n = ((*kw)[j - 1] == '|');
				if (j - n != end - i)
					continue;
				if (memcmp(ltext(lp) + i, *kw,
				    end - i) != 0)
					continue;
				for (j = i; j < end; j++)
					setattr(attr, j,
					    n ? SYN_TYPE : SYN_KEYWORD);
				break;
			}
			i = end;
			prev_sep = 0;
			continue;
		}
		prev_sep = issep(c);
		i++;
	}
	return (incom);
}

/*
 * The multiline comment state at the start of line stop, found by
 * scanning the buffer from the top.
 */
int
syn_state(const struct syntax *sy, struct buffer *bp, struct line *stop)
{
	struct line	*lp;
	int	 incom = 0;

	if (sy->sy_mcs == NULL)
		return (0);
	for (lp = bfirstlp(bp); lp != stop && lp != bp->b_headp;
	     lp = lforw(lp))
		incom = syn_parse(sy, lp, incom, NULL);
	return (incom);
}
