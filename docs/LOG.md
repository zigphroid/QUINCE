# QUINCE decision log

Newest last. One entry per real decision: the date, what was decided, the
alternatives, and why.

## 2026-11-11: scaffolded

`make new-app NAME=QUINCE KIND=screen` in pectinos at bafa5bf wrote the
repo: a screen program, the core in `lib/` (host-testable), the checks
shared by `make test` and `/SELFTEST`, and the build pinned to that commit
(`PECTIN_REV`).

## 2026-11-11: the Quake source, its licence, and M1's shape

Jacob's OK on M1's plan in chat. QUINCE ports id Software's GPL WinQuake
source rather than writing a Quake of its own, so CLAUDE.md's rule 1 ("no
copied code") gets one named exception: `quake/`, brought in unchanged
from `github.com/id-Software/Quake` at `bf4ac42`, every later change marked
`QUINCE:` in the file. The repo is relicensed from MIT to GPL-2.0-or-later
(id's terms), which settles #2. Alternatives: Quakespasm or another later
port (much more code, and further from the renderer id wrote), or a Quake
of our own (not a port). Only the portable files came in; the DOS,
Windows, Linux and Solaris layers and the x86 assembly stayed out
(`quake/README.md`).

M1 times two things, not one: `timedemo demo1` alone flatters a build
that draws nothing (playback is mostly message parsing), so a server
benchmark on e1m1 times QuakeC and the physics too. A GCC `-O2` build is
measured beside CANNER's to separate the compiler's share of the time.
The game data (`pak0.pak`) never goes in the repo.

## 2026-11-12: M1 times GLQuake's renderer over a null GL layer

Jacob, in chat: the pixelboard is to give Glide-style 3D acceleration, and
on the decision card he chose GLQuake's renderer over a null GL layer for
M1, not no renderer at all. A Glide-style card takes screen-space
triangles; the BSP and PVS walk, the polygon lists, the models' vertices,
their transform to the screen and the sending of them all stay on the
RP2350. Leaving the renderer out would time none of that and overstate
the frame rate. So `gl_*` came into `quake/` beside the software renderer,
and the null GL layer transforms each vertex and counts the triangles,
vertices and texture bytes, which gives M1 two more answers: triangles per
frame, and the bytes per second the PB link would need. The alternative,
no renderer, was quicker to build but answers less.

## 2026-11-12: fitting Quake into the board's memory

Quake as id wrote it doesn't fit, and the changes to make it are
QUINCE's first to id's files (each marked `QUINCE:`). Measured, not
estimated: GLQuake's statics are 9.3 MB, most of them texture scratch
sized for 1024x1024 textures and 4 MB of lightmaps, beside a hunk of at
least 5.3 MB; the board's PSRAM pool is 7 MB free (SIMMER, `HEADSPACE`'s
pool), the program area 384 KB, and the stack 16 KB at most.

- **Textures 256x256 at most** (`gl_max_size`), a Glide card's own limit,
  so the upload buffers shrink from 3.7 MB to 512 KB; the scrap holds a
  byte a texel, as its users read it, not 4.
- **32 lightmaps, a byte a texel** (GLQuake's default format): 512 KB.
  Every shareware map loads with 32 (checked on the PC, `start` and
  `e1m1` to `e1m8`).
- **256 model slots**, not 512, as id's own software renderer has: the
  pak has 85 models, and e1m3's brush models need more than 128.
- **The big arrays come from the pool** (`malloc` is `mem_alloc`):
  lightmaps, the scrap, the upload buffers, the model slots. The middling
  ones (8 to 105 KB) are `static` in PSRAM (`QPSRAM`, `lib/qproto.h`,
  the kernel's 1 MB heap), and so are the stack arrays over 8 KB, which
  overflowed the 16 KB stack on SIMMER. Three stack arrays used once a
  map or less (the pak's directory, the sky, a player's skin) are
  allocated for the call, or borrow an upload buffer.
- **A 5.5 MB hunk** (`MINIMUM_MEMORY`), found on the PC: demo1 runs at 5
  MB, but e1m1's player model needs 5.5. The PC test runs at the same
  size, so the PC's counts and the board's agree.

Alternatives: a smaller hunk (5.25 MB fails e1m1), or keeping id's sizes
and cutting the game (no player model). What the tight hunk costs: Quake's
cache gives way sooner, so demo1 uploads 3.8 MB of textures where an 8 MB
hunk uploads 75 KB. That's a real cost on the board, and the counts show
it.

`console.c`'s `-condebug` log is left out on the board (PectinOS's
`open()` takes no POSIX flags), `view.c` loses an unused `extern` whose
type was wrong (CANNER compiles the program whole and refuses it), and
the calls id made before declaring a function get prototypes
(`lib/qproto.h`), since CANNER, as C99, has no implicit declarations.

## 2026-11-12: Quake's file structs and short enums

On SIMMER, CANNER's build misread `player.mdl`. Every skin and frame
came out 3 bytes short, and the run failed in `Mod_LoadAliasGroup`. The
cause is not a CANNER bug. On arm-none-eabi an enum is only as big as its
values need. That is GCC's default there, CANNER matches it
(`apps/canner/README.md`, "Types"), and the `type` fields Quake declares
as enums in its model and sprite file structs became 1 byte. The files
hold 4. The decision: those fields become `int` (`quake/modelgen.h`,
`quake/spritegn.h`, marked `QUINCE:`), and the PC build takes
`-fshort-enums`, so `make test-quake` sees the board's sizes and fails
without the fix.

Alternatives: build with 4-byte enums (CANNER has no such switch, and the
SDK's ABI fixes the size), or read the fields with `LittleLong` on a
`byte *`, which changes more of id's code than changing the types does.

## 2026-11-12: a 5.75 MB hunk and 20 lightmaps

The PR 2 review found that E1M4 fails at the 5.5 MB hunk
(`Cache_TryAlloc: 428688 is greater then free hunk`). The hunk is now
5.75 MB (`MINIMUM_MEMORY` 0x5C0000), and every shareware map loads at
that size on the PC. `make test-quake` and `make sim-bench` load each map,
the board's build on SIMMER too. To make room in the 7 MB pool,
`MAX_LIGHTMAPS` goes from 32 to 20, which saves 192 KB. The maps need 13
to 16: on the PC, E1M2, E1M3 and E1M4 fail at 12 and all pass at 16. On
SIMMER, a 6 MB hunk leaves too little of the pool for the lightmaps even
at 20, so 5.75 MB is the most that fits (inferred from a few sizes tried).

The same review corrected one figure. The 75 KB that demo1 sends at an
8 MB hunk are lightmap updates (`glTexSubImage2D`), not textures. Those
updates change as the game runs, so they cross the PB link every frame.
They are now counted apart from textures and added to the link's rate.

Alternatives: keep 5.5 MB and say which maps fit, which would drop a
shareware level; or shrink the upload buffers, which a 256x256 texture
needs whole.

## 2026-11-12: M1's board run: Quake is worth going on with

These are the first timings on the Fruit Jam itself. The run used PectinOS
at the pin (bafa5bf, API 63, flashed for it on Jacob's OK) and QUINCE at
0844958, built by CANNER. The pak went to `J:\LARDER\QUINCE\ID1` and is
not committed. The log of every command is on Jacob's Mac in
`~/src/quince-board-run/`.

| | frames | seconds | fps | ms/frame | tris/frame | verts/frame | PB link |
|---|---|---|---|---|---|---|---|
| `/TIMEDEMO DEMO1` | 969 | 29.35 | 33.0 | 30.3 | 2,094 | 3,953 | 3,571 KB/s |
| `/SERVER E1M1 300` | 300 | 6.47 | 46.4 | 21.6 | 1,103 | 2,167 | 2,799 KB/s |

- `/SELFTEST` passed 41 checks. The server's totals (330,776 tris, 650,152
  verts) are the same as the PC's and SIMMER's. The timedemo's totals vary
  a little from run to run, because the client interpolates by the clock.
- What the link carries:
  - Almost all of it is vertices, at 28 bytes each.
  - Lightmap updates are 2 KB/s in demo1 and 50 KB/s in e1m1.
  - About 12 MB of textures cross once at load (demo1 952 uploads, e1m1
    1,119, mip levels included). None are reloaded during a run at the
    5.75 MB hunk.
- THERMO, at 4 kHz, on the timedemo:
  - The null GL's vertex transform (`vertex`, `glVertex3fv`) is about 15%
    of the program's time, and `R_RecursiveWorldNode` 5%.
  - `softfp.c`'s double arithmetic (`__aeabi_dmul`, `unpack`, `pack64`) is
    about 9%: Quake's double constants promote float expressions.
  - Texture resampling and mipmaps (`GL_ResampleTexture`, `GL_MipMap`,
    `GL_Upload8`) are about 10%. That is all at load, and it leads the
    short server run (23%).
  - The kernel and interrupts take 13% (timedemo) and 22% (server).

What it says: the RP2350 runs Quake's game and the CPU's half of its
renderer at about 30 fps in demo1, behind the XIP cache, with no tuning.
The PB's half is rasterizing about 2,000 triangles a frame and taking about
3.6 MB/s over its link. Both are inferred to be within a Glide-class
card's reach, and the link's real capacity has yet to be measured. The
cheapest CPU wins in view:
- float constants where id wrote doubles;
- mip levels made by the PB, not the CPU, which affects load time only;
- the hot code in the jar by THERMO's heat file, still to do in PR 3.

THERMO's heat files from these two runs (`J:\QTD.HOT`, `J:\QSV.HOT`)
came off the board over the console (`TASTE`, through `jamterm.py`), and
`heat.py sum` added them into `quince/QUINCE.HOT`, which drives the cut
below.

## 2026-11-12: the heat cut and GCC's -O2 build on the board

All three builds were timed on one boot of the pinned PectinOS (bafa5bf,
flashed on Jacob's OK), QUINCE at 06cfd17, in the order heat, GCC, plain.
Each passed `/SELFTEST`. The logs are on Jacob's Mac in
`~/src/quince-board-run/builds2/`.

| build | jar (code and data) | `/TIMEDEMO DEMO1` | `/SERVER E1M1 300` |
|---|---|---|---|
| CANNER, plain (`make`) | 251 KB | 30.9 ms, 32.3 fps | 22.1 ms, 45.3 fps |
| CANNER, heat cut (`make heat`) | 122 KB, plus 143 KB cold | 31.8 ms (+3%) | 23.6 ms (+7%) |
| GCC -O2 (`make gcc`) | 254 KB | 24.0 ms (-22%) | 19.4 ms (-12%) |

- **The heat cut (Jacob chose: time it, and report the room it frees).**
  Its 64 KB budget kept 240 functions, which held more than 99% of the
  samples. It moved the other 843 to PSRAM and freed 129 KB of the
  384 KB program area. Quake's code already runs from SRAM, so a cut
  can't make it faster. It only costs something, and that cost is
  small: 3% and 7%. Like CANNER's own cut (3.7% for 81 KB freed,
  pectinos's LOG 2026-10-31), the cost is the cold code that still runs,
  now behind the XIP cache. Why the server run pays more than the
  timedemo isn't known: its clock starts after the map has loaded, so it
  isn't the load. The freed room is
  for hot data, which today sits in PSRAM behind the same cache. THERMO
  samples where the code is, not what it reads, so which data goes
  there needs its own plan.
- **GCC -O2** is the reference for CANNER's code: 22% less time a frame
  on the timedemo, and 12% on the server run. Its code is 15% bigger
  than CANNER's (228 KB against 198 KB), but it still fits. The build needed two things of its
  own:
  - QUINCE's `quince/libc/` comes first on the include path, as in
    CANNER's build. The SDK's `math.h` has no `ceil`, so GCC took the
    call as C89's implicit `int ceil()`, and map loading failed with
    "Bad surface extents". `-Werror=implicit-function-declaration` now
    catches that.
  - `setjmp` and `longjmp` are GCC's built-ins (`quince/libc/setjmp.h`).
- **Run to run.** The plain build was about 2% slower than in the first
  run (30.9 against 30.3 ms on demo1, 22.1 against 21.6 on e1m1),
  and the server's totals were the same in every build and every run.
  The timedemo's totals vary, because the client interpolates by the
  clock. So their checksums vary too, and only its frame count is fixed
  (969).

What it says: CANNER's code is within about a quarter of GCC's on Quake,
and both are well inside the frame budget for a first renderer. The heat
cut is cheap, and it frees room the PB work can use. Alternatives: a
smaller budget (32 KB, about 95% of the samples) would free more room at
a larger cost. Leaving the cut until a renderer milestone would have left
its cost unmeasured.

## 2026-11-12: what QUINCE asks of PB for 3D

Jacob took every recommendation of the PB plan (`docs/PB-ASKS.md`): a
Voodoo-class engine on PB, fed screen-space triangles. The Jam
transforms, clips and projects, and PB sets up and draws. Drawing is
640x480 RGB565 with 16-bit depth, shown at x1.5 on 720p. Wall and
lightmap are two textures a pixel, in one pass. Walls are point sampled
and lightmaps bilinear. Textures go up as 8-bit indices, and PB expands
them and builds the mips. It is all new `DRAW` commands on today's link.
QUINCE writes the triangle engine's C model first, as the spec that runs.

Alternatives: transforms on PB (B), or PB holding the world and walking
it (C). Both save more of the Jam's time (the vertex transform was
about 15% of M1's timedemo), but need a matrix unit and clipping in
gateware, and the 60K's DSP blocks are untried; B or C is decided after
A's board timings. A second blended lightmap pass needs no second texture
unit, but it sends the world twice and reads the picture back. RGBA
uploads keep PB simpler, but cost 13 MB and the Jam's load-time work
(about 10% of the timedemo), against about 2.5 MB.

Why: M1 says the link has room (4 MB/s against 37.9 MB/s) and the Jam's
CPU doesn't, so PB takes the work that is per-pixel and load-time, and
what is cheapest to build comes first. The plan names the one real limit
on the link, flow control over I2C (about 4 MB/s, inferred), and asks
pectinos for a deep queue or a faster bus.

## 2026-11-12: M2, Quake's hot math in float (the plan)

Jacob took every recommendation. M1's profile (SIMMER, `-icount`,
THERMO, project files `quince/canner-vs-gcc/`) had doubles in 27% of
GCC's demo1 instructions and 40% of CANNER's. Quake's vectors are
`float`, but C promotes: `v * 0.5` is a double multiply, and
`sqrt(x*x + y*y)` a double square root, each a call into `softfp.c`
because the M33's FPU is single precision only (RP2350 datasheet
§3.6.4). GCC's `-Wdouble-promotion` finds 289 such places in the files
QUINCE builds.

- **Scope: the hot functions only**, chosen by the profile, file by file,
  each step measured. Alternatives: all 289 places, mechanical but
  touching cold load code where precision matters (gl_model.c's
  `floor(mins/16)` surface extents) for no speed; or no edits to id's
  files, with `#define sqrt sqrtf` and GCC's
  `-fsingle-precision-constant`, which CANNER hasn't got, and which
  would quietly make the clocks float too.
- **The clocks stay double** (`realtime`, `cl.time`, `sv.time`,
  `host_frametime`): a float clock loses its fractions after hours.
- **Correctness**: the PC rounds floats as the board does, and e1m1's
  state after 300 frames is a gate between the PC and SIMMER, not only
  printed. Float against double on the PC is reported, not gated:
  physics after collisions is chaotic, so small differences may grow.
- **Two PRs**: ours first (below), then id's files.

## 2026-11-12: M2 PR 1, our side of the float work

- **`quince/libc/math.h` was missing the SDK's built-ins.** The SDK's
  `math.h` spells `sqrtf`, `fabsf`, `floorf` and `ceilf` as
  `__builtin_sqrtf` and so on (on `__arm__`), because `-ffreestanding`
  stops GCC knowing the names and CANNER only makes the instruction for
  the built-in. Ours declared them as plain functions, so `glRotatef`'s
  `sqrtf` was a call to `sdk/lib/math.c`'s integer square root. Now each
  is one instruction. On SIMMER (instructions, deterministic), demo1 went
  from 14.5 to 14.2 ms a frame in CANNER's build (THERMO samples 71,860
  to 70,596, -1.8%) and 10.3 to 10.1 in GCC's (-1.8%); the server run
  moved under 0.5%. softfp is untouched: that is PR 2.
- **The PC rounds floats as the board does.** `-m32`'s default is the
  x87, which keeps values in 80 bits until they are stored, so the PC's
  float and double results can differ from the M33's FPU and
  `softfp.c` in the last bits. The PC build is now `-msse2 -mfpmath=sse
  -ffp-contract=off` and links the SDK's own `math.c` (and softfp.c's
  packing) ahead of `-lm`. Checked: with the x87, e1m1's null-GL
  checksum was -3593997312; with SSE it is -3592846848, which CANNER's
  and GCC's builds on SIMMER print too. glibc's math against the SDK's
  made no difference to it yet, but PR 2's `sinf` and `cosf` will
  depend on it. `asin` (mirrors only) is still glibc's.
- **`/SERVER` prints the game's state**: an FNV-1a hash of each entity in
  use, its origin and angles as the network quantizes them (eighths of
  a unit, 256ths of a turn: `MSG_WriteCoord`, `MSG_WriteAngle`), its
  frame and its health. `make test-quake` and `make sim-bench` both hold
  e1m1's 300 frames to `c8c17318, 164 entities` and checksum
  -3592846848, beside the triangle totals; the PC, CANNER's build and
  GCC's all print them. Alternatives: the triangle totals alone, which
  are what a float change is likely to leave the same while the game
  differs; a dump of every entity, compared by a script, which says more
  about where two runs part, but which a hash and the PC's own dump can
  do when that's needed.

