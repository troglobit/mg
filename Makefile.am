# Written by Joachim Nilsson <troglobit@gmail.com> and put in the public domain.
SUBDIRS        = src
dist_man1_MANS = mg.1
doc_DATA       = README.md UNLICENSE tutorial
EXTRA_DIST     = README.md UNLICENSE tutorial

#
# Target to run when building a release
#
release: distcheck
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
