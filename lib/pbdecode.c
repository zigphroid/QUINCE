/* lib/pbdecode.c: PB's 3D commands into the triangle engine's C model.
 *
 * Our own code (docs/PB-ASKS.md step 2, PR 2). The words are lib/pb3d.h's,
 * read only through what it defines; what each one does to the model is
 * docs/PB3D.md's. A vertex's fields are taken in the order the format
 * names them, a 32-bit value as two words, the low one first.
 */
#include "pbdecode.h"
#include "pb3d.h"

/* The most vertices one TRIS can have: its count is one word. */
#define MAX_VERTS 65535u

static int log2_side(uint32_t v)
{
	int n = 0;
	if (v == 0 || (v & (v - 1)) != 0 || v > PB3D_TEX_MAX)
		return -1;
	while ((1u << n) != v)
		n++;
	return n;
}

int pbd_teximage(struct pbm *m, uint32_t id, uint32_t format, uint32_t flags,
                 uint32_t w, uint32_t h, const uint8_t *texels)
{
	int lw = log2_side(w), lh = log2_side(h), e, fmt;
	if (lw < 0 || lh < 0)
		return PBM_E_SIZE;
	if (format == PB3D_TEX_INDEX8)
		fmt = PBM_TEX_INDEX8;
	else if (format == PB3D_TEX_RGBA8888)
		fmt = PBM_TEX_RGBA8888;
	else if (format == PB3D_TEX_GREY8)
		fmt = PBM_TEX_GREY8;
	else
		return PBM_E_FORMAT;
	e = pbm_texture(m, id, fmt, lw, lh, (flags & PB3D_TEXF_MIPMAP) != 0,
	                (flags & PB3D_TEXF_ALPHA) != 0);
	if (e != PBM_OK)
		return e;
	return pbm_texture_load(m, id, 0, 0, (int)w, (int)h, texels);
}

int pbd_texsub(struct pbm *m, uint32_t id, uint32_t x, uint32_t y, uint32_t w,
               uint32_t h, const uint8_t *texels)
{
	return pbm_texture_load(m, id, (int)x, (int)y, (int)w, (int)h, texels);
}

void pbd_texfree(struct pbm *m, uint32_t id)
{
	if (id != 0 && id < PBM_TEX_IDS)
		m->tex[id].made = 0;
}

static void state(struct pbm *m, uint32_t bits)
{
	struct pbm_state s;
	uint32_t depth = bits & PB3D_S_DEPTH_MASK, blend = bits & PB3D_S_BLEND_MASK,
	         cull = bits & PB3D_S_CULL_MASK;
	s.depth_func = depth == PB3D_S_DEPTH_LEQUAL ? PBM_DEPTH_LEQUAL :
	               depth == PB3D_S_DEPTH_GEQUAL ? PBM_DEPTH_GEQUAL : PBM_DEPTH_ALWAYS;
	s.depth_write = (bits & PB3D_S_DEPTH_WRITE) != 0;
	s.blend = blend == PB3D_S_BLEND_ALPHA ? PBM_BLEND_ALPHA :
	          blend == PB3D_S_BLEND_MULINV ? PBM_BLEND_MULINV :
	          blend == PB3D_S_BLEND_ADD ? PBM_BLEND_ADD : PBM_BLEND_OFF;
	s.alpha_test = (bits & PB3D_S_ALPHA_TEST) != 0;
	s.modulate = (bits & PB3D_S_MODULATE) != 0;
	s.tex0 = (bits & PB3D_S_TEX0) != 0;
	s.tex1 = (bits & PB3D_S_TEX1) != 0;
	s.cull = cull == PB3D_S_CULL_CW ? PBM_CULL_CW :
	         cull == PB3D_S_CULL_CCW ? PBM_CULL_CCW : PBM_CULL_NONE;
	s.smooth = (bits & PB3D_S_SMOOTH) != 0;
	pbm_set_state(m, &s);
}

static void texture(struct pbm *m, uint32_t unit, uint32_t id, uint32_t how)
{
	struct pbm_bind b;
	b.id = id;
	b.clamp_s = (how & PB3D_T_CLAMP_S) != 0;
	b.clamp_t = (how & PB3D_T_CLAMP_T) != 0;
	b.linear = (how & PB3D_T_LINEAR) != 0;
	b.mipmap = (how & PB3D_T_MIPMAP) != 0;
	pbm_bind_texture(m, (int)unit, &b);
}

/* A vertex out of its words, by the format; the fields it leaves out
 * take q 1.0 (depth 0, so near after DEPTHRANGE) and white, opaque. */
static const uint16_t *vertex(const uint16_t *w, uint32_t f, struct pbm_vertex *v)
{
	v->x = (int16_t)w[0];
	v->y = (int16_t)w[1];
	w += 2;
	v->q = 0x80000000u;
	v->s = v->t = v->s2 = v->t2 = 0;
	v->r = v->g = v->b = v->a = 255;
	if (f & PB3D_F_Q) {
		v->q = (uint32_t)w[0] | (uint32_t)w[1] << 16;
		w += 2;
	}
	if (f & PB3D_F_ST) {
		v->s = (int16_t)w[0];
		v->t = (int16_t)w[1];
		w += 2;
	}
	if (f & PB3D_F_ST2) {
		v->s2 = (int16_t)w[0];
		v->t2 = (int16_t)w[1];
		w += 2;
	}
	if ((f & PB3D_F_COLOUR_MASK) == PB3D_F_GREY) {
		v->r = v->g = v->b = (uint8_t)(w[0] & 0xffu);
		w += 1;
	} else if ((f & PB3D_F_COLOUR_MASK) == PB3D_F_RGBA) {
		v->r = (uint8_t)(w[0] & 0xffu);
		v->g = (uint8_t)(w[0] >> 8);
		v->b = (uint8_t)(w[1] & 0xffu);
		v->a = (uint8_t)(w[1] >> 8);
		w += 2;
	}
	return w;
}

/* A TRIS's vertices, held while its triangles are drawn: a strip or fan
 * shares them. The most one command can carry. */
static struct pbm_vertex verts[MAX_VERTS];

static void tris(struct pbm *m, const uint16_t *w)
{
	uint32_t arg = PB3D_ARG(w[0]), n = w[1], f = arg & PB3D_FORMAT_MASK, i;
	uint32_t kind = arg & PB3D_KIND_MASK;
	const uint16_t *p = w + PB3D_TRIS_WORDS;
	for (i = 0; i < n; i++)
		p = vertex(p, f, &verts[i]);
	pbm_draw(m, kind == PB3D_KIND_STRIP ? PBM_STRIP : kind == PB3D_KIND_FAN ? PBM_FAN : PBM_LIST,
	         verts, n);
}

int pbd_frame(struct pbm *m, const uint16_t *w, uint32_t n)
{
	uint32_t at = 0;
	int cmds = 0;
	pbm_frame(m);
	while (at < n) {
		const uint16_t *c = w + at;
		uint32_t len = pb3d_cmd_words(c, n - at);
		if (len == 0)
			return -1;
		switch (PB3D_OP(c[0])) {
		case PB3D_DEPTH:
			if (PB3D_ARG(c[0]) & PB3D_DEPTH_CLEAR)
				pbm_clear_depth(m, 0xffff);
			break;
		case PB3D_STATE:
			state(m, c[1]);
			break;
		case PB3D_TEXTURE:
			texture(m, PB3D_ARG(c[0]), c[1], c[2]);
			break;
		case PB3D_DEPTHRANGE:
			pbm_depth_range(m, c[1], c[2]);
			break;
		case PB3D_CLIP:
			pbm_clip(m, c[1], c[2], c[3], c[4]);
			break;
		case PB3D_TRIS:
			tris(m, c);
			break;
		}
		at += len;
		cmds++;
	}
	return cmds;
}
