# Written by Joachim Wiberg <troglobit@gmail.com> and put in the public domain.
AUTOMAKE_OPTIONS = subdir-objects
if DOCS
doc_DATA         = README
endif

bin_PROGRAMS     = mg
mg_SOURCES       = basic.c bell.c buffer.c cinfo.c dir.c		\
		   display.c echo.c						\
		   extend.c file.c fileio.c funmap.c help.c interpreter.c	\
		   kbd.c keymap.c line.c macro.c main.c match.c		\
		   modes.c							\
		   paragraph.c region.c search.c spawn.c			\
		   tty.c ttyio.c ttykbd.c					\
		   ttydef.h undo.c utf8.c util.c version.c window.c word.c	\
		   yank.c							\
		   chrdef.h def.h funmap.h kbd.h key.h macro.h pathnames.h
mg_SOURCES      += queue.h tree.h
mg_SOURCES      += extensions.c

EXTRA_DIST       = genwidth.py

mg_CFLAGS        = -W -Wall -Wextra -Wno-unused-parameter
mg_CFLAGS       += -D_GNU_SOURCE -DDATADIR=\"$(pkgdatadir)\" -DDOCDIR=\"$(docdir)\"
if TINY
mg_CFLAGS       += -fomit-frame-pointer -fdata-sections -ffunction-sections	\
		   -fno-stack-protector -fno-asynchronous-unwind-tables -flto	\
		   -fmerge-all-constants -fno-ident -finline-small-functions	\
		   -Wl,--gc-sections -Wl,-z,norelro -Wl,--hash-style=gnu	\
		   -Wl,--build-id=none
endif
mg_LDADD         = $(LIBS) $(LIBOBJS)

#
# More or less standalone extensions.
#
if AUTOEXEC
mg_SOURCES     += autoexec.c
endif
if CMODE
mg_SOURCES     += cmode.c
endif
if COMPILE
mg_SOURCES     += grep.c
endif
if CSCOPE
mg_SOURCES     += cscope.c
endif
if CTAGS
mg_SOURCES     += tags.c
endif
if DIRED
mg_SOURCES     += dired.c
endif
if REGEX
mg_SOURCES     += re_search.c
endif
if MGLOG
mg_SOURCES     += log.c log.h
endif
if SYNTAX
mg_SOURCES     += syntax.c comment.c
endif

#
# The file modes, see --with-modes
#
if MODE_CMAKE
mg_SOURCES     += cmakemode.c
endif
if MODE_CONF
mg_SOURCES     += confmode.c
endif
if MODE_DIFF
mg_SOURCES     += diffmode.c
endif
if MODE_GIT
mg_SOURCES     += gitmode.c
endif
if MODE_M4
mg_SOURCES     += m4mode.c
endif
if MODE_MAKE
mg_SOURCES     += mkmode.c
endif
if MODE_MARKDOWN
mg_SOURCES     += mdmode.c
endif
if MODE_PYTHON
mg_SOURCES     += pymode.c
endif
if MODE_SH
mg_SOURCES     += shmode.c
endif
if MODE_TEXT
mg_SOURCES     += textmode.c
endif
if MODE_YAML
mg_SOURCES     += yamlmode.c
endif

#
# Building w/o terminfo/termcap/curses/ncurses
#
if NOCURSES
mg_SOURCES     += ansi.c ansi.h
endif
