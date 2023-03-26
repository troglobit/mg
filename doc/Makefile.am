if DOCS
dist_man1_MANS = mg.1
dist_doc_DATA  = tutorial .mg mg.png
endif
EXTRA_DIST     = AUTHORS

if TUTOR
pkgdata_DATA   = tutorial

install-data-hook:
	gzip -f $(DESTDIR)$(pkgdatadir)/tutorial
endif
