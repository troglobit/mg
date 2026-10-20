/* This file is in the public domain. */

/*
 * auto-revert-mode and auto-revert-tail-mode
 */

#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"

static int	ftail(struct buffer *);
static void	bufeob(struct buffer *);

/*
 * auto-revert-mode: while waiting for a key, reload the file buffers
 * whose file changed on disk, unless they have unsaved edits.
 */
static int	autorevert = TRUE;
static int	autorevert_interval = 5;	/* seconds */

void
autorevert_init(void)
{
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("/var/log/*", "auto-revert-tail-mode");
#endif
}

int
autorevertmode(int f, int n)
{
	if (f & FFARG)
		autorevert = n > 0;
	else
		autorevert = !autorevert;
	ewprintf("Auto revert mode %sabled", autorevert ? "en" : "dis");

	return (TRUE);
}

int
setautorevertint(int f, int n)
{
	char buf[32], *rep;
	const char *es;

	if (f & FFARG) {
		if (n < 1)
			return (FALSE);
	} else {
		if ((rep = eread("Set auto-revert-interval: ", buf, sizeof(buf),
		    EFNEW | EFCR)) == NULL)
			return (ABORT);
		if (rep[0] == '\0')
			return (FALSE);
		n = strtonum(rep, 1, INT_MAX, &es);
		if (es != NULL)
			return (dobeep_msgs("Auto revert interval", es));
	}
	autorevert_interval = n;
	ewprintf("Auto revert interval set to %d seconds", n);

	return (TRUE);
}

/*
 * Reload the file buffers whose file changed on disk, leaving those
 * with unsaved edits, or told to ignore the disk, alone.  A buffer in
 * tail mode gets what was added to its file instead, and is reloaded
 * only when the file shrank or was replaced, to be shown from the end.
 * Tail mode works without auto-revert-mode.  Returns TRUE if any
 * buffer changed.
 */
int
autorevert_check(void)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	struct stat	 sb;
	int		 tail, changed = FALSE;

	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (bp->b_fname[0] == '\0' ||
		    (bp->b_flag & (BFCHG | BFIGNDIRTY)) != 0)
			continue;
		tail = buf_hasmode(bp, "tail");
		if (!autorevert && !tail)
			continue;
		if (stat(bp->b_fname, &sb) == -1 ||
		    (sb.st_size == bp->b_fi.fi_size &&
		    sb.st_mtim.tv_sec == bp->b_fi.fi_mtime.tv_sec &&
		    sb.st_mtim.tv_nsec == bp->b_fi.fi_mtime.tv_nsec) ||
		    access(bp->b_fname, R_OK) != 0)
			continue;
		if (tail && sb.st_dev == bp->b_fi.fi_dev &&
		    sb.st_ino == bp->b_fi.fi_ino &&
		    sb.st_size >= bp->b_fi.fi_size) {
			if (ftail(bp) == TRUE)
				changed = TRUE;
			continue;
		}
		if (revertbp(bp) != TRUE)
			continue;
		if (tail) {
			for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
				if (wp->w_bufp == bp)
					wineob(wp);
			bufeob(bp);
		}
		ewprintf("Reverting buffer %s", bp->b_bname);
		changed = TRUE;
	}

	return (changed);
}

/*
 * Milliseconds to wait before autorevert_check, or 0 when it has no
 * buffer to look at.
 */
int
autorevert_wait(void)
{
	struct buffer *bp;

	for (bp = bheadp; bp != NULL && !autorevert; bp = bp->b_bufp)
		if (buf_hasmode(bp, "tail"))
			break;
	return (autorevert || bp != NULL ? autorevert_interval * 1000 : 0);
}

/*
 * Append to bp what its file gained since it was read, like tail -f.
 * Each window with dot at the end of the buffer follows the new end.
 * Returns FALSE if the file could not be read.
 */
static int
ftail(struct buffer *bp)
{
	struct mgwin	*wp;
	struct line	*olast, *last;
	struct stat	 sb;
	FILE		*ffp;
	char		 buf[BUFSIZ], *nl;
	size_t		 i, n, s;
	int		 olen, len, size;

	if ((ffp = fopen(bp->b_fname, "r")) == NULL)
		return (FALSE);
	if (fstat(fileno(ffp), &sb) == -1 ||
	    fseeko(ffp, bp->b_fi.fi_size, SEEK_SET) == -1)
		goto fail;

	olast = last = blastlp(bp);
	olen = llength(olast);
	if (last == bp->b_headp) {
		/* nothing read yet, b_lines already counts this line */
		if ((last = lalloc(0)) == NULL)
			goto fail;
		lappend(bp, last);
		bp->b_lines--;
	}

	while ((n = fread(buf, 1, sizeof(buf), ffp)) > 0) {
		for (s = 0; s <= n; s = i + 1) {
			nl = memchr(&buf[s], *bp->b_nlchr, n - s);
			i = nl ? (size_t)(nl - buf) : n;
			len = i - s;
			if (len > 0) {
				/* grow by doubling, a log line may span many reads */
				size = last->l_size;
				if (size < llength(last) + len)
					size *= 2;
				if (size < llength(last) + len)
					size = llength(last) + len;
				if (lrealloc(last, size) == FALSE)
					goto fail;
				memcpy(ltext(last) + llength(last), &buf[s], len);
				last->l_used += len;
			}
			if (nl == NULL)
				break;
			if ((last = lalloc(0)) == NULL)
				goto fail;
			lappend(bp, last);
		}
	}
	if (ferror(ffp))
		goto fail;

	bp->b_fi.fi_size = ftello(ffp);
	bp->b_fi.fi_mtime = sb.st_mtim;
	(void)fclose(ffp);

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_bufp != bp)
			continue;
		if (wp->w_dotp == olast && wp->w_doto >= olen)
			wineob(wp);
		wp->w_rflag |= WFFULL | WFMODE;
	}
	if (bp->b_nwnd == 0 && bp->b_dotp == olast && bp->b_doto >= olen)
		bufeob(bp);

	return (TRUE);
fail:
	(void)fclose(ffp);
	return (FALSE);
}

/*
 * Follow a file that grows, like tail -f, see autorevert_check().
 * Turning it on moves to the end, to follow from there.
 */
int
tailmode(int f, int n)
{
	int	 was = buf_hasmode(curbp, "tail");

	if (changemode(f, n, "tail") != TRUE)
		return (FALSE);
	if (!inrc && !was && buf_hasmode(curbp, "tail"))
		wineob(curwp);
	return (TRUE);
}

/*
 * Put the dot bp keeps for when it is shown again at its end.
 */
static void
bufeob(struct buffer *bp)
{
	bp->b_dotp = blastlp(bp);
	bp->b_doto = llength(bp->b_dotp);
	bp->b_dotline = bp->b_lines;
}
