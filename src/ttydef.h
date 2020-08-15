/*	$OpenBSD: ttydef.h,v 1.12 2015/03/24 22:28:10 bcallah Exp $	*/
/*
 *	Terminfo terminal file, nothing special, just make it big
 *	enough for windowing systems.
 */

/* This file is in the public domain. */

#ifndef TTYDEF_H
#define TTYDEF_H

#include <termios.h>
#include <term.h>

#define STANDOUT_GLITCH		/* possible standout glitch	 */
#define	putpad(str, num)	tputs(str, num, ttputc)

/*
 * This flag is used on *BSD when calling tcsetattr() to prevent it
 * from changing speed, duplex, parity.  GNU says we should use the
 * CIGNORE flag to c_cflag, but that doesn't exist so ... we rely on
 * our initial tcgetattr() and prey that nothing changes on the TTY
 * before we exit and restore with tcsetattr()
 */
#ifndef TCSASOFT
#define TCSASOFT 0
#endif

#endif /* TTYDEF_H */
