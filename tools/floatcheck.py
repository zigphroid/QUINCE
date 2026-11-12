#!/usr/bin/env python3
"""make test-float: Quake's hot functions call no double code (M3).

QUINCE's own. Reads CANNER's -S output for the whole program and
tests/float_hot.txt, and fails if a function on the list calls one of
softfp.c's double helpers, or a double math.h function, more often than
the list allows, or isn't in the code at all (renamed, or folded into its
callers, where it can't be checked).
"""
import collections
import re
import sys

DOUBLE = re.compile(r"\tb(?:l|\.w)?\t(__aeabi_(?:d\w+|[fil]2d|ul2d)|sqrt|sin|cos|tan|floor|ceil|"
                    r"fabs|fmod|atan2?|asin|pow)$")


def calls(asm):
    out, fn = {}, None
    for line in open(asm):
        m = re.match(r"^([A-Za-z_][\w.]*):$", line)
        if m:
            fn = m.group(1)
            out.setdefault(fn, collections.Counter())
            continue
        m = DOUBLE.search(line.rstrip("\n"))
        if m and fn:
            out[fn][m.group(1).replace("__aeabi_", "")] += 1
    return out


def main():
    asm, listing = sys.argv[1:3]
    got = calls(asm)
    failed = n = 0
    for line in open(listing):
        line = line.split("#")[0].strip()
        if not line:
            continue
        words = line.split()
        fn, allowed = words[0], collections.Counter(words[1:])
        n += 1
        if fn not in got:
            print("test-float: FAIL: %s isn't in CANNER's code" % fn)
            failed += 1
            continue
        extra = got[fn] - allowed
        if extra:
            print("test-float: FAIL: %s calls %s, more than tests/float_hot.txt allows"
                  % (fn, ", ".join("%s x%d" % kv for kv in sorted(extra.items()))))
            failed += 1
    if failed:
        return 1
    print("test-float: %d hot functions call no more double code than listed" % n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
