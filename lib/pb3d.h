/* lib/pb3d.h: the 3D commands QUINCE sends the pixelboard (PB), and the
 * dump the host writes of them.
 *
 * Our own code (docs/PB-ASKS.md, step 1; LOG 2026-11-12, "PB's wire
 * format"). This is QUINCE's proposal: pectinos owns PB's command numbers
 * and will take these over, or change them, when it builds the 3D
 * service. Until then this header is the contract between the GL layer
 * that writes the commands (lib/pbgl.c) and the C model that draws them
 * (lib/pbmodel.c): the model reads its input only through what is here.
 *
 * The commands are more of PB's drawing commands, the payload of a DRAW
 * packet ($PECTIN_ROOT/docs/PB.md, "Drawing: PB fills shapes itself";
 * $PECTIN_ROOT/kernel/drivers/pb_link.h, PB_DL_*). Each is one or more
 * 16-bit words, and the first word's high byte is the command, its low
 * byte an argument. Numbers 0-14 are PB's today (14 is POLYLINE), so ours
 * start at 15. A frame is START, SURFACE, these, SHOW: the kernel writes
 * START, SURFACE and SHOW, as it does for drawn mode, and a program
 * never names a PB address. A 32-bit value is two words, the low one
 * first, as START's frame number is.
 *
 * Textures are not drawing commands: making one, filling it and changing
 * a rectangle of it are calls to the kernel, which finds the texture's
 * place in PB's memory from its id. The dump (end of this file) holds
 * them as records between the frames, in the order the program made them.
 */
#ifndef QUINCE_PB3D_H
#define QUINCE_PB3D_H

#include <stdint.h>

/* --- the commands --- */

enum {
	PB3D_DEPTH      = 15,   /* the frame's depth surface: arg bit 0, clear it */
	PB3D_STATE      = 16,   /* the drawing state: one word of PB3D_S_* bits */
	PB3D_TEXTURE    = 17,   /* bind a texture to a unit (arg) */
	PB3D_TRIS       = 18,   /* triangles: arg is kind and vertex format */
	PB3D_DEPTHRANGE = 19,   /* near and far, in the stored depth's units */
	PB3D_CLIP       = 20    /* the rectangle drawn in, in pixels */
};

#define PB3D_WORD(op, arg)  ((uint16_t)((uint32_t)(op) << 8 | ((uint32_t)(arg) & 0xffu)))
#define PB3D_OP(word)       ((uint32_t)(word) >> 8)
#define PB3D_ARG(word)      ((uint32_t)(word) & 0xffu)

/* Words each command takes, its first word included. */
#define PB3D_DEPTH_WORDS      1u
#define PB3D_STATE_WORDS      2u
#define PB3D_TEXTURE_WORDS    3u
#define PB3D_TRIS_WORDS       2u    /* and the vertices' words */
#define PB3D_DEPTHRANGE_WORDS 3u
#define PB3D_CLIP_WORDS       5u

/* DEPTH: the depth surface is the kernel's, one for the program's drawn
 * mode, so the command names none. Every frame that tests or writes depth
 * sends one before its first TRIS; without a clear, the depth of the
 * frame before is kept (GLQuake clears it every frame: its gl_ztrick,
 * which saved the clear on 1990s cards, is off). The clear writes 0xFFFF,
 * the farthest depth. */
#define PB3D_DEPTH_CLEAR  0x01u

/* STATE, word 1. What GL calls state is all here but the textures, the
 * depth range and the matrices (the Jam's): GLQuake changes it between
 * primitives, so it is one word, and the GL layer sends it only when it
 * changes. A frame starts with every bit 0, and STATE's word replaces all
 * of them. Bits not named are 0. */
#define PB3D_S_DEPTH_MASK   0x0003u  /* the depth test: */
#define PB3D_S_DEPTH_ALWAYS 0x0000u  /*   every pixel passes */
#define PB3D_S_DEPTH_LEQUAL 0x0001u  /*   passes if nearer or the same */
#define PB3D_S_DEPTH_GEQUAL 0x0002u  /*   passes if farther or the same */
#define PB3D_S_DEPTH_WRITE  0x0004u  /* a pixel that passes the depth test, and
                                      * isn't dropped by the alpha test, writes
                                      * its depth */
#define PB3D_S_BLEND_MASK   0x0018u  /* how a pixel meets the picture: */
#define PB3D_S_BLEND_OFF    0x0000u  /*   replaces it */
#define PB3D_S_BLEND_ALPHA  0x0008u  /*   src a + dst (1 - a): water, particles, the tint */
#define PB3D_S_BLEND_MULINV 0x0010u  /*   dst (1 - src): GLQuake's lightmap pass */
#define PB3D_S_BLEND_ADD    0x0018u  /*   src + dst: dynamic light flashes */
#define PB3D_S_ALPHA_TEST   0x0020u  /* drop a pixel whose alpha, after MODULATE,
                                      * is below 170 (GL's GREATER 0.666): a
                                      * see-through texel, or a low vertex alpha */
#define PB3D_S_MODULATE     0x0040u  /* unit 0's texel times the vertex colour;
                                      * 0 is the texel alone (GL_REPLACE) */
#define PB3D_S_TEX0         0x0080u  /* unit 0 is on; 0 is the colour alone */
#define PB3D_S_TEX1         0x0100u  /* unit 1 is on: the colour so far darkened by its
                                      * texel, a grey g, as colour (256 - g) / 256,
                                      * since GLQuake stores 255 - light
                                      * (gl_rsurf.c). Close to GL_BLEND's
                                      * (255 - g) / 255 but not it: the model
                                      * and PB match each other, not GL exactly */
#define PB3D_S_CULL_MASK    0x0600u  /* drop triangles by their winding as seen on the
                                      * screen, y down: clockwise is a positive
                                      * (x1 - x0)(y2 - y0) - (y1 - y0)(x2 - x0) */
#define PB3D_S_CULL_NONE    0x0000u
#define PB3D_S_CULL_CW      0x0200u  /*   clockwise ones */
#define PB3D_S_CULL_CCW     0x0400u  /*   counter-clockwise ones */
#define PB3D_S_SMOOTH       0x0800u  /* colour shaded across the triangle (Gouraud);
                                      * 0 takes each triangle's first vertex's, in
                                      * TRIS's order below (GL_FLAT) */
#define PB3D_S_ALL          0x0fffu

/* TEXTURE: arg is the unit, 0 (the wall, model or picture) or 1 (the
 * lightmap). Word 1 the texture's id, word 2 how it is read. Id 0 is no
 * texture. A frame starts with no texture on either unit. */
#define PB3D_T_CLAMP_S      0x0001u  /* clamp s at the edges; 0 repeats */
#define PB3D_T_CLAMP_T      0x0002u
#define PB3D_T_LINEAR       0x0004u  /* bilinear; 0 is the nearest texel */
#define PB3D_T_MIPMAP       0x0008u  /* read the mip the triangle's size picks */
#define PB3D_T_ALL          0x000fu

/* TRIS: arg's bits 0-1 the kind, bits 2-6 the vertex format; word 1 the
 * number of vertices, n; then n vertices, each PB3D_VERTEX_WORDS(format)
 * words. A list takes n / 3 triangles (vertices 3k, 3k + 1, 3k + 2), a
 * strip n - 2 (k, k + 1, k + 2 for even k, and k + 1, k, k + 2 for odd,
 * as GL's, so every triangle of a strip winds the same way), a fan n - 2
 * (0, k + 1, k + 2); fewer than three vertices draw nothing, and a list's
 * leftover one or two are ignored. */
#define PB3D_KIND_MASK      0x03u
#define PB3D_KIND_LIST      0x00u
#define PB3D_KIND_STRIP     0x01u
#define PB3D_KIND_FAN       0x02u

/* The vertex format: what each vertex carries, in this order, each field
 * present only if its bit is set but x, y.
 *   x, y    2 words  signed 12.4 screen pixels (plus or minus 2,048: the
 *                    guard band; PB cuts every span to the surface)
 *   q       2 words  1/w as near / w, unsigned 1.31: 1.0 (0x80000000) at
 *                    the near plane, smaller farther. Perspective and
 *                    depth come from it. Without it a triangle is flat on
 *                    the screen (the HUD, the console) and its depth d is
 *                    0 before DEPTHRANGE, so it stores near
 *   s, t    2 words  unit 0's texel position, signed 12.4 texels: the Jam
 *                    scales by the texture's size and takes whole sizes
 *                    off a primitive's coordinates, and PB repeats
 *   s2, t2  2 words  unit 1's, signed 10.6 texels (a lightmap, 128 wide)
 *   colour  1 word   PB3D_F_GREY: grey in the low byte (models: Quake
 *                    lights them in grey), the high byte 0
 *           2 words  PB3D_F_RGBA: r | g << 8, then b | a << 8
 *   A vertex without colour is white, opaque.
 * A world vertex (q, s and t, s2 and t2) is 16 bytes, a model's (q, s and
 * t, grey) 14, a HUD picture's (s and t) 8. */
#define PB3D_F_Q            0x04u
#define PB3D_F_ST           0x08u
#define PB3D_F_ST2          0x10u
#define PB3D_F_COLOUR_MASK  0x60u
#define PB3D_F_NOCOLOUR     0x00u
#define PB3D_F_GREY         0x20u
#define PB3D_F_RGBA         0x40u
/* 0x60 is not a format: PB refuses it. */
#define PB3D_FORMAT_MASK    0x7cu
#define PB3D_ARG_ALL        0x7fu

#define PB3D_VERTEX_WORDS(f) \
	(2u + (((f) & PB3D_F_Q) ? 2u : 0u) + (((f) & PB3D_F_ST) ? 2u : 0u) + \
	 (((f) & PB3D_F_ST2) ? 2u : 0u) + \
	 (((f) & PB3D_F_COLOUR_MASK) == PB3D_F_GREY ? 1u : \
	  ((f) & PB3D_F_COLOUR_MASK) == PB3D_F_RGBA ? 2u : 0u))

/* DEPTHRANGE: word 1 near, word 2 far, in the stored depth's units, 0 to
 * 0xFFFF. PB turns q into a 16-bit depth d that keeps q's order and
 * stores near + (d (far - near + 1) >> 16). A frame starts at 0, 0xFFFF.
 * GLQuake's view weapon is drawn at 0 to 0.3 of the range, so it is
 * always in front of the world. */
#define PB3D_DEPTH_NEAR     0x0000u
#define PB3D_DEPTH_FAR      0xffffu

/* CLIP: words 1 to 4 x0, y0, x1, y1, in whole pixels (not 12.4), y down,
 * x0 <= x1 <= the surface's width and y0 <= y1 <= its height: PB draws
 * only the pixels with x0 <= x < x1 and y0 <= y < y1, so none when
 * x0 == x1 or y0 == y1 (a viewport wholly off the surface, held to it).
 * A frame starts with the whole surface. The Jam still cuts at the near
 * plane and the guard band: CLIP only narrows which pixels are written.
 * GL's viewport is a 3D view smaller than the screen, GLQuake's above
 * its status bar, which GLQuake draws only when it changes: real GL clips
 * to the viewport, and so must PB, as a Voodoo's clip registers did. */

/* The words the command at w takes, out of left words: 0 if it isn't one
 * of these, its argument has a bit no command has, or it runs past left. */
uint32_t pb3d_cmd_words(const uint16_t *w, uint32_t left);

/* --- the encoder: commands into a buffer --- */

struct pb3d_buf {
	uint16_t *w;          /* the words */
	uint32_t n;           /* how many are written */
	uint32_t cap;         /* how many fit */
	uint32_t dropped;     /* commands that didn't fit, and so weren't written */
	uint32_t tris_at;     /* where the open TRIS's count is, if one is open;
	                       * nothing reads it yet: the next PR's fix for a
	                       * TRIS opened inside another will (#15) */
	uint32_t tris_left;   /* vertices the open TRIS still takes */
	uint8_t tris_format;
};

/* A vertex as the GL layer has it, after the divide: x, y in pixels from
 * the surface's top left, q as near / w, s, t and s2, t2 in texels, the
 * colour as bytes. Fields the format leaves out aren't read. */
struct pb3d_vertex {
	float x, y, q;
	float s, t, s2, t2;
	uint8_t r, g, b, a;   /* PB3D_F_GREY takes r */
};

void pb3d_init(struct pb3d_buf *b, uint16_t *words, uint32_t cap);
void pb3d_clear(struct pb3d_buf *b);   /* empty again, for the next frame */

/* Each writes one command, whole, or nothing and counts it in dropped. */
void pb3d_depth(struct pb3d_buf *b, int clear);
void pb3d_state(struct pb3d_buf *b, uint32_t bits);
void pb3d_texture(struct pb3d_buf *b, uint32_t unit, uint32_t id, uint32_t how);
void pb3d_depthrange(struct pb3d_buf *b, uint32_t near, uint32_t far);
void pb3d_clip(struct pb3d_buf *b, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
/* TRIS of n vertices (3 or more, and their words must fit), then exactly
 * n pb3d_vertex calls. Returns 0, or -1 having written nothing (the
 * vertex calls after it then write nothing either). */
int pb3d_tris(struct pb3d_buf *b, uint32_t kind, uint32_t format, uint32_t n);
void pb3d_vertex(struct pb3d_buf *b, const struct pb3d_vertex *v);
/* The same, from the fields themselves: the GL layer's way, with no
 * struct between it and the words (M4 PR 3; the floats go in s0-s6, by
 * the hard-float calling convention). colour is r | g << 8 | b << 16 |
 * a << 24. */
void pb3d_vertex_of(struct pb3d_buf *b, float x, float y, float q, float s, float t,
                    float s2, float t2, uint32_t colour);

/* The conversions pb3d_vertex makes, for the checks: to the nearest
 * step, halves away from zero, and held at the field's ends (NaN is 0). */
int16_t pb3d_fix_12_4(float v);
int16_t pb3d_fix_10_6(float v);
uint32_t pb3d_fix_q(float q);   /* 1.31, 0 to 0xFFFFFFFF */

/* --- the dump: what the host writes for the C model ---
 *
 * QUINCE_PBDUMP=FILE makes the PC build write every texture call and
 * every frame it sends, in order, so the model can draw them. All of it
 * little-endian. The file starts with PB3D_DUMP_MAGIC and
 * PB3D_DUMP_VERSION, a 32-bit word each; then records, each a header of
 * a 16-bit type, a 16-bit 0 and a 32-bit length in bytes, then that many
 * bytes and, if the length is odd, one byte of 0 to keep words aligned. */
#define PB3D_DUMP_MAGIC   0x33425051u   /* "QPB3" */
#define PB3D_DUMP_VERSION 2u   /* 2: CLIP */

enum {
	/* 768 bytes: the palette 8-bit textures are read through, r, g, b
	 * for each of 256 indices. Sent before the first texture. Index 255
	 * is see-through, whatever its colour, but only in a texture made
	 * with PB3D_TEXF_ALPHA: GLQuake's world and skins use it as a
	 * colour. */
	PB3D_REC_PALETTE  = 1,
	/* Make (or make again) a texture and fill it: a 16-bit id (1-65535),
	 * an 8-bit PB3D_TEX_* format, 8 bits of PB3D_TEXF_* flags, a 16-bit
	 * width and height (powers of two, 1-1,024), then the texels, row by
	 * row from the top. */
	PB3D_REC_TEXIMAGE = 2,
	/* A rectangle of a texture already made: id, format (the texture's),
	 * a byte of 0, x, y, width, height (16 bits each, inside it), then
	 * the texels. GLQuake's lightmaps, changed by dynamic lights. */
	PB3D_REC_TEXSUB   = 3,
	/* One frame's drawing commands, the words above: what the program
	 * hands the kernel between its START and SHOW. */
	PB3D_REC_FRAME    = 4,
	/* A 16-bit id: the texture is gone (never sent by GLQuake today). */
	PB3D_REC_TEXFREE  = 5
};

#define PB3D_REC_HEADER   8u   /* bytes */
#define PB3D_TEXIMAGE_HEADER 8u
#define PB3D_TEXSUB_HEADER   12u

enum {
	PB3D_TEX_INDEX8   = 0,  /* a byte a texel, through the palette */
	PB3D_TEX_RGBA8888 = 1,  /* r, g, b, a: the few that aren't palette colours;
	                         * PB stores alpha under 128 as see-through */
	PB3D_TEX_GREY8    = 2   /* a byte a texel, a grey level: lightmaps */
};

#define PB3D_TEXF_MIPMAP  0x01u   /* PB builds the mips (2x2 averages) */
#define PB3D_TEXF_ALPHA   0x02u   /* it has see-through texels: index 255, or
                                   * RGBA alpha under 128 */

#define PB3D_TEX_MAX      1024u   /* a side */

#endif
