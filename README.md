# QUINCE

Quake, ported to PectinOS.

A program for [PectinOS](https://github.com/zigphroid/pectinos), the
DOS-like operating system for the Adafruit Fruit Jam, built against a
pectinos checkout at the commit `PECTIN_REV` names.

```sh
make            # build/QUINCE.JAM (CANNER)
make test       # the host suite, under the sanitizers
make sim-smoke  # on SIMMER (PectinOS in QEMU), no window
make sim        # on SIMMER, in a window
make send       # onto the board's card over USB-C
make os         # the pinned PectinOS onto the board (BOOTSEL)
make preflight  # build/QUINCE.CRT, checked as the store checks it
make submit     # ask for it in the store, for anyone's GROCER FETCH
```

`GETTING-STARTED.md` has the setup from nothing; `CLAUDE.md` is how
Claude works on it; `docs/ROADMAP.md` is where it's going.

## Licence

GPL-2.0-or-later (`LICENSE`), because QUINCE ports id Software's GPL
Quake source, which is in `quake/` (`quake/README.md` says where it came
from and how changes to it are marked). Quake's game data isn't here: it
comes from your own copy of Quake (the shareware `pak0.pak` is enough).
