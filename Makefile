# $OpenBSD: Makefile,v 1.30 2015/03/17 18:08:52 bcallah Exp $

PROG=	mg

#LDADD+=	-lcurses -lutil
LDADD=-ltermcap -lite

# (Common) compile-time options:
#
#	REGEX		-- create regular expression functions.
#	STARTUPFILE	-- look for and handle initialization file.
#
CFLAGS+=-Wall -DREGEX -D_GNU_SOURCE

SRCS=	autoexec.c basic.c bell.c buffer.c cinfo.c dir.c display.c \
	echo.c extend.c file.c fileio.c funmap.c help.c kbd.c keymap.c \
	line.c macro.c main.c match.c modes.c paragraph.c \
	re_search.c region.c search.c spawn.c tty.c ttyio.c ttykbd.c \
	undo.c util.c version.c window.c word.c yank.c

#
# More or less standalone extensions.
#
# theo.c
SRCS+=	cmode.c cscope.c dired.c grep.c tags.c
OBJS = $(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $^ $(LDADD)

afterinstall:
	${INSTALL} -d ${DESTDIR}${DOCDIR}/mg
	${INSTALL} -m ${DOCMODE} -c ${.CURDIR}/tutorial \
		${DESTDIR}${DOCDIR}/mg

clean:
	$(RM) $(OBJS) $(PROG)

distclean: clean
	$(RM) *.o *~ *.bak
