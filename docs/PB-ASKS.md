# What QUINCE asks of PB

What Quake needs from the pixelboard's 3D, sized from M1's board run
(`docs/ROADMAP.md`; LOG 2026-11-12, "M1's board run"). Jacob took every recommendation
here on 2026-11-12 ("All recs"). How PB meets these asks is pectinos's
work and its own plan: QUINCE asks and tests, and never patches pectinos
(CLAUDE.md, rule 3). Figures marked *inferred* are estimates still to
measure; the rest are M1's counts or PB as `$PECTIN_ROOT/docs/PB.md`
describes it at the pin.

In short: a Voodoo-class rasterizer. Screen-space textured triangles at
640x480 in RGB565 with a 16-bit depth buffer, two textures a pixel (wall
and lightmap), and textures that live on PB for good. The Jam keeps the
game, the BSP walk and the transforms.

## The asks at a glance

| Ask | Size | Where the number comes from |
| --- | --- | --- |
| Textured triangles in screen space, PB does the setup | 2,100 triangles and 3,950 vertices a frame (demo1), 790 primitives | M1 counts, `lib/nullgl.c` |
| Fans, strips and lists, vertices shared within one | about 1.9 vertices a triangle today | M1: 3,950 / 2,100 |
| A compact vertex: 16 bytes for a world vertex | 4 MB/s at 60 fps (2.8 with two textures a pixel), against 6.5 MB/s at today's 28 bytes | "Link and memory", below |
| Two textures a pixel: wall times lightmap | one pass over the world instead of two | GLQuake's `gl_mtexable` path, `gl_rsurf.c` |
| Textures sent once as 8-bit indices; PB expands them and builds the mips | about 2.5 MB at load instead of 12-13 MB; the Jam's mip and resample work gone (about 10% of the timedemo's time, all at load) | M1: 952 uploads, 13 MB with mips |
| Partial texture updates (lightmaps) | 75-330 KB a run, 128x128 blocks of a byte a texel | M1's `sub_bytes`, `BLOCK_WIDTH` 128 |
| 640x480 RGB565, shown at x1.5 on 720p | 307,200 pixels; the scan-out mode exists | PB.md, `fb_scan` mode 2 |
| A 16-bit depth buffer, with the depth tests Quake uses | less-equal, greater-equal, depth mask, depth range | `gl_rmain.c` |
| Blending and alpha test | three blend modes, alpha test | `gl_rsurf.c`, `gl_rlight.c`, `gl_draw.c` |
| Perspective-correct texturing | every world and model triangle | GLQuake asks GL_NICEST for the world |
| Frames with flow control: `START`, draw, `SHOW` | two frames in flight, as drawn mode has | PB.md, "Drawn mode" |

The link is not the limit: 4 MB/s is about a tenth of the 37.9 MB/s that
`pktest` put into PB's memory. The Jam's CPU is, so most choices below
trade Jam time for PB gateware.

## What GLQuake really uses

About 45 GL entry points, and a small part of each. Our GL layer (M1's
null GL grown up, `lib/`) takes them as Quake calls them, so `quake/`
needs no changes for this; what reaches PB is far smaller. Counts are
call sites in `quake/gl_*.c`.

| Area | What Quake calls | What PB does |
| --- | --- | --- |
| Primitives | `GL_POLYGON` (9 sites), `GL_QUADS` (11), `GL_TRIANGLE_FAN` (6), `GL_TRIANGLE_STRIP` (2), `GL_TRIANGLES` (1) | Fans, strips and lists; a polygon or a quad is a fan. `GL_LINES` is only in `gl_test.c`, under `#if 0` |
| Vertices | `glVertex2f/3f/3fv`, `glTexCoord2f`, `glColor3f/4f/3ubv` | x, y, 1/w, two texture coordinate pairs and a colour a vertex |
| Matrices | `glFrustum`, `glOrtho`, `glRotatef`, `glTranslatef`, `glScalef`, push and pop, `glLoadMatrixf`, `glViewport` | Nothing: the Jam transforms |
| Depth | `GL_DEPTH_TEST`, `glDepthFunc` LEQUAL and GEQUAL, `glDepthMask`, `glDepthRange` (the view weapon gets 0 to 0.3) | Test and write 16 bits; the depth range is a scale and offset the Jam applies |
| Culling | `GL_CULL_FACE`, front or back | Optional: the Jam can cull, having the screen positions |
| Blending | `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` (water, particles, the screen tint), `GL_ZERO, GL_ONE_MINUS_SRC_COLOR` (the lightmap pass), `GL_ONE, GL_ONE` (dynamic light flashes) | Three blend modes, each a read of the picture |
| Alpha test | `GL_ALPHA_TEST` on the HUD, the console's text, sprites and the sky's top layer | Drop a pixel whose texel is see-through. id's `GL_Init` set `glAlphaFunc(GL_GREATER, 0.666)`; `lib/qvid.c` doesn't yet (inferred from id's `gl_vidnt.c`) |
| Texture combine | `glTexEnvf` MODULATE, REPLACE, BLEND | Texture times vertex colour, or texture alone; BLEND is the lightmap on the second texture |
| Shading | `glShadeModel` SMOOTH for models and light flashes, FLAT for the world | Gouraud colour across a triangle |
| Textures | `glBindTexture`, `glTexImage2D`, `glTexSubImage2D`, `glTexParameterf` (filters, wrap) | Resident textures by id, partial updates, wrap and clamp, nearest and linear |
| Frame | `glClear`, `glFinish`, `glDrawBuffer` | Clear depth, end the frame, show it |
| Not needed | `glFog*` (commented out in `gl_rmain.c`), `glReadPixels` (screenshots), the vertex array extension, `glHint` | Nothing; a screenshot can say it isn't on PB |

Our GL layer drops `gl_ztrick`, which flips the depth range each frame to
save a depth clear: PB clears depth itself. And PB corrects perspective on
every triangle, not only the world's.

## Where transforms happen

Decided: the Jam transforms, clips and projects, as a Voodoo 1's host did,
and PB sets up and draws the triangles (option A). The command format
leaves room to move transforms onto PB later, after A has been timed on
the board.

| Option | Jam's work a vertex | PB's gateware | Link a frame (demo1) | Cost and risk |
| --- | --- | --- | --- | --- |
| **A. The Jam transforms (Glide's split)** | 4x4 matrix, divide, clip to the near plane | Triangle setup, raster | about 63 KB | The vertex transform's 15% of M1's timedemo stays on the Jam, plus clipping (not measured yet) |
| B. PB transforms | none: object-space vertices and a matrix | A, plus a matrix unit, the divide, and clipping that makes new vertices | about 50 KB | Clipping in gateware is the hard part; needs the 60K's DSP blocks |
| C. B, with the world resident on PB | a list of visible surfaces, a few bytes each | B, plus walking surface lists in PB's memory | a few KB | The most Jam time saved; moves a piece of Quake's renderer into gateware |

Why A first:
- It is what "Glide-style" means: Glide took screen-space vertices and
  left transforms and clipping to the host, and GLQuake on a Voodoo 1 ran
  that way.
- It is the least gateware, and all of it can be held to a C model byte
  for byte, as `pb_fill.v` is held to `gfx.c` today.
- It frees PB from floats: the Jam sends fixed-point screen positions.
- B and C are an optimisation to measure for. pectinos's ROADMAP (Later,
  "A DSP engine of our own on the Tang 60K") already names vertex
  transforms as a use of that multiplier array.

Only triangles that cross the near plane need clipping, since PB cuts
spans to the surface (a guard band, as its fills do). That should keep
the Jam's clipping to a few triangles a frame (inferred: M1's null GL
doesn't clip).

## Commands and transfer

3D is more `DRAW` commands in the packets PB already takes, with drawn
mode's frames, sequence numbers, `taken` and two frames in flight. Nothing
new on the wire, and nothing comes back but the status block over I2C.

**A frame:** `START`, `SURFACE` (the colour picture), a depth surface and
its clear, state and primitives, `SHOW`. The kernel writes `START`,
`SURFACE` and `SHOW` as it does for drawn mode, so a program never names
a PB address. Textures are ids the kernel hands out, as a program's
surfaces are (API 60).

**New commands** (numbers past `POLYLINE`; a first cut for pectinos to
shape):

| Command | Carries | Why |
| --- | --- | --- |
| `DEPTH` | the depth surface, and clear it or not | One depth clear a frame instead of `gl_ztrick` |
| `STATE` | one word of mode bits: depth test (always, less-equal, greater-equal), depth write, blend (off, alpha, multiply-inverse, add), alpha test, combine (replace, modulate), second texture on or off, cull | Quake changes state between primitives, so it must be small |
| `TEXTURE` | the texture id for unit 0 or 1, its wrap and filter | about 35 binds a frame |
| `TRIS` | kind (fan, strip, list), the vertex format, n vertices | 790 primitives a frame in demo1, about 5 vertices each |

**The vertex**, fixed point, its format in `TRIS`'s argument byte:

| Field | Bytes | Form |
| --- | --- | --- |
| x, y | 4 | signed 12.4 screen pixels: a guard band of plus or minus 2,048 |
| q (1/w) | 4 | unsigned fixed point, 1.0 at the near plane; depth comes from it too |
| s, t | 4 | signed 10.6 texels; the Jam takes whole texture sizes off a polygon's coordinates so they stay small, and PB repeats |
| s2, t2 (lightmap) | 4 | as s, t; world surfaces only |
| colour | 0, 1 or 4 | none (the world), a grey level (models: Quake lights them in grey), or RGBA (particles, the screen tint) |

A world vertex is 16 bytes, a model vertex 13, a HUD vertex 8 (no q).
M1's guess in `lib/nullgl.h` is 28.

**What the kernel checks.** For drawn mode `pbdrawq_check` reads every
word. For `TRIS` it needs only the command word and the count: PB cuts
every span to the surface and every texel address to its texture, so no
vertex value reaches memory it shouldn't, and the check costs a
primitive, not a vertex.

**The CRC is already free.** `pb_ring.c` copies each packet into the
link's ring with DMA and reads its CRC-32 off the DMA sniffer
(`dma_copy_crc32`; RP2350 datasheet section 12.6.8.2). The Jam's cost a
frame is our GL layer writing the vertices and that copy, one more pass
over 50-70 KB (inferred; step 1 times it).

## Textures and lightmaps

**Wall, model and HUD textures** go as they are in the pak, a byte a
texel. GLQuake turns each into 32-bit RGBA on the CPU (`GL_Upload8`),
resamples it to a power of two and builds its mips (`GL_MipMap`,
`GL_ResampleTexture`): M1's 13 MB, and about 10% of the timedemo's time (23% of
the short server run), all of it at load. As indices they
are about 2.5 MB (13 MB / 4 bytes / 4/3 for the mips; inferred). PB:
- expands each texel through the palette the Jam sent once, into a texel
  format of its own (RGB565 is the obvious one; pectinos's choice);
- keeps index 255 as see-through, for the HUD's pictures, sprites and the
  sky's top layer;
- builds the mips by averaging 2x2 blocks, as `GL_MipMap` does, at load;
- takes power-of-two sizes only; the Jam resamples the few that aren't,
  in indices.

The Jam frees each texture's pixels once sent. The hunk is 5.75 MB of the
7 MB pool (LOG 2026-11-12), so that may matter as much as the time.

**Lightmaps** stay a byte a texel: 128x128 blocks (`GL_LUMINANCE`,
`BLOCK_WIDTH` 128), at most 20 (`MAX_LIGHTMAPS`, LOG 2026-11-12), 320 KB,
always on PB. Dynamic lights change a band of rows in a block,
`glTexSubImage2D`: 75-330 KB a run in M1: 2 KB/s in demo1, 50 KB/s in e1m1. PB needs:
- a partial update: a rectangle of rows into an existing texture, the
  address worked out by the kernel from the id;
- no mips;
- bilinear filtering on the lightmap only. A lightmap texel covers 16x16
  wall texels, so point sampling shows its squares. Wall textures stay
  point sampled: software Quake's look, half the texel reads;
- an update that waits for its frame. A memory packet lands as soon as it
  comes, while PB may still be drawing the frame before (PB.md,
  "Copies"); two copies of a changed block used in turn, or an update
  command inside the frame's own `DRAW` stream, keep it from tearing.

**Memory:** about 6.5 MB of RGB565 with mips plus the lightmaps. A
texture pool of 16 MB covers Quake with room to spare.

## Pixels

640x480 RGB565 with 16-bit depth, at x1.5 on 720p by the scan-out mode PB
already has (`fb_scan` mode 2). It needs the DDR3, not the SDRAM module.
The resolution costs PB only: the Jam's work is the same at any size.

**The fill budget** (all inferred; overdraw 2 is a guess, which our GL
layer can count):

| A frame | Bytes | At 30 fps | At 60 fps |
| --- | --- | --- | --- |
| Pixels drawn, overdraw 2 | 614,000 pixels | 18 Mpx/s | 37 Mpx/s |
| Depth read and write, colour write: 6 bytes a pixel | 3.7 MB | 110 MB/s | 220 MB/s |
| Blended pixels' colour read (a guess of 20%) | 0.25 MB | 7 MB/s | 15 MB/s |
| Depth clear | 0.6 MB | 18 MB/s | 37 MB/s |
| Scan-out (60 Hz either way) | 0.6 MB | 37 MB/s | 37 MB/s |
| Texture cache refills (a guess of a byte a pixel) | 0.6 MB | 18 MB/s | 37 MB/s |
| **Total** | | **about 190 MB/s** | **about 350 MB/s** |

The DDR3-800 x16 peaks at 1.6 GB/s, the SDRAM module at 266 MB/s (16 bits
at 133 MHz). Scattered small accesses get a fraction of peak, so 60 fps
needs PB to work in bursts (a span's depth in one, its writes in
another), and 3D starts on `pbshow3`'s DDR3. At a pixel a clock on the
screen's 74 MHz the engine has twice what 60 fps needs.

Step 2's C model settles these as rules (`docs/PB3D.md`; Jacob,
2026-11-12), for pectinos to change in the model first if the gateware
wants them otherwise:

- **Depth:** 16 bits from q, in a small floating format (4 bits of
  exponent, 12 of mantissa, as Voodoo's w-buffer) so far walls don't fight.
- **Perspective:** a divide every pixel, one reciprocal shared by every
  attribute, so nothing swims. Software Quake divided every 16 pixels.
- **Mips:** one level a triangle, from its texel and pixel areas.
- **Colour:** 8 bits a channel inside, RGB565 out with no dithering. The
  lightmap multiply is coarse in 565; dithering can come later as a STATE
  bit.
- **1080p** (`pbshow3_1080`): 640x480 has no whole scale there; x2 with
  bars, or 960x540 at x2 (1.7 times the pixels).

## Link and memory

The link carries a 60 fps Quake in under a tenth of its capacity. What
can hold it back is flow control.

| demo1, a frame | Vertices | Bytes a vertex | KB a frame | MB/s at 33 fps | MB/s at 60 fps |
| --- | --- | --- | --- | --- | --- |
| M1 as measured (two passes, the 28-byte guess) | 3,950 | 28 | 108 | 3.6 | 6.5 |
| The compact vertex, two passes | 3,950 | 16 | 66 (3 KB of headers) | 2.2 | 4.0 |
| The compact vertex, two textures a pixel | about 2,750 (inferred) | 16 | 47 | 1.6 | 2.8 |

The last row assumes the world is about 60% of the vertices and the
lightmap pass sends it twice (inferred; our GL layer can count it). Load
is about 2.5 MB of texture indices, well under a second.

**Flow control is the catch.** Drawn mode reads `taken` over I2C (about
0.6 ms at 100 kHz) and sends only what fits PB's queue, 7,144 words (14
KB) with the big queue. Within the tick's 3 ms budget that is about five
reads, roughly 70 KB a tick, or 4 MB/s at 60 Hz (inferred from PB.md's
figures), against 2.8 MB/s needed. Either fix is enough:
- a deep command queue in the DDR3, about 1 MB, two whole frames;
- the I2C at 1 MHz (`tb_i2c` already tests it).

| PB's memory for QUINCE | MB |
| --- | --- |
| Textures, RGB565 with mips | about 6.5 |
| Lightmaps, twice (one in use, one being updated) | 0.6 |
| Colour pictures, three of 640x480 RGB565 | 1.8 |
| Depth, 640x480 | 0.6 |
| Command queue | 1 |
| **Total** | **about 10.5**, 2% of 512 MB |

## Asks of pectinos, and the risks

| Layer | Ask | At the pin |
| --- | --- | --- |
| Gateware | The triangle engine: setup, spans, perspective, depth test and write, two textures with combine, blend, alpha test | Fills, lines, text and copies on 8-bit surfaces |
| Gateware | RGB565 drawing surfaces and a depth surface | Drawing is mode 5, a byte a pixel; RGB565 is scan-out only |
| Gateware | Texture upload: expand through a palette, mark index 255, build mips | None |
| Gateware | A deep command queue in the DDR3 (or the I2C at 1 MHz) | 7,144 words in block RAM |
| Kernel | A 3D service: a command list checked a primitive at a time; texture ids (make, upload, update a rectangle, free) from a texture pool | AH=14h's 2D list; surfaces by id (API 60) |
| Kernel | A 640x480 RGB565 drawn mode, and `video_info` saying the 3D service is there | pectinos DESIGN section 17 ("Future", "Stock Fruit Jam and PB&J") names "whether there is a 3D service" as still to add |
| Tools | A C model of the triangle engine, byte for byte with the gateware | `drawmodel.c`, for 2D |

Risks, largest first:
1. **Multipliers.** Setup, perspective and the combine all multiply. No
   PB design uses the GW5AT's DSP blocks yet, and whether the open tools
   place them on the 60K is unchecked. Multipliers in LUTs cost a lot of
   the chip.
2. **Room and timing.** `pbshow` (the SDRAM build, with `COPY`) uses about
   31% of the LUTs, and both it and `pbshow3` sweep seeds to meet their
   clocks. A triangle engine would be the largest thing on PB (inferred).
3. **DDR3 efficiency.** 350 MB/s is about a fifth of peak, but only in
   bursts.
4. **The Jam's time.** M1 is 30 ms a frame for demo1 with nothing drawn.
   Clipping, packing and the copy are added, texture work goes. 30 fps is
   the first target; 60 needs option B or C.

To start now, since they don't wait on the command format: the DSP check,
flow control, and the RGB565 and depth surfaces on `pbshow3`. The command
set and the kernel's service follow once step 2's C model has drawn
Quake.

## Steps

1. **QUINCE: the real GL layer, still drawing nothing.** It writes the
   actual command list (compact vertices, fans and strips, state,
   near-plane clipping, two textures a pixel, `gl_ztrick` off), counts the
   bytes exactly, and the board times it. This replaces the inferred
   figures above.
2. **The C model draws Quake.** QUINCE writes the triangle engine's C
   model in `lib/` (our own code), and the host test writes demo1's
   frames as pictures: Quake drawn by PB's rules before any Verilog, while
   the format is still cheap to change. pectinos can take the file over as
   its gateware's reference with Jacob's OK.
3. **PB: flat triangles with depth.** Quake's rooms in flat colours on the
   monitor.
4. **PB: textures.** Upload with palette and mips, point sampling,
   perspective.
5. **PB: lightmaps and the rest.** The second texture, partial updates,
   blending, alpha test, the HUD; demo1 timed on the board.
6. **Measure, then decide** whether option B or C is worth it.

Steps 1 and 2 are QUINCE's; 3 to 5 wait on pectinos's gateware, and
QUINCE's side of each is its GL layer and a board run.

## Decided (Jacob, 2026-11-12, all recommendations)

| # | Decision | Taken | Instead of |
| --- | --- | --- | --- |
| 1 | Where transforms happen | The Jam (A), B or C after step 5's timings | PB from the start |
| 2 | Resolution | 640x480 at x1.5 | 320x240 at x3; 960x540 on 1080p |
| 3 | Lightmaps | Two textures a pixel | A second blended pass |
| 4 | Wall filtering | Point sampled, bilinear on lightmaps only | Bilinear everywhere |
| 5 | Texture upload | 8-bit indices; PB expands and builds mips | RGBA from the Jam |
| 6 | Where this plan lives | Here, with an issue on zigphroid/pectinos | pectinos's `plans/` |
| 7 | The triangle engine's C model | QUINCE first, in `lib/` | pectinos from the start |
