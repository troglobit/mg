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

static const char *sh_keywords[] = {
	"break", "case", "continue", "do", "done", "elif", "else",
	"esac", "exit", "fi", "for", "function", "if", "in", "return",
	"select", "shift", "then", "time", "until", "while",
	"alias|", "bg|", "cd|", "command|", "echo|", "eval|", "exec|",
	"export|", "false|", "fg|", "getopts|", "hash|", "jobs|",
	"kill|", "local|", "printf|", "pwd|", "read|", "readonly|",
	"set|", "test|", "trap|", "true|", "type|", "ulimit|", "umask|",
	"unalias|", "unset|", "wait|",
	NULL
};

static int	 md_parse(const struct line *, int, char *);

struct syntax {
	const char	 *sy_mode;	/* buffer mode this applies to	*/
	const char	**sy_keywords;
	const char	 *sy_slcomm;	/* single line comment starter	*/
	int		  sy_slsep;	/* which needs a separator first */
	const char	 *sy_mcs;	/* multiline comment start	*/
	const char	 *sy_mce;	/* multiline comment end	*/
	int		  sy_preproc;	/* #directive lines		*/
	int		  sy_dollar;	/* $variable references		*/
	/* the keyword machinery does not fit all languages */
	int		(*sy_parse)(const struct line *, int, char *);
};

static const struct syntax syntab[] = {
	{ "c", c_keywords, "//", 0, "/*", "*/", 1, 0, NULL },
	{ "shell-script", sh_keywords, "#", 1, NULL, NULL, 0, 1, NULL },
	{ "markdown", NULL, NULL, 0, NULL, NULL, 0, 0, md_parse },
	{ NULL, NULL, NULL, 0, NULL, NULL, 0, 0, NULL }
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
	return (sy != NULL && (sy->sy_mcs != NULL || sy->sy_parse != NULL));
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

	if (sy->sy_parse != NULL)
		return (sy->sy_parse(lp, incom, attr));

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
			if (c == instr) {
				instr = 0;
				prev_sep = 0;
			}
			i++;
			continue;
		}
		if (sy->sy_slcomm != NULL &&
		    (!sy->sy_slsep || prev_sep) &&
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
		if (sy->sy_dollar && c == '$' && i + 1 < len) {
			setattr(attr, i, SYN_TYPE);
			i++;
			c = lgetc(lp, i);
			if (c == '{') {
				for (; i < len; i++) {
					setattr(attr, i, SYN_TYPE);
					if (lgetc(lp, i) == '}') {
						i++;
						break;
					}
				}
			} else if (c == '#' || c == '?' || c == '@' ||
			    c == '*' || c == '$' || c == '!' || c == '-') {
				setattr(attr, i, SYN_TYPE);
				i++;
			} else {
				for (; i < len; i++) {
					c = lgetc(lp, i);
					if (!isalnum(c) && c != '_')
						break;
					setattr(attr, i, SYN_TYPE);
				}
			}
			prev_sep = 0;
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
 * The index of the next occurrence of c at or after i, or the
 * line length when not found.
 */
static int
scanto(const struct line *lp, int i, int c)
{
	while (i < llength(lp) && lgetc(lp, i) != c)
		i++;
	return (i);
}

/*
 * A line of one repeated punctuation character, at least two of
 * = - * or _ with nothing else but spaces: a setext heading
 * underline (= and -) or a horizontal rule.  Returns the
 * character.  Four columns of indent make it a code block, not
 * an underline.
 */
static int
md_underline(const struct line *lp)
{
	int	 c, i, n;

	for (i = 0; i < llength(lp) && lgetc(lp, i) == ' '; i++)
		;
	if (i >= 4 || i >= llength(lp))
		return (0);
	c = lgetc(lp, i);
	if (c != '=' && c != '-' && c != '*' && c != '_')
		return (0);
	for (n = 0; i < llength(lp); i++) {
		if (lgetc(lp, i) == c)
			n++;
		else if (lgetc(lp, i) != ' ')
			return (0);
	}
	return (n >= 2 ? c : 0);
}

/*
 * Markdown line classifier, used through sy_parse.  Colors the
 * common core that all the markdown variants agree on; everything
 * else stays plain.  The cross-line state is the fence character
 * while inside a fenced code block, otherwise zero.
 */
static int
md_parse(const struct line *lp, int infence, char *attr)
{
	int	 c, i, j, len, n, u;

	len = llength(lp);

	/* a fence, ``` or ~~~, opens and closes code blocks */
	i = 0;
	while (i < 3 && i < len && lgetc(lp, i) == ' ')
		i++;
	c = i < len ? lgetc(lp, i) : 0;
	if ((c == '`' || c == '~') && (infence == 0 || infence == c)) {
		for (n = 0, j = i; j < len && lgetc(lp, j) == c; j++)
			n++;
		if (n >= 3) {
			if (attr != NULL)
				memset(attr, SYN_STRING, len);
			return (infence ? 0 : c);
		}
	}
	if (infence) {
		if (attr != NULL)
			memset(attr, SYN_STRING, len);
		return (infence);
	}
	if (attr == NULL)	/* only the fence state matters */
		return (0);

	i = 0;
	n = 0;
	while (i < len && (lgetc(lp, i) == ' ' || lgetc(lp, i) == '\t')) {
		n = lgetc(lp, i) == '\t' ? ntabstop(n, 4) : n + 1;
		i++;
	}
	if (i >= len)
		return (0);
	c = lgetc(lp, i);

	/* an indented code block, four columns or more */
	if (n >= 4) {
		memset(attr, SYN_STRING, len);
		return (0);
	}
	/* heading */
	if (c == '#') {
		memset(attr, SYN_HEADING, len);
		return (0);
	}
	/* block quote */
	if (c == '>') {
		memset(attr, SYN_COMMENT, len);
		return (0);
	}
	/* setext heading: text with a ==== or ---- line under it */
	u = md_underline(lp);
	n = md_underline(lforw(lp));
	if (u == 0 && (n == '=' || n == '-')) {
		memset(attr, SYN_HEADING, len);
		return (0);
	}
	/* the underline under a setext heading */
	if ((u == '=' || u == '-') && llength(lback(lp)) > 0) {
		memset(attr, SYN_HEADING, len);
		return (0);
	}
	/* horizontal rules */
	if (u != 0) {
		memset(attr, SYN_KEYWORD, len);
		return (0);
	}
	/* a link reference definition, [label]: url */
	if (c == '[') {
		j = scanto(lp, i + 1, ']');
		if (matchat(lp, j, "]:") != 0 && lgetc(lp, i + 1) != '^') {
			memset(attr, SYN_PREPROC, len);
			return (0);
		}
	}
	/* list markers, -, +, * and 1. */
	if ((c == '-' || c == '+' || c == '*') && i + 1 < len &&
	    lgetc(lp, i + 1) == ' ') {
		attr[i] = SYN_NUMBER;
		i += 2;
	} else if (isdigit(c)) {
		for (j = i; j < len && isdigit(lgetc(lp, j)); j++)
			;
		if (j + 1 < len && lgetc(lp, j) == '.' &&
		    lgetc(lp, j + 1) == ' ') {
			for (; i <= j; i++)
				attr[i] = SYN_NUMBER;
			i++;
		}
	}

	/* inline `code`, *emphasis*, [text](url) links and <urls> */
	while (i < len) {
		c = lgetc(lp, i);
		if (c == '`') {
			j = scanto(lp, i + 1, '`');
			if (j < len) {
				for (; i <= j; i++)
					attr[i] = SYN_STRING;
				continue;
			}
		} else if (c == '*' || c == '_') {
			n = (i + 1 < len && lgetc(lp, i + 1) == c) ? 2 : 1;
			for (j = i + n; j + n - 1 < len; j++)
				if (lgetc(lp, j) == c &&
				    (n == 1 || lgetc(lp, j + 1) == c))
					break;
			if (j + n - 1 < len) {
				for (; i < j + n; i++)
					attr[i] = SYN_TYPE;
				continue;
			}
		} else if (c == '[') {
			/* inline (url), reference [label] and [] forms */
			j = scanto(lp, i + 1, ']');
			if (j < len && lgetc(lp, i + 1) == '^') {
				/* a [^1] footnote, with or without : */
				for (; i <= j; i++)
					attr[i] = SYN_PREPROC;
				continue;
			}
			c = j + 1 < len ? lgetc(lp, j + 1) : 0;
			c = c == '(' ? ')' : c == '[' ? ']' : 0;
			if (c != 0) {
				j = scanto(lp, j + 2, c);
				if (j < len) {
					for (; i <= j; i++)
						attr[i] = SYN_PREPROC;
					continue;
				}
			}
		} else if (c == '<') {
			/* an autolink: no spaces and a : before the > */
			for (n = 0, j = i + 1; j < len; j++) {
				c = lgetc(lp, j);
				if (c == '>' || c == ' ' || c == '\t')
					break;
				if (c == ':')
					n = 1;
			}
			if (n != 0 && j < len && c == '>') {
				for (; i <= j; i++)
					attr[i] = SYN_PREPROC;
				continue;
			}
		}
		i++;
	}
	return (0);
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

	if (sy->sy_mcs == NULL && sy->sy_parse == NULL)
		return (0);
	for (lp = bfirstlp(bp); lp != stop && lp != bp->b_headp;
	     lp = lforw(lp))
		incom = syn_parse(sy, lp, incom, NULL);
	return (incom);
}
