# PB3D: how the pixelboard draws a triangle

The rules of PB's triangle engine, as QUINCE's C model draws them
(`lib/pbmodel.c`; `docs/PB-ASKS.md`, step 2). The model is the rules: where
this page and the code differ, the code is right and this page gets fixed.
Gateware may draw in spans, bursts and caches, in any order, but it must
write the same bytes the model writes. The commands that carry all this
over the link are `lib/pb3d.h`.

Every number below is an integer. "Floor" means rounded toward minus
infinity, for either sign. Choices Jacob made on 2026-11-12 are marked
(Jacob); the rest are the model's, with the reason, in `docs/LOG.md`.

## What a vertex carries

| Field | Form | Note |
| --- | --- | --- |
| x, y | signed 12.4 pixels | plus or minus 2,048: the guard band |
| q | unsigned 1.31, near / w | 1.0 (0x80000000) at the near plane; perspective and depth both come from it |
| s, t | signed 12.4 texels | unit 0: the wall, model or picture |
| s2, t2 | signed 10.6 texels | unit 1: the lightmap |
| r, g, b, a | 0-255 | white and opaque when the vertex has none |

A vertex sent without q has q = 1.0, which is depth 0, the nearest.

## Which pixels a triangle takes

A pixel is its centre, at (px + 0.5, py + 0.5): 8 sixteenths past the
whole pixel in 12.4.

The edge function of a to b at p is `(bx - ax)(py - ay) - (by - ay)(px - ax)`,
exact in 64 bits (the guard band makes it up to 35). The triangle's area is
the edge function of v0 to v1 at v2.

- **Area 0:** nothing is drawn.
- **Winding:** on the screen with y down, a positive area is clockwise.
  `CULL_CW` drops those and `CULL_CCW` drops the others. After culling, the
  model swaps v1 and v2 if the area is negative, so the area is positive.
- **Inside:** a centre is inside if all three edge functions are positive,
  or zero on an edge the triangle owns.
- **The top-left rule:** a triangle owns a top edge (flat, with the triangle
  below it) and a left edge (the triangle to its right). That is an edge
  whose dy is below 0, or whose dy is 0 and dx above 0. Two triangles that
  share an edge walk it in opposite directions, so exactly one of them owns
  it. Neither leaves a gap and neither draws a pixel twice.
- Pixels off the surface are never touched, nor pixels outside the clip
  rectangle (`CLIP`: x0 <= px < x1 and y0 <= py < y1, held to the
  surface; a frame starts with the whole surface). GL's viewport is the
  rectangle, so GLQuake's 3D view stops above its status bar, as it does
  on real GL. The Jam still cuts at the near plane and the guard band;
  the rectangle only narrows which pixels are written. The depth clear
  takes the whole surface whatever the rectangle.

A strip's odd triangles are taken as (k + 1, k, k + 2), so every triangle
in a strip winds the same way. Flat shading uses each triangle's first
vertex in that order.

## Perspective (Jacob: a divide every pixel)

Each vertex's weight at a pixel is its edge function (the edge facing it)
times its q. Every attribute is the weighted mean of the vertices' values,
so it is perspective-correct with no setup divides. The cost is one
reciprocal per pixel, shared by every attribute.

To keep within 64 bits:
1. **Per triangle:** `es` = max(0, bits(area) - 24), and the edge values
   are taken `>> es`, so each is at most 24 bits. `qs` = max(0, bits(largest q) - 16),
   and each q is taken `>> qs`, so each is 16 bits. A weight is then at most
   40 bits.
2. **Per pixel:** S is the sum of the weights. If S is 0, the pixel isn't
   drawn. `sh` = bits(S) - 16, which is -15 to 26, and S16 = floor(S / 2^sh).
   S16 is always 16 bits, 2^15 to 2^16 - 1, cut down from a long S or moved
   up from a short one. R = floor(2^40 / S16), 2^24 to 2^25.
3. **An attribute** is N = the sum of weight times value, and the result is
   floor(floor(N / 2^sh) x R / 2^36). That is the value with 4 more
   fraction bits: s and t come out as 12.8, s2 and t2 as 10.10, and colour
   as 8.4 (then floored to 8 bits and held to 0-255).

PB can find R with a table and one Newton step, as long as a final
correction makes it exactly floor(2^40 / S16).

Colour is perspective-correct too. It costs nothing extra, because it
uses the same R.

## Depth (Jacob: a 16-bit float of q)

- **q at the pixel:** once per triangle, RA = floor(2^46 / (area >> es)).
  At each pixel, q = floor(S16 x RA / 2^(46 - sh - qs)), held to 32 bits.
  That is the screen-linear mean of the vertices' q, to about a 32,768th
  (S16's 16 bits), under the 12-bit mantissa's step below. A triangle drawn
  twice gets the same depth both times.
- **The format:** a 4-bit exponent and a 12-bit mantissa, as Voodoo's
  w-buffer, so every distance gets about a 4,096th of itself in precision.
  q at or above 1.0 is taken as 0x7FFFFFFF. With lz the leading zeros of q
  in 32 bits (1 or more) and e = lz - 1: if e > 15 (q below 2^-16, past
  65,536 near planes), or q is 0, d = 0xFFFF. Otherwise the mantissa m is
  the 12 bits after q's leading 1, and d = 0xFFFF - ((15 - e) << 12 | m).
  So d is 0 at the near plane and grows with distance.
- **The depth range:** stored = near + floor(d x (far - near + 1) / 2^16).
  A frame starts at 0, 0xFFFF. The view weapon is at 0 to 0x4CCC.
- **The test:** `ALWAYS`, `LEQUAL` (stored <= the buffer's) or `GEQUAL`. A
  pixel that fails is done. A pixel that passes and is drawn (not dropped
  by the alpha test) writes its depth if `DEPTH_WRITE` is set.

## Textures

**Sizes:** powers of two, 1 to 1,024 a side. Ids 1 to 65,535.

**Storage:**

| Sent as | Kept as | See-through |
| --- | --- | --- |
| `INDEX8`, through the palette | RGB565 | never: index 255 is a colour |
| `INDEX8` with `TEXF_ALPHA` | ARGB1555 | index 255 |
| `RGBA8888` | ARGB1555 | alpha below 128 |
| `GREY8` (lightmaps) | 8 bits | never |

A channel goes in by dropping its low bits. It comes out at 8 bits by
copying its top bits into the bottom, so 5 bits of 31 is 255 and 0 is 0.
ARGB1555 has a bit for alpha that 565 has no room for, so it keeps green to
5 bits like red and blue. GLQuake marks only HUD pictures, sprites and the
sky's top layer as alpha.

**Mips** (`TEXF_MIPMAP`) go down to 1x1. Each texel of level l + 1 is the
2x2 block of level l above it, read as 8 bits a channel: each channel is
(sum + 2) / 4. A side of 1 uses its one texel twice. A mip texel is
see-through if two or more of the four are. That is GLQuake's average
alpha against its alpha test (above 0.666 means three or four of the four
are opaque). Levels are made from the level above as stored, so PB can
build them in place. A load into any rectangle of level 0 makes all the
mips again.

**The mip level (Jacob: one per triangle).** This is the largest l such
that the triangle's texel area is at least 4^l times its pixel area. The
texel area is the edge function over s, t, and for unit 1 it is compared
with 16 times the pixel area, because 10.6 is finer. The level is held to
the texture's last level, and is 0 unless the unit's `MIPMAP` is set. The
cost of choosing per triangle: a long wall gets one level from end to end,
so its far end shimmers where per-pixel levels wouldn't. Software Quake
had the same per surface.

**Reading** at level l: s and t are shifted down l places (floor), then:
- **Nearest:** the texel floor(s), floor(t).
- **Bilinear** (`LINEAR`; Jacob: lightmaps only): take the point half a
  texel up and to the left. Its whole part names the top-left texel of
  four, and the top 4 bits of its fraction (fu, fv) are the weights. Each
  channel is ((c00 (16 - fu) + c10 fu)(16 - fv) + (c01 (16 - fu) + c11 fu) fv + 128) / 256,
  floored.
- **Wrap** (the default) takes a coordinate modulo the side. **Clamp**
  holds it to 0 and the side minus 1. Each of s and t has its own setting.

## A pixel's colour

In order, at 8 bits a channel:
1. **The vertex colour V:** the perspective mean if `SMOOTH`, else the
   triangle's first vertex's.
2. **Unit 0:** if `TEX0` is off, C = V. If it is on, T is its texel (white
   and opaque if no texture is bound). With `MODULATE`, C = floor(T (V + 1) / 256)
   per channel, alpha included. Otherwise C = T.
3. **Unit 1 (Jacob: one pass, 1x):** if `TEX1` is on with a texture bound,
   with g its texel, each of r, g, b becomes floor(C (256 - g) / 256).
   GLQuake keeps 255 minus the light in its lightmaps, so a stored 0 is full
   light. Software Quake's 2x "overbright" would be a STATE bit later.
4. **The alpha test:** if `ALPHA_TEST` is on and C's alpha is below 170,
   the pixel is dropped. GLQuake's test is GREATER 0.666, and 170 / 255 is
   just over.
5. **Blending,** against the picture's pixel P read back from 565, with
   w(x) = x + (x >> 7), so 0 counts as 0 and 255 as 256:
   - `ALPHA`: floor((C w(a) + P (256 - w(a))) / 256).
   - `MULINV`: floor(P (256 - w(C)) / 256), each channel by its own.
   - `ADD`: C + P, held at 255.
6. **The write:** RGB565, each channel's low bits dropped, with no
   dithering (Jacob). The depth is written if `DEPTH_WRITE` is set.

## Reading the commands

`lib/pbdecode.c` turns `lib/pb3d.h`'s words into the model's calls, as
PB's command reader is to. A frame (the words between `START` and `SHOW`)
starts from a frame's start: every `STATE` bit 0, no texture on either
unit, the depth range 0 to 0xFFFF, the whole surface to draw in. Then:

- `DEPTH` clears the depth surface to 0xFFFF if its argument's bit 0 is
  set, and otherwise does nothing (the depth of the frame before stays).
- `STATE`, `TEXTURE`, `DEPTHRANGE` and `CLIP` replace what they name
  until the frame ends or they come again.
- `TRIS` takes each vertex's fields in the order the format names them,
  a 32-bit q as two words, the low one first. A field the format leaves
  out takes q 1.0, s, t, s2 and t2 0, and white, opaque. A grey colour
  is the word's low byte; an RGBA one is r, g in the first word and b, a
  in the second, each low byte first.
- A command `pb3d_cmd_words` doesn't know (an unknown op, an argument
  with a stray bit, one running past the frame's words) is refused: the
  commands before it are drawn, it and the rest of the frame are not.

The texture records (`PB3D_REC_TEXIMAGE`, `TEXSUB`, `TEXFREE`) are the
kernel's calls, not commands: a side that isn't a power of two up to
1,024, or an unknown format, is refused, and an update must lie inside
a texture already made.

`tests/pbplay.c` draws a `QUINCE_PBDUMP` file this way, frame after
frame on one picture, as PB would (the colour isn't cleared between
frames: GLQuake draws every pixel of its view).

## What the model counts

Triangles set up, culled or with no area, pixels covered, failing depth,
failing the alpha test, drawn, and blended (read the picture first). These
are the fill budget's numbers in `docs/PB-ASKS.md`, measured on Quake's
frames by `tests/pbplay.c`.

## Changing the rules

A change to any rule changes the model's code first. `tests/test_pbmodel.c`
holds each rule with a check of its own, and `make test`'s scene
(`build/pbmodel/scene.png`) holds them together, as Quake uses them, by
its CRC. A rule
changed on purpose gets a new CRC, with the picture looked at and the
reason in `docs/LOG.md`. When pectinos takes this over for its gateware,
these rules are what its test benches hold PB to.
