# Quake software-renderer spans, 640x480

These are the spans that Quake's software renderer draws, for pectinos's memory bench. QUINCE made them on 2026-11-12.

The renderer is id's WinQuake one, run on the PC at 640x480. The source is QUINCE's `quake/` as imported (QUINCE commit `19eee2d`, before any `QUINCE:` change), built with `-m32` and the C span code (not id's x86 assembly). QUINCE itself draws through GLQuake's renderer to PB, so this build exists only to make the dump. The dump holds span geometry and nothing from the game data.

## Frames

The view is Quake's default, viewsize 100: the status bar takes the bottom 48 rows, so the 3D view is 640x432 at (0,0) and every frame's spans cover all 276,480 of its pixels. No chosen frame is underwater (`warp 0` in each header). Frames are numbered from 1, the demo's first drawn frame. The clock is fixed (1/72 s per frame), so every run draws the same frames.

| File | Frame | Textured spans (px) | z spans | Water spans (px) | Sky spans (px) |
|---|---|---|---|---|---|
| `demo1-f0466.txt` | demo1's busiest of 969 | 8,981 (272,771) | 9,104 | 123 (3,709) | 0 |
| `demo2-f0786.txt` | demo2's busiest of 985 | 9,980 (268,535) | 10,314 | 334 (7,945) | 0 |
| `demo3-f0334.txt` | demo3's busiest of 1090 | 8,998 (270,528) | 9,089 | 70 (3,817) | 21 (2,135) |
| `e1m1-f0001.txt` | e1m1's first frame, a quiet baseline | 5,681 (276,480) | 5,681 | 0 | 0 |

"Busiest" means the most z spans, which is one per span of every surface drawn. The demos average about 3,600 to 4,200 z spans a frame. `counts/DEMO.count` has every frame, so other frames can be picked: one line per frame, `frame` and then `spans pixels` for each kind, in the order tex, z, sky, turb, solid.

## Format

Each file is text. It starts with two `#` lines and ends with a `# end` line of totals. In between is one line per span, in the order the renderer drew them:

```
# frame 466 screen 640x480 view 0 0 640 432 warp 0 zwidth 640
# kind surf u v count izi izistep [cache cachew cacheh s0 t0 s1 t1]
tex 4 614 175 26 8918818 44420 416944 96 240 5529522 9159519 3899762 9551962
...                (the rest of surface 4's tex spans)
z 4 614 175 26 8918818 44420
```

- **kind**: the call that drew the span (`d_edge.c`'s `D_DrawSurfaces`).
  - `tex` is `D_DrawSpans` (the build runs `D_DrawSpans16`, as Quake does by default): it reads a lit surface from the surface cache and writes the framebuffer.
  - `z` is `D_DrawZSpans`. It runs for every surface, right after that surface's own spans, so the same span list shows up twice: once as `tex`, `turb`, `sky` or `solid`, then again as `z`.
  - `turb` is `Turbulent8` (water, slime, lava, teleporters). It reads a 64x64 texture straight, with no surface cache.
  - `sky` is `D_DrawSkyScans8`.
  - `solid` is `D_DrawSolidSurface`, which only runs for the background with nothing behind it. It doesn't appear in these frames.
- **surf**: the surface's index in `surfaces[]` for this frame, so spans can be grouped by surface.
- **u v count**: the screen x, the row and the length in pixels. A span covers pixels u to u+count-1 of row v.
- **izi izistep**: 1/z in fixed point, as `D_DrawZSpans` works it out: `izi = (int)(zi * 0x80000000)` at pixel u, plus `izistep` per pixel. The zbuffer is 16 bits per pixel, `zwidth` shorts per row, and it is given `izi >> 16`, stepping `izi += izistep` along the span. Bigger means nearer. The same two values are printed on the `tex`/`turb`/`sky` lines too; there they describe the same surface, but those calls write no z.
- **`tex` only: cache cachew cacheh s0 t0 s1 t1**
  - `cache` is the byte offset of the surface's texels in the surface cache. The cache is one block of 1,344,000 bytes, `D_SurfaceCacheForRes(640, 480)`.
  - `cachew` and `cacheh` are the cached surface's width and height in texels (one byte each).
  - `s0 t0` and `s1 t1` are the texel coordinates in 16.16 at the span's first and last pixel, worked out and clamped as `D_DrawSpans8` does.
  - The span reads `cache + (t >> 16) * cachew + (s >> 16)` for each pixel. In between, s and t follow perspective (`D_DrawSpans16` corrects every 16 pixels), so a straight line from (s0,t0) to (s1,t1) is close but not exact.

## Not in the dump

Models, the view weapon, sprites and particles are drawn after the spans, by `D_PolysetDraw`, `D_SpriteDrawSpans` and `D_DrawParticle`. They read and write the zbuffer and framebuffer per pixel, and they aren't recorded here. In these frames they touch a few thousand pixels: in demo1 frame 466, 17,485 zbuffer pixels changed after the spans.

## Checked

With `SPANS_CHECK=1` set, the tool rebuilds every pixel's z from the `z` lines and compares it with the real zbuffer at the end of the frame. To do that it turns off models and particles (`SPANS_NOMODELS=1`). Every pixel of every frame of demo1 and demo3 matches, and so does e1m1's frame 1. In demo2, two of its 985 frames don't match: frames 459 and 709, where the view goes into and out of water. That's when Quake changes the view, and with it the zbuffer's layout, partway through the frame (inferred, not chased). The four frames above match pixel for pixel. The dumps are the normal game, with models on.

## Rebuilding

`tool/` has the code. `quake.patch` adds the hooks to `d_edge.c` and lets `SPANS_NOMODELS` skip particles in `r_main.c`. The rest are new files: `spandump.c` (writes the spans), `vid_spans.c` (a 640x480 screen in memory) and `sys_spans.c` (a fixed clock and the main loop). It needs a 32-bit gcc: `gcc-multilib` on Debian and Ubuntu.

```
sh tool/build.sh ~/src/QUINCE /tmp/sp            # -> /tmp/sp/spans
mkdir -p base/id1 && cp pak0.pak base/id1/       # the shareware pak
SPANS_COUNT=demo1.count /tmp/sp/spans base demo1           # counts per frame
SPANS_FRAMES=466 SPANS_OUT=demo1 /tmp/sp/spans base demo1  # -> demo1-f0466.txt
SPANS_FRAMES=1 SPANS_OUT=e1m1 /tmp/sp/spans base map e1m1 1
```

`SPANS_FRAMES` takes a comma-separated list of frames.
