#!/usr/bin/env python3
"""make sim-smoke: PectinOS in SIMMER (QEMU, no window) with QUINCE.JAM on
a scratch card, driven the way a person at the board would.

1. `QUINCE /SELFTEST`: the host test's checks, on CANNER's code. A check
   that passes on the host and fails here is a compiler bug, or code that
   leans on something the host gives for free.
2. `QUINCE` itself: for a full-screen program, its screen read back as text
   (and saved as build/simsmoke/screen.png to look at), then Esc and the
   prompt back.

The driving is pectinos's tools/simdrive.py: it waits for the prompt
before typing, matches only what came after each step, and fails at once
on "Not a jar", "Jam failed to set" and the other refusals. Add steps here
as the program grows; a failure prints the console's last lines, and the
whole log is build/simsmoke/console.log.
"""
import argparse
import os
import re
import sys

KIND = "screen"         # "screen" or "cli", as make new-app wrote it


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--root", required=True, help="the pectinos checkout")
    ap.add_argument("--app", required=True, help="the .JAM to put on the card")
    ap.add_argument("--work", required=True, help="a directory for the card and the log")
    args = ap.parse_args()

    sys.path.insert(0, os.path.join(args.root, "tools"))
    import simdrive

    qemu = os.path.join(args.root, "simmer", "qemu", "bin", "qemu-system-arm")
    kernel = os.path.join(args.root, "build", "sim", "pectinos.elf")
    jarcard = os.path.join(args.root, "build", "host", "jarcard")
    if not os.path.exists(qemu):
        sys.exit("sim-smoke: no SIMMER QEMU; run make -C %s sim-qemu once" % args.root)
    os.makedirs(args.work, exist_ok=True)
    card = os.path.join(args.work, "card.img")
    simdrive.new_card(jarcard, card, [args.app])

    with open(os.path.join(args.work, "console.log"), "w") as log, \
            simdrive.Sim(qemu, kernel, card, log,
                         screen=args.work if KIND == "screen" else None) as sim:
        try:
            sim.boot()
            sim.run("QUINCE /SELFTEST", want=[re.compile(r"selftest: \d+ checks OK")], wait=60)
            if KIND == "screen":
                sim.run("QUINCE JAM", prompt=False, wait=15)
                sim.pause(1)             # let it draw
                text = sim.text()
                sim.snap(os.path.join(args.work, "screen.png"))
                if not any("Hello from JAM." in row for row in text):
                    raise simdrive.SimError("the greeting isn't on the screen",
                                            "\n".join(text))
                sim.keys(b"\x1b", wait=10)                # Esc: the prompt comes back
            else:
                sim.run("QUINCE JAM", want=["Hello from JAM."])
        except simdrive.SimError as e:
            print("sim-smoke: FAILED: %s" % e, file=sys.stderr)
            print(e.out[-2000:] if e.out else sim.tail(), file=sys.stderr)
            return 1
    print("sim-smoke: QUINCE passed its selftest on SIMMER, ran, and left cleanly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
