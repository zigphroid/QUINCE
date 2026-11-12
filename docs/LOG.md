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

## 2026-11-12: PB's wire format (M2 PR 1)

Jacob took every recommendation of the M2 plan (the GL layer to PB, still
drawing nothing): `GL_Upload8` hands Quake's 8-bit textures to our layer
through one `QUINCE:` change, unit 0's texel coordinates are 12.4, and
`lib/nullgl.c` is replaced, not kept beside the new layer. PR 1 writes
the format down as `lib/pb3d.h`, with an encoder and exact-word checks,
before the GL layer, so the C model's thread can read the stream
through the header while the GL layer is still being built.

Changed from `docs/PB-ASKS.md`'s first cut, agreed with the C model's
thread:
- s, t are 12.4, not 10.6. GLQuake allows surfaces 512 texels across
  (`gl_model.c`), and after the Jam takes whole texture sizes off, a
  coordinate can reach about 768, past 10.6's 512. 12.4 holds 2,048, and
  a sixteenth of a texel is plenty for point-sampled walls (the model
  interpolates s q and t q). Lightmaps stay 10.6: 128 wide, and bilinear
  wants the fraction.
- The depth range is its own command, `DEPTHRANGE` (near and far in the
  stored depth's units), not bits of `STATE`. Scaling q on the Jam would
  move the weapon in front but bend its perspective; PB maps the depth
  it makes from q instead. It changes about twice a frame, so it stays
  out of `STATE`'s one word.
- A model's grey colour takes a word, so its vertex is 14 bytes, not 13:
  the commands are 16-bit words.
- Textures have a format byte: 8-bit indices, a grey byte (lightmaps),
  or 32-bit RGBA for the few that aren't palette colours (the particle
  dot). PB or the model turns RGBA into its own texels.

Alternatives: 10.6 with the Jam splitting wide polygons (Jam time, more
vertices); the weapon's depth by a scale on q alone (no offset, so it
can't put the weapon in front of a wall it is inside). The numbers 15 to
19 are QUINCE's proposal; pectinos owns PB's command numbers and may
change them when it builds the 3D service.

## 2026-11-12: M3, Quake's hot math in float (the plan)

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

## 2026-11-12: M3 PR 1, our side of the float work

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
  depend on it. `asin` (mirrors only) is still glibc's. The PC build is
  `-fno-builtin` too, as the board's `-ffreestanding` is: without it GCC
  had turned a `sin` and a `cos` of one angle into a call to glibc's
  `sincos`, in `AngleVectors` and seven other functions, so the SDK's
  `sin` and `cos` weren't the ones running there. It changed neither
  e1m1's state nor its checksum.
- **`/SERVER` prints the game's state**: an FNV-1a hash of each entity in
  use, its origin in eighths of a unit (as `MSG_WriteCoord` sends it),
  its angles in 256ths of a turn (finer than `MSG_WriteAngle`, which
  drops the fraction of a degree first), its frame and its health.
  `make test-quake` and `make sim-bench` both hold
  e1m1's 300 frames to `c8c17318, 164 entities` and checksum
  -3592846848, beside the triangle totals; the PC, CANNER's build and
  GCC's all print them. Alternatives: the triangle totals alone, which
  are what a float change is likely to leave the same while the game
  differs; a dump of every entity, compared by a script, which says more
  about where two runs part, but which a hash and the PC's own dump can
  do when that's needed.
- **`make test-builtins`** keeps the `math.h` fix: CANNER compiles
  `tests/fpu_builtins.c` through `quince/libc/math.h`, with the SDK's
  `math.c` beside it as in the real build, and fails if `sqrtf`,
  `fabsf`, `floorf` or `ceilf` is a call. The state can't catch it,
  since `math.c`'s `sqrtf` is correctly rounded too, only slower.
  Checked: with the built-ins taken out of the header, all four fail.

## 2026-11-12: the triangle engine's rules (PB-ASKS step 2, PR 1)

QUINCE's C model of PB's triangle engine (`lib/pbmodel.c`) is the spec PB
draws to. Its rules are in `docs/PB3D.md`. Jacob took all five
recommendations of the plan:
- **Perspective:** a divide every pixel, rather than every 8 with straight
  steps between. Each vertex weighs its edge function times its q, and one
  reciprocal of the weights' sum serves every attribute, with no divides
  at setup. A divide every 8 pixels saves PB the reciprocal unit, but adds
  a second way of stepping and can swim.
- **Depth:** a 16-bit float of q (4 bits of exponent, 12 of mantissa), as
  Voodoo's w-buffer, rather than linear z, which spends nearly all its
  precision near the eye over Quake's 4 to 4,096 units.
- **The mip level:** one per triangle, from its texel and pixel areas,
  rather than per pixel, which costs more multipliers. The scene shows the
  cost: a long wall gets one level end to end, and its far end shimmers.
  If Quake's frames show it too, per pixel is the change to make.
- **Colour:** 8 bits a channel inside and RGB565 out, with no dithering
  yet.
- **The lightmap:** GLQuake's 1x, with software Quake's 2x left as a later
  STATE bit.

The model's own choices, each one a rule PB has to keep:
- **The top-left rule** for pixels on an edge, so a shared edge is drawn
  once.
- **Colour is perspective-correct** like the texture coordinates, since
  the same reciprocal makes it free. Gouraud in screen space would need a
  second divide.
- **Textures with see-through texels are kept as ARGB1555,** and the rest
  as RGB565. 565 has no bit to spare, and a colour key breaks under mip
  averaging. Only HUD pictures, sprites and the sky's top layer lose a bit
  of green.
- **Mips are made from the level above as stored,** not from the palette's
  8 bits, so PB can build them in place. A mip texel is see-through if two
  or more of its four are (GLQuake's alpha test passes three of four).
- **Weights are 1/256** (w(x) = x + x/128), so alpha 0 and 255 give exactly
  the picture and exactly the source.
- **Exact integer rules,** with the widths said: 64-bit edge functions
  (35 bits in the guard band), weights cut to 40 bits, reciprocals as
  floors. Floats would leave the gateware no way to match bit for bit.

The model isn't in QUINCE.JAM, since PB draws, not the Jam. Its checks are
a host test (`tests/test_pbmodel.c`), and `make` compiles it with CANNER
so it stays inside the subset pectinos can take. The checks were tried
against rules broken on purpose in the model: a dozen at first, and the
review's eight more that had slipped through (depth written by a pixel the
alpha test drops, a mip with a side of 1, negative repeats, s and t
clamped apart, the lightmap and alpha, unit 1's mip level, the depth
range's +1, MULINV's weight). Each break now fails at least one check.

The review also found the per-pixel reciprocal held at 32 bits when the
weights' sum was 256 or less, which happens on slivers far away. The sum
is now always brought to 16 bits, moved up when it is short as well as cut
when it is long, so the reciprocal is always 25 bits and needs no clamp.

## 2026-11-12: the GL layer to PB, measured (M2 PR 2)

`lib/pbgl.c` replaces M1's null GL. It writes PB's commands (`lib/pb3d.h`)
for every frame, and the timings print what it sent. On the PC, demo1
averages 16,400 words a frame (32 KB): 466 `TRIS` holding 2,412 vertices
and 1,293 triangles, 7 `STATE`, 56 `TEXTURE`, and 2 `DEPTH` and
`DEPTHRANGE`. Vertices are 93% of the words. At M1's board rate of 33
frames a second that is 1.1 MB/s, and 1.9 MB/s at 60, against PB-ASKS's
inferred 2.8 and M1's 6.5. Load is 151 textures, 2.6 MB of palette
indices, none as RGBA, against M1's 13 MB with mips. e1m1's 300 server
frames are the same on the PC and on SIMMER, word for word: 2,836,051
words, one checksum. That took the PC's build off the x87: with its
80-bit registers the word counts matched but the words didn't, so the
PC's Quake now keeps floats in SSE registers (`-msse2 -mfpmath=sse`),
rounding as the board's FPU does. (M3 PR 1, merged first, went on to
make the PC's floats the board's bit for bit; the counts here are
re-measured on top of it.)

Where the bytes went:
- **One pass.** With `gl_mtexable` on, GLQuake draws wall and lightmap
  together (`R_DrawSequentialPoly`), so the world goes once. GLQuake's
  own counts drop too: e1m1's 300 frames ask for 186,776 triangles, not
  330,776, so `test-quake`'s and `sim-bench`'s totals moved.
- **Culling.** About 240 primitives a frame are wholly off the screen or
  behind the eye and aren't sent. GLQuake culls BSP nodes by their boxes,
  not each surface.
- **Cutting.** About 4 primitives a frame cross the near plane or run past
  the guard band. Each is cut a triangle at a time, the ones inside going
  as one list and each cut one as a small fan, rather than cutting the
  whole polygon. That needs buffers for 8 vertices, not 2 x 264, and it
  costs a few bytes on a few primitives.
- **Lightmaps.** The one-pass path updates a block each time it draws a
  surface a dynamic light touched, so e1m1's 300 frames made 2,146
  updates (1.7 MB). The layer holds them to the frame's end and sends
  one band of rows a block: 148 updates, 331,520 bytes, M1's figures
  exactly. The texels are read at the frame's end from GLQuake's own
  copy, which by then holds every change.
- **Particles** are the biggest frames: each is a list triangle of 30
  words (q, s and t, RGBA), so an explosion's 2,000 particles take 60,000
  words. The largest demo1 frame is 77,401 words, and the board's buffer
  is 128 K words (256 KB) from the PSRAM pool. With the texture table and
  the scratch it is 328 KB, about what is freed by the
  320 KB of id's RGBA upload buffers no longer allocated, since
  `GL_Upload8` now hands its indices straight to the layer. A strip or a
  particle list longer than 256 vertices is sent in pieces.

Smaller decisions:
- GL's filters and wraps are ignored. Walls are point sampled and
  lightmaps bilinear and clamped (PB-ASKS, decision 4), so `TEXTURE`
  carries PB's choice, not GL's.
- The colour buffer isn't cleared, since GLQuake draws its whole view
  (`gl_clear` 0). The depth is cleared each frame.
- RGBA uploads that GLQuake makes itself (the sky, a player's translated
  skin) are turned back into indices through a table from colour to
  index. Only a texture with a colour off the palette goes as RGBA, and
  in the shareware maps none does.
- `quince/QUINCE.HOT` names M1's functions. `make heat` still builds, but
  it puts the new layer in the cold code until a board run makes a new
  heat file (PR 3).

Alternatives: cutting whole polygons (fewer bytes on a crossing
primitive, more SRAM), and sending each lightmap update as it comes
(simpler, but up to 6.5 times the bytes on e1m1).

## 2026-11-12: M3 PR 2, Quake's hot functions in float

- **Finding them.** THERMO's profile counts softfp's own functions, not
  who calls them, so the PC said where the doubles are: the PC build
  with inlining off, under valgrind's callgrind, counting each executed
  double SSE instruction (and each call to a double `math.h` function)
  by the function it is in. demo1 executed 6.4 million of them; 44% in
  `EmitWaterPolys`, 14% in `R_RecursiveWorldNode`, 11% in
  `R_DrawParticles`, 9% in `Draw_Character`, 7% in `R_DrawAliasModel`.
  The double `sqrt` was a cost of its own: about 3,000 instructions a
  call, 3.2 million calls over demo1 (most from `VectorNormalize`).
  CANNER's `-S` listing then showed what each one calls.
- **What changed** (each edit marked `QUINCE:`): `sqrtf`
  for `sqrt` (`Length`, `VectorNormalize`, `EmitSkyPolys`,
  `SV_UserFriction`); `sinf` and `cosf` (`AngleVectors`,
  `R_DrawAliasModel`, `R_RenderDlight`, `SV_StepDirection`); float
  constants, three of them `#define`s (`BACKFACE_EPSILON` in
  `glquake.h`, `DIST_EPSILON`, `STOP_EPSILON`); `R_RecursiveWorldNode`'s `dot` a
  float; and, where a loop compared each particle's or light's float
  `die` with the double `cl.time`, the clock taken as a float once
  before it. The swaying in `V_AddIdle` and `CalcGunAngle`, three double
  sines a frame each, is skipped when `v_idlescale` is 0, as it is
  unless set.
- **The clocks stay double**, as planned. `EmitWaterPolys` needed the
  clock in every vertex: its table repeats every 2*pi seconds, so the
  clock is wrapped to that once a call with `fmod`, in double, and the
  rest is float.
- **Many of these are exact.** A float's square root taken in double and
  rounded back is the correctly rounded float one, so `sqrtf` gives the
  same bits; so do multiplying or dividing by a power of two (`1/8`,
  `1/16`, `1/64`), `MSG_ReadAngle`'s `45/32` on a char, and a compare
  with a constant whose float lies on the safe side of it (`0.01`; `0.1`
  in `ClipVelocity`'s inside test; `1.0`). Where it lies on the wrong
  side the compare is turned round: `0.1f` is the first float above
  `0.1`, so `SV_WriteEntitiesToClient`'s `miss > 0.1` is `miss >= 0.1f`
  for every float (the review found the `>` first written there). The
  rest can differ in the last bits.
- **Result on SIMMER** (instructions, `-icount`), against PR 1:

  | build | demo1 ms/frame | e1m1 server ms/frame |
  |---|---|---|
  | CANNER | 14.2 → 7.7 (-46%) | 7.9 → 6.7 (-15%) |
  | GCC -O2 | 10.1 → 6.9 (-32%) | 6.6 → 5.7 (-14%) |

  softfp went from 38% of CANNER's demo1 samples to 7%, and most of
  what's left is load time. CANNER's code is now 12% behind GCC's on
  demo1, where it was 41%: the gap was mostly softfp (pectinos#2107).
  On the board, where the XIP cache and PSRAM weigh more, the gain will
  be smaller (inferred); its timing waits for the Jam.
- **The game is the same.** e1m1's state and checksum didn't change
  (`c8c17318, 164 entities`, -3592846848), so the gate values stay. The
  PC's float build and PR 1's double build print the same state and
  checksum after 300, 1,000 and 3,000 frames of e1m1, e1m3 and e1m5.
  demo1 can't be compared that way, since it runs by the clock; with the
  PC's clock made a fixed step (a scratch change, not committed), both
  sent the same triangles and vertices, and the sum of their screen
  positions differed by 7 parts in a million.
- **`make test-float`** keeps it: CANNER writes the program as assembly,
  and `tools/floatcheck.py` fails if a function in `tests/float_hot.txt`
  calls softfp's double helpers or a double `math.h` function more often
  than the list allows, or is missing. The list names what stays and why
  (the clock as a float, once a frame or a call). Checked against PR 1's
  code: all 15 functions then on the list fail.
- **Left as they were:** load-time code (`BuildSurfaceDisplayList`,
  `BuildTris`, `CalcSurfaceExtents`: no frame time, and the surface
  extents are where M1's "Bad surface extents" came from), once-a-frame
  clock sums (`V_CalcBob`, `CL_LerpPoint`, `SV_RunThink`), and
  `lib/nullgl.c`'s `glOrtho` and `glFrustum`, whose GLdouble divides the
  GL layer thread's `lib/pbgl.c` makes float as it replaces the file.
- `quince/QUINCE.HOT`, M1's board heat file for `make heat`, was taken
  from the double code; the next board run should take a new one.

## 2026-11-12: M2's and M3's board run: the GL layer costs the Jam 23 ms a frame

One boot of PectinOS at the pin (bafa5bf, flashed for the run on Jacob's
OK, Fruitfly put back after), four builds in turn: main at 51ae703 (the
GL layer and M3's floats) and 227d465 (the GL layer alone), each by
CANNER and by GCC -O2. Every one passed `/SELFTEST`'s 127 checks, and
every server run gave the PC's e1m1 exactly: state `c8c17318, 164
entities`, 2,836,051 words, checksum 666297688. The logs are on Jacob's
Mac in `~/src/quince-board-run/m2m3/`.

| build | `/TIMEDEMO DEMO1` | `/SERVER E1M1 300` |
|---|---|---|
| M1, null GL, CANNER (LOG above) | 30.9 ms | 22.1 ms |
| M1, null GL, GCC -O2 | 24.0 ms | 19.4 ms |
| GL layer, CANNER (227d465) | 53.9 ms, 18.5 fps | 34.1 ms |
| GL layer, GCC -O2 (227d465) | 41.3 ms | 28.0 ms |
| GL layer and floats, CANNER (51ae703) | **45.8 ms, 21.8 fps** | **32.4 ms** |
| GL layer and floats, GCC -O2 (51ae703) | **38.1 ms, 26.2 fps** | **27.1 ms** |

- **The GL layer** costs about 23 ms a frame on demo1 in CANNER's build
  (53.9 against 30.9) and 17 in GCC's. THERMO at 4 kHz on 51ae703's
  CANNER build puts it in a few small functions, as shares of the
  program's own samples (the kernel and interrupts were 14%):
  `outcode_of` 17.4% (every vertex against nine planes, and again for
  each vertex a cut makes), `vertex` 9.7% (the transform), `pb3d_vertex`
  7.2% and `fix` 5.1% (float to fixed point, with its clamps and
  rounding), `pbgl_end_frame` 4.8% (the checksum of every word), and
  `project` 2.8%. Those six are 47% of the program; id's renderer in
  the top 25 (BSP walk, surfaces, models, lights, particles) is about
  21%.
- **M3's floats** took demo1 from 53.9 to 45.8 ms in CANNER's build
  (-15%) and from 41.3 to 38.1 in GCC's (-8%); e1m1 -5% and -3%. SIMMER
  had -46% and -15% against the null GL. On the board the saving is 8.1
  ms on demo1 and 1.7 ms on e1m1, about half what SIMMER's counts
  predict at M1's board-to-SIMMER ratios (2.1 and 2.8), and a smaller
  share of a frame the GL layer has made longer. Why it's half isn't
  measured: the XIP cache and PSRAM weigh more on the board (inferred). softfp is out of the
  timedemo's top 25; `__aeabi_ddiv` is 1.1% of the server run.
- **GCC against CANNER**: GCC's build takes 17% less time a frame on
  demo1 and 16% less on e1m1, against 22% and 12% in M1 (CANNER's is 20%
  slower on both).
- **What PB is sent**, at the board's own frame rate: demo1 27 KB a
  frame, 590 KB/s at 21.8 fps (vertices 554 KB/s), e1m1 602 KB/s; PB-ASKS
  has the row. Textures at load are 151 and 2.6 MB, none reloaded.
- **The heat file.** THERMO's `/HEAT` on both runs, pulled off the card
  with pectinos main's `jamterm.py --get` (a host tool; it uses only
  `TASTE` and `CRC` on the board, so it works with the pin's kernel),
  summed by `heat.py sum` into `quince/QUINCE.HOT`: 460 functions with
  samples. `make heat`'s 64 KB budget keeps 257 functions (64.7 KB),
  which hold more than 99% of the samples, and moves 881 (142.5 KB) to
  PSRAM. M1's file named blocks (`nullgl.c:glVertex3fv+10`, 5,187 of
  them); this one names whole functions, because the kernel had no room
  for the count by offset (THERMO.md: it takes a halfword a halfword of
  code; inferred: Quake's hunk had the PSRAM). `make heat` cuts by
  function, so it takes either. The heat build wasn't timed this run.

What it says: the CPU's half of a real renderer runs at 22 fps in
CANNER's build and 26 in GCC's, short of the 30 fps target, and the
largest single cost is now our own GL layer, not Quake. Its hot
functions are small and new, which makes them the cheapest place to win
time back (`outcode_of`'s nine plane tests a vertex first). That is the
next plan, for Jacob's OK. Alternatives considered for this run: timing
the heat build too (left for a run that tests it on the new file), and
timing on SIMMER only, whose instruction counts don't weigh the XIP
cache, PSRAM or the FPU's latencies.
