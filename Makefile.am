bin_PROGRAMS    = mg
mg_SOURCES      = basic.c bell.c buffer.c cinfo.c dir.c display.c echo.c 		\
		  extend.c file.c fileio.c funmap.c help.c kbd.c keymap.c		\
		  line.c macro.c main.c match.c modes.c paragraph.c			\
		  re_search.c region.c search.c spawn.c tty.c ttyio.c ttykbd.c		\
		  undo.c util.c version.c window.c word.c yank.c
mg_LDADD        = -ltermcap -lite
dist_man1_MANS  = mg.1
doc_DATA        = README
EXTRA_DIST      = tutorial

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
if THEO
mg_SOURCES     += theo.c
endif

#
# Target to run when building a release
#
release: dist
	@for file in $(DIST_ARCHIVES); do	\
		md5sum $$file > ../$$file.md5;	\
	done
	@mv $(DIST_ARCHIVES) ../
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do					\
		printf "$$file    \tDistribution tarball\n";			\
		printf "$$file.md5\t"; cat ../$$file.md5 | cut -f1 -d' ';	\
	done
