# Written by Joachim Nilsson <troglobit@gmail.com> and put in the public domain.
SUBDIRS        = src
dist_man1_MANS = mg.1
doc_DATA       = README.md UNLICENSE tutorial
EXTRA_DIST     = README.md UNLICENSE tutorial

# lintian --profile debian -i -I --show-overrides ../$PKG.changes
package:
	dpkg-buildpackage -uc -us -B

#
# Target to run when building a release
#
release: distcheck package
	@for file in $(DIST_ARCHIVES); do	\
		md5sum $$file > ../$$file.md5;	\
	done
	@mv $(DIST_ARCHIVES) ../
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do					\
		printf "%-32s Distribution tarball\n" $$file;			\
		printf "%-32s " $$file.md5; cat ../$$file.md5 | cut -f1 -d' ';	\
	done
	@for file in `cd ..; ls $(PACKAGE)_$(VERSION)*`; do			\
		printf "%-32s Debian/Ubuntu package\n" $$file;			\
	done
