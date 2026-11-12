/* This file is in the public domain. */

#include <sys/types.h>

static size_t
put(char *buf, size_t bufsize, size_t n, const char *s, size_t len)
{
	for (; len > 0; len--, s++, n++)
		if (n + 1 < bufsize)
			buf[n] = *s;
	return (n);
}

/*
 * Quote arg for the shell, between single quotes, in buf of size
 * bufsize.  Always NUL terminates (unless bufsize == 0).  Returns the
 * length of the quoted string; if retval >= bufsize, it was truncated.
 */
size_t
shquote(const char *arg, char *buf, size_t bufsize)
{
	size_t	 n;

	n = put(buf, bufsize, 0, "'", 1);
	for (; *arg != '\0'; arg++) {
		if (*arg == '\'')
			n = put(buf, bufsize, n, "'\\''", 4);
		else
			n = put(buf, bufsize, n, arg, 1);
	}
	n = put(buf, bufsize, n, "'", 1);

	if (bufsize > 0)
		buf[n < bufsize ? n : bufsize - 1] = '\0';

	return (n);
}
