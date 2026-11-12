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
