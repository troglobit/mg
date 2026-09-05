#!/usr/bin/env python3
# Written for mg and put in the public domain.
"""Generate the character width tables in utf8.c.

Fetch the two files the tables come from, then run this and paste the
output over the tables in mk_wcwidth():

    curl -O https://www.unicode.org/Public/UCD/latest/ucd/EastAsianWidth.txt
    curl -O https://www.unicode.org/Public/UCD/latest/ucd/extracted/DerivedGeneralCategory.txt
    ./genwidth.py EastAsianWidth.txt DerivedGeneralCategory.txt

Zero width is general category Mn or Me, the format characters (Cf) but
not the soft hyphen, and the conjoining Hangul jamo, which compose onto
the syllable before them.  Two columns is East Asian Wide or Fullwidth,
plus the ranges EastAsianWidth.txt documents as defaulting to Wide where
they are unassigned; the file itself lists only assigned ranges.
"""
import re
import sys

# From the commentary at the top of EastAsianWidth.txt.
DEFAULT_WIDE = [(0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xF900, 0xFAFF),
                (0x20000, 0x2FFFD), (0x30000, 0x3FFFD)]
LINE = re.compile(r'^([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?\s*;\s*(\w+)')


def parse(path, want):
    """The code points in path whose property is one of want."""
    out = set()
    for line in open(path):
        match = LINE.match(line.split('#')[0].strip())
        if match and match.group(3) in want:
            lo = int(match.group(1), 16)
            hi = int(match.group(2), 16) if match.group(2) else lo
            out.update(range(lo, hi + 1))
    return out


def merge(points):
    """Sorted code points as a list of closed intervals."""
    out = []
    for c in sorted(points):
        if out and c <= out[-1][1] + 1:
            out[-1][1] = c
        else:
            out.append([c, c])
    return out


def table(name, intervals):
    cells = ["{ 0x%04X, 0x%04X }," % (lo, hi) for lo, hi in intervals]
    rows = ["\t\t" + " ".join(cells[i:i + 3]) for i in range(0, len(cells), 3)]
    rows[-1] = rows[-1].rstrip(",")
    return "\n".join(["\tstatic const struct interval %s[] = {" % name] +
                     rows + ["\t};"])


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: genwidth.py EastAsianWidth.txt "
                 "DerivedGeneralCategory.txt")

    zero = parse(sys.argv[2], ('Mn', 'Me', 'Cf'))
    zero -= {0x00AD}
    zero |= set(range(0x1160, 0x1200))

    wide = parse(sys.argv[1], ('W', 'F'))
    for lo, hi in DEFAULT_WIDE:
        wide.update(range(lo, hi + 1))
    wide -= zero

    print(table("zerowidth", merge(zero)))
    print()
    print(table("wide", merge(wide)))


if __name__ == '__main__':
    main()
