#!/usr/bin/env python3
"""tests/test_quake.py: M1's two timings on the PC (make test-quake).

Our own code. Runs build/quake_host's timedemo of demo1 and its server
benchmark on e1m1, prints their lines, and checks what doesn't depend on
the PC's speed: demo1's frame count, and the server benchmark's totals,
which its fixed 0.1-second steps make the same every run. It also loads
every shareware map for a few frames, to show each fits the hunk. The
pak is the shareware 1.06 pak0.pak (md5 5906e5998fc3d896ddaf5e6a62e03abb), from the
person running it: it's never committed.
"""
import re
import subprocess
import sys

DEMO1_FRAMES = 969
# 300 server frames of e1m1 from the start, this build's renderer:
SERVER_FRAMES = 300
SERVER_TOTALS = (330776, 650152)   # triangles, vertices
MAPS = ["START"] + ["E1M%d" % i for i in range(1, 9)]


def run(exe, base, *words):
    out = subprocess.run([exe, base, *words], capture_output=True, text=True,
                         timeout=600)
    if out.returncode != 0:
        sys.exit("test-quake: %s %s failed:\n%s%s" % (exe, " ".join(words),
                                                      out.stdout, out.stderr))
    sys.stdout.write(out.stdout)
    return out.stdout


def check_load(what, out):
    """The textures sent at load, before the timed window, are counted."""
    m = re.search(r"^\w+: at load, textures (\d+) uploads (\d+) bytes", out, re.M)
    if not m or int(m.group(2)) == 0:
        print("FAIL: %s's load-time textures not counted (%s)" % (what, m and m.group(0)))
        return 1
    return 0


def main():
    exe, base = sys.argv[1], sys.argv[2]
    failed = 0
    demo = run(exe, base, "timedemo", "DEMO1")
    m = re.search(r"^timedemo: (\d+) frames", demo, re.M)
    if not m or int(m.group(1)) != DEMO1_FRAMES:
        print("FAIL: demo1 ran %s frames, want %d" % (m and m.group(1), DEMO1_FRAMES))
        failed += 1
    failed += check_load("demo1", demo)
    server = run(exe, base, "server", "E1M1", str(SERVER_FRAMES))
    failed += check_load("e1m1", server)
    m = re.search(r"^server: totals (\d+) tris (\d+) verts", server, re.M)
    got = (int(m.group(1)), int(m.group(2))) if m else None
    if got != SERVER_TOTALS:
        print("FAIL: e1m1's %d frames sent %s (tris, verts), want %s"
              % (SERVER_FRAMES, got, SERVER_TOTALS))
        failed += 1
    for name in MAPS:
        run(exe, base, "server", name, "5")
    if failed:
        sys.exit(1)
    print("test-quake: demo1, e1m1 and every shareware map OK")


if __name__ == "__main__":
    main()
