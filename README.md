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
the project is not completely self hosting, you need a termcap library,
or ncurses, to provide terminal manipulation APIs like `setupterm()`,
`tgoto()`, and `tputs()`.

On Debian/Ubuntu systems:

    sudo apt install libncurses5-dev

Then build mg from the unpacked release tarball, the `configure` flags
are optional, see `./configure --help` for a description:

    ./configure --disable-all --enable-size-optimizations
    make
    sudo make install-strip

Users who checked out the source from GitHub must run `./autogen.sh`
first to create the configure script.  This requires GNU autotools to be
installed on the build system.

To build a completely static mg with all features:

    ./configure LDFLAGS="-static"


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
maintained source.  The intention of this project is to track OpenBSD
development and other sources of Mg functionality, enable as much of it
as possible using a standard GNU configure script, and be friendly for
porting to resource constrained systems.

Please report any bugs and problems with the packaging and porting to
the GitHub issue tracker <https://github.com/troglobit/mg/issues>

Note: the official [portable Mg][] project is maintained by Han Boetes.
It uses libbsd to maintain portability and follows the OpenBSD version
very closely.  It is also the version used by Debian and others.

[Micro Emacs]:     https://www.emacswiki.org/emacs/MicroEmacs
[ErsatzEmacs]:     https://www.emacswiki.org/emacs/ErsatzEmacs
[portable Mg]:     https://github.com/hboetes/mg
[GitHub]:          https://github.com/troglobit/mg
[AUTHORS]:         https://github.com/troglobit/mg/blob/master/AUTHORS
[Joachim Nilsson]: http://troglobit.com
[Travis]:          https://travis-ci.org/troglobit/mg
[Travis Status]:   https://travis-ci.org/troglobit/mg.png?branch=master
[Coverity Scan]:   https://scan.coverity.com/projects/8859
[Coverity Status]: https://scan.coverity.com/projects/8859/badge.svg
