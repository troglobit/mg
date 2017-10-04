/*
 * This file is in the public domain.
 *
 * Authors:
 *   * Brian Callahan
 *   * Kyle Isom
 */

#include <sys/queue.h>

#include <signal.h>
#include <stdio.h>

#include "def.h"

/*
 * Extensions unique to Mg portable.
 */

int	shownlprompt = TRUE;

/* Check for a newline at the end of a file? */
int
togglenewlineprompt(int f, int n)
{
	if (shownlprompt == TRUE)
		shownlprompt = FALSE;
	else
		shownlprompt = TRUE;

	return (TRUE);
}
