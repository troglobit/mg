Change Log
==========

All relevant changes to the project are documented in this file.

[v3.0][] - 2016-05-15
---------------------

First release based on [Mg2a][] from OpenBSD 5.9.  The work on [Mg3a][],
by Bengt Larsson, is not a part of this project.  The version number was
chosen based on: 2A < 30 < 3A HEX.  The OpenBSD Mg is greatly improved
over the original Mg2a, from 1986, but does not have the same feature
set as Mg3a.

### Changes
- Import mg from OpenBSD 5.9
- Use libite (-lite) to provide missing OpenBSD/NetBSD API's
- Add GNU configure and build system for portability to other systems:
  - Detect existence and correct version of libite (v1.6.0 or later)
  - Detect `-ltermcap`, `-ltinfo`, or `-lcurses`.  We only use the termcap
    functionality in Mg, but can link with either of them
  - Detect `term.h`, we cannot use `termcap.h` because of cookies
  - Make OpenBSD developed features optional with configure,
    e.g. integrated ctags and cscope support
  - Add `--enable-size-optimizations` option
  - Add `--with-startup=FILE` for alternate init file
- Conditionalize OpenBSD specific API's and modules, e.g. the `pledge()`
  API and the `theo.c` module ... `#ifdef __OpenBSD__`
- Add missing `M-x version` for compatibility with GNU Emacs
- Add `-h` command line option for a simple usage text
- Change Mg built-in version to use configure script's `PACKAGE_STRING`
  rather than hardcoded, and rather outdated, `Mg 2a`
- Add LICENSE file, from licensing info (Public Domain) in README
- Add AUTHORS file, from author listing in README
- Create README.md from text in README and `mg.1` with information
  about this project and the motivation for it

### Fixes
- Convert from `fgetln()` to standard POSIX `getline()`
- Convert old `st_mtimespec` to POSIX `st_mtim`
- Add workaround for systems missing `TCSASOFT` flag to `tcsetattr()`
- Import `SO_NOSIGPIPE` patch for OX X from by Han Boetes' Mg porting
  project <http://homepage.boetes.org/software/mg/>
- Encapsulate private `globalwd`data in optional `grep.c` module
- Fix build warnings for missing `asprintf()` family of C API's
- Integration fixes:
  - `fisdir()` already exists in [libite][]
  - `makedir()` is a [libite][] function, rename to `make_dir()`
- Fix unsafe enabling of non-blocking mode on file descriptor used
  when piping buffer contents (`M-|`) to an external program.
- Collect forked off children in `M-|`, missing `waitpid()`
- Fixes for bugs found by Coverity Scan:
  - Fix file descriptor leaks
  - Add missing NULL pointer checks
  - Missing checks for return value from both internal and C library
	functions.  Some simply marked as "don't care"
  - Fix missing initialization of stack variables

[UNRELEASED]: https://github.com/troglobit/mg/compare/TAIL...HEAD
[v3.0]:       https://github.com/troglobit/mg/compare/TAIL...v3.0
[Mg2a]:       http://cvsweb.openbsd.org/cgi-bin/cvsweb/src/usr.bin/mg/
[Mg3a]:       http://www.bengtl.net/files/mg3a/
[libite]:     https://github.com/troglobit/libite/
<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
