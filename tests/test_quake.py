#!/usr/bin/env python3
"""tests/test_quake.py: M1's two timings on the PC (make test-quake).

Our own code. Runs build/quake_host's timedemo of demo1 and its server
benchmark on e1m1, prints their lines, and checks what doesn't depend on
the PC's speed: demo1's frame count, and the server benchmark's totals,
which its fixed 0.1-second steps make the same every run. The pak is the
shareware 1.06 pak0.pak (md5 5906e5998fc3d896ddaf5e6a62e03abb), from the
person running it: it's never committed.
"""
import re
import subprocess
import sys

DEMO1_FRAMES = 969
# 300 server frames of e1m1 from the start, this build's renderer:
SERVER_FRAMES = 300
SERVER_TOTALS = (331290, 651180)   # triangles, vertices


def run(exe, base, *words):
    out = subprocess.run([exe, base, *words], capture_output=True, text=True,
                         timeout=600)
    if out.returncode != 0:
        sys.exit("test-quake: %s %s failed:\n%s%s" % (exe, " ".join(words),
                                                      out.stdout, out.stderr))
    sys.stdout.write(out.stdout)
    return out.stdout


def main():
    exe, base = sys.argv[1], sys.argv[2]
    failed = 0
    demo = run(exe, base, "timedemo", "demo1")
    m = re.search(r"^timedemo: (\d+) frames", demo, re.M)
    if not m or int(m.group(1)) != DEMO1_FRAMES:
        print("FAIL: demo1 ran %s frames, want %d" % (m and m.group(1), DEMO1_FRAMES))
        failed += 1
    server = run(exe, base, "server", "e1m1", str(SERVER_FRAMES))
    m = re.search(r"^server: totals (\d+) tris (\d+) verts", server, re.M)
    got = (int(m.group(1)), int(m.group(2))) if m else None
    if got != SERVER_TOTALS:
        print("FAIL: e1m1's %d frames sent %s (tris, verts), want %s"
              % (SERVER_FRAMES, got, SERVER_TOTALS))
        failed += 1
    if failed:
        sys.exit(1)
    print("test-quake: demo1 and e1m1 OK")


if __name__ == "__main__":
    main()
