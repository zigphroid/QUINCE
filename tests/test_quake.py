#!/usr/bin/env python3
"""tests/test_quake.py: M1's two timings on the PC (make test-quake).

Our own code. Runs build/quake_host's timedemo of demo1 and its server
benchmark on e1m1, prints their lines, and checks what doesn't depend on
the PC's speed: demo1's frame count, and the server benchmark's totals,
which its fixed 0.1-second steps make the same every run, both what
GLQuake asked the GL layer for and what the layer sent PB (M2). It writes
30 of e1m1's frames with QUINCE_PBDUMP and reads them back with
tests/pbcheck.py, then draws them with the triangle engine's C model
(tests/pbplay.c, docs/PB3D.md) and holds the pictures to their CRCs. It
also loads every shareware map for a few frames, to
show each fits the hunk. The
pak is the shareware 1.06 pak0.pak (md5 5906e5998fc3d896ddaf5e6a62e03abb), from the
person running it: it's never committed.
"""
import os
import re
import struct
import subprocess
import sys

DEMO1_FRAMES = 969
# 300 server frames of e1m1 from the start, this build's renderer:
SERVER_FRAMES = 300
SERVER_TOTALS = (186776, 357952)   # triangles, vertices GLQuake asked for
SERVER_SENT = (2837701, 330356, 162178)   # words, vertices, triangles sent PB
SERVER_CHECKSUM = 3500115068   # of every word: the same commands, word for word
# ...and where those frames leave the game (lib/qbench.c's print_state).
# Since M3 the PC rounds floats as the board does (the Makefile's QHOST_CC),
# so CANNER's build on SIMMER must print these too (tools/simbench.py).
SERVER_STATE = "c8c17318, 164 entities"
DUMP_FRAMES = 30   # e1m1's frames written by QUINCE_PBDUMP and read back
# ...drawn by the model: the CRC of every frame's picture's CRC in turn
# (the load's frames, then the 30), and the last one's. A change to the
# model's rules, the GL layer or Quake's drawing moves them: look at
# build/pbframes/e1m1/frame0075.png before taking the new numbers.
PLAY_FRAMES, PLAY_LAST = 76, 75
PLAY_ALL, PLAY_LAST_CRC = "641e154e", "290e4c6a"
# id's GL_Init state (lib/qvid.c): the first frame's first STATE has
# unit 0's texture and the alpha test on (lib/pb3d.h's PB3D_S_TEX0 and
# PB3D_S_ALPHA_TEST), or the console goes to PB as white quads.
FIRST_STATE_BITS = 0x0080 | 0x0020
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


def first_state(path):
    """The first STATE word of the first frame that has one."""
    data = open(path, "rb").read()
    at = 8
    while at + 8 <= len(data):
        rtype, _, length = struct.unpack_from("<HHI", data, at)
        if rtype == 4:   # FRAME
            words = struct.unpack_from("<%dH" % (length // 2), data, at + 8)
            i = 0
            while i < len(words):
                op = words[i] >> 8
                if op == 16:
                    return words[i + 1]
                # The few commands a 2D frame starts with; a TRIS ends it.
                if op == 15:
                    i += 1
                elif op == 17:
                    i += 3
                else:
                    break
            if words:
                return None
        at += 8 + length + (length & 1)
    return None


def play(pbplay, dump, out):
    """The model draws the dump: none refused, the pictures' CRCs."""
    os.makedirs(out, exist_ok=True)
    res = subprocess.run([pbplay, dump, "-o", out, str(PLAY_LAST)], capture_output=True,
                         text=True, timeout=600)
    sys.stdout.write(res.stdout + res.stderr)
    failed = 0
    if res.returncode != 0:
        print("FAIL: pbplay refused a record or a command of e1m1's dump")
        failed += 1
    m = re.search(r"^pbplay: (\d+) frames, \d+ drawn, all (\w+)", res.stdout, re.M)
    if not m or (int(m.group(1)), m.group(2)) != (PLAY_FRAMES, PLAY_ALL):
        print("FAIL: the model drew %s (frames, all), want %s"
              % (m and m.groups(), (PLAY_FRAMES, PLAY_ALL)))
        failed += 1
    m = re.search(r"^pbplay: frame %d (\w+)" % PLAY_LAST, res.stdout, re.M)
    if not m or m.group(1) != PLAY_LAST_CRC:
        print("FAIL: frame %d's picture is %s, want %s"
              % (PLAY_LAST, m and m.group(1), PLAY_LAST_CRC))
        failed += 1
    return failed


def main():
    exe, base = sys.argv[1], sys.argv[2]
    pbplay = sys.argv[3] if len(sys.argv) > 3 else None
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
    m = re.search(r"^server: state (.*)$", server, re.M)
    if not m or m.group(1) != SERVER_STATE:
        print("FAIL: e1m1's %d frames left state %s, want %s"
              % (SERVER_FRAMES, m and m.group(1), SERVER_STATE))
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
    s = first_state(dump)
    if s is None or s & FIRST_STATE_BITS != FIRST_STATE_BITS:
        print("FAIL: the first frame's STATE is %s, want TEX0 and ALPHA_TEST (%#x) on"
              % (s is not None and hex(s), FIRST_STATE_BITS))
        failed += 1
    if pbplay:
        failed += play(pbplay, dump, os.path.join(os.path.dirname(os.path.abspath(exe)),
                                                  "pbframes", "e1m1"))
    for name in MAPS:
        run(exe, base, "server", name, "5")
    if failed:
        sys.exit(1)
    print("test-quake: demo1, e1m1%s and every shareware map OK"
          % (", e1m1 drawn by the model" if pbplay else ""))


if __name__ == "__main__":
    main()
