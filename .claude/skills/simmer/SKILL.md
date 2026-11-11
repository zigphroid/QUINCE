---
name: simmer
description: Run QUINCE.JAM on SIMMER (PectinOS in QEMU) - the smoke test, its screen as text and as a picture, and bisecting a CANNER miscompile. Use when a change needs checking beyond `make test`, when SIMMER and the host disagree, or when asked to run QUINCE without the board.
---

# QUINCE on SIMMER

`make test` proves the logic on the host. SIMMER proves the .JAM loads and
runs under PectinOS with CANNER's code. The board proves the real
hardware: timing, the USB keyboard, sound, WiFi (the `pectin-board`
skill).

## Setup

Cloud sessions: `.claude/hooks/session-start.sh` does all of it (Arm's GCC,
QEMU's build packages, `make sim-qemu` and `sim-build` in `$PECTIN_ROOT`).
If its last line says SIMMER setup failed, read the hook's output before
trying by hand.

A computer of your own: `make -C $PECTIN_ROOT sim-qemu` once
(`$PECTIN_ROOT/docs/SIMMER.md`, "Setting it up"). The Makefile's SIMMER
targets bring the kernel up to date themselves (`sim-kernel`).

## Checks, cheapest first

| Command | What it proves | Pass looks like |
|---|---|---|
| `make sim-smoke` (~15 s) | The .JAM loads, `/SELFTEST` passes on CANNER's code, the program runs and Esc leaves cleanly | `sim-smoke: QUINCE passed its selftest on SIMMER, ran, and left cleanly`; a full-screen program's screen is in `build/simsmoke/screen.png`. Look at it after changing the screen |
| `make sim` | Anything, by hand: the screen in a window, the serial console in the terminal | What you see |

`tools/simsmoke.py` drives SIMMER with `$PECTIN_ROOT/tools/simdrive.py`.
Grow the smoke test there, step by step, rather than writing a new driver:
`sim.run(line, want=[...])` types a line and waits for its output and the
prompt; `prompt=False` leaves a program running; `sim.keys(b"\x1b")` sends
keys as a terminal would (simdrive's docstring lists the modified keys);
`sim.text()` is the screen's rows as text and `sim.snap(path)` a PNG, for
a full-screen program whose drawing never reaches the serial console.
Each step returns as soon as it has its answer and fails at once on a
refusal (`Not a jar`, `Jam failed to set`). A smoke test that takes
minutes means a wait timed out: read `build/simsmoke/console.log`, don't
raise the timeout.

## Listening

SIMMER plays the board's sound stream (`$PECTIN_ROOT/docs/SIMMER.md`, "The
sound"), so a program's sound can be recorded headless:

```
python3 $PECTIN_ROOT/tools/simdrive.py --qemu $PECTIN_ROOT/simmer/qemu/bin/qemu-system-arm \
    --kernel $PECTIN_ROOT/build/sim/pectinos.elf --jarcard $PECTIN_ROOT/build/host/jarcard \
    --put build/QUINCE.JAM --work build/simwav \
    --line QUINCE --running --wav build/simwav/QUINCE.wav --seconds 3
```

It prints when the sound started and stopped, its peak, and the underruns
the card counted. `--fills` records each refill once, without the buffers
an underrun played twice. In `tools/simsmoke.py`, `simdrive.Screen.capture(path)`
before a step and `Screen.capture("")` after record the same (it needs
`screen=`); `simdrive.read_wav`, `sounding` and `period_pitch` check a
pitch and a level. Don't pin a CRC of the file: where a sound lands moves
by a buffer from run to run. A mix's CRC belongs in the host test.
Underruns on SIMMER are mostly the host's timing; whether the program
feeds its ring in time, and how it sounds, is the board's question.

`Not a jar. (needs a newer PectinOS)` means the SIMMER kernel is older
than the CANNER that built the .JAM: `make sim-kernel`.

## When SIMMER and the host disagree

The host build is the reference. If `make test` is clean and SIMMER is
wrong, suspect CANNER before the logic:

1. Build with `make CANNER_FLAGS=-O0`. If the bug goes away, bisect with
   `CANNER_FLAGS=-Olimit=N` (`$PECTIN_ROOT/apps/canner/README.md`): N names
   the first miscompiled function in CANNER's write order.
2. If it's the same at `-O0`, it may not be CANNER at all: first look for
   code the host suite never runs (everything in `quince/QUINCE.c`).
3. A real CANNER bug: shrink it to a standalone case and file it as an
   issue on zigphroid/pectinos. Never change the pectinos checkout from
   here.
