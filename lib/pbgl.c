/* lib/pbgl.c: QUINCE's GL layer to the pixelboard, under GLQuake's
 * renderer (quake/gl_*.c).
 *
 * Our own code (M2, docs/ROADMAP.md; docs/PB-ASKS.md step 1; it grew out
 * of M1's null GL, lib/nullgl.c). GLQuake talks OpenGL 1.1: a state
 * machine of small calls, matrices, and vertices one call at a time. PB
 * takes far less: screen-space triangles, one word of state, two texture
 * units (lib/pb3d.h). This file is the translation, done on the Jam as a
 * 3dfx Voodoo's host did it under a MiniGL:
 *
 * - The matrices are kept as OpenGL 1.1 defines them (section 2.10,
 *   "Coordinate Transformations": column-major 4x4, each call multiplying
 *   the current matrix on the right), and every vertex goes through both
 *   to clip space as it arrives.
 * - A primitive is kept until glEnd, then sent as one TRIS: a fan
 *   (GL_POLYGON, GL_TRIANGLE_FAN, each GL_QUADS quad), a strip or a list.
 * - A primitive wholly off one edge of the screen, or behind the eye, is
 *   dropped. One that crosses the near plane, or runs past the guard band
 *   PB's 12.4 positions can hold, is cut there, a triangle at a time
 *   (Sutherland and Hodgman's polygon clipping, 1974, in clip space before
 *   the divide, where it is linear: Blinn and Newell, "Clipping using
 *   homogeneous coordinates", SIGGRAPH 1978). The rest is sent whole: PB
 *   cuts each span to the viewport itself (lib/pb3d.h's CLIP).
 * - GL's state calls only change copies here. At each glEnd the state the
 *   primitive needs (lib/pb3d.h's STATE word, the two textures, the depth
 *   range) is compared with what PB was last sent, and only a change is
 *   sent.
 * - Textures go to PB as palette indices, made once (pbgl_upload8, from
 *   quake/gl_draw.c's GL_Upload8): PB expands them and builds the mips.
 *
 * Nothing here calls the kernel, so the host test and QUINCE /SELFTEST
 * check the same code. Where the commands go is the caller's sink.
 */
#include "GL/gl.h"
#include "pbgl.h"
#include "pb3d.h"
#include <math.h>
#include <string.h>

struct pbgl_counts pbgl_total;

/* --- matrices (M1's null GL) --- */

#define STACK_DEPTH 8
typedef struct { float m[16]; } mat4;

static mat4 modelview[STACK_DEPTH], projection[STACK_DEPTH];
static int mv_top, pr_top;
static GLenum matrix_mode = GL_MODELVIEW;
static mat4 mvp;            /* projection x modelview, remade when stale */
static int mvp_stale = 1;
static int perspective;     /* the projection divides by w (glFrustum's) */
static float near_z = 1.0f; /* its near plane, for q = near / w */

static mat4 *current(void)
{
	return matrix_mode == GL_PROJECTION ? &projection[pr_top] : &modelview[mv_top];
}

static void identity(mat4 *a)
{
	memset(a, 0, sizeof *a);
	a->m[0] = a->m[5] = a->m[10] = a->m[15] = 1.0f;
}

/* out = a x b, column-major: element (row r, column c) is m[c * 4 + r]. */
static void multiply(mat4 *out, const mat4 *a, const mat4 *b)
{
	mat4 t;
	int r, c, k;
	for (c = 0; c < 4; c++)
		for (r = 0; r < 4; r++) {
			float s = 0.0f;
			for (k = 0; k < 4; k++)
				s += a->m[k * 4 + r] * b->m[c * 4 + k];
			t.m[c * 4 + r] = s;
		}
	*out = t;
}

static void apply(const mat4 *b)
{
	mat4 *a = current();
	multiply(a, a, b);
	mvp_stale = 1;
}

/* The projection is a perspective one if its bottom row is glFrustum's
 * (0, 0, -1, 0). Then w in clip space is the eye's distance, and the near
 * plane is n = m14 / (m10 - 1): glFrustum's m10 is -(f + n) / (f - n) and
 * m14 is -2 f n / (f - n). */
static void remake_mvp(void)
{
	const float *p = projection[pr_top].m;
	multiply(&mvp, &projection[pr_top], &modelview[mv_top]);
	perspective = p[3] == 0.0f && p[7] == 0.0f && p[11] == -1.0f && p[15] == 0.0f;
	near_z = perspective && p[10] != 1.0f ? p[14] / (p[10] - 1.0f) : 1.0f;
	if (!(near_z > 0.0f))
		near_z = 1.0f;
	mvp_stale = 0;
}

static void count_call(void)
{
	pbgl_total.calls++;
}

void glMatrixMode(GLenum mode) { count_call(); matrix_mode = mode; }
void glLoadIdentity(void) { count_call(); identity(current()); mvp_stale = 1; }

void glLoadMatrixf(const GLfloat *m)
{
	count_call();
	memcpy(current()->m, m, sizeof(float) * 16);
	mvp_stale = 1;
}

void glPushMatrix(void)
{
	count_call();
	/* A push past STACK_DEPTH is dropped and counted, so a GL user
	 * that deep sees it in ignored, not as wrong transforms alone (#7).
	 * GLQuake never goes near it. */
	if (matrix_mode == GL_PROJECTION) {
		if (pr_top + 1 < STACK_DEPTH) { projection[pr_top + 1] = projection[pr_top]; pr_top++; }
		else pbgl_total.ignored++;
	} else if (mv_top + 1 < STACK_DEPTH) {
		modelview[mv_top + 1] = modelview[mv_top];
		mv_top++;
	} else {
		pbgl_total.ignored++;
	}
}

void glPopMatrix(void)
{
	count_call();
	if (matrix_mode == GL_PROJECTION) { if (pr_top > 0) pr_top--; }
	else if (mv_top > 0) mv_top--;
	mvp_stale = 1;
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	mat4 t;
	count_call();
	identity(&t);
	t.m[12] = x; t.m[13] = y; t.m[14] = z;
	apply(&t);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	mat4 t;
	count_call();
	identity(&t);
	t.m[0] = x; t.m[5] = y; t.m[10] = z;
	apply(&t);
}

/* The rotation of the specification's glRotate: angle in degrees about the
 * axis (x, y, z), normalised first. */
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	mat4 t;
	float len = sqrtf(x * x + y * y + z * z), a, c, s, d;
	count_call();
	if (len == 0.0f)
		return;
	x /= len; y /= len; z /= len;
	a = angle * (3.14159265f / 180.0f);
	c = cosf(a); s = sinf(a); d = 1.0f - c;
	identity(&t);
	t.m[0] = x * x * d + c;     t.m[4] = x * y * d - z * s; t.m[8]  = x * z * d + y * s;
	t.m[1] = y * x * d + z * s; t.m[5] = y * y * d + c;     t.m[9]  = y * z * d - x * s;
	t.m[2] = x * z * d - y * s; t.m[6] = y * z * d + x * s; t.m[10] = z * z * d + c;
	apply(&t);
}

/* GL takes these in double; the board has only a float FPU, so a double
 * divide is CANNER's soft float, about 2,000 instructions. Each is made
 * float once and the rest done in float. */
void glFrustum(GLdouble dl, GLdouble dr, GLdouble db, GLdouble dt, GLdouble dn, GLdouble df)
{
	float l = (float)dl, r = (float)dr, b = (float)db, t = (float)dt, n = (float)dn, f = (float)df;
	mat4 p;
	count_call();
	memset(&p, 0, sizeof p);
	p.m[0] = 2.0f * n / (r - l);
	p.m[5] = 2.0f * n / (t - b);
	p.m[8] = (r + l) / (r - l);
	p.m[9] = (t + b) / (t - b);
	p.m[10] = -(f + n) / (f - n);
	p.m[11] = -1.0f;
	p.m[14] = -2.0f * f * n / (f - n);
	apply(&p);
}

void glOrtho(GLdouble dl, GLdouble dr, GLdouble db, GLdouble dt, GLdouble dn, GLdouble df)
{
	float l = (float)dl, r = (float)dr, b = (float)db, t = (float)dt, n = (float)dn, f = (float)df;
	mat4 p;
	count_call();
	identity(&p);
	p.m[0] = 2.0f / (r - l);
	p.m[5] = 2.0f / (t - b);
	p.m[10] = -2.0f / (f - n);
	p.m[12] = -(r + l) / (r - l);
	p.m[13] = -(t + b) / (t - b);
	p.m[14] = -(f + n) / (f - n);
	apply(&p);
}

void glGetFloatv(GLenum what, GLfloat *out)
{
	count_call();
	if (what == GL_MODELVIEW_MATRIX)
		memcpy(out, modelview[mv_top].m, sizeof(float) * 16);
}

/* --- the screen, and the planes primitives are tested against --- */

/* Clip space to the surface's pixels, y down: x = ax ndc + bx and
 * y = by - ay ndc, where ndc is x / w (GL's viewport has y up from the
 * window's bottom). */
static int surf_w = 640, surf_h = 480;
static float ax = 320, bx = 320, ay = 240, by = 240;
/* The viewport as PB's CLIP takes it: pixels, y down, x1 and y1 past the
 * last, held to the surface (lib/pb3d.h). */
static uint32_t clip[4] = {0, 0, 640, 480};

static int32_t held(int32_t v, int32_t hi)
{
	return v < 0 ? 0 : v > hi ? hi : v;
}

/* Each plane a x + b y + c z + d w, 0 or more inside. The first five
 * only drop primitives wholly outside one: the near plane (also cut at)
 * and the screen's four edges. The last four are the guard band: as far
 * as PB's 12.4 positions reach, with 8 pixels to spare. There is no far
 * plane, where GL has one: what lies past GL's far distance is drawn
 * (Jacob, LOG 2026-11-12, #24). GLQuake's far plane, 4,096 units, holds
 * every shareware map, and a sixth test would cost outcode_of, the
 * layer's hottest function, time on every vertex. */
#define PLANES      9
#define P_NEAR      0x001u
#define P_REJECT    0x01fu   /* near and the screen's edges */
#define P_GUARD     0x1e0u
#define P_CUT       (P_NEAR | P_GUARD)
#define GUARD       2040.0f
static float plane[PLANES][4];
static int guard_wider = 1;   /* the guard band is outside the viewport on every side */

static void set_plane(int i, float a, float b, float c, float d)
{
	plane[i][0] = a; plane[i][1] = b; plane[i][2] = c; plane[i][3] = d;
}

/* On the screen x = ax x/w + bx is under GUARD while x <= (GUARD - bx)/ax
 * w, and so on for the other three. */
static void remake_planes(void)
{
	set_plane(0, 0, 0, 1, 1);      /* z + w: the near plane, z = -w */
	set_plane(1, 1, 0, 0, 1);      /* the screen's left, x = -w */
	set_plane(2, -1, 0, 0, 1);     /* right */
	set_plane(3, 0, 1, 0, 1);      /* bottom (GL's y up) */
	set_plane(4, 0, -1, 0, 1);     /* top */
	set_plane(5, 1, 0, 0, (GUARD + bx) / ax);
	set_plane(6, -1, 0, 0, (GUARD - bx) / ax);
	set_plane(7, 0, -1, 0, (GUARD + by) / ay);
	set_plane(8, 0, 1, 0, (GUARD - by) / ay);
	/* With each guard factor at least 1, a vertex inside the screen's
	 * four planes (x + w >= 0, say) is inside the guard's too: a rounded
	 * g w is at least w (rounding keeps order). A viewport wider than the
	 * guard band (none GLQuake makes) gets every test. */
	guard_wider = plane[5][3] >= 1.0f && plane[6][3] >= 1.0f && plane[7][3] >= 1.0f &&
	              plane[8][3] >= 1.0f;
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
	count_call();
	/* GL's y counts up from the bottom, PB's down from the top. */
	clip[0] = (uint32_t)held(x, surf_w);
	clip[1] = (uint32_t)held(surf_h - (y + h), surf_h);
	clip[2] = (uint32_t)held(x + w, surf_w);
	clip[3] = (uint32_t)held(surf_h - y, surf_h);
	ax = (float)w * 0.5f;
	bx = (float)x + ax;
	ay = (float)h * 0.5f;
	by = (float)surf_h - (float)y - ay;
	if (ax <= 0.0f) ax = 1.0f;
	if (ay <= 0.0f) ay = 1.0f;
	remake_planes();
}

/* --- GL's state, as GLQuake sets it --- */

#define UNITS 2
static int unit;                           /* glSelectTextureSGIS's */
static uint32_t bound[UNITS];              /* glBindTexture's */
static int tex_on[UNITS];                  /* glEnable(GL_TEXTURE_2D) */
static GLenum tex_env[UNITS] = { GL_MODULATE, GL_MODULATE };
static int depth_test, blend, alpha_test, cull_face;
static GLenum depth_func = GL_LEQUAL, blend_src = GL_ONE, blend_dst = GL_ZERO;
static GLenum cull_which = GL_BACK, shade = GL_SMOOTH;
static int depth_mask = 1;
static float range_near = 0.0f, range_far = 1.0f;

/* The vertex being made: glTexCoord's, glColor's (kept with the
 * primitives below). */
static float cur_s, cur_t, cur_s2, cur_t2;
static uint8_t cur_r = 255, cur_g = 255, cur_b = 255, cur_a = 255;
static uint32_t nprim;
static int prim_open;

/* --- textures: 2 bytes each in mem.textures, by GL's name --- */

#define TI_KNOWN  0x01u
#define TI_MIPMAP 0x02u
#define TI_ALPHA  0x04u
static struct pbgl_mem mem;
static const struct pbgl_sink *sink;

/* A texture's size as log2 of each side (byte 0: width low, height high),
 * and TI_* flags (byte 1). 0 for a name past the table or never made. */
static uint8_t *tex_info(uint32_t id)
{
	return id < mem.max_textures ? mem.textures + id * PBGL_TEXTURE_BYTES : 0;
}

static int log2_of(uint32_t v)
{
	int n = 0;
	while (v > 1u) {
		v >>= 1;
		n++;
	}
	return n;
}

/* The palette, and a table from colour back to index, for the RGBA
 * uploads GLQuake makes itself (the sky, a player's skin), whose texels
 * are palette colours: as indices they are a quarter of the bytes. Open
 * addressing on the 24-bit colour (Knuth vol. 3, 6.4, linear probing). */
#define BACK 512
#define BACK_USED 0x1000000u
static uint8_t palette[768];
static uint32_t back[BACK];     /* the colour, and BACK_USED */
static uint8_t back_index[BACK];

static uint32_t back_hash(uint32_t c)
{
	return (c * 2654435761u) >> 23;   /* Knuth's multiplicative hash, 9 bits */
}

void pbgl_palette(const uint8_t *rgb)
{
	int i;
	memcpy(palette, rgb, sizeof palette);
	memset(back, 0, sizeof back);
	for (i = 0; i < 256; i++) {
		uint32_t c = (uint32_t)rgb[i * 3] | (uint32_t)rgb[i * 3 + 1] << 8 |
		             (uint32_t)rgb[i * 3 + 2] << 16;
		uint32_t h = back_hash(c);
		while (back[h] & BACK_USED) {
			if ((back[h] & 0xffffffu) == c)
				break;   /* the palette has a colour twice: the first index wins */
			h = (h + 1) & (BACK - 1);
		}
		if (!(back[h] & BACK_USED)) {
			back[h] = BACK_USED | c;
			back_index[h] = (uint8_t)i;
		}
	}
	if (sink && sink->palette)
		sink->palette(palette);
}

/* The index of colour c, or -1 if it isn't one of the palette's. Index
 * 255 is see-through in a texture with alpha, so it never stands for a
 * colour here. */
static int back_find(uint32_t c)
{
	uint32_t h = back_hash(c);
	while (back[h] & BACK_USED) {
		if ((back[h] & 0xffffffu) == c)
			return back_index[h] == 255u ? -1 : (int)back_index[h];
		h = (h + 1) & (BACK - 1);
	}
	return -1;
}

static void flush_subs(void);
static struct pb3d_buf buf;
static void spill(struct pb3d_buf *b);

static void tex_made(uint32_t id, uint32_t w, uint32_t h, uint32_t flags, uint32_t bytes,
                     uint32_t format, const void *texels)
{
	uint8_t *ti = tex_info(id);
	/* The window's words so far, and an update held for a texture,
	 * go before it is made again: the stream keeps GL's order. */
	spill(&buf);
	if (!ti) {
		/* A name past the table couldn't be drawn (bound as 0), so it
		 * isn't sent either (#24): GLQuake makes about 1,200 of 4,096. */
		pbgl_total.ignored++;
		return;
	}
	ti[0] = (uint8_t)(log2_of(w) | log2_of(h) << 4);
	ti[1] = (uint8_t)(TI_KNOWN | ((flags & PB3D_TEXF_MIPMAP) ? TI_MIPMAP : 0) |
	                  ((flags & PB3D_TEXF_ALPHA) ? TI_ALPHA : 0));
	pbgl_total.tex_uploads++;
	pbgl_total.tex_bytes += bytes;
	if (format == PB3D_TEX_RGBA8888)
		pbgl_total.tex_rgba++;
	if (sink && sink->teximage)
		sink->teximage(id, format, flags, w, h, texels);
}

static int pow2(uint32_t v)
{
	return v != 0 && (v & (v - 1u)) == 0;
}

void pbgl_upload8(const uint8_t *data, int w, int h, int mipmap, int alpha, int max_side)
{
	uint32_t sw, sh, i, j, n = (uint32_t)w * (uint32_t)h, flags = 0;
	const uint8_t *px = data;
	if (w <= 0 || h <= 0)
		return;
	if (max_side <= 0 || max_side > (int)PB3D_TEX_MAX)
		max_side = (int)PB3D_TEX_MAX;
	/* GL_Upload32's sizes: the power of two at or over each side, held
	 * at gl_max_size (gl_picmip is 0 and isn't taken). */
	for (sw = 1; sw < (uint32_t)w; sw <<= 1)
		;
	for (sh = 1; sh < (uint32_t)h; sh <<= 1)
		;
	if (sw > (uint32_t)max_side) sw = (uint32_t)max_side;
	if (sh > (uint32_t)max_side) sh = (uint32_t)max_side;
	/* GL_Upload8's test: a texture with alpha that has no index 255 has
	 * no see-through texel, so it goes as a solid one. */
	if (alpha) {
		alpha = 0;
		for (i = 0; i < n && !alpha; i++)
			alpha = data[i] == 255;
	}
	if (sw != (uint32_t)w || sh != (uint32_t)h) {
		/* Point sampled, a row at a time, as id's GL_ResampleTexture
		 * steps through RGBA (gl_draw.c): 16.16 steps across, starting
		 * half a step in, and the nearest row down. */
		uint32_t step = (uint32_t)w * 0x10000u / sw;
		if (sw * sh > mem.nscratch) {
			pbgl_total.dropped++;
			return;
		}
		for (i = 0; i < sh; i++) {
			const uint8_t *row = data + (uint32_t)w * (i * (uint32_t)h / sh);
			uint8_t *out = mem.scratch + i * sw;
			uint32_t frac = step >> 1;
			for (j = 0; j < sw; j++) {
				out[j] = row[frac >> 16];
				frac += step;
			}
		}
		px = mem.scratch;
	}
	if (mipmap)
		flags |= PB3D_TEXF_MIPMAP;
	if (alpha)
		flags |= PB3D_TEXF_ALPHA;
	tex_made(bound[unit], sw, sh, flags, sw * sh, PB3D_TEX_INDEX8, px);
}

/* glTexImage2D: the textures GLQuake makes itself, not through
 * GL_Upload8. Lightmaps are a grey byte a texel (GL_LUMINANCE). RGBA ones
 * whose texels are all palette colours (or see-through) go as indices;
 * the others, the particle dot, as RGBA. Mips past level 0 are dropped:
 * PB makes its own. */
void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h,
                  GLint border, GLenum fmt, GLenum type, const GLvoid *px)
{
	uint32_t n = (uint32_t)w * (uint32_t)h, i, flags = 0;
	const uint8_t *in = px;
	count_call();
	(void)target; (void)internal; (void)border; (void)type;
	if (level != 0 || w <= 0 || h <= 0 || !pow2((uint32_t)w) || !pow2((uint32_t)h) ||
	    w > (int)PB3D_TEX_MAX || h > (int)PB3D_TEX_MAX) {
		pbgl_total.ignored++;
		return;
	}
	if (fmt == GL_COLOR_INDEX) {
		tex_made(bound[unit], (uint32_t)w, (uint32_t)h, 0, n, PB3D_TEX_INDEX8, px);
		return;
	}
	if (fmt != GL_RGBA) {
		/* a byte a texel: GL_LUMINANCE, GL_INTENSITY, GL_ALPHA */
		tex_made(bound[unit], (uint32_t)w, (uint32_t)h, 0, n, PB3D_TEX_GREY8, px);
		return;
	}
	if (n <= mem.nscratch) {
		for (i = 0; i < n; i++) {
			const uint8_t *t = in + i * 4u;
			int k;
			if (t[3] < 128) {
				k = 255;
				flags = PB3D_TEXF_ALPHA;
			} else if (t[3] != 255 ||
			           (k = back_find((uint32_t)t[0] | (uint32_t)t[1] << 8 | (uint32_t)t[2] << 16)) < 0)
				break;
			mem.scratch[i] = (uint8_t)k;
		}
		if (i == n) {
			tex_made(bound[unit], (uint32_t)w, (uint32_t)h, flags, n, PB3D_TEX_INDEX8,
			         mem.scratch);
			return;
		}
	}
	flags = 0;
	for (i = 0; i < n; i++)
		if (in[i * 4u + 3] < 128)
			flags = PB3D_TEXF_ALPHA;
	tex_made(bound[unit], (uint32_t)w, (uint32_t)h, flags, n * 4u, PB3D_TEX_RGBA8888, px);
}

/* Lightmap updates, held to the next window handed on, or the frame's
 * end (M4 PR 4). GLQuake's one-pass path
 * (gl_rsurf.c's R_DrawSequentialPoly) updates a lightmap's changed rows
 * each time it draws a surface on it after a dynamic light touched it, so
 * one block can go up many times a frame. Held, the rows each block
 * changed go once, as one band, before the window of words that may use
 * them (lib/pbgl.h's sink). The texels are read
 * then, from GLQuake's own copy (lightmaps[], gl_rsurf.c), which by then
 * holds every change. Only full-width bands are held (all of GLQuake's
 * are); anything else goes at once. */
#define PENDING 32
struct pending {
	uint32_t id, format, w, y0, y1;
	const uint8_t *base;   /* row 0 of the texture's texels */
};
static struct pending pending[PENDING];
static uint32_t npending;

static void send_sub(uint32_t id, uint32_t format, uint32_t x, uint32_t y, uint32_t w,
                     uint32_t h, const void *px)
{
	pbgl_total.sub_uploads++;
	pbgl_total.sub_bytes += w * h * (format == PB3D_TEX_RGBA8888 ? 4u : 1u);
	if (sink && sink->texsub)
		sink->texsub(id, format, x, y, w, h, px);
}

static void flush_subs(void)
{
	uint32_t i;
	for (i = 0; i < npending; i++) {
		const struct pending *p = &pending[i];
		uint32_t tb = p->format == PB3D_TEX_RGBA8888 ? 4u : 1u;
		send_sub(p->id, p->format, 0, p->y0, p->w, p->y1 - p->y0, p->base + p->y0 * p->w * tb);
	}
	npending = 0;
}

void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,
                     GLenum fmt, GLenum type, const GLvoid *px)
{
	uint32_t format = fmt == GL_RGBA ? PB3D_TEX_RGBA8888 : PB3D_TEX_GREY8, id = bound[unit], i;
	uint32_t tb = format == PB3D_TEX_RGBA8888 ? 4u : 1u;
	const uint8_t *ti = tex_info(id);
	count_call();
	(void)target; (void)type;
	if (level != 0 || w <= 0 || h <= 0 || x < 0 || y < 0) {
		pbgl_total.ignored++;
		return;
	}
	pbgl_total.sub_calls++;
	if (x != 0 || !ti || !(ti[1] & TI_KNOWN) || (uint32_t)w != 1u << (ti[0] & 15u)) {
		spill(&buf);   /* after the words before it, as tex_made */
		send_sub(id, format, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, px);
		return;
	}
	for (i = 0; i < npending; i++)
		if (pending[i].id == id && pending[i].format == format)
			break;
	if (i == npending) {
		if (npending == PENDING)
			flush_subs();
		i = npending++;
		pending[i].id = id;
		pending[i].format = format;
		pending[i].w = (uint32_t)w;
		pending[i].y0 = (uint32_t)y;
		pending[i].y1 = (uint32_t)(y + h);
		pending[i].base = (const uint8_t *)px - (uint32_t)y * (uint32_t)w * tb;
		return;
	}
	if ((uint32_t)y < pending[i].y0)
		pending[i].y0 = (uint32_t)y;
	if ((uint32_t)(y + h) > pending[i].y1)
		pending[i].y1 = (uint32_t)(y + h);
}

void glBindTexture(GLenum target, GLuint tex)
{
	count_call();
	(void)target;
	bound[unit] = tex;
	pbgl_total.binds++;
}

void glTexEnvf(GLenum t, GLenum w, GLfloat v)
{
	count_call();
	(void)t; (void)w;
	/* GL_TEXTURE_ENV_MODE is the only one GLQuake sets: its value is the
	 * enum, passed as a float (exact: QUINCE's enums are small). */
	tex_env[unit] = (GLenum)v;
}

/* Filters and wraps are PB's to choose (PB-ASKS, decision 4): walls point
 * sampled, the lightmap bilinear. */
void glTexParameterf(GLenum t, GLenum w, GLfloat v) { count_call(); (void)t; (void)w; (void)v; }

void pbgl_select_texture(unsigned int which)
{
	count_call();
	unit = which == PBGL_TEXTURE1 ? 1 : 0;
}

/* --- state --- */

void glEnable(GLenum cap)
{
	count_call();
	if (cap == GL_TEXTURE_2D) tex_on[unit] = 1;
	else if (cap == GL_DEPTH_TEST) depth_test = 1;
	else if (cap == GL_BLEND) blend = 1;
	else if (cap == GL_ALPHA_TEST) alpha_test = 1;
	else if (cap == GL_CULL_FACE) cull_face = 1;
}

void glDisable(GLenum cap)
{
	count_call();
	if (cap == GL_TEXTURE_2D) tex_on[unit] = 0;
	else if (cap == GL_DEPTH_TEST) depth_test = 0;
	else if (cap == GL_BLEND) blend = 0;
	else if (cap == GL_ALPHA_TEST) alpha_test = 0;
	else if (cap == GL_CULL_FACE) cull_face = 0;
}

void glBlendFunc(GLenum s, GLenum d) { count_call(); blend_src = s; blend_dst = d; }
void glDepthFunc(GLenum f) { count_call(); depth_func = f; }
void glDepthMask(GLboolean on) { count_call(); depth_mask = on != 0; }
void glCullFace(GLenum face) { count_call(); cull_which = face; }
void glShadeModel(GLenum mode) { count_call(); shade = mode; }

void glDepthRange(GLclampd n, GLclampd f)
{
	count_call();
	range_near = (float)n;
	range_far = (float)f;
}

/* The STATE word GL's state makes now (lib/pb3d.h). */
static uint32_t state_bits(void)
{
	uint32_t s = 0;
	if (depth_test) {
		s |= depth_func == GL_GEQUAL ? PB3D_S_DEPTH_GEQUAL : PB3D_S_DEPTH_LEQUAL;
		if (depth_mask)
			s |= PB3D_S_DEPTH_WRITE;   /* GL writes depth only while it tests it */
	}
	if (blend) {
		if (blend_src == GL_ZERO && blend_dst == GL_ONE_MINUS_SRC_COLOR)
			s |= PB3D_S_BLEND_MULINV;
		else if (blend_src == GL_ONE && blend_dst == GL_ONE)
			s |= PB3D_S_BLEND_ADD;
		else
			s |= PB3D_S_BLEND_ALPHA;   /* GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA */
	}
	if (alpha_test)
		s |= PB3D_S_ALPHA_TEST;
	if (tex_on[0]) {
		s |= PB3D_S_TEX0;
		if (tex_env[0] != GL_REPLACE)
			s |= PB3D_S_MODULATE;
	}
	/* GLQuake's second unit is always the lightmap, in GL_BLEND. */
	if (tex_on[1])
		s |= PB3D_S_TEX1;
	/* GL's front face winds counter-clockwise as seen (glFrontFace's
	 * default), and so does PB's CULL_CCW. */
	if (cull_face)
		s |= cull_which == GL_FRONT ? PB3D_S_CULL_CCW : PB3D_S_CULL_CW;
	if (shade == GL_SMOOTH)
		s |= PB3D_S_SMOOTH;
	return s;
}

/* --- the frame: what has been sent, so only changes are --- */

static uint32_t sent_state, sent_tex[UNITS], sent_how[UNITS], sent_near, sent_far;
static uint32_t sent_clip[4];
static int sent_depth;
static uint32_t checksum;

static void frame_start(void)
{
	pb3d_clear(&buf);
	sent_state = 0;
	sent_tex[0] = sent_tex[1] = 0;
	sent_how[0] = sent_how[1] = 0;
	sent_near = PB3D_DEPTH_NEAR;
	sent_far = PB3D_DEPTH_FAR;
	sent_clip[0] = sent_clip[1] = 0;
	sent_clip[2] = (uint32_t)surf_w;
	sent_clip[3] = (uint32_t)surf_h;
	sent_depth = 0;
}

static uint32_t range16(float v)
{
	if (!(v > 0.0f))
		return 0;
	if (v >= 1.0f)
		return 0xffffu;
	return (uint32_t)(v * 65535.0f + 0.5f);
}

/* Whatever the next TRIS needs that PB hasn't been sent this frame. */
static void sync_state(uint32_t s)
{
	uint32_t u;
	if ((s & (PB3D_S_DEPTH_MASK | PB3D_S_DEPTH_WRITE)) && !sent_depth) {
		pb3d_depth(&buf, 0);
		sent_depth = 1;
		pbgl_total.other_cmds++;
	}
	if (s != sent_state) {
		pb3d_state(&buf, s);
		sent_state = s;
		pbgl_total.state_cmds++;
	}
	for (u = 0; u < UNITS; u++) {
		uint32_t on = u == 0 ? (s & PB3D_S_TEX0) : (s & PB3D_S_TEX1), how;
		const uint8_t *ti = tex_info(bound[u]);
		uint32_t id = ti && (ti[1] & TI_KNOWN) ? bound[u] : 0;
		if (!on)
			continue;
		how = u == 1 ? PB3D_T_LINEAR | PB3D_T_CLAMP_S | PB3D_T_CLAMP_T
		             : (ti && (ti[1] & TI_MIPMAP) ? PB3D_T_MIPMAP : 0u);
		if (id != sent_tex[u] || how != sent_how[u]) {
			pb3d_texture(&buf, u, id, how);
			sent_tex[u] = id;
			sent_how[u] = how;
			pbgl_total.texture_cmds++;
		}
	}
	/* PB draws to the whole surface until told otherwise; GL to the
	 * viewport. GLQuake's 3D view stops above the status bar, which it
	 * draws only when it changes, so the view mustn't spill on it. */
	if (memcmp(clip, sent_clip, sizeof clip) != 0) {
		pb3d_clip(&buf, clip[0], clip[1], clip[2], clip[3]);
		memcpy(sent_clip, clip, sizeof clip);
		pbgl_total.other_cmds++;
	}
	if (s & PB3D_S_DEPTH_MASK) {
		uint32_t n = range16(range_near), f = range16(range_far);
		if (n != sent_near || f != sent_far) {
			pb3d_depthrange(&buf, n, f);
			sent_near = n;
			sent_far = f;
			pbgl_total.other_cmds++;
		}
	}
}

void glClear(GLbitfield mask)
{
	count_call();
	/* The colour needn't be cleared: GLQuake draws every pixel of its
	 * view (gl_clear is 0). Depth is cleared every frame. */
	if (mask & GL_DEPTH_BUFFER_BIT) {
		pb3d_depth(&buf, 1);
		sent_depth = 1;
		pbgl_total.other_cmds++;
	}
}

/* FNV-1a (Fowler, Noll and Vo's hash, from its description), two words
 * a step: half the multiplies of a word a step, which M2's board run had
 * at 5% of the program (LOG 2026-11-12). Only the tests read it. The
 * frame comes a window at a time, so a window's odd last word waits in
 * sum_word to pair with the next one's first: the sum is the whole
 * frame's, as it was before the words streamed (M4 PR 4). */
static uint32_t sum_word;
static int sum_held;

static void words_sum(const uint16_t *w, uint32_t n)
{
	uint32_t h = checksum, i = 0;
	if (sum_held && n > 0) {
		h = (h ^ (sum_word | (uint32_t)w[0] << 16)) * 16777619u;
		sum_held = 0;
		i = 1;
	}
	for (; i + 1 < n; i += 2)
		h = (h ^ ((uint32_t)w[i] | (uint32_t)w[i + 1] << 16)) * 16777619u;
	if (i < n) {
		sum_word = w[i];
		sum_held = 1;
	}
	checksum = h;
}

/* The words the frame has handed on so far, for the counts. */
static uint32_t frame_words;

/* The window's words go on: summed, then to the sink, after any texture
 * update still held, since PB draws a window as it arrives and the
 * words may use the update (M4 PR 4). Then the window is empty again;
 * what has been sent to PB this frame stays sent. */
static void spill(struct pb3d_buf *b)
{
	words_sum(b->w, b->n);
	flush_subs();
	if (sink && sink->words && b->n)
		sink->words(b->w, b->n);
	frame_words += b->n;
	b->n = 0;
}

void pbgl_end_frame(void)
{
	uint32_t n;
	spill(&buf);
	if (sum_held) {
		checksum = (checksum ^ sum_word) * 16777619u;
		sum_held = 0;
	}
	n = frame_words;
	frame_words = 0;
	pbgl_total.frames++;
	pbgl_total.words += n;
	if (n > pbgl_total.max_frame_words)
		pbgl_total.max_frame_words = n;
	pbgl_total.dropped += buf.dropped;
	buf.dropped = 0;
	if (sink && sink->frame)
		sink->frame();
	frame_start();
}

uint32_t pbgl_checksum(void)
{
	return checksum;
}

void pbgl_reset(void)
{
	memset(&pbgl_total, 0, sizeof pbgl_total);
	checksum = 2166136261u;
}

void pbgl_init(const struct pbgl_mem *m, int width, int height, const struct pbgl_sink *s)
{
	int i;
	mem = *m;
	sink = s;
	memset(mem.textures, 0, (size_t)mem.max_textures * PBGL_TEXTURE_BYTES);
	pb3d_init(&buf, mem.words, mem.nwords);
	buf.full = spill;
	frame_words = 0;
	sum_held = 0;
	surf_w = width;
	surf_h = height;
	for (i = 0; i < STACK_DEPTH; i++) {
		identity(&modelview[i]);
		identity(&projection[i]);
	}
	mv_top = pr_top = 0;
	matrix_mode = GL_MODELVIEW;
	mvp_stale = 1;
	unit = 0;
	bound[0] = bound[1] = 0;
	tex_on[0] = tex_on[1] = 0;
	tex_env[0] = tex_env[1] = GL_MODULATE;
	depth_test = blend = alpha_test = cull_face = 0;
	depth_func = GL_LEQUAL;
	blend_src = GL_ONE;
	blend_dst = GL_ZERO;
	cull_which = GL_BACK;
	shade = GL_SMOOTH;
	depth_mask = 1;
	range_near = 0.0f;
	range_far = 1.0f;
	cur_s = cur_t = cur_s2 = cur_t2 = 0.0f;
	cur_r = cur_g = cur_b = cur_a = 255;
	nprim = 0;
	prim_open = 0;
	npending = 0;
	glViewport(0, 0, width, height);
	pbgl_reset();
	frame_start();
}

/* --- vertices --- */

/* A vertex as it arrives: in clip space, with what goes with it. */
struct vert {
	float x, y, z, w;
	float s, t, s2, t2;
	uint8_t r, g, b, a;
};

/* The longest primitive kept whole. A longer one is sent in pieces: a
 * fan keeps its first and last vertex, a strip its last two (at an even
 * count, so the next piece winds as the strip does), a list starts again.
 * Most of GLQuake's primitives are well under it; what is split is its
 * particle lists, about 1,370 primitives over demo1's 969 frames, and an
 * alias model's longest strips and fans now and then. */
#define PRIM_MAX 256   /* PBGL_WORDS_MIN counts on it */
static struct vert prim[PRIM_MAX];
static uint16_t outcode[PRIM_MAX];
static GLenum prim_mode;

/* Which planes a vertex is outside of. Every plane is one coordinate
 * against w (remake_planes), so each test is a compare, or a multiply and
 * a compare for the guard band's, not a dot product of four: the same
 * sums as plane[i] . v for finite coordinates, since the terms left out
 * are exact zeros (an infinite y or z made the old 0 * inf a NaN in every
 * other plane's sum, so those planes all said inside; a compare never
 * reads a coordinate its plane doesn't use). M2's board run had the dot
 * products at 17% of the program (LOG 2026-11-12). A vertex on the
 * screen is inside the guard band too, which is wider (guard_wider), so
 * the guard's four tests are made only off it. */
static uint16_t outcode_of(const struct vert *v)
{
	float x = v->x, y = v->y, w = v->w;
	uint16_t c = 0;
	if (v->z + w < 0.0f) c |= 0x001u;
	if (x + w < 0.0f) c |= 0x002u;
	if (w - x < 0.0f) c |= 0x004u;
	if (y + w < 0.0f) c |= 0x008u;
	if (w - y < 0.0f) c |= 0x010u;
	if ((c & 0x01eu) || !guard_wider) {
		if (x + plane[5][3] * w < 0.0f) c |= 0x020u;
		if (plane[6][3] * w - x < 0.0f) c |= 0x040u;
		if (plane[7][3] * w - y < 0.0f) c |= 0x080u;
		if (y + plane[8][3] * w < 0.0f) c |= 0x100u;
	}
	return c;
}

/* Whether a vertex is outside plane p alone: the clipper's test, as
 * outcode_of makes it for that plane. */
static int outside(const struct vert *v, int p)
{
	float x = v->x, y = v->y, w = v->w;
	switch (p) {
	case 0: return v->z + w < 0.0f;
	case 1: return x + w < 0.0f;
	case 2: return w - x < 0.0f;
	case 3: return y + w < 0.0f;
	case 4: return w - y < 0.0f;
	case 5: return x + plane[5][3] * w < 0.0f;
	case 6: return plane[6][3] * w - x < 0.0f;
	case 7: return plane[7][3] * w - y < 0.0f;
	default: return y + plane[8][3] * w < 0.0f;
	}
}

static uint32_t kind_of(GLenum mode)
{
	if (mode == GL_TRIANGLES)
		return PB3D_KIND_LIST;
	if (mode == GL_TRIANGLE_STRIP)
		return PB3D_KIND_STRIP;
	return PB3D_KIND_FAN;   /* GL_POLYGON, GL_TRIANGLE_FAN, a GL_QUADS quad */
}

static uint32_t tris_of(uint32_t kind, uint32_t n)
{
	if (kind == PB3D_KIND_LIST)
		return n / 3;
	return n >= 3 ? n - 2 : 0;
}

/* Triangle k of a primitive of this kind: its three vertices, in the
 * order lib/pb3d.h's TRIS takes them, so each winds as GL's would. */
static void tri_of(uint32_t kind, uint32_t k, uint32_t *a, uint32_t *b, uint32_t *c)
{
	if (kind == PB3D_KIND_LIST) {
		*a = 3 * k; *b = 3 * k + 1; *c = 3 * k + 2;
	} else if (kind == PB3D_KIND_STRIP) {
		if (k & 1u) { *a = k + 1; *b = k; }
		else { *a = k; *b = k + 1; }
		*c = k + 2;
	} else {
		*a = 0; *b = k + 1; *c = k + 2;
	}
}

/* What a primitive's vertices carry (lib/pb3d.h's format) and where its
 * texel positions start: the whole texture sizes taken off. */
struct sendfmt {
	uint32_t format;
	float s0, t0;        /* taken off s, t before scaling, in textures */
	float sw, th;        /* unit 0's size in texels */
	float s2w, t2h;      /* unit 1's */
};

static float tex_side(uint32_t id, int high)
{
	const uint8_t *ti = tex_info(id);
	if (!ti || !(ti[1] & TI_KNOWN))
		return 1.0f;
	return (float)(1u << (high ? ti[0] >> 4 : ti[0] & 15u));
}

static void choose_format(uint32_t s, struct sendfmt *f)
{
	uint32_t i, grey = 1, white = 1;
	f->format = perspective ? PB3D_F_Q : 0;
	f->s0 = f->t0 = 0.0f;
	f->sw = f->th = f->s2w = f->t2h = 1.0f;
	if (s & PB3D_S_TEX0) {
		float mins = prim[0].s, mint = prim[0].t;
		f->format |= PB3D_F_ST;
		f->sw = tex_side(bound[0], 0);
		f->th = tex_side(bound[0], 1);
		for (i = 1; i < nprim; i++) {
			if (prim[i].s < mins) mins = prim[i].s;
			if (prim[i].t < mint) mint = prim[i].t;
		}
		/* PB repeats a texture, so whole sizes can come off: the
		 * coordinates then start in the first copy and stay small. */
		f->s0 = floorf(mins);
		f->t0 = floorf(mint);
	}
	if (s & PB3D_S_TEX1) {
		f->format |= PB3D_F_ST2;
		f->s2w = tex_side(bound[1], 0);
		f->t2h = tex_side(bound[1], 1);
	}
	/* The colour matters only where it is drawn: no texture, or one
	 * modulated by it. A white one is the default and isn't sent. */
	if ((s & PB3D_S_TEX0) && !(s & PB3D_S_MODULATE))
		return;
	for (i = 0; i < nprim; i++) {
		const struct vert *v = &prim[i];
		if (v->a != 255 || v->r != v->g || v->g != v->b)
			grey = white = 0;
		else if (v->r != 255)
			white = 0;
	}
	if (!white)
		f->format |= grey ? PB3D_F_GREY : PB3D_F_RGBA;
}

/* A clip-space vertex onto the surface, straight into PB's words: only
 * the fields the format sends are worked out (M4 PR 3; this and
 * pb3d_vertex were 16% of demo1 in M4's board run, LOG 2026-11-12). */
static void send_vertex(const struct vert *v, const struct sendfmt *f)
{
	uint32_t fm = f->format;
	float iw = v->w != 0.0f ? 1.0f / v->w : 0.0f;
	float q = 0.0f, s = 0.0f, t = 0.0f, s2 = 0.0f, t2 = 0.0f;
	if (fm & PB3D_F_Q)
		q = near_z * iw;
	if (fm & PB3D_F_ST) {
		s = (v->s - f->s0) * f->sw;
		t = (v->t - f->t0) * f->th;
	}
	if (fm & PB3D_F_ST2) {
		s2 = v->s2 * f->s2w;
		t2 = v->t2 * f->t2h;
	}
	pb3d_vertex_of(&buf, ax * (v->x * iw) + bx, by - ay * (v->y * iw), q, s, t, s2, t2,
	               (uint32_t)v->r | (uint32_t)v->g << 8 | (uint32_t)v->b << 16 |
	                   (uint32_t)v->a << 24);
}

static int open_tris(uint32_t kind, const struct sendfmt *f, uint32_t n)
{
	if (pb3d_tris(&buf, kind, f->format, n) != 0)
		return -1;
	pbgl_total.tris_cmds++;
	pbgl_total.sent_verts += n;
	pbgl_total.sent_tris += tris_of(kind, n);
	pbgl_total.vertex_words += n * PB3D_VERTEX_WORDS(f->format);
	return 0;
}

/* Where the segment a to b crosses plane p, as a new vertex: every
 * field taken linearly, which in clip space is right for all of them. */
static void cross(const struct vert *a, const struct vert *b, int p, struct vert *o)
{
	float da = plane[p][0] * a->x + plane[p][1] * a->y + plane[p][2] * a->z + plane[p][3] * a->w;
	float db = plane[p][0] * b->x + plane[p][1] * b->y + plane[p][2] * b->z + plane[p][3] * b->w;
	float k = da / (da - db);
	o->x = a->x + (b->x - a->x) * k;
	o->y = a->y + (b->y - a->y) * k;
	o->z = a->z + (b->z - a->z) * k;
	o->w = a->w + (b->w - a->w) * k;
	o->s = a->s + (b->s - a->s) * k;
	o->t = a->t + (b->t - a->t) * k;
	o->s2 = a->s2 + (b->s2 - a->s2) * k;
	o->t2 = a->t2 + (b->t2 - a->t2) * k;
	o->r = (uint8_t)((float)a->r + ((float)b->r - (float)a->r) * k + 0.5f);
	o->g = (uint8_t)((float)a->g + ((float)b->g - (float)a->g) * k + 0.5f);
	o->b = (uint8_t)((float)a->b + ((float)b->b - (float)a->b) * k + 0.5f);
	o->a = (uint8_t)((float)a->a + ((float)b->a - (float)a->a) * k + 0.5f);
}

/* One triangle cut by each plane in cuts, in turn (Sutherland and
 * Hodgman): a triangle and five planes make at most eight vertices. */
#define CUT_MAX (3 + 5)
static int cut_triangle(const struct vert *a, const struct vert *b, const struct vert *c,
                        uint16_t cuts, const struct sendfmt *f)
{
	struct vert poly[2][CUT_MAX];
	uint32_t n = 3, i, src = 0;
	int p;
	poly[0][0] = *a; poly[0][1] = *b; poly[0][2] = *c;
	for (p = 0; p < PLANES; p++) {
		uint32_t m = 0;
		if (!((P_CUT & cuts) >> p & 1u))
			continue;
		for (i = 0; i < n; i++) {
			const struct vert *u = &poly[src][i], *v = &poly[src][(i + 1) % n];
			int uin = !outside(u, p), vin = !outside(v, p);
			if (uin && m < CUT_MAX)
				poly[src ^ 1][m++] = *u;
			if (uin != vin && m < CUT_MAX)
				cross(u, v, p, &poly[src ^ 1][m++]);
		}
		n = m;
		src ^= 1;
		if (n < 3)
			return 0;
	}
	/* Flat: the cut's new vertices took colours between the old ones,
	 * but GL's flat colour is the triangle's last vertex's, c (flush has
	 * given a polygon's or quad's every vertex theirs), so the fan's
	 * every vertex takes c's (#24). */
	if (shade == GL_FLAT)
		for (i = 0; i < n; i++) {
			poly[src][i].r = c->r; poly[src][i].g = c->g;
			poly[src][i].b = c->b; poly[src][i].a = c->a;
		}
	if (open_tris(PB3D_KIND_FAN, f, n) != 0)
		return 0;
	pbgl_total.clip_tris++;
	for (i = 0; i < n; i++)
		send_vertex(&poly[src][i], f);
	return 1;
}

/* The primitive kept since glBegin (or since its last piece), sent. */
static void flush(void)
{
	uint32_t kind = kind_of(prim_mode), i, s, all = 0xffffu, any = 0, ntri;
	struct sendfmt f;
	if (prim_mode != GL_TRIANGLES && prim_mode != GL_TRIANGLE_STRIP &&
	    prim_mode != GL_TRIANGLE_FAN && prim_mode != GL_POLYGON && prim_mode != GL_QUADS)
		return;   /* GL_LINES: GLQuake's only lines are debugging ones */
	ntri = tris_of(kind, nprim);
	if (ntri == 0)
		return;
	for (i = 0; i < nprim; i++) {
		all &= outcode[i];
		any |= outcode[i];
	}
	if (all & P_REJECT) {
		pbgl_total.culled++;
		return;
	}
	s = state_bits();
	sync_state(s);
	/* Flat shading's colour (GL 1.1, section 2.13.7) is each triangle's
	 * last vertex's for lists, strips and fans, which PB takes itself
	 * (lib/pb3d.h's SMOOTH). A polygon takes its first vertex's and a
	 * quad its fourth's, so their vertices all get that one, where the
	 * colour is drawn at all (#24). GLQuake's flat polygons are one
	 * colour, or textured without MODULATE: this costs it nothing. */
	if (shade == GL_FLAT && (prim_mode == GL_POLYGON || prim_mode == GL_QUADS) &&
	    (!(s & PB3D_S_TEX0) || (s & PB3D_S_MODULATE))) {
		const struct vert *p = &prim[prim_mode == GL_POLYGON ? 0 : nprim - 1];
		uint8_t r = p->r, g = p->g, b = p->b, a = p->a;
		for (i = 0; i < nprim; i++) {
			prim[i].r = r; prim[i].g = g; prim[i].b = b; prim[i].a = a;
		}
	}
	choose_format(s, &f);
	if (!(any & P_CUT)) {
		if (open_tris(kind, &f, kind == PB3D_KIND_LIST ? ntri * 3 : nprim) != 0)
			return;
		for (i = 0; i < (kind == PB3D_KIND_LIST ? ntri * 3 : nprim); i++)
			send_vertex(&prim[i], &f);
		return;
	}
	/* Cut: the triangles wholly inside as one list, then each one that
	 * crosses a plane as a fan of its own. The primitive counts as
	 * clipped only if a fan went: a crossing triangle can still be wholly
	 * off the screen, and drops (#24). */
	{
		uint32_t k, a, b, c, inside = 0, fans = 0;
		for (k = 0; k < ntri; k++) {
			tri_of(kind, k, &a, &b, &c);
			if (!((outcode[a] | outcode[b] | outcode[c]) & P_CUT))
				inside++;
		}
		/* As lists of what fits the words window: a cut strip or fan
		 * of 256 has 254 triangles inside, 762 vertices, more than
		 * PBGL_WORDS_MIN holds (M4 PR 4). Whole, it would be dropped. */
		{
			uint32_t per = (buf.cap - PB3D_TRIS_WORDS) / (3u * PB3D_VERTEX_WORDS(f.format));
			k = 0;
			while (inside) {
				uint32_t m = inside < per ? inside : per;
				if (open_tris(PB3D_KIND_LIST, &f, m * 3) != 0)
					break;
				inside -= m;
				for (; m; k++) {
					tri_of(kind, k, &a, &b, &c);
					if ((outcode[a] | outcode[b] | outcode[c]) & P_CUT)
						continue;
					send_vertex(&prim[a], &f);
					send_vertex(&prim[b], &f);
					send_vertex(&prim[c], &f);
					m--;
				}
			}
		}
		for (k = 0; k < ntri; k++) {
			uint16_t o;
			tri_of(kind, k, &a, &b, &c);
			o = (uint16_t)(outcode[a] | outcode[b] | outcode[c]);
			if (!(o & P_CUT) || (outcode[a] & outcode[b] & outcode[c] & P_REJECT))
				continue;
			fans += (uint32_t)cut_triangle(&prim[a], &prim[b], &prim[c], o, &f);
		}
		if (fans)
			pbgl_total.clipped++;
	}
}

void glBegin(GLenum mode)
{
	count_call();
	prim_mode = mode;
	nprim = 0;
	prim_open = 1;
	if (mvp_stale)
		remake_mvp();
}

/* Triangles GL makes from a primitive of n vertices, as M1 counted them. */
static uint32_t gl_triangles(GLenum mode, uint32_t n)
{
	if (mode == GL_TRIANGLES)
		return n / 3;
	if (mode == GL_QUADS)
		return n / 4 * 2;
	if (mode == GL_TRIANGLE_FAN || mode == GL_TRIANGLE_STRIP || mode == GL_POLYGON)
		return n >= 3 ? n - 2 : 0;
	return 0;
}

static uint32_t prim_verts_total;

void glEnd(void)
{
	count_call();
	if (prim_mode != GL_QUADS)
		flush();
	pbgl_total.prims++;
	pbgl_total.tris += gl_triangles(prim_mode, prim_verts_total);
	prim_verts_total = 0;
	nprim = 0;
	prim_open = 0;
}

static void vertex(float x, float y, float z)
{
	const float *m;
	struct vert *v;
	count_call();
	if (!prim_open)
		return;
	if (mvp_stale)
		remake_mvp();
	m = mvp.m;
	v = &prim[nprim];
	v->x = m[0] * x + m[4] * y + m[8] * z + m[12];
	v->y = m[1] * x + m[5] * y + m[9] * z + m[13];
	v->z = m[2] * x + m[6] * y + m[10] * z + m[14];
	v->w = m[3] * x + m[7] * y + m[11] * z + m[15];
	v->s = cur_s; v->t = cur_t; v->s2 = cur_s2; v->t2 = cur_t2;
	v->r = cur_r; v->g = cur_g; v->b = cur_b; v->a = cur_a;
	outcode[nprim] = outcode_of(v);
	nprim++;
	prim_verts_total++;
	pbgl_total.verts++;
	if (prim_mode == GL_QUADS) {
		if (nprim == 4) {   /* each quad is a fan of its own */
			flush();
			nprim = 0;
		}
		return;
	}
	if (prim_mode == GL_TRIANGLES ? nprim == PRIM_MAX - PRIM_MAX % 3 : nprim == PRIM_MAX) {
		flush();
		pbgl_total.split++;
		if (prim_mode == GL_TRIANGLE_STRIP) {
			prim[0] = prim[nprim - 2]; outcode[0] = outcode[nprim - 2];
			prim[1] = prim[nprim - 1]; outcode[1] = outcode[nprim - 1];
			nprim = 2;
		} else if (prim_mode == GL_TRIANGLE_FAN || prim_mode == GL_POLYGON) {
			prim[1] = prim[nprim - 1]; outcode[1] = outcode[nprim - 1];
			nprim = 2;
		} else
			nprim = 0;
	}
}

void glVertex2f(GLfloat x, GLfloat y) { vertex(x, y, 0.0f); }
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { vertex(x, y, z); }
void glVertex3fv(const GLfloat *v) { vertex(v[0], v[1], v[2]); }

void glTexCoord2f(GLfloat s, GLfloat t) { count_call(); cur_s = s; cur_t = t; }
void glTexCoord2fv(const GLfloat *v) { count_call(); cur_s = v[0]; cur_t = v[1]; }

void pbgl_mtexcoord(unsigned int which, float s, float t)
{
	count_call();
	if (which == PBGL_TEXTURE1) {
		cur_s2 = s;
		cur_t2 = t;
	} else {
		cur_s = s;
		cur_t = t;
	}
}

static uint8_t byte_of(float c)
{
	if (!(c > 0.0f))
		return 0;
	if (c >= 1.0f)
		return 255;
	return (uint8_t)(c * 255.0f + 0.5f);
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b)
{
	count_call();
	cur_r = byte_of(r); cur_g = byte_of(g); cur_b = byte_of(b); cur_a = 255;
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	count_call();
	cur_r = byte_of(r); cur_g = byte_of(g); cur_b = byte_of(b); cur_a = byte_of(a);
}

void glColor4fv(const GLfloat *v)
{
	count_call();
	cur_r = byte_of(v[0]); cur_g = byte_of(v[1]); cur_b = byte_of(v[2]); cur_a = byte_of(v[3]);
}

void glColor3ubv(const unsigned char *v)
{
	count_call();
	cur_r = v[0]; cur_g = v[1]; cur_b = v[2]; cur_a = 255;
}

void glColor4ubv(const unsigned char *v)
{
	count_call();
	cur_r = v[0]; cur_g = v[1]; cur_b = v[2]; cur_a = v[3];
}

/* --- calls that change nothing PB is sent --- */

void glHint(GLenum what, GLenum mode) { count_call(); (void)what; (void)mode; }
void glDrawBuffer(GLenum b) { count_call(); (void)b; }
void glReadBuffer(GLenum b) { count_call(); (void)b; }
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { count_call(); (void)r; (void)g; (void)b; (void)a; }
void glFinish(void) { count_call(); }
void glFogi(GLenum w, GLint v) { count_call(); (void)w; (void)v; }
void glFogf(GLenum w, GLfloat v) { count_call(); (void)w; (void)v; }
void glFogfv(GLenum w, const GLfloat *v) { count_call(); (void)w; (void)v; }
/* id's GL_Init sets GL_GREATER 0.666; PB's alpha test drops see-through
 * texels, which are the ones under it. */
void glAlphaFunc(GLenum f, GLclampf ref) { count_call(); (void)f; (void)ref; }
void glPolygonMode(GLenum face, GLenum mode) { count_call(); (void)face; (void)mode; }

/* Screenshots read the frame back; it is on PB, so they get black. */
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, GLvoid *px)
{
	count_call();
	(void)x; (void)y; (void)type;
	memset(px, 0, (size_t)w * (size_t)h * (fmt == GL_RGBA ? 4u : fmt == GL_RGB ? 3u : 1u));
}
