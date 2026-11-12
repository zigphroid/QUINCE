/* lib/pbmodel.h: a C model of the pixelboard's triangle engine.
 *
 * Our own code (docs/PB-ASKS.md, step 2; the rules are docs/PB3D.md).
 * It draws screen-space triangles the way PB is to draw them, pixel by
 * pixel from exact integer rules, into a picture of RGB565 and a 16-bit
 * depth buffer the caller owns. What it writes is the spec: gateware that
 * draws in spans, bursts and caches must still write the same bytes, as
 * pectinos's pb/sim/drawmodel.c is to its 2D engine.
 *
 * No libc and no kernel calls, so it builds anywhere (lib/'s rule). It
 * isn't part of QUINCE.JAM: the Jam sends triangles, PB draws them, and
 * the model draws them on the PC for the tests.
 */
#ifndef QUINCE_PBMODEL_H
#define QUINCE_PBMODEL_H

#include <stdint.h>

/* A vertex, as PB has it once the command is read. The values are the
 * wire's (lib/pb3d.h), held in wider ints: x, y signed 12.4 pixels; q
 * near / w as unsigned 1.31 (0x80000000 is 1.0, the near plane); s, t
 * unit 0's texel position, signed 12.4; s2, t2 unit 1's, signed 10.6;
 * the colour 0-255 a channel. */
struct pbm_vertex {
	int32_t x, y;
	uint32_t q;
	int32_t s, t, s2, t2;
	uint8_t r, g, b, a;
};

enum { PBM_DEPTH_ALWAYS, PBM_DEPTH_LEQUAL, PBM_DEPTH_GEQUAL };
enum { PBM_BLEND_OFF, PBM_BLEND_ALPHA, PBM_BLEND_MULINV, PBM_BLEND_ADD };
enum { PBM_CULL_NONE, PBM_CULL_CW, PBM_CULL_CCW };
enum { PBM_LIST, PBM_STRIP, PBM_FAN };

/* The drawing state: what lib/pb3d.h's STATE word says, as ints. */
struct pbm_state {
	int depth_func;     /* PBM_DEPTH_* */
	int depth_write;    /* a pixel that is drawn writes its depth */
	int blend;          /* PBM_BLEND_* */
	int alpha_test;     /* drop a pixel whose alpha is under 170 */
	int tex0;           /* unit 0 on; off, the vertex colour alone */
	int modulate;       /* unit 0's texel times the vertex colour; 0, the texel */
	int tex1;           /* unit 1 on: darken by its grey (the lightmap) */
	int cull;           /* PBM_CULL_* */
	int smooth;         /* colour across the triangle; 0, its first vertex's */
};

/* Texture formats (lib/pb3d.h's PB3D_TEX_*), and how they are kept. */
enum { PBM_TEX_INDEX8, PBM_TEX_RGBA8888, PBM_TEX_GREY8 };
enum { PBM_KEEP_565, PBM_KEEP_1555, PBM_KEEP_GREY };

#define PBM_TEX_IDS     65536u  /* ids 1 to 65535, as the wire's; 0 is no texture */
#define PBM_TEX_LOG_MAX 10      /* 1,024 a side */
#define PBM_LEVELS_MAX  (PBM_TEX_LOG_MAX + 1)

struct pbm_texture {
	uint16_t *level[PBM_LEVELS_MAX]; /* each level's texels in the pool, rows from the top */
	uint8_t lw, lh;      /* log2 of the width and height */
	uint8_t levels;      /* 1, or every level down to 1x1 */
	uint8_t format;      /* PBM_TEX_*: what pbm_texture_load is given */
	uint8_t keep;        /* PBM_KEEP_*: how the texels are kept */
	uint8_t alpha;       /* index 255 is see-through (index textures) */
	uint8_t made;
};

/* How a unit reads its texture (lib/pb3d.h's PB3D_T_*). */
struct pbm_bind {
	uint32_t id;
	int clamp_s, clamp_t, linear, mipmap;
};

/* What happened to the pixels: the fill budget's numbers. */
struct pbm_counts {
	uint32_t tris;        /* every triangle seen, culled ones too */
	uint32_t culled;      /* dropped by their winding, or with no area */
	uint32_t covered;     /* pixels inside a triangle and on the surface: */
	uint32_t unweighted;  /*   of those, ones whose weights sum to 0, skipped */
	uint32_t depth_fail;
	uint32_t alpha_fail;
	uint32_t drawn;       /* colour written: covered is unweighted +
	                       * depth_fail + alpha_fail + drawn */
	uint32_t blended;     /* of those, read the picture first */
};

/* The mip study (LOG 2026-11-12): how a pixel's level is chosen, for
 * tests/pbplay.c to compare. PBM_LOD_TRI is the rule (docs/PB3D.md's
 * "The mip level"); the others are what PB was weighed against and isn't
 * asked for. Their numbers are doubles, not rules; all zeros is the rule. */
enum {
	PBM_LOD_TRI,        /* one level a triangle, from its areas (the rule) */
	PBM_LOD_Q,          /* a level a pixel, from the areas and q there */
	PBM_LOD_EXACT,      /* a level a pixel, from the s and t gradients (GL's) */
	PBM_LOD_TRILINEAR   /* GL's trilinear: EXACT's level unrounded, blending
	                     * floor(lambda) and the next by its fraction */
};
struct pbm_study {
	int lod;            /* PBM_LOD_* */
	int scale;          /* 0 or 1: as sent; N: positions times N (supersampling) */
	int bias;           /* half levels added to every choice: -1 rounds the
	                     * single-level modes down; TRILINEAR is GL's at 0 */
	int linear0;        /* unit 0 read bilinear whatever its bind says */
	void (*texel)(void *ctx, uint32_t pool_index);   /* each texel read */
	void *ctx;
	uint64_t level_moved;   /* pixel reads whose level isn't the triangle's */
	uint64_t mip_reads;     /* pixel reads of a mipmapped texture */
};

struct pbm {
	uint16_t *colour, *depth;   /* w x h each, rows from the top */
	int w, h;
	uint16_t *pool;             /* the texture memory */
	uint32_t pool_words, pool_used;
	uint8_t palette[768];
	struct pbm_texture tex[PBM_TEX_IDS];
	struct pbm_state state;
	struct pbm_bind unit[2];
	uint32_t depth_near, depth_far;
	int clip[4];                /* x0, y0, x1, y1: the pixels drawn */
	struct pbm_counts counts;
	struct pbm_study study;
};

/* Errors the calls return: 0 is done. */
enum { PBM_OK, PBM_E_ID = -1, PBM_E_SIZE = -2, PBM_E_FULL = -3, PBM_E_FORMAT = -4 };

/* A model drawing into colour and depth (w x h, w and h 1-2,048), with
 * pool_words of texture memory. Its state is a frame's start (pbm_frame). */
void pbm_init(struct pbm *m, uint16_t *colour, uint16_t *depth, int w, int h,
              uint16_t *pool, uint32_t pool_words);
/* A frame's start: state all off, no textures bound, the depth range 0 to
 * 0xFFFF, the whole surface drawn in, the counts kept. */
void pbm_frame(struct pbm *m);
/* r, g, b for each of 256 indices: index textures loaded after it. */
void pbm_palette(struct pbm *m, const uint8_t *rgb);

/* Make texture id (again): format PBM_TEX_*, 2^lw x 2^lh, mips if mipmap,
 * index 255 see-through if alpha (RGBA8888 is always kept with alpha).
 * Making an id again throws its texels away; the pool isn't given back
 * (pbm_textures_reset gives all of it back). */
int pbm_texture(struct pbm *m, uint32_t id, int format, int lw, int lh,
                int mipmap, int alpha);
/* Fill the rectangle x, y, w, h of level 0 from data (rows from the top,
 * a byte a texel, or four for RGBA8888) and make the mips again. */
int pbm_texture_load(struct pbm *m, uint32_t id, int x, int y, int w, int h,
                     const uint8_t *data);
void pbm_textures_reset(struct pbm *m);

void pbm_set_state(struct pbm *m, const struct pbm_state *s);
void pbm_bind_texture(struct pbm *m, int unit, const struct pbm_bind *b);
void pbm_depth_range(struct pbm *m, uint32_t near_d, uint32_t far_d);
/* Triangles draw only the pixels with x0 <= x < x1 and y0 <= y < y1 (and
 * on the surface); the clears still take the whole surface. */
void pbm_clip(struct pbm *m, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
/* The whole picture to colour (RGB565), the whole depth buffer to depth. */
void pbm_clear_colour(struct pbm *m, uint16_t colour);
void pbm_clear_depth(struct pbm *m, uint16_t depth);

/* Triangles from n vertices: a list (3k, 3k+1, 3k+2), a strip (k, k+1,
 * k+2 for even k, k+1, k, k+2 for odd, so all wind one way) or a fan
 * (0, k+1, k+2). */
void pbm_draw(struct pbm *m, int kind, const struct pbm_vertex *v, uint32_t n);
void pbm_triangle(struct pbm *m, const struct pbm_vertex *v0,
                  const struct pbm_vertex *v1, const struct pbm_vertex *v2);

/* The rules' pieces, for the checks and for anyone holding gateware to
 * them. */
uint32_t pbm_depth_of_q(uint32_t q);        /* q (1.31) to d, 0 nearest */
uint32_t pbm_recip(uint32_t s16);           /* floor(2^40 / s16), s16 2^15-65535 */
uint16_t pbm_565(uint32_t r, uint32_t g, uint32_t b);

#endif
