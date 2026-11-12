# QUINCE

Quake, ported to PectinOS.

A program for [PectinOS](https://github.com/zigphroid/pectinos), the
DOS-like operating system for the Adafruit Fruit Jam, built against a
pectinos checkout at the commit `PECTIN_REV` names.

```sh
make            # build/QUINCE.JAM (CANNER)
make test       # the host suite, under the sanitizers
make test-quake QUINCE_PAK=path/to/id1/pak0.pak
                # Quake on the PC (32-bit: gcc-multilib), in make test too,
                # its e1m1 frames drawn by PB's C model and held to CRCs
make pb-frames QUINCE_PAK=path/to/id1/pak0.pak
                # demo1 drawn by PB's C model: build/pbframes/demo1/*.png
make sim-smoke  # on SIMMER (PectinOS in QEMU), no window
make sim-bench QUINCE_PAK=path/to/id1/pak0.pak
                # Quake's two benchmarks on SIMMER, CANNER's build
make heat       # build/heat/QUINCE.JAM: cold code cut by quince/QUINCE.HOT
make gcc        # build/gcc/QUINCE.JAM: GCC -O2, to compare with CANNER's
                # (sim-smoke, sim-bench and send take either: QUINCE_JAM=...)
make sim        # on SIMMER, in a window
make send       # onto the board's card over USB-C
make os         # the pinned PectinOS onto the board (BOOTSEL)
make preflight  # build/QUINCE.CRT, checked as the store checks it
make submit     # ask for it in the store, for anyone's GROCER FETCH
```

`GETTING-STARTED.md` has the setup from nothing; `CLAUDE.md` is how
Claude works on it; `docs/ROADMAP.md` is where it's going, and
`docs/PB-ASKS.md` is what it asks of the pixelboard for 3D.

## Licence

GPL-2.0-or-later (`LICENSE`), because QUINCE ports id Software's GPL
Quake source, which is in `quake/` (`quake/README.md` says where it came
from and how changes to it are marked). Quake's game data isn't here: it
comes from your own copy of Quake (the shareware `pak0.pak` is enough).
