Micro Emacs
===========
[![Travis Status][]][Travis] [![Coverity Status]][Coverity Scan]

Mg is a public domain [Micro Emacs][] derivative.  Created in 1986 based
on the original MicroEMACS released by Dave Conroy in 1985.

The intention is to be a small, fast, and portable Emacs-like editor for
users who cannot, or do not want to, run the real Emacs for one reason
or another.  Compatibility with GNU Emacs is key for Mg, separating it
from other [ErsatzEmacs][] clones, because there should never be any
reason to learn more than one Emacs flavor.

The size constraints and target audience pose challenging limitations on
the feature set of Mg.  It is not scriptable or extensible like a true
Emacs, although a startup file is supported, and not all features are
available.  For instance `M-x split-window-horizontally`, or `C-x 3`, is
currently not supported.

Feature patches are of course most welcome, as long as they remain in
the spirit of Mg -- to be small, fast, and portable.


Building
--------

This project maintains a set of patches on top of OpenBSD Mg, one of
which is a GNU configure script to simplify building on multiple UNIX
systems, as well as cross compilation to different targets.  However,
the project is not self hosting, you also need to download and install
[libite][] (-lite), v1.6.0 or later.  It provides some API's otherwise
only available on OpenBSD.

Having installed [libite][], checked out the Mg source from GitHub, or
unpacked a release tarball, you simply have to:

    ./configure --disable-all --enable-size-optimizations
    make

Users who checked out the source from GitHub must run `./autogen.sh`
first to create the configure script.  This requires GNU autotools to be
installed on the build system.


History
-------

The history is long and intertwined with other MicroEMACS spin-offs but
goes something like this:

* Nov 16, 1986: First release to mod.sources
* Mar  3, 1987: First Release (mg1a) via comp.sources.unix
* May 26, 1988: Second release: (mg2a) via comp.sources.misc
* Jan 26, 1992: Linux port released by Charles Hedrick. This version
  later makes its way onto tsx-11, Infomagic, and various other Linux
  repositories.
* Feb 25, 2000: First import into the OpenBSD tree, where it is
  currently maintained with contributions from many others.
* May  8, 2016: First import from OpenBSD 5.9 to [GitHub][]

See the source distribution for the list of [AUTHORS][].


Origin and References
---------------------

This project is a derivative of OpenBSD Mg, which currently is the best
maintained source.  The intention is to follow the OpenBSD development
and release portable versions with every OpenBSD release.

Current maintainer of the portable Mg project is [Joachim Nilsson][].
Please report any bugs and problems with the packaging and porting to
the GitHub issue tracker <https://github.com/troglobit/mg/issues>

[Micro Emacs]:     https://www.emacswiki.org/emacs/MicroEmacs
[ErsatzEmacs]:     https://www.emacswiki.org/emacs/ErsatzEmacs
[libite]:          https://github.com/troglobit/libite/releases/tag/v1.6.0
[GitHub]:          https://github.com/troglobit/mg
[AUTHORS]:         https://github.com/troglobit/mg/blob/master/AUTHORS
[Joachim Nilsson]: http://troglobit.com
[Travis]:          https://travis-ci.org/troglobit/mg
[Travis Status]:   https://travis-ci.org/troglobit/mg.png?branch=master
[Coverity Scan]:   https://scan.coverity.com/projects/8859
[Coverity Status]: https://scan.coverity.com/projects/8859/badge.svg
