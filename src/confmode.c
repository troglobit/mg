/* This file is in the public domain. */

/*
 * Configuration file mode.  Tags the buffer so that syntax
 * highlighting picks the conf rules, which cover the shapes the
 * ini and rc dialects share: [section] headers, keys, comments
 * started by # or ;, and quoted strings and numbers in values.
 */

#include <signal.h>
#include <stdio.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/* Pull in from modes.c */
extern int changemode(int, int, char *);

static struct KEYMAPE (1) confmodemap = {
	0,
	1,		/* 1 to avoid 0 sized array */
	rescan,
	{
		/* unused dummy entry, see keymap.c */
		{
			(KCHAR)0, (KCHAR)0, NULL, NULL
		}
	}
};

static int
confmode(int f, int n)
{
	int	 s;

	if ((s = changemode(f, n, "conf")) != TRUE)
		return (s);
	if (!buf_hasmode(curbp, "conf"))
		return (TRUE);	/* mode was toggled off */

	/* /etc is written for the traditional tab stop */
	modetabw(8);
	return (TRUE);
}

void
confmode_init(void)
{
	funmap_add(confmode, "conf-mode", 0);
	maps_add((KEYMAP *)&confmodemap, "conf");
#ifdef ENABLE_AUTOEXEC
	(void)add_autoexec("*.conf", "conf-mode");
	(void)add_autoexec("*.cfg", "conf-mode");
	(void)add_autoexec("*.ini", "conf-mode");
	(void)add_autoexec("*.toml", "conf-mode");
	/* systemd units and freedesktop entries */
	(void)add_autoexec("*.desktop", "conf-mode");
	(void)add_autoexec("*.mount", "conf-mode");
	(void)add_autoexec("*.service", "conf-mode");
	(void)add_autoexec("*.socket", "conf-mode");
	(void)add_autoexec("*.target", "conf-mode");
	(void)add_autoexec("*.timer", "conf-mode");
	(void)add_autoexec(".editorconfig", "conf-mode");
	(void)add_autoexec(".gitconfig", "conf-mode");
	(void)add_autoexec("fstab", "conf-mode");
	(void)add_autoexec("ssh_config", "conf-mode");
	(void)add_autoexec("sshd_config", "conf-mode");
	/* kconfig output, as used by the kernel and buildroot */
	(void)add_autoexec(".config", "conf-mode");
	(void)add_autoexec("*defconfig", "conf-mode");
#endif
}
