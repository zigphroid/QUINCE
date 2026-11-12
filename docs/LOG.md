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

The heat files (`J:\QTD.HOT`, `J:\QSV.HOT`) are still on the card. The
console can't send a file from the board, so they come off with the card
in the Mac.
