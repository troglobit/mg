# Written by Joachim Wiberg <troglobit@gmail.com> and put in the public domain.
if DOCS
dist_doc_DATA  = README.md ChangeLog.md UNLICENSE
endif
SUBDIRS        = doc src

package:
	@debuild -uc -us -B --lintian-opts --profile debian -i -I --show-overrides
