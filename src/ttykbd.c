/*	$OpenBSD: ttykbd.c,v 1.22 2023/03/30 19:00:02 op Exp $	*/

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
#include "funmap.h"

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
char	*key_aup    = "\e[1;3A";
char	*key_adown  = "\e[1;3B";
char	*key_aright = "\e[1;3C";
char	*key_aleft  = "\e[1;3D";
char	*key_cpgup  = "\e[5;5~";
char	*key_cpgdn  = "\e[6;5~";
char	*key_shiftup    = "\e[1;2A";
char	*key_shiftdown  = "\e[1;2B";
char	*key_shiftright = "\e[1;2C";
char	*key_shiftleft  = "\e[1;2D";
char	*key_shifthome  = "\e[1;2H";
char	*key_shiftend   = "\e[1;2F";
char	*key_asup       = "\e[1;4A";
char	*key_asdown     = "\e[1;4B";
char	*key_asright    = "\e[1;4C";
char	*key_asleft     = "\e[1;4D";
char	*key_csup       = "\e[1;6A";
char	*key_csdown     = "\e[1;6B";
char	*key_csright    = "\e[1;6C";
char	*key_csleft     = "\e[1;6D";

/*
 * Shifted movement, like shift-select-mode in GNU Emacs: activate
 * the mark at dot, move, and let the next unshifted command drop
 * the selection.  A mark the user set himself stays active.
 */
static int
shiftmove(int (*move)(int, int), int f, int n)
{
	if (!curwp->w_markact) {
		isetmark();
		curwp->w_markact = TRUE;
		thisflag |= CFSHIFT;
	} else if (lastflag & CFSHIFT)
		thisflag |= CFSHIFT;
	return (move(f, n));
}

static int
shiftleft(int f, int n)
{
	return (shiftmove(backchar, f, n));
}

static int
shiftright(int f, int n)
{
	return (shiftmove(forwchar, f, n));
}

static int
shiftup(int f, int n)
{
	return (shiftmove(backline, f, n));
}

static int
shiftdown(int f, int n)
{
	return (shiftmove(forwline, f, n));
}

static int
shifthome(int f, int n)
{
	return (shiftmove(gotobol, f, n));
}

static int
shiftend(int f, int n)
{
	return (shiftmove(gotoeol, f, n));
}

static int
shiftbword(int f, int n)
{
	return (shiftmove(backword, f, n));
}

static int
shiftfword(int f, int n)
{
	return (shiftmove(forwword, f, n));
}

static int
shiftbpara(int f, int n)
{
	return (shiftmove(gotobop, f, n));
}

static int
shiftfpara(int f, int n)
{
	return (shiftmove(gotoeop, f, n));
}

/*
 * Turn on function keys using keypad_xmit, then load a keys file, if
 * available.  The keys file is located in the same manner as the startup
 * file is, depending on what startupfile() does on your system.
 */
void
ttykeymapinit(void)
{
	char	*cp, file[NFILEN];
	FILE	*ffp;

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
	if (key_home)
		dobindkey(fundamental_map, "beginning-of-line", key_home);
	if (key_eol)
		dobindkey(fundamental_map, "end-of-line", key_eol);
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

	/* Bind function keys */
	if (key_f1)
		dobindkey(fundamental_map, "quick-help", key_f1);
	if (key_f2)
		dobindkey(fundamental_map, "save-buffer", key_f2);
	if (key_f3)
		dobindkey(fundamental_map, "start-kbd-macro", key_f3);
	if (key_f4)
		dobindkey(fundamental_map, "end-or-call-last-kbd-macro", key_f4);
	if (key_f10)
		dobindkey(fundamental_map, "save-buffers-kill-emacs", key_f10);

	/* Local extensions, not available in termcap */
	if (key_cup)
		dobindkey(fundamental_map, "backward-paragraph", key_cup);
	if (key_cdown)
		dobindkey(fundamental_map, "forward-paragraph", key_cdown);
	if (key_cright)
		dobindkey(fundamental_map, "forward-word", key_cright);
	if (key_cleft)
		dobindkey(fundamental_map, "backward-word", key_cleft);

	if (key_asup)
		dobindkey(fundamental_map, "resize-window-up", key_asup);
	if (key_asdown)
		dobindkey(fundamental_map, "resize-window-down", key_asdown);
	if (key_asright)
		dobindkey(fundamental_map, "resize-window-right", key_asright);
	if (key_asleft)
		dobindkey(fundamental_map, "resize-window-left", key_asleft);
	if (key_aup)
		dobindkey(fundamental_map, "windmove-up", key_aup);
	if (key_adown)
		dobindkey(fundamental_map, "windmove-down", key_adown);
	if (key_aright)
		dobindkey(fundamental_map, "windmove-right", key_aright);
	if (key_aleft)
		dobindkey(fundamental_map, "windmove-left", key_aleft);

	if (key_cpgup)
		dobindkey(fundamental_map, "beginning-of-buffer", key_cpgup);
	if (key_cpgdn)
		dobindkey(fundamental_map, "end-of-buffer", key_cpgdn);

	/* The goto-line prefix from newer GNU Emacs; C-x g remains */
	dobindkey(fundamental_map, "goto-line", "\egg");
	dobindkey(fundamental_map, "goto-line", "\eg\eg");

	/* Shift and an arrow key mark text, like in GNU Emacs */
	funmap_add(shiftleft, "shift-backward-char", 0);
	funmap_add(shiftright, "shift-forward-char", 0);
	funmap_add(shiftup, "shift-previous-line", 0);
	funmap_add(shiftdown, "shift-next-line", 0);
	funmap_add(shifthome, "shift-beginning-of-line", 0);
	funmap_add(shiftend, "shift-end-of-line", 0);
	funmap_add(shiftbword, "shift-backward-word", 0);
	funmap_add(shiftfword, "shift-forward-word", 0);
	funmap_add(shiftbpara, "shift-backward-paragraph", 0);
	funmap_add(shiftfpara, "shift-forward-paragraph", 0);
	dobindkey(fundamental_map, "shift-backward-char", key_shiftleft);
	dobindkey(fundamental_map, "shift-forward-char", key_shiftright);
	dobindkey(fundamental_map, "shift-previous-line", key_shiftup);
	dobindkey(fundamental_map, "shift-next-line", key_shiftdown);
	dobindkey(fundamental_map, "shift-beginning-of-line", key_shifthome);
	dobindkey(fundamental_map, "shift-end-of-line", key_shiftend);
	dobindkey(fundamental_map, "shift-backward-word", key_csleft);
	dobindkey(fundamental_map, "shift-forward-word", key_csright);
	dobindkey(fundamental_map, "shift-backward-paragraph", key_csup);
	dobindkey(fundamental_map, "shift-forward-paragraph", key_csdown);

	/* Check for $TERM specific .mg startup file */
	if ((cp = getenv("TERM")) != NULL &&
	    (ffp = startupfile(cp, NULL, file, sizeof(file))) != NULL) {
		if (load(ffp, file) != TRUE)
			ewprintf("Error reading key initialization file");
		(void)ffclose(ffp, NULL);
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
