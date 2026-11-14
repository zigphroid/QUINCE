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
  depend on it. `asin` (mirrors only) and `ceil` (exact, so it can't
  differ) are still glibc's. The PC build is `-fno-builtin` too, as the
  board's `-ffreestanding` is: without it GCC had turned a `sin` and a
  `cos` of one angle into a call to glibc's `sincos`, in `AngleVectors`
  and seven other functions, so the SDK's `sin` and `cos` weren't the
  ones running there. It changed neither e1m1's state nor its checksum.
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

## 2026-11-12: Quake's frames drawn by the model (PB-ASKS step 2, PR 2)

- **The decoder** (`lib/pbdecode.c`) reads `lib/pb3d.h`'s words only
  through what the header defines, into the model's calls; its rules are
  `docs/PB3D.md`'s "Reading the commands". A command the reader refuses
  stops the frame there, as a gateware reader that loses its place
  would have to: the alternative, skipping a word and reading on, would
  draw garbage from the middle of a TRIS. `tests/test_pbmodel.c` holds
  each field and kind against the model given the same triangles
  straight (each check failed by a mutation of the decoder).
- **Two gaps in the GL side, found by looking at the pictures:**
  - `lib/qvid.c` never made id's `GL_Init` calls (`gl_vidnt.c`), so the
    console and the loading plaque went to PB untextured, as white quads,
    until the renderer set the state itself. Now it makes the ones
    `lib/pbgl.c` reads. `test-quake` checks the first frame's STATE.
  - The 3D view is `glViewport(0, 115, 640, 365)` above the status bar,
    but PB drew to the whole surface, so the world and the weapon spilled
    over the bar, which GLQuake redraws only when it changes: the HUD
    vanished. Real GL clips to the viewport. **Chosen:** `CLIP`, command
    20, a clip rectangle PB applies (a Voodoo's `clipLeftRight` and
    `clipLowYHighY`), sent by `lib/pbgl.c` when the viewport changes.
    **Alternatives:** cut every triangle at the viewport on the Jam
    (Sutherland-Hodgman, which `lib/pbgl.c` already does at the guard
    band), costing the RP2350 a cut for every polygon crossing the
    view's edge, many a frame; or have GLQuake redraw the status bar
    every frame (`Sbar_Changed`), which hides the spill but still pays
    PB's fill for it, and leaves any other smaller viewport wrong.
    The dump's version is 2.
- **Frames the same every run.** A dump's timedemo runs on a fixed clock
  of 1/72 s a frame (`tests/sys_host.c`), since timedemo frames follow
  the clock (the console's slide, the models between messages), and
  `quake/*.o` build with `SOURCE_DATE_EPOCH=0`, since `host.c` prints
  `__TIME__` on the console. e1m1's server benchmark already ran in fixed
  steps.
- **What's held:** `make test-quake` draws e1m1's 76 dumped frames and
  holds the CRC of every frame's CRC and the last frame's, and that none
  was refused. demo1's 1,020 frames take about 40 seconds, so `make
  pb-frames` draws them to pictures instead, outside `make test`.
- **Measured fill** (demo1, a frame): 1.78 screens covered, 1.40 drawn,
  0.40 blended, against PB-ASKS's guess of overdraw 2 and 20% blended.
  The budget there is redone with them: about 160 MB/s at 30 fps and 285
  at 60, from 190 and 350.

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

## 2026-11-12: M4 PR 1, the GL layer's hot spots made cheap

Jacob took every recommendation. M2's board run had our own GL layer at
47% of the program, most of it in four small functions. Three of them:

- **`outcode_of`** (17.4%) tested every vertex against nine planes as
  four-term dot products: 36 multiplies. Every plane is one coordinate
  against w, so each test is now a compare (`x + w < 0` for the left
  edge), or a multiply and a compare for the guard band's. The sums are
  the same ones the dot products made for finite coordinates, since the
  terms dropped were exact zeros, and e1m1's 300 frames come out word
  for word as before (the commands compared, not just the checksum).
  The guard band's four tests are made only for a vertex off the
  screen: with each guard factor at least 1, a vertex inside the
  screen's planes is inside the guard's too (a rounded g w is at least
  w). The clipper tests the one plane it cuts at, where it worked out
  all nine for both ends of every edge.
- **`fix`** (5.1%, and `pb3d_fix_12_4` 1.7%) added a half and cut, with
  four branches. It is now `roundf`, one `VRINTA` on the M33, which
  rounds halves away from zero as the format says. The old way, adding
  a half and cutting, was wrong in two places. The one that moves words
  is q's: where q times 2^31 is between 2^23 and 2^24 a float's step is
  1, so v + 0.5 is a tie, which rounds to even, and every odd v went up
  one. That is 1.7% of e1m1's words (48,160, and 259,775 of demo1's),
  each a q's low word one step down, so the checksum moved. The other,
  0.49999997 + 0.5 being 1.0 in float, moves none of the two demos'
  words but was as wrong. A selftest holds each case (found by the
  review, which also added the guard band's edge checks).
- **The checksum** (`pbgl_end_frame`, 4.8%) hashes two words a step,
  half the multiplies. Jacob's choice of three: two words a step, a
  switch that turns it off for timed runs, or leaving it. Only the
  tests read it, but the PC-to-SIMMER check needs it in every build.
  The gate is now 3500115068 (with #26's CLIP).

On SIMMER (wall clock, CANNER's build, against main at 698d71e on the
same machine): demo1 24.9 to 22.2 ms a frame (-11%), e1m1 10.8 to 9.7
(-10%), the state and the commands' counts unchanged. The board's
timing is this PR's next step.

Left as they were: `vertex` (9.7%), the 4x4 transform of each vertex,
which a cheaper form would need GLQuake's matrices to be affine, and
`pb3d_vertex`'s writes, which are a store each.

## 2026-11-12: the mip level rounds, per triangle (the mip study)

- **Decided (Jacob):** PB keeps one mip level a triangle, but rounds log4
  of the texels a pixel instead of rounding it down: `pick_level`
  compares twice the texel area. No per-pixel level, no trilinear.
- **Measured** by the C model on three 40-frame walks of demo1 (frames
  200-240, 490-530, 900-940), against the same frames drawn 4x4 times
  larger with GL's trilinear (the exact per-pixel level, rounded down,
  blended with the next by its fraction) and averaged down. Far flicker
  (frame-to-frame change the reference lacks, past about 256 units):
  rounded down 3.00, rounded 2.78, per pixel from q 2.98, the exact
  per-pixel level rounded down 2.79 and rounded (GL's rule for one
  level) 2.86, GL's trilinear 2.95, bilinear walls 2.59 (but error near
  1.93 against 1.27: Quake's look changes). Per pixel from q moved 1.1%
  of reads off the triangle's level: Quake's faces are at most about 256
  texels across, so q varies too little inside one. The exact level
  helped only by being blurrier, as rounding the triangle's is.
- **Cost** (an 8 KB direct-mapped cache of 32-byte lines standing in for
  PB's, a frame): rounded down 373 KB of misses, rounded 322, exact 313,
  trilinear 513, bilinear walls 533 (exact here is the rounded-down one). Rounding costs PB nothing a pixel;
  the per-pixel rules need a log2 and the gradients in the pixel
  pipeline, more DSP, which is PB's biggest risk.
- **Alternatives:** per pixel from q (an add and a shift on log2 q):
  buys nothing here. GL's exact rule: the same as rounding, for gradient
  arithmetic every pixel. Trilinear (a Voodoo 2's): worse here, and a
  second texel a pixel. Caveat: one demo, a dark one. The write-up is the
  project doc "Mip choice on PB: per triangle or per pixel".
- The study's switches stay in the model (`struct pbm_study`, all zeros
  is the rule) and `tests/pbplay.c` (`-lod`, `-bias`, `-linear`,
  `-scale`, `-raw`), so a later question can be measured the same way.
  The runs above were made before the rule rounded; now `-bias -1`
  gives the rounded-down levels, and `-lod tri3`, `-lod exact -bias -1`
  and `-lod tri -bias -1` draw those runs' frames byte for byte.

## 2026-11-12: M4's board run, and GCC's build over the program area

On the Jam, PectinOS bafa5bf, one boot, main before M4 (698d71e) and
with it (08790a2, #30); ms a frame, demo1 then e1m1:

| build | before | M4 |
|---|---|---|
| CANNER | 46.4, 32.9 | 38.3 (-17%), 28.9 (-12%) |
| GCC -O2 | 38.4, 27.4 | did not load; with #35, 33.2 (-14%), 25.1 (-8%) |

- Every gate held: state c8c17318 with 164 entities, checksum
  122102208 before and 3500115068 with M4, selftest 136 and 210 checks.
  The PB link at M4's rate is about 710 KB/s for demo1.
- CANNER's build gained more than SIMMER said (-11%): the board's 8 ms
  is about a third of the 23 ms our GL layer cost after M2. CANNER's
  build with M4 now runs demo1 as fast as GCC's did without it.
- **GCC's build with M4 printed `Headspace exceeded.`** GCC's build puts
  all its code in SRAM, and the jar needed 368,208 bytes (code, data and
  SRAM .bss) plus the 16 KB stack and the loader's 1 KB guard, 848 bytes
  past the 384,768 HEADSPACE gives with nothing resident. Review 1's
  guard-band checks were the last 2 KB. The fix: the selftest's two 2 KB
  frame buffers go in PSRAM (`.pbss`, as `lib/qproto.h`'s QPSRAM), which
  leaves GCC's build (on main with #29) 4.7 KB spare here, and 2.7 KB
  with the Mac's GCC, whose code is 2.9 KB bigger: it loaded and ran on
  the Jam (#35's head 3494938, both builds 210 checks, state and
  checksum as above; CANNER's 38.2 and 28.9 again). And `make gcc` now
  lays the .JAM out as the loader's `top_down` does (a 512-byte
  allowance for the start block, the stack's start on 32, the .bss's
  end on 8) inside `PROGRAM_SPAN`, and fails one that wouldn't load,
  so this shows on the PC: the largest stack it takes now is 21,184
  bytes with the cloud session's GCC, and about 19.1 KB with the Mac's
  (its SRAM ends 2 KB later). A new `STACK` makes the .JAM again, and
  checks it.
  Alternatives: a smaller stack (16 KB is the rule, CLAUDE.md), or a
  heat cut for GCC's build too (a bigger change, for when the code
  grows). CANNER's build keeps its cold code out by the heat file and
  has room (inferred from its .JAM's sizes, not measured on the board).
- The run's logs are on Jacob's Mac, `~/src/quince-board-run/m4/`.

## 2026-11-12: clearing the review issues labelled later

Jacob OKed a plan to clear the 21 open `later` issues in four PRs (docs,
tests, small code fixes, then `lib/pbgl.c`), and took every
recommendation on the five choices in them:

- **#4, the headers in `quake/` that nothing builds:** a note in
  `quake/README.md`, not a delete, so `quake/` stays id's bytes as
  imported. Alternative: drop them; it would save nothing that matters.
- **#21, the C model's depth a step too far:** round to nearest and
  tighten `depth_test` (PR 3). It changes the model's depth values, not
  a word Quake sends.
- **#16, values the format leaves undefined:** refuse depth test 3 and a
  `DEPTHRANGE` with far below near, and let `PB3D_TEXF_ALPHA` alone
  decide see-through for RGBA8888 (PR 3). Refusing keeps the contract
  pectinos reads small; treating 3 as ALWAYS was the alternative.
- **#24.1, flat shading's colour:** the last vertex of each triangle, as
  GL 1.1 does (section 2.13.7), with `pb3d.h` saying so (PR 4).
- **#24.2, the far plane:** still not clipped, now said so. GLQuake's
  4,096 holds every shareware map, and a sixth plane would cost time in
  `outcode_of`, M4's hot path.

#6, the millisecond clock, is an SDK gap: it goes to zigphroid/pectinos
as an issue asking for a microsecond clock.

## 2026-11-12: M4 PR 3, where the frame goes now, and one pass a vertex

THERMO at 4 kHz on main 705e4ae's CANNER build, on the Jam (bafa5bf),
shares of the program's own samples in demo1 (the kernel and interrupts
16%): `pb3d_vertex` 13.2%, `vertex` 11.8%, `words_sum` 5.1%,
`R_RecursiveWorldNode` 4.6%, `outcode_of` 4.4%, `R_DrawSequentialPoly`
4.2%, `GL_DrawAliasFrame` 3.8%, `project` 3.3%, `flush` 2.5%. e1m1's
server run has `pb3d_vertex` first too (9.3%). Plain timing the same
boot: 38.2 and 28.9 ms a frame. The two runs' heat files, summed, are
the new `quince/QUINCE.HOT` (477 functions; the 64 KB budget keeps 249).
The run was before the change below, so its counts for `pb3d_vertex`
and `project` are carried by hand to `pb3d_vertex_of` and `send_vertex`,
which do that work now; without that, `make heat` cut the two hottest
send functions cold (review 1). The next THERMO run replaces the file.

- **The heat build is slower than the plain one** on this run: 39.5
  against 38.2 ms (demo1) and 30.7 against 28.9 (e1m1), as M1's was by
  3-7%. The plain build fits SRAM whole, so moving code to PSRAM can only
  cost; the heat cut pays when a program doesn't fit. Not looked into
  further.
- **The send, in one pass.** `project` wrote a `struct pb3d_vertex`,
  then `pb3d_vertex` read it back and wrote each word through the
  buffer's struct, a load and a store of its count a word (CANNER can't
  tell the words from the count). Now `send_vertex` works out only the
  fields the format sends and hands them to `pb3d_vertex_of` in
  registers (s0-s6, the hard-float convention), which writes through a
  local pointer and sets the count once; `pb3d_fix_q`'s body is inlined
  there. `pb3d_vertex` stays, a wrapper, for the checks. PB is sent the
  same words: the gate's checksum is unchanged.
- On SIMMER (CANNER, against main 705e4ae, one at a time): demo1 21.9 to
  21.2 ms (-3%), e1m1 9.9 to 9.7 (-2%). Smaller than THERMO's share
  suggests, which hints that on the board the cost is in memory: the
  frame's words are on the PSRAM heap (inferred, not measured).
- **On the board** (bafa5bf, one boot, against main 705e4ae): CANNER's
  demo1 38.3 to 37.9 ms (-1%), e1m1 28.9 to 28.6 (-1%); GCC's build of
  this PR 32.6 and 24.7. Gate held in all three. A sixth of the
  program's samples bought 1%, so what those samples were waiting on is
  mostly not the instructions this PR took out: the stores of the
  frame's 27 KB of words into PSRAM are the likely cost (inferred).

## 2026-11-12: the model's depth rounds to nearest (#21)

Later issues PR 3 changes one of PB3D.md's rules. Under the old rule, q
at a pixel rounded down at all three of its steps: S's 16 bits, RA's
reciprocal and the last shift. So a triangle of constant q 0.5 stored
4096, where `pbm_depth_of_q` puts q 0.5 at 4095. Now RA rounds up, and
the 16-bit sum and the last shift round to nearest.

Rounding the last shift alone, the issue's first suggestion, wasn't
enough. RA's floor left q 0.5 at 0x3FFFFFFF, and the shift can't undo
that. `depth_test`'s two checks are now exact, not within 1.

Pictures do move, a little. The scene's CRC and the 76 e1m1 frames
`test-quake` draws through the model are unchanged, but on the demo
dumps about 40% of frames do (demo1 418 of 1,020, demo2 344 of 1,033,
demo3 534 of 1,143): 1 to 4 pixels each in the frames the review
sampled, at most 33 in a channel. A constant q on a step's edge still
lands a step farther about one pixel in 20 (one in 7 before), so the
bias is cut, not gone (PB3D.md's depth rule). `test_pbmodel`'s
`depth_rounding` holds it: random triangles, a pixel a step off as often
nearer as farther.

The cost to PB: one add each for the sum's and the shift's rounding,
plus a different constant in RA's divide, which runs once a triangle.

Alternatives:
- Keep the floors and say "about" in PB3D.md. That leaves the bias.
- Interpolate depth from the full S. Its product with RA still fits in
  64 bits, but it needs a wider multiplier per pixel.

## 2026-11-12: M4 PR 4, the frame's words through a window in SRAM

**Decision (Jacob, option A).** The GL layer writes a frame's PB
commands into a 4 K-word (8 KB) static buffer, in SRAM on the board,
and hands it to the sink each time a command doesn't fit
(`pb3d_buf.full`), then once at the frame's end. Before, the whole
frame, up to 128 K words, was in a 256 KB block from the PSRAM pool.

- **Why.** PR 3's board run hinted that the words' stores into PSRAM
  were the cost, and a measurement settled it: the same heat build of
  0c819b8 with the words buffer in SRAM instead (an experiment, kept off
  main) ran demo1 39.1 to 34.0 ms a frame (-13%) and e1m1 30.6 to 27.2
  (-11%) on the board. PSRAM behind the XIP cache takes writes slowly,
  and a frame is 30-50 KB of them.
- **The alternatives.** B: the heat build (code to PSRAM) to free
  144 KB of SRAM for the whole frame's buffer, which costs the 3-7% the
  heat cut costs. C: leave it, for PB's real command path to settle. A
  is what that path will want anyway: PB takes a stream, so a window
  handed on as it fills is how commands will reach it.
- **What changes for a sink.** `frame(words, n)` became `words(w, n)`,
  each a window of whole commands, and `frame()` at the end. The PC's
  dump gathers the windows, so its file is the same; the selftest checks
  a frame through a 30-word window comes in two windows of whole
  commands with the same words and checksum, and that only a command
  bigger than the window is dropped. The checksum carries a window's odd
  last word to the next, so it is the whole frame's: the gate's
  3500115068 is unchanged.
- **Held updates.** A held lightmap update (`glTexSubImage2D`'s, coalesced)
  is sent before each window, not only before the frame, since PB will
  draw a window as it arrives and the window may use it. The shareware
  runs send the same lightmap bytes (the gate's totals).
- **The biggest command.** A primitive is at most 256 vertices of 11
  words, 2,818 (`PBGL_WORDS_MIN`; `qvid.c` checks its window holds it).
  A cut strip or fan of 256 could make an inside list of 254 triangles,
  more than that, so the inside now goes in lists that fit the window.
- **The SRAM.** 8 KB more `.bss` put GCC's build 4.4 KB past the program
  area, so three statics went to PSRAM: `menuplyr_pixels` (4 KB, the
  menu's player picture), `scrap_allocated` (2 KB, used at load) and
  qlibc's `fmt_buf` (4 KB, a printf's). GCC's build still fits, with
  less margin than before: 3,752 bytes spare in the cloud against 4,800,
  by the Makefile's own check, since its code, in SRAM too, grew about
  3 KB (the new checks, `flush`'s split, the ordered texture calls)
  as its `.bss` fell 2 KB (review 1). On the Mac's GCC, whose code is
  bigger, about 1.1 KB by its sizes; it loads on the board.
- **Texture calls in order.** A texture made, or an update sent at once,
  first hands on the window's words so far, so the stream keeps GL's
  order (review 1). Such calls come at load: none in the timed runs.
- **On the board** (the bafa5bf pin, one boot, two samples each, which
  agreed): CANNER's demo1 37.8 to 32.9 ms a frame (-13%), e1m1 28.7 to
  25.3 (-12%), main f45eff0 against 3a1f77f; GCC's build of 3a1f77f 27.7
  and 21.4, against 32.6 and 24.7 before. The 5 ms the SRAM experiment
  measured on the heat build is what the plain build gained too. The
  gate held in all six runs (checksum 3500115068, state c8c17318, 164
  entities, nothing dropped), and the selftest passed (257 checks).

## 2026-11-13: the later issues' second round (#41, #42, #45, #46, #52)

**Decisions (Jacob, every recommendation).**
- #45: a reversed `glDepthRange` (far below near) is counted in
  `ignored` and dropped, and the range before it stands. GLQuake sends
  one only with `gl_ztrick 1`, which `qvid.c` keeps at 0, now with a
  comment saying why. The alternative, swapping near and far and
  flipping the depth test, is more code for a setting nothing uses.
- #52: a flat polygon's or quad's vertices always take its colour, even
  where a texture replaces it, the same rule as a cut triangle's fan.
  The alternative, testing "the colour is drawn" in both places, keeps
  two conditions in step for no change on screen.
- #46: PB3D.md says a constant q on a step's edge lands nearer "almost
  never", not "never". Finding which triangles do it was the other
  choice; it changes nothing PB is asked for.

## 2026-11-14: GCC's build puts the menus and the selftest in PSRAM (#57)

GCC's build keeps all its code in SRAM, in the program area, and had
outgrown it on the Mac: main's build ended SRAM 2,448 bytes past what a
16 KB stack leaves, and the pin's move (#53) adds 1,024. In the cloud it
fit by 648 bytes until #54, after which #53 merged with main was 376
bytes over too, since `.data` starts on a 1 KB boundary after `.text`
and #54 left 20 bytes to it (#54's note). Jacob took every
recommendation (2026-11-14):

- **What goes.** `make gcc` puts every function `quake/menu.c` and
  `lib/selftest.c` define in .JAM v4's cold segment, PSRAM behind the
  XIP cache, but `M_Draw`, which `SCR_UpdateScreen` calls each frame. The
  list is read from the two objects' symbol tables (`build/gcc/QUINCE/
  QUINCE.cold`, 106 functions) and written as `cold.ld` by pectinos's
  `tools/heat2ld.py`, found before the SDK's empty one. 28,096 bytes of
  code leave SRAM: 27,272 bytes spare in the cloud now, against 648.
- **Why these.** Neither runs in a timed demo (THERMO saw `M_Draw` once
  in demo1, nothing else of either file in any run since M1), so every
  function the benchmarks time stays in SRAM and GCC's numbers stay
  comparable with the runs before. The alternatives: GCC through
  THERMO's heat cut, which frees far more but moves hot code too (3 to
  7% on CANNER, so a new baseline); or more `.bss` to PSRAM, a few KB at
  most, soon gone again.
- **A margin.** `make gcc`'s check now also fails with less than
  `GCC_SRAM_MARGIN` (4 KB) spare, since the Mac's GCC writes about 2.8 KB
  more code than the cloud's: a cloud `make test` catches a build the
  Mac would refuse.
- **Checked.** Built at the bafa5bf pin: `make test` (its test-builds
  builds `make gcc`) and `make`; then GCC's build on SIMMER (the 20978ee
  kernel, which takes the older .JAM, as the board did): its selftest
  passes from PSRAM, and sim-bench's e1m1 gate holds (checksum
  3500115068, state c8c17318, 164 entities). The Mac's build isn't
  checked yet: by its 2.8 KB it should have about 24 KB spare.

## 2026-11-14: the pin moves to pectinos main (20978ee)

`PECTIN_REV` goes from bafa5bf to 20978ee, pectinos main on 2026-11-12,
164 commits on, to take CANNER's newer code (Jacob: "let's re pin on
pectin main and check it out"): softfp.c's pieces always inlined
(pectinos#2107), small struct copies and zeroings as straight-line code,
late results spaced from their uses, a 0/1 used as it is, and shifts
folded into SSAT, PKHBT and RSB. The SDK gained the `__pectin_user`
qualifier and its VERSION went up, so the board needs PectinOS built at
the new pin. Nothing in QUINCE had to change. The alternative, waiting
for more of CANNER's phase 5, was declined: each pin move is its own PR
anyway, and this one measures what has landed.

- **Checks.** `make test` (test-quake's demo1, e1m1 and every shareware
  map, test-builtins, test-float, test-builds), `make`, `sim-smoke` and
  `sim-bench` pass at the new pin; e1m1's gate is unchanged (checksum
  3500115068, state c8c17318, 164 entities).
- **Sizes.** CANNER's `QUINCE.JAM` grows 793 bytes (341,445 to 342,238
  with main's #51 merged; 338,473 to 339,266 before), the inlining; its assembly 425 lines more, and the same 1,109 calls
  into softfp (the calls from Quake's double code stay calls: #2107
  inlines softfp.c's own pieces into its functions). GCC's build is
  unchanged but for 120 bytes of the SDK's code, and its SRAM spare by
  the Makefile's check is the same at both pins: 648 bytes in the cloud
  (`__bss_end` at 366,200 against 366,848), after #51 took about 2 KB of
  it (2,696 before).
- **SIMMER at the board's pace** (`-icount`, so time is instructions),
  both pins' builds on the new pin's kernel: CANNER demo1 80.0 to 79.5 ms
  a frame (-0.6%), e1m1 57.0 to 56.4 (-1.1%); GCC 70.1 to 70.0 and 48.2
  to 48.2. So in instructions the new CANNER saves about 1%. Its
  scheduling change saves stalls, which instructions don't count, so the
  board can show more.
- **On the board** (one boot of PectinOS 20978ee, two samples each,
  alternating; the new kernel takes the old pin's .JAM): CANNER's build of
  main e72c1be at bafa5bf ran demo1 at 32.8 and 32.9 ms a frame and e1m1
  at 25.2 and 25.2; this PR's (a8ca1d2, at 20978ee) at 32.6 and 32.6, and
  24.9 and 24.9: -0.8% and -1.2%, about what SIMMER's instructions said,
  so the scheduling change bought little on the board either. The gate
  held in all eight runs (checksum 3500115068, state c8c17318, 164
  entities, nothing dropped) and the selftest passed (280 checks).
- **GCC's build doesn't fit on the Mac**, at either pin, so it wasn't
  timed: the Mac's GCC writes bigger code than the cloud's, and with the
  16 KB stack SRAM must end by 366,848 bytes, where main's build ends at
  369,296 (2,448 over) and this PR's at 370,320 (3,472 over: the SDK's
  code grows 592 bytes, 1,024 once aligned). It last fitted at 3a1f77f,
  with about 1.1 KB spare; main had already outgrown it before the pin
  moved (#57). In the cloud it still fit at both pins, with 648 bytes
  spare, until #54. #58 then put the menus and the selftest in PSRAM
  (the entry before this one): at the new pin GCC's build has 27,272
  bytes spare in the cloud, the same as at bafa5bf.
- **THERMO on the new pin** (this PR's CANNER build, same boot), for the
  next round of speed work: demo1 34.1 ms a frame under sampling, kernel
  17.0%, then `vertex` (pbgl.c) 13.3%, `pb3d_vertex_of` 9.5%,
  `outcode_of` 5.1%, `R_RecursiveWorldNode` 5.0%, `R_DrawSequentialPoly`
  4.4%; e1m1 26.4 ms, kernel 27.6%, then `pb3d_vertex_of` 6.3%,
  `AllocBlock` 5.9%, `vertex` 5.4%, `R_RecursiveWorldNode` 4.7%,
  `R_DrawSequentialPoly` 4.4%.

## 2026-11-14: M4 PR 5, the world's polygons off the screen left out

M4's second round (Jacob, "all recs", the plan in the thread): PR 5 culls
the world's polygons before they reach the GL layer, PR 6 makes the
layer's per-vertex code cheaper, PR 7 gives the world's polygons GL 1.1
vertex arrays. This entry is PR 5.

- **Why.** THERMO samples the whole run, the load too (demo1 42.2 s
  sampled against 33.1 s timed, e1m1 17.1 against 7.9), so `AllocBlock`,
  `FanLength`, `StripLength`, `Mod_FloodFillSkin`, `PR_LoadProgs`,
  `pbgl_upload8` and most of the kernel's share are the load's, not the
  frame's. In demo1's frame the GL layer is about 17 of 34 ms, about
  2.5 µs a vertex transformed. Counted on the PC by `glBegin`'s mode
  (throwaway counters): 31% of demo1's transformed vertices were in
  primitives the layer then dropped whole, off the screen. Most were the
  world's: GLQuake culls whole BSP nodes (`R_CullBox`) but sends every
  surface of a node that shows, and 60% of the world's polygons (81% of
  the water's 64-unit pieces) were off the screen.
- **What.** Each `glpoly_t` gets a bounding sphere at load
  (`GL_PolySphere`: centred on its box, its farthest vertex plus a unit).
  `R_PolyOffScreen` tests it against the frustum's four planes (each
  normal's length kept in `r_frustumlen`, since at a fov of 90 they are
  sqrt(2) long) and the eye's plane. `R_RecursiveWorldNode` calls it for
  a lightmapped surface and `EmitWorldWaterPolys` for each piece of the
  world's water; a brush model's polygons are moved by its entity's
  matrix, so they aren't tested. All `QUINCE:` edits in `quake/`.
- **The words stay the same.** The layer's view volume lies inside
  Quake's frustum, which is a little wider (at a fov of 90 its up and
  down planes are 45 degrees off the view, the layer's fov_y/2, 36.9)
  and has no near plane, so a polygon behind one of its planes is off
  the screen, and the layer would have dropped it and sent nothing
  whenever all its vertices are outside one of the layer's own planes. A
  vertex behind the eye could break that: the polygon could cross two of
  the layer's planes and be cut into a fan that draws nothing, which is
  sent. No culled polygon of demo1's or the maps' was one (the words
  match), nor any of five million tried at random by review 1. An
  off-screen lightmapped surface still updates and sends its lightmap
  and binds its textures (`R_SurfacePoly` with draw false). e1m1's gate
  holds (checksum 3500115068); GLQuake asks for 3,000 fewer triangles
  and 6,000 fewer vertices over its 300 frames, so `SERVER_TOTALS` moves
  to (183776, 351952).
- **The lightmaps' updates kept** (Jacob). They are texture updates,
  which the checksum of the command words doesn't cover, and e1m1's
  culled surfaces never need one, so `test-quake` now also holds demo1
  on the dump's fixed clock: its words, their checksum (1849523503) and
  its lightmap updates (35, 74,752 bytes). Skipping an off-screen
  surface's update would send more, not less: the lightmap goes later
  with more rows, 42 updates and 90,112 bytes (review 1's mutant).
- **The texture coordinates.** The first version also left GL's current
  texture coordinates as a skipped polygon's last vertex would. No run
  changes without it (demo1 and every map), since each of GLQuake's
  primitives sets them before every vertex, so review 1's fixes took it
  out.
- **A sphere, not a box** (Jacob): 16 bytes a polygon, and four dot
  products, against 24 bytes and `BoxOnPlaneSide`'s four. A sphere is
  looser for a long thin polygon; it only culls a little less.
- **Clipflags.** The first board run (8d9a4d5) had demo1 5.1% faster but
  e1m1 2% slower, and SIMMER's instruction count said the same: e1m1
  culls 13 polygons a frame and tested about 250. So
  `R_RecursiveWorldNode` now carries clipflags, as the software
  renderer's does (`r_bsp.c`): a node wholly in front of a plane drops
  it for everything under it, and a surface with no plane left isn't
  tested. That makes `R_CullBox`'s node tests cheaper too. SIMMER at the
  board's pace (instructions): demo1 79.9 to 75.3 ms a frame, e1m1 57.0
  to 55.6.
- **A struct's size moves Quake's cache.** Growing `glpoly_t` by 16
  bytes, with nothing else changed, changes E1M2's, E1M3's and E1M6's
  words: about 6,000 polygons a map make the hunk about 96 KB fuller,
  Quake's model cache, already evicting near its limit, evicts other
  models, and `GL_LoadTexture` uploads their skins again in another
  order. demo1 and e1m1 are unaffected. So the words were compared with
  main built with the same `glpoly_t` (16 bytes of padding): demo1 on
  the dump's fixed clock and all nine maps' 300 server frames match.
- **On the board** (one boot of PectinOS 20978ee, two samples each,
  alternating, main aae0a51 against 18d4f80): CANNER demo1 32.7 to 30.7
  ms a frame (-6.0%), e1m1 25.0 to 24.4 (-2.4%). demo1 transforms 2,560
  vertices a frame instead of 3,056, and the layer drops 119 primitives a
  frame instead of 239. GCC's build fits (23,136 bytes of SRAM spare) and
  runs demo1 at 26.1 and e1m1 at 20.8. The gate held in every e1m1 run,
  nothing was dropped, and the selftest passed (354 checks). Main on its
  own ran about 0.5 ms faster on this boot than on the first run's, so
  only builds timed on the same boot compare.
