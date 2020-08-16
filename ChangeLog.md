Change Log
==========

All relevant changes to the project are documented in this file.

[v3.4][UNRELEASED]
------------------

Most important news is the new modeline and support for building
and running without Ncurses, or termcap/terminfo!

### Changes
- Support for building without termcap/terminfo/ncurses.  This
  feature will be welcome for embedded systems, and others who
  cannot, or do not want to, bundle the complete ncurses
- Sync with OpenBSD:
  - Update all $OpenBSD: id$ strings
  - Fix -Wshadow warnings
  - Fix missing return value checks
- Modeline changes to mimic GNU Emacs
  - Drop `Mg:`, similar to yur3i/mg/commit/@84ce23b
  - Use space instead of dash `-` as separator, same as @yur3i
  - Hard code std encoding and UNIX end-of-line mode
  - Show (line,column) and place before buffer modes
  - Upper case for each mode listed
- Enable column-number-mode by default
- Add new internal `mglog_misc()` debug API
- Rename Debian package: mg -> mg2a, provides mg

### Fixes
- Check return value from all `fopen()` calls in internal log API
- Duplicate definitions of global variables in def.h, found by GCC-10.
  Patched by Ulrich Mueller of Gentoo, via OpenBSD and Han Boetes
- Avoid NULL deref in regexec when searching for empty lines, from OpenBSD
  by Hiltjo Posthuma
- Prevent segfault with query-replace-regex replacing ^, from OpenBSD
  by Mark Williamson
- Avoid running out of memory with query-replace-regex ^, from OpenBSD
  by Mark Williamson


[v3.3][] - 2019-12-11
---------------------

### Changes
- Initial support for scheme-like scriptiing functionality to `~/.mg`,
  by Mark Lumsden
- Add `set-case-replaced` to toggle case-preserving replace on or off.
  By Reyk Flöter, OpenBSD
- Complement `mg -R` with new function: `toggle-read-only-all`,
  by Mark Lumsden
- Distribute and install example `~/.mg` file in `/usr/shared/doc/mg`
- Improved internal support for debugging mg, by Mark Lumsden
- Add `dired-revert` to the dired funmap, by Mark Lumsden
- Display an error message if trying to copy or rename a file to itself,
  by Mark Lumsden
- Add `dired-goto-file`, by Mark Lumsden

### Fixes
- Fix undo in transpose-paragraph, by Mark Lumsden
- Fix GCC truncation warning in help.c
- Fixes to various GCC 8 compiler warnings, by Joachim Nilsson
- Fix `--with-startup=foo` and `--disable-all` configure options
- Fix #2: Update build instructions for Ubuntu 16.04
- Fix switch-to-buffer (`C-x b`) doesn't cancel properly if `C-g` is
  pressed, by Mark Lumsden
- Fix to always display correct file name in minibuffer, by Mark Lumsden
- Fix to allow fns with >0 param receive and process when eval from
  startup file, by Mark Lumsden


[v3.2][] - 2018-08-26
---------------------

Portability fixes for current and older UNIX and Linux distributions.
Extensive testing on many systems: Debian GNU/Linux, Ubuntu, Fedora,
CentOS, Alpine Linux, FreeBSD, NetBSD, OpenBSD, DragonFly BSD, Apple
macOS >= 10.10, Cygwin, and a few embedded Linux systems.

### Changes
- New function: toggle-newline-prompt.  From ibara/mg@befd2cf
- Continuous integration support, Travis-CI, for macOS > 10.10
- Support for Cygwin, tested on Windows 10
- Verified support for DragonFly BSD, NetBSD, FreeBSD, and OpenBSD
- Adopt Debian packaging and adapt to this project
- Add example `~/.mg` from Han Boetes' portable Mg project

### Fixes
- Fix missing `futimens()` on older UNIX or Linux distributions, in
  particular macOS < 10.13, by Martin Kühl.  From ibara/mg@4a4ac31
- Fix missing `libutil.h` detection on FreeBSD
- Fix missing `LOGIN_NAME_MAX` on macOS
- Fix missing `st_mtim` on macOS and NetBSD
- Fix missing `FIONREAD` on Cygwin
- Silence compiler warning, uninitialized variable.  From ibara/mg@f62f966


[v3.1][] - 2018-07-22
---------------------

The standalone release, with lots of OpenBSD fixes and improvements.

### Changes
- Remove libite (-lite) build/runtime requirement.  All OpenBSD compat
  functions now maintained in-tree, used automatically when missing
- Keep current buffer if switch-to-buffer is aborted, from OpenBSD
- Present the default choice before the colon in prompts, from OpenBSD
- Extract compile_mode command status with WEXITSTATUS, from OpenBSD
- Let the Insert key toggle overwrite mode, from OpenBSD
- GNU Emacs compatibility with query-replace & capitilisation, from OpenBSD
- When exiting i-search with ^M, drop mark, like GNU Emacs, from OpenBSD
- Remove theo analyzer from OpenBSD build, archived upstream
- Enable regexp search by default in configure script, now all features
  are on parity and can be disabled/enabled in the same fashion
- Clarify licensing of project, everything is in the public domain

### Fixes
- Fix #1: Enable `M-x no-tab-mode`, disabled in OpenBSD by default
- Fix line numbering bug, from OpenBSD
- Fix display of overlong lines containing non-ASCII bytes, from OpenBSD
- Fix description of delete-non-matching-lines, from OpenBSD
- Backups saved to wrong directory when running as another user, from OpenBSD
- Fix editing files in a directory without read permission, use relative
  paths if we cannot get the CWD, from OpenBSD
- Fixes to warnings from Coverity Scan (static code analyzer):
  - Insecure data handling in `getkeyname()`
  - Possible NULL pointer deref(s) in `dired.c`
  - Fix explicit NULL pointer deref in `dired.c`
  - Fix unchecked return value in `delleadwhite()` and `indent()`
  - Fix explicit NULL pointer deref in `excline()`
  - Fix many dereference after NULL check in `veread()`
  - Fix possible memory leak in error handling of `veread()`


[v3.0.2][] - 2016-08-28
-----------------------

Bugfix release.

### Fixes
- Fix segfault when closing `*scratch*` buffer with `C-x k RET` having
  issued `C-x b RET` from a fresh started `mg`.
- Bring `fileio.c` up to speed with OpenBSD, r1.102, replacing `TMPDIR`
  with the standard `/tmp` directory for temp files.


[v3.0.1][] - 2016-05-16
-----------------------

Bugfix release.

### Fixes
- Fix `C-a`, `C-e`, `C-space`, etc. regression when Cscope integration,
  `--disable-cscope` or `--disable-all` is selected.


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

[UNRELEASED]: https://github.com/troglobit/mg/compare/v3.3...HEAD
[v3.3]:       https://github.com/troglobit/mg/compare/v3.2...v3.3
[v3.2]:       https://github.com/troglobit/mg/compare/v3.1...v3.2
[v3.1]:       https://github.com/troglobit/mg/compare/v3.0.2...v3.1
[v3.0.2]:     https://github.com/troglobit/mg/compare/v3.0.1..v3.0.2
[v3.0.1]:     https://github.com/troglobit/mg/compare/v3.0..v3.0.1
[v3.0]:       https://github.com/troglobit/mg/compare/TAIL...v3.0
[Mg2a]:       http://cvsweb.openbsd.org/cgi-bin/cvsweb/src/usr.bin/mg/
[Mg3a]:       http://www.bengtl.net/files/mg3a/
[libite]:     https://github.com/troglobit/libite/
