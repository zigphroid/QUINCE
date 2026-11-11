---
name: pectin-board
description: Test QUINCE on the real Fruit Jam from the computer it is plugged into - put the .JAM on the card, run commands over the serial console, read the screen as text or a picture, record the sound, and reflash PectinOS. Use when a change needs the real hardware, or Jacob asks for a board check.
---

# QUINCE on the board

The board is a Fruit Jam running PectinOS, plugged into Jacob's computer
over USB-C. A cloud session can't reach it: board work runs in a Claude
Code session on that computer (local, or a Remote Control session started
from the cloud one), in this repo's checkout there. Close `tio` (or any
terminal on the board's ports) first: it steals the reads.

Test with these tools yourself; ask Jacob only for what a script can't
judge (does it look right, does it sound right, how it feels, anything
that needs hands on the hardware), saying exactly what to run and what
they should see.

| Step | Command |
|---|---|
| Put the new .JAM on the card | `make send` |
| Run a command, print what comes back | `python3 $PECTIN_ROOT/tools/jamterm.py -s 'QUINCE /SELFTEST' --until prompt` |
| Send keys (Esc, arrows, Ctrl+...) | `jamterm.py -s` with the bytes a terminal sends (`$PECTIN_ROOT/docs/LLM-INTERFACE.md` B3 lists the modified keys) |
| The screen as text | `make -C $PECTIN_ROOT view ARGS=--text` |
| The screen as a picture | `make -C $PECTIN_ROOT view ARGS="--snap $PWD/build/board.png"`, then look at it |
| The sound | `make -C $PECTIN_ROOT view ARGS="--wav $PWD/build/board.wav --seconds 5"`; `python3 $PECTIN_ROOT/tools/wavnotes.py build/board.wav` lists its notes |
| Reflash PectinOS at the pin | `make -C $PECTIN_ROOT bootsel`, then `make os` |

`jamterm.py` needs pyserial (`python3 -m pip install pyserial`). One
viewer at a time: if `make view` says another program has the port open,
it's Jacob's browser page or tio.

The board must run the PectinOS of `PECTIN_REV` (`make os`): one older
than the CANNER that built the .JAM refuses it with `Not a jar. (needs a
newer PectinOS)`. Build serial logging into the program early (`printf`
to the console), so a failure on the board shows up in `jamterm.py`'s
output rather than only on the screen.
