if DOCS
dist_man1_MANS = mg.1
dist_doc_DATA  = tutorial .mg mg.png
endif
pkgdata_DATA   = tutorial
EXTRA_DIST     = AUTHORS

install-data-hook:
	gzip -f $(DESTDIR)$(pkgdatadir)/tutorial
