/*	$OpenBSD: ttykbd.c,v 1.20 2021/02/23 08:10:51 lum Exp $	*/

/* This file is in the public domain. */

/*
 * Name:	MG 2a
 *		Terminfo keyboard driver using key files
 * Created:	22-Nov-1987 Mic Kaczmarczik (mic@emx.cc.utexas.edu)
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "ttydef.h"
#include "def.h"
#include "kbd.h"

/*
 * Get keyboard character.  Very simple if you use keymaps and keys files.
 */

char	*keystrings[] = {NULL};

/*
 * Not available in termcap
 */
char	*key_cup    = "\e[1;5A";
char	*key_cdown  = "\e[1;5B";
char	*key_cright = "\e[1;5C";
char	*key_cleft  = "\e[1;5D";
char	*key_aright = "\e[1;3C";
char	*key_aleft  = "\e[1;3D";
char	*key_cpgup  = "\e[5;5~";
char	*key_cpgdn  = "\e[6;5~";

/*
 * Turn on function keys using keypad_xmit, then load a keys file, if
 * available.  The keys file is located in the same manner as the startup
 * file is, depending on what startupfile() does on your system.
 */
void
ttykeymapinit(void)
{
	char	*cp;

	/* Bind keypad function keys. */
	if (key_left)
		dobindkey(fundamental_map, "backward-char", key_left);
	if (key_right)
		dobindkey(fundamental_map, "forward-char", key_right);
	if (key_up)
		dobindkey(fundamental_map, "previous-line", key_up);
	if (key_down)
		dobindkey(fundamental_map, "next-line", key_down);
	if (key_beg)
		dobindkey(fundamental_map, "beginning-of-line", key_beg);
	else if (key_home)
		dobindkey(fundamental_map, "beginning-of-line", key_home);
	if (key_end)
		dobindkey(fundamental_map, "end-of-line", key_end);
	if (key_npage)
		dobindkey(fundamental_map, "scroll-up", key_npage);
	if (key_ppage)
		dobindkey(fundamental_map, "scroll-down", key_ppage);
	if (key_ic)
		dobindkey(fundamental_map, "overwrite-mode", key_ic);
	if (key_dc)
		dobindkey(fundamental_map, "delete-char", key_dc);

	/* Local extensions, not available in termcap */
	if (key_cup)
		dobindkey(fundamental_map, "backward-paragraph", key_cup);
	if (key_cdown)
		dobindkey(fundamental_map, "forward-paragraph", key_cdown);
	if (key_cright)
		dobindkey(fundamental_map, "forward-word", key_cright);
	if (key_cleft)
		dobindkey(fundamental_map, "backward-word", key_cleft);

	if (key_aright)
		dobindkey(fundamental_map, "forward-word", key_aright);
	if (key_aleft)
		dobindkey(fundamental_map, "backward-word", key_aleft);

	if (key_cpgup)
		dobindkey(fundamental_map, "beginning-of-buffer", key_cpgup);
	if (key_cpgdn)
		dobindkey(fundamental_map, "end-of-buffer", key_cpgdn);

	/* Check for $TERM specific .mg startup file */
	if ((cp = getenv("TERM"))) {
		if (((cp = startupfile(cp, NULL)) != NULL) &&
		    (load(cp) != TRUE))
			ewprintf("Error reading key initialization file");
	}
	if (keypad_xmit)
		/* turn on keypad */
		putpad(keypad_xmit, 1);
}

/*
 * Clean up the keyboard -- called by tttidy()
 */
void
ttykeymaptidy(void)
{
	if (keypad_local)
		/* turn off keypad */
		putpad(keypad_local, 1);
}

