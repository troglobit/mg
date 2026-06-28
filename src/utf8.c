/* This file is in the public domain. */

/*
 * UTF-8 helper functions.  The buffer holds raw bytes; these
 * functions identify and decode multibyte sequences so that
 * movement, deletion and redisplay can treat a sequence as a
 * single character.  One codepoint occupies one display column.
 */

#include <langinfo.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "def.h"

int	 utf8_mode = FALSE;

/*
 * Detect a UTF-8 locale and adjust the character class table:
 * bytes with the high bit set make up multibyte sequences, so
 * they are word constituents without case.  Latin-1 locales
 * keep the case-aware table from cinfo.c.
 */
void
utf8_init(void)
{
	char	*codeset;
	int	 c;

	codeset = nl_langinfo(CODESET);
	utf8_mode = (codeset != NULL && strcmp(codeset, "UTF-8") == 0);
	if (utf8_mode)
		for (c = 0x80; c <= 0xff; c++)
			cinfo[c] = _MG_W;
}

/*
 * True for UTF-8 continuation bytes (0x80-0xBF).
 */
int
utf8_iscont(int c)
{
	return ((CHARMASK(c) & 0xc0) == 0x80);
}

/*
 * Length of the sequence started by lead byte c: 2, 3 or 4.
 * Zero if c cannot start a sequence.
 */
int
utf8_seqlen(int c)
{
	unsigned char	b = CHARMASK(c);

	if (b >= 0xc2 && b <= 0xdf)
		return (2);
	if (b >= 0xe0 && b <= 0xef)
		return (3);
	if (b >= 0xf0 && b <= 0xf4)
		return (4);
	return (0);
}

/*
 * Decode the sequence at s, of which avail bytes can be read.
 * On success return the codepoint and set *len to the number of
 * bytes consumed.  Return -1 on overlong forms, surrogates and
 * anything else that is not a valid sequence.
 */
int
utf8_decode(const char *s, int avail, int *len)
{
	int	 cp, i, n;

	n = utf8_seqlen(s[0]);
	if (n == 0 || n > avail)
		return (-1);
	cp = CHARMASK(s[0]) & (0x7f >> n);
	for (i = 1; i < n; i++) {
		if (!utf8_iscont(s[i]))
			return (-1);
		cp = (cp << 6) | (CHARMASK(s[i]) & 0x3f);
	}
	if (cp < 0x80 || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
	    (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff)
		return (-1);
	*len = n;
	return (cp);
}

/*
 * Decode the sequence at byte offset o in line lp.
 */
int
utf8_get(const struct line *lp, int o, int *len)
{
	if (!utf8_mode || o >= llength(lp))
		return (-1);
	return (utf8_decode(&lp->l_text[o], llength(lp) - o, len));
}
