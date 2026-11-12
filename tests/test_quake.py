#!/usr/bin/env python3
"""tests/test_quake.py: M1's two timings on the PC (make test-quake).

Our own code. Runs build/quake_host's timedemo of demo1 and its server
benchmark on e1m1, prints their lines, and checks what doesn't depend on
the PC's speed: demo1's frame count, and the server benchmark's totals,
which its fixed 0.1-second steps make the same every run, both what
GLQuake asked the GL layer for and what the layer sent PB (M2). It writes
30 of e1m1's frames with QUINCE_PBDUMP and reads them back with
tests/pbcheck.py. It also loads every shareware map for a few frames, to
show each fits the hunk. The
pak is the shareware 1.06 pak0.pak (md5 5906e5998fc3d896ddaf5e6a62e03abb), from the
person running it: it's never committed.
"""
import os
import re
import subprocess
import sys

DEMO1_FRAMES = 969
# 300 server frames of e1m1 from the start, this build's renderer:
SERVER_FRAMES = 300
SERVER_TOTALS = (186776, 357952)   # triangles, vertices GLQuake asked for
SERVER_SENT = (2836051, 330356, 162178)   # words, vertices, triangles sent PB
SERVER_CHECKSUM = 666297688   # of every word: the same commands, word for word
DUMP_FRAMES = 30   # e1m1's frames written by QUINCE_PBDUMP and read back
MAPS = ["START"] + ["E1M%d" % i for i in range(1, 9)]


def run(exe, base, *words, env=None):
    out = subprocess.run([exe, base, *words], capture_output=True, text=True,
                         timeout=600, env=env)
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


def check_dropped(what, out):
    """Every command fitted its frame's buffer."""
    m = re.search(r"^\w+: culled .* dropped (\d+) commands", out, re.M)
    if not m or int(m.group(1)) != 0:
        print("FAIL: %s dropped commands (%s)" % (what, m and m.group(0)))
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
    failed += check_dropped("demo1", demo)
    server = run(exe, base, "server", "E1M1", str(SERVER_FRAMES))
    failed += check_load("e1m1", server)
    failed += check_dropped("e1m1", server)
    m = re.search(r"^server: totals (\d+) tris (\d+) verts", server, re.M)
    got = (int(m.group(1)), int(m.group(2))) if m else None
    if got != SERVER_TOTALS:
        print("FAIL: e1m1's %d frames sent %s (tris, verts), want %s"
              % (SERVER_FRAMES, got, SERVER_TOTALS))
        failed += 1
    m = re.search(r"^server: sent totals (\d+) words (\d+) verts (\d+) tris", server, re.M)
    got = m and tuple(int(g) for g in m.groups())
    if got != SERVER_SENT:
        print("FAIL: e1m1's %d frames sent PB %s (words, verts, tris), want %s"
              % (SERVER_FRAMES, got, SERVER_SENT))
        failed += 1
    m = re.search(r"^server: checksum (\d+)", server, re.M)
    if not m or int(m.group(1)) != SERVER_CHECKSUM:
        print("FAIL: e1m1's commands' checksum is %s, want %d" % (m and m.group(1), SERVER_CHECKSUM))
        failed += 1
    # The dump, read back by a reader written apart from the writer.
    dump = os.path.join(os.path.dirname(os.path.abspath(exe)), "e1m1.pb")
    env = dict(os.environ, QUINCE_PBDUMP=dump)
    run(exe, base, "server", "E1M1", str(DUMP_FRAMES), env=env)
    check = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "pbcheck.py"),
                            dump], capture_output=True, text=True)
    sys.stdout.write(check.stdout)
    if check.returncode != 0:
        failed += 1
    for name in MAPS:
        run(exe, base, "server", name, "5")
    if failed:
        sys.exit(1)
    print("test-quake: demo1, e1m1 and every shareware map OK")


if __name__ == "__main__":
    main()
