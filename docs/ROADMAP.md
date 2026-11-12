# QUINCE roadmap

Each milestone ends in something that runs on the board. Plan each one in
chat before its code; update this file in the PR that lands or reorders a
milestone.

## M0: frame (done 2026-11-11)

Scaffolded by pectinos's `make new-app` at bafa5bf: `QUINCE.JAM` builds,
its host test and `/SELFTEST` pass, `make sim-smoke` runs it on SIMMER.

## M1: Quake with nothing drawn, timed on the board (done 2026-11-12)

Planned 2026-11-11. Can the RP2350 run Quake's game and the CPU's share of
its renderer, before anything is drawn? id's source (`quake/`) built by
CANNER with GLQuake's renderer (`gl_*`) over a null GL layer: every `gl*`
call draws nothing, but the layer transforms each vertex to the screen
(the work a Glide-style card, as the pixelboard is to be, leaves to the
CPU) and counts the triangles, vertices and texture bytes a card would
have been sent. Sound and CD are stubs, the network loopback only, and the
hunk a `mem_alloc` block in PSRAM. Two timings on the board, each with
THERMO's profile:

1. `timedemo demo1`: the client side (demo playback and the renderer's
   CPU work).
2. A server benchmark: e1m1 loaded and a fixed number of server frames run,
   which times QuakeC and the physics that a demo skips.

Beside the timings, M1 reports the triangles and vertices per frame and
the bytes per second the PB link would have to carry at that frame rate:
the vertices and the lightmaps' updates, with the textures apart, sent
once (the PB keeps them all in its 512 MB).
A GCC `-O2` build is timed beside CANNER's, for measuring only. Drawing on
the pixelboard is a later milestone.

- PR 1: id's source in `quake/`, unchanged (both renderers), and the
  licence (GPL-2.0 or later) and rule 1's exception for it.
- PR 2: the null-GL build, on the PC first (`make test-quake`, against
  `pak0.pak` when `QUINCE_PAK` names one, else skipped), then CANNER's,
  run on SIMMER (`make sim-bench`), with Quake's memory fitted to the
  board's (LOG 2026-11-12).
- PR 3: the board run, with `id1/pak0.pak` on the card: the timings, the
  counts and the profiles, and what they say about feasibility. Done
  2026-11-12 (LOG): demo1 at 33 fps, e1m1's server at 46 fps, the link
  at about 3.6 MB/s. Then the same timings with the code cut by THERMO's
  heat file (`make heat`: 129 KB of the program area freed, 3% and 7%
  slower) and GCC's `-O2` build beside them (`make gcc`: 22% and 12%
  faster).

## Later: drawing on the pixelboard

Planned 2026-11-12 (`docs/PB-ASKS.md`; Jacob, all recommendations). PB
draws Quake as a Voodoo-class card would: the Jam transforms and sends
screen-space triangles, and PB rasterizes them at 640x480 RGB565 with
depth and two textures a pixel. QUINCE's steps come first and need no new
gateware: the real GL layer, still drawing nothing, timed on the board,
then a C model of the triangle engine drawing demo1's frames on the PC.
PB's side (flat triangles, then textures, then lightmaps) is pectinos's
work, to be asked for in an issue there once Jacob hands it over. Each
step is planned in chat first.
