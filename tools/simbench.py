#!/usr/bin/env python3
"""make sim-bench: M1's two timings on SIMMER (PectinOS in QEMU).

QUINCE's own (M1). The same QUINCE.JAM the board runs, on a scratch card
with the shareware pak at J:\\LARDER\\QUINCE\\ID1\\PAK0.PAK, then
`QUINCE /TIMEDEMO DEMO1` and `QUINCE /SERVER E1M1 FRAMES`, their lines
printed. SIMMER's times say nothing about the board's (QEMU runs as fast
as the PC lets it); what this checks is that CANNER's build runs Quake
through, and that its counts match the PC's (tests/test_quake.py), so the
board run in M1's PR 3 is only a matter of the clock.
"""
import argparse
import os
import re
import shutil
import sys

DEMO1_FRAMES = 969
MAPS = ["START"] + ["E1M%d" % i for i in range(1, 9)]
# e1m1's 300 server frames send what the PC's do (tests/test_quake.py's
# SERVER_TOTALS): the server is deterministic, and both builds take
# lib/qrand.c's rand, so CANNER's build has to match GCC's on the PC
# exactly. demo1's totals follow the clock (the
# client's interpolation), so only its frame count is checked.
SERVER_FRAMES = 300
SERVER_TOTALS = (186776, 357952)   # triangles, vertices GLQuake asked for
SERVER_SENT = (2836051, 330356, 162178)   # words, vertices, triangles sent PB
SERVER_CHECKSUM = 666297688   # of every word: the same commands, word for word
# Where those frames leave the game, as the PC's (tests/test_quake.py):
# since M3 the PC rounds floats as the M33 does, so these match exactly
# too, and a difference is a bug in one build (LOG 2026-11-12).
SERVER_STATE = "c8c17318, 164 entities"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--root", required=True, help="the pectinos checkout")
    ap.add_argument("--app", required=True, help="QUINCE.JAM")
    ap.add_argument("--pak", required=True, help="the shareware id1/pak0.pak")
    ap.add_argument("--work", required=True, help="a directory for the card and the log")
    ap.add_argument("--frames", type=int, default=SERVER_FRAMES, help="server frames")
    args = ap.parse_args()

    sys.path.insert(0, os.path.join(args.root, "tools"))
    import simdrive

    qemu = os.path.join(args.root, "simmer", "qemu", "bin", "qemu-system-arm")
    kernel = os.path.join(args.root, "build", "sim", "pectinos.elf")
    jarcard = os.path.join(args.root, "build", "host", "jarcard")
    os.makedirs(args.work, exist_ok=True)
    tree = os.path.join(args.work, "tree")
    shutil.rmtree(tree, ignore_errors=True)
    os.makedirs(os.path.join(tree, "ID1"))
    shutil.copyfile(args.pak, os.path.join(tree, "ID1", "PAK0.PAK"))
    card = os.path.join(args.work, "card.img")
    simdrive.new_card(jarcard, card, [args.app])
    simdrive.subprocess.run([jarcard, card, "tree", tree, "\\LARDER\\QUINCE"], check=True,
                            stdout=simdrive.subprocess.DEVNULL)

    failed = 0
    with open(os.path.join(args.work, "console.log"), "w") as log, \
            simdrive.Sim(qemu, kernel, card, log) as sim:
        try:
            sim.boot()
            out = sim.run("QUINCE /TIMEDEMO DEMO1", want=[re.compile(r"timedemo: checksum")],
                          wait=1800)
            print("\n".join(l for l in out.splitlines() if l.startswith("timedemo:")))
            m = re.search(r"timedemo: (\d+) frames", out)
            if not m or int(m.group(1)) != DEMO1_FRAMES:
                print("FAIL: demo1 ran %s frames, want %d" % (m and m.group(1), DEMO1_FRAMES))
                failed += 1
            if not re.search(r"timedemo: at load, textures \d+ uploads [1-9]", out):
                print("FAIL: demo1's load-time textures not counted")
                failed += 1
            out = sim.run("QUINCE /SERVER E1M1 %d" % args.frames,
                          want=[re.compile(r"server: state .* entities")], wait=1800)
            print("\n".join(l for l in out.splitlines() if l.startswith("server:")))
            m = re.search(r"server: totals (\d+) tris (\d+) verts", out)
            got = m and (int(m.group(1)), int(m.group(2)))
            if args.frames == SERVER_FRAMES and got != SERVER_TOTALS:
                print("FAIL: e1m1's %d frames sent %s (tris, verts), want %s, as the PC's"
                      % (SERVER_FRAMES, got, SERVER_TOTALS))
                failed += 1
            m = re.search(r"server: state ([^\r\n]*)", out)
            got = m and m.group(1).strip()
            if args.frames == SERVER_FRAMES and got != SERVER_STATE:
                print("FAIL: e1m1's %d frames left state %s, want %s, as the PC's"
                      % (SERVER_FRAMES, got, SERVER_STATE))
                failed += 1
            m = re.search(r"server: sent totals (\d+) words (\d+) verts (\d+) tris", out)
            got = m and tuple(int(g) for g in m.groups())
            if args.frames == SERVER_FRAMES and got != SERVER_SENT:
                print("FAIL: e1m1's %d frames sent PB %s (words, verts, tris), want %s, as the PC's"
                      % (SERVER_FRAMES, got, SERVER_SENT))
                failed += 1
            m = re.search(r"server: checksum (\d+)", out)
            if args.frames == SERVER_FRAMES and (not m or int(m.group(1)) != SERVER_CHECKSUM):
                print("FAIL: e1m1's commands' checksum is %s, want %d, as the PC's"
                      % (m and m.group(1), SERVER_CHECKSUM))
                failed += 1
            # Every shareware map fits the board's pool (LOG 2026-11-12).
            for name in MAPS:
                sim.run("QUINCE /SERVER %s 5" % name, want=[re.compile(r"server: state .* entities")],
                        wait=600)
            print("sim-bench: every shareware map loaded")
        except simdrive.SimError as e:
            print("sim-bench: FAILED: %s" % e, file=sys.stderr)
            print(e.out[-3000:] if e.out else sim.tail(), file=sys.stderr)
            return 1
    if failed:
        return 1
    print("sim-bench: Quake ran on SIMMER")
    return 0


if __name__ == "__main__":
    sys.exit(main())
