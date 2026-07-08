Change Log
==========

All relevant changes to the project are documented in this file.

[v4.0][] - 2026-07-08
---------------------

The UTF-8 release: multibyte text can now be typed, displayed, and
edited in UTF-8 locales.  Also new: syntax highlighting, a visible
region, and side by side windows.

### Changes
- Initial UTF-8 support, active in UTF-8 locales:
  - Multibyte characters display as single characters, cursor motion
    and delete operate on whole characters, and files always round
    trip byte for byte
  - Limitations, see mg(1): every character is drawn one column wide,
    mini-buffer editing is still byte-wise, case folding is ASCII only
- 8-bit character input works out of the box: `meta-key-mode` is now
  disabled by default, use `(meta-key-mode 1)` in `~/.mg` to get the
  old behavior back.  Terminals that send Meta as an ESC prefix, which
  is all of them these days, are unaffected
- Character class table updated from DEC multinational to Latin-1:
  word motion, case conversion, and case-insensitive search now treat
  Ð, Þ, ð, þ as letters, and no longer the × and ÷ signs
- Visual mark mode: the region between mark and dot is drawn in
  reverse video, like transient-mark-mode in GNU Emacs.  New command
  `visual-mark-mode` toggles it, enabled by default
- Syntax highlighting of comments, strings, keywords, types, numbers,
  and preprocessor directives in buffers with a language mode, such as
  `c-mode`.  New command `font-lock-mode` toggles it, enabled by
  default.  Terminals without color show plain text
- New command `shell-script-mode` with shell highlighting rules:
  POSIX reserved words, builtins, and `$variables`.  Also sets tab
  width 8 with hard tabs, for here documents, and RET keeps the
  indent.  Enabled automatically for *.sh files and files with a #!
  line naming a Bourne compatible shell
- New command `split-window-horizontally`, bound to `C-x 3` like GNU
  Emacs: side by side windows, freely mixed with `C-x 2`
- New command `require-final-newline <nil | T | ask>`
- New command `use-short-answers`: a single y or n answers important
  questions.  Enabled by default, unlike GNU Emacs; get the strict
  behavior back with `use-short-answers 0` in `~/.mg`.  From PR #37
  by Glubbfreund
- The startup help text in the echo line is dismissed on the first
  key press, or after ten seconds.  Keep the old always-on behavior
  with `display-help-mode 1` in `~/.mg`
- Smart TAB in c-mode and shell-script-mode: TAB indents the current
  line, or every line in the region when the mark is active, like in
  GNU Emacs.  Shell scripts indent like the previous non-blank line
- Dired: new command `dired-up-directory`, bound to `^`, also used by
  `dired-jump`, from OpenBSD
- `C-u M-!` and `C-u M-|` insert the shell command output in the
  current buffer instead of a separate one, from OpenBSD
- C-mode: respect user defined tab width in indentation, by Daniel
  Hennigar

### Fixes
- Sync with OpenBSD, as of March 2026:
  - Fix `replace-regexp` looping forever on `(replace-regexp "^.*$"
    "")` and replacing anchored patterns more than once per line
  - Saving the `*scratch*` buffer no longer prompts for a file path
    when there are no changes to save
  - Plug memory leaks in the interpreter and word handling routines,
    from Han Boetes
  - Handle `strdup()` failure in several places, from Han Boetes
  - Fix wrongly sized externs, found by Gentoo building with `-flto`
- Fix `auto-indent-mode` with custom tab widths, from OpenBSD
- Fix build with dired disabled
- Fix missing mini-buffer help texts

[v3.7][] - 2023-08-13
---------------------

### Changes
- New command `display-help-toggle`, ensures the quick-help text is
  always shown in the status area, enabled by default
- New command `apply-macro-to-region-lines` by Ben Scuron
- New command `end-or-call-last-kbd-macro`, used in key bindings
- Map function keys F1-F4 + F10 to common functions:
  - F1: Toggle quick help
  - F2: Save current buffer
  - F3: Start keyboard macro
  - F4: End keyboard macro, or call latest macro from mark
- Sync with OpenBSD as of April 21, 2023:
  - Add command `set-tab-width` to change per-buffer tab width
  - Sync manual changes: grammar, visual improvements
- Drop support for disabling `no-tab-mode` when calling `configure`
  script.  Simplifies code greatly and not needed anymore since it is
  enabled by default upstream

### Fixes
- Fix Home/End key for builds both with and without curses
- Fixes bug where the current markline is not reset after being
  cleared, by Ben Scuron
- Sync with OpenBSD as of April 21, 2023:
  - Fixes to `no-tab-mode`.  OpenBSD had previously removed this mode,
    relevant change in this fork: indent next line with spaces
  - Fix buffer overflow when no match is found for a search


[v3.6][] - 2023-04-10
---------------------

### Changes
- Add support for exuberant/universal Ctags `tags` file format
- Add support for M-, to `pop-tag-mark`, like GNU Emacs
- Sync with mg upstream, OpenBSD 7.2, as of April 10 2023
  - Drop trailing whitespace on RET (c-mode)
  - Add `zap-to-char` and `zap-up-to-char`, binding the former to M-z
  - Change `visit-tags-table` to immediately load tags file
- Add support for `--without-docs` to skip installation of files to
  `/usr/share/doc/mg`, including the manual
- Install gzipped tutorial in `/usr/share/mg/`, for built-in help

### Fixes
- Sync with mg upstream, OpenBSD 7.2, as of March 26 2023
  - Fix `dobeep_msgs()` usage, does not support format strings, this may
    have caused unexpected crashes for some operations, e.g. goto line
    that does not exist
  - Fix memleaks and possible crashes in ctags support
  - Fall back to `/bin/sh` if `$SHELL` is undefined
  - Use basename of `argv[0]` instead of hard coding "sh"
- Fix #17: typos in `*quick*` help buffer, also replace duplicate undo
  with mark command instead
- Fix #18: revert-buffer crash on macOS


[v3.5][] - 2021-10-17
---------------------

### Changes
- Add support for Solaris/Illumos based UNIX systems.  Tested on OmniOS
- Add Dockerfile and GitHub integration for container images
- Sync with mg upstream, OpenBSD 7.0, as of May 12, 2021
  - New replace-regexp function, for scripting
  - New dired function, dired-jump activated with C-x C-j
  - New dired-shell-command function for piping file to a command
  - New mgrc batch mode, mostly intended for regression testing
  - New command line option, -u mgrc, to specify startup mgrc file
  - Clarify markers in list-buffers output, more similar to GNU Emacs

### Fixes
- Fix regression; lost path in status line while trying to open a file.  
  Caused by a fix introduced in v3.4 to clear the status line on kill or
  switch buffer by name.  A new fix has been made to handle both cases.
- Fix #10: replace deprecated signal functions with `sigaction()`
- Fix #11: segfault on disabled keybindings, `--disable-ctags/cscope`
- Fix various gcc build warnings


[v3.4][] - 2020-08-23
---------------------

Most important news is the new modeline and support for building
and running without Ncurses, or termcap/terminfo!

### Changes
- Support for building without termcap/terminfo/ncurses.  This
  feature will be welcome for embedded systems, and others who
  cannot, or do not want to, bundle the complete ncurses
- Support for opening `.gz` text files, as read-only
- Support for `C-h t` to access the Mg tutorial
- Support for `C-h q` to toggle `*quick*` help buffer
- New default key bindings for cursor movement, from GNU Emacs:
  - C-up    backward-paragraph
  - C-down  forward-paragraph
  - C-left  backward-word, also M-left
  - C-right forward-word, also M-right
  - C-PgUp  beginning-of-buffer
  - C-PgDn  end-of-buffer
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
  - New `display-time-mode`, toggle current time in modeline
- Enable column-number-mode by default
- Add new internal `mglog_misc()` debug API
- Clear status line after killing/switching buffer by name, fixes
  lingering prompt after said action
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
- Initial support for scheme-like scripting functionality to `~/.mg`,
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

[UNRELEASED]: https://github.com/troglobit/mg/compare/v3.7...HEAD
[v4.0]:       https://github.com/troglobit/mg/compare/v3.7...v4.0
[v3.7]:       https://github.com/troglobit/mg/compare/v3.6...v3.7
[v3.6]:       https://github.com/troglobit/mg/compare/v3.5...v3.6
[v3.5]:       https://github.com/troglobit/mg/compare/v3.4...v3.5
[v3.4]:       https://github.com/troglobit/mg/compare/v3.3...v3.4
[v3.3]:       https://github.com/troglobit/mg/compare/v3.2...v3.3
[v3.2]:       https://github.com/troglobit/mg/compare/v3.1...v3.2
[v3.1]:       https://github.com/troglobit/mg/compare/v3.0.2...v3.1
[v3.0.2]:     https://github.com/troglobit/mg/compare/v3.0.1..v3.0.2
[v3.0.1]:     https://github.com/troglobit/mg/compare/v3.0..v3.0.1
[v3.0]:       https://github.com/troglobit/mg/compare/TAIL...v3.0
[Mg2a]:       http://cvsweb.openbsd.org/cgi-bin/cvsweb/src/usr.bin/mg/
[Mg3a]:       http://www.bengtl.net/files/mg3a/
[libite]:     https://github.com/troglobit/libite/
