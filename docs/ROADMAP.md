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

## M2: the GL layer to PB, still drawing nothing (done 2026-11-12)

Planned 2026-11-12 (`docs/PB-ASKS.md`, step 1; Jacob, all
recommendations). M1's null GL grows into the real layer: it writes the
commands PB would be sent, exactly, so PB-ASKS's inferred byte counts
become measured ones. PB's 3D service doesn't exist yet, so the board
counts the commands and drops them; the PC can write them to a file for
the C model to draw.

- PR 1: the wire format (`lib/pb3d.h`: the new `DRAW` commands, the
  vertex layouts, the dump's records) and its encoder (`lib/pb3d.c`),
  held to exact words by the shared checks.
- PR 2: `lib/pbgl.c` in place of `lib/nullgl.c`: fans, strips and lists,
  clipping at the near plane and the guard band, state sent only when it
  changes, wall and lightmap in one pass, lightmap updates held to one
  band a block a frame, and textures as 8-bit indices through a
  `QUINCE:` change in `GL_Upload8`. `QUINCE_PBDUMP` writes the stream on
  the PC, and `tests/pbcheck.py` reads it back. Checked on the PC and on
  SIMMER: demo1 sends PB 32 KB a frame (LOG 2026-11-12).
- PR 3: the board run: demo1 at 45.8 ms a frame in CANNER's build
  (with M3's floats), against M1's 30.9 with the null GL. Making the
  commands costs the Jam about 23 ms a frame, most of it in a few small
  functions (`outcode_of` alone is 17%), and a new `quince/QUINCE.HOT`
  (LOG 2026-11-12).

## M3: Quake's hot math in float (done 2026-11-12)

Planned 2026-11-12 (LOG; Jacob, all recommendations). M1's profile
found doubles in 27% of GCC's timedemo instructions and 40% of CANNER's:
Quake's vectors are float, but its double constants and its calls to
`sqrt`, `sin` and the rest turn float sums into softfp calls, since the
M33's FPU is single precision only. M3 makes the hot code float, and
checks that the game still plays the same way on the PC and on SIMMER.
Measured with SIMMER's instruction counts; the board's timing waits
until the Jam is free.

- PR 1: our code only. `quince/libc/math.h` makes `sqrtf` and the
  rounding functions their one FPU instruction; the PC build rounds
  floats as the board does (SSE, nothing fused, the SDK's `math.c`);
  `/SERVER` prints a state hash, which the PC and CANNER's build on
  SIMMER must agree on exactly, with the null GL's checksum (since M2 PR 2,
  the checksum of PB's commands).
- PR 2: id's hot functions to float (`QUINCE:` edits): float constants,
  `sqrtf`, `sinf`, `cosf`, and the clock taken as a float once where a
  loop compared against it. The clocks themselves stay double. `make
  test-float` fails if a function on the hot list calls softfp again.
  On SIMMER, demo1 went from 14.2 to 7.7 ms a frame in CANNER's build and
  e1m1's server from 7.9 to 6.7; the game's state is the same as in
  double (LOG 2026-11-12).
- Then the board's timing, in M2 PR 3's run: with the GL layer, demo1
  went from 53.9 to 45.8 ms a frame in CANNER's build (-15%) and from
  41.3 to 38.1 in GCC's (-8%) (LOG 2026-11-12).

## M4: the GL layer made cheap

Planned 2026-11-12 (Jacob, all recommendations). M2's board run had our
own GL layer at 47% of the program and 23 ms of demo1's frame, most of
it in four small functions (LOG 2026-11-12). M4 makes them cheap without
changing what PB is sent, but for q's rounding, which was a step high
for odd values past 2^23 and is now right.

- PR 1: `outcode_of` as compares, the guard band's tests only off the
  screen; `fix` through `roundf`'s one FPU instruction; the checksum two
  words a step. Done (#30): on the board CANNER's demo1 is 46.4 to 38.3
  ms a frame (-17%), level with GCC's before M4, and e1m1 32.9 to 28.9.
- PR 2: the board run's write-up, and GCC's build made to fit the
  program area again (the selftest's 4 KB to PSRAM; `make gcc` checks
  it now).

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

Step 2, the C model, planned 2026-11-12 (Jacob, all recommendations):
- PR 1: the model alone (`lib/pbmodel.c`, its rules in `docs/PB3D.md`),
  held to them by `tests/test_pbmodel.c`, with a scene it draws in
  `build/pbmodel/scene.png`.
- PR 2: the model reads the GL layer's commands (`lib/pbdecode.c`), and
  `tests/pbplay.c` draws a `QUINCE_PBDUMP` file with it. `make test-quake`
  draws e1m1's dump and holds the pictures to their CRCs; `make
  pb-frames` draws demo1's, dumped on a fixed clock, to pictures with
  the fill numbers (about 40 seconds, so not in `make test`). Drawing them found two
  gaps on the GL side, fixed here: id's `GL_Init` state, and `CLIP`, PB's
  clip rectangle, a sixth command.
