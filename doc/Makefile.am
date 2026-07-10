if DOCS
dist_man1_MANS = mg.1
dist_doc_DATA  = tutorial.md .mg mg.png utf8.txt
endif
EXTRA_DIST     = AUTHORS

if TUTOR
pkgdata_DATA   = tutorial.gz

clean-local:
	-rm -f $(builddir)/tutorial.gz

tutorial.gz: tutorial.md
	gzip -c $< >$(builddir)/$@
endif
