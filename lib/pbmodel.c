/* lib/pbmodel.c: the pixelboard's triangle engine, modelled in C.
 *
 * Our own code (docs/PB-ASKS.md, step 2). docs/PB3D.md gives these rules
 * in words, with the reasons; this file is the rules themselves, and where
 * the two differ this file is right and the doc is fixed. Every number is
 * an integer and every rounding is said, so gateware can be held to it
 * byte for byte. It is written to be read, a pixel at a time, not to be
 * fast: PB walks spans, and the order it visits pixels in doesn't change
 * what it writes.
 *
 * Shifts of negative numbers go through asr() (C leaves >> of a negative
 * number to the compiler), and products that can pass 32 bits are int64_t.
 */
#include "pbmodel.h"

/* x / 2^n, rounded down, for either sign. */
static int64_t asr(int64_t x, int n)
{
	if (x >= 0)
		return x >> n;
	return -((-x - 1) >> n) - 1;
}

/* The number of bits x needs: 0 for 0, 1 for 1, 2 for 2-3 and so on. */
static int bitlen(uint64_t x)
{
	int n = 0;
	while (x) {
		n++;
		x >>= 1;
	}
	return n;
}

static void zero(void *p, uint32_t bytes)
{
	uint8_t *b = p;
	while (bytes--)
		*b++ = 0;
}

uint16_t pbm_565(uint32_t r, uint32_t g, uint32_t b)
{
	return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

/* --- the parts PB needs a unit for: the reciprocal and depth --- */

/* floor(2^40 / s16). PB may find it by a table and a Newton step, as long
 * as a last step makes it exactly this. */
uint32_t pbm_recip(uint32_t s16)
{
	if (s16 == 0)
		return 0;
	return (uint32_t)(((uint64_t)1 << 40) / s16 > 0xffffffffu ? 0xffffffffu
	                                                           : ((uint64_t)1 << 40) / s16);
}

/* q (1.31, larger nearer) to a 16-bit depth d, 0 nearest: a float of q
 * with 4 bits of exponent and 12 of mantissa, as Voodoo's w-buffer, so
 * every distance gets the same share of precision (a 4,096th). */
uint32_t pbm_depth_of_q(uint32_t q)
{
	uint32_t n, lz, e, mant;
	if (q > 0x7fffffffu)
		q = 0x7fffffffu;    /* 1.0 and past it: the nearest */
	if (q == 0)
		return 0xffff;
	lz = (uint32_t)(32 - bitlen(q));   /* 1 or more */
	e = lz - 1;
	if (e > 15)
		return 0xffff;      /* past 65,536 times the near plane */
	mant = ((q << lz) >> 19) & 0xfffu;
	n = (15 - e) << 12 | mant;
	return 0xffff - n;
}

/* --- textures --- */

static uint32_t tex_w(const struct pbm_texture *t, int l)
{
	int s = t->lw - l;
	return s > 0 ? 1u << s : 1u;
}

static uint32_t tex_h(const struct pbm_texture *t, int l)
{
	int s = t->lh - l;
	return s > 0 ? 1u << s : 1u;
}

/* A texel as 8 bits a channel: bits copied down into the low ones, so 31
 * of 5 bits is 255 and 0 is 0. */
struct rgba {
	uint32_t r, g, b, a;
};

static struct rgba decode(int keep, uint16_t v)
{
	struct rgba c;
	uint32_t r5, g, b5;
	if (keep == PBM_KEEP_GREY) {
		c.r = c.g = c.b = v & 0xffu;
		c.a = 255;
		return c;
	}
	if (keep == PBM_KEEP_565) {
		r5 = v >> 11;
		g = (v >> 5) & 63u;
		b5 = v & 31u;
		c.r = r5 << 3 | r5 >> 2;
		c.g = g << 2 | g >> 4;
		c.b = b5 << 3 | b5 >> 2;
		c.a = 255;
		return c;
	}
	r5 = (v >> 10) & 31u;
	g = (v >> 5) & 31u;
	b5 = v & 31u;
	c.r = r5 << 3 | r5 >> 2;
	c.g = g << 3 | g >> 2;
	c.b = b5 << 3 | b5 >> 2;
	c.a = v & 0x8000u ? 255 : 0;
	return c;
}

static uint16_t encode(int keep, struct rgba c)
{
	if (keep == PBM_KEEP_GREY)
		return (uint16_t)c.r;
	if (keep == PBM_KEEP_565)
		return pbm_565(c.r, c.g, c.b);
	return (uint16_t)((c.a >= 128 ? 0x8000u : 0) | (c.r >> 3) << 10 |
	                  (c.g >> 3) << 5 | c.b >> 3);
}

void pbm_textures_reset(struct pbm *m)
{
	zero(m->tex, sizeof m->tex);
	m->pool_used = 0;
}

int pbm_texture(struct pbm *m, uint32_t id, int format, int lw, int lh,
                int mipmap, int alpha)
{
	struct pbm_texture *t;
	int keep, levels, l;
	uint32_t words = 0;

	if (id == 0 || id >= PBM_TEX_IDS)
		return PBM_E_ID;
	if (lw < 0 || lh < 0 || lw > PBM_TEX_LOG_MAX || lh > PBM_TEX_LOG_MAX)
		return PBM_E_SIZE;
	if (format == PBM_TEX_INDEX8)
		keep = alpha ? PBM_KEEP_1555 : PBM_KEEP_565;
	else if (format == PBM_TEX_RGBA8888)
		keep = PBM_KEEP_1555;
	else if (format == PBM_TEX_GREY8)
		keep = PBM_KEEP_GREY;
	else
		return PBM_E_FORMAT;
	levels = mipmap ? (lw > lh ? lw : lh) + 1 : 1;

	t = &m->tex[id];
	/* The same size made again keeps its place in the pool. */
	if (!(t->made && t->lw == lw && t->lh == lh && t->levels == levels)) {
		for (l = 0; l < levels; l++) {
			uint32_t w = lw - l > 0 ? 1u << (lw - l) : 1u;
			uint32_t h = lh - l > 0 ? 1u << (lh - l) : 1u;
			words += w * h;
		}
		if (words > m->pool_words - m->pool_used)
			return PBM_E_FULL;
		for (l = 0; l < levels; l++) {
			t->level[l] = m->pool + m->pool_used;
			m->pool_used += (lw - l > 0 ? 1u << (lw - l) : 1u) *
			                (lh - l > 0 ? 1u << (lh - l) : 1u);
		}
	}
	t->lw = (uint8_t)lw;
	t->lh = (uint8_t)lh;
	t->levels = (uint8_t)levels;
	t->keep = (uint8_t)keep;
	t->format = (uint8_t)format;
	t->alpha = (uint8_t)(alpha != 0 && format == PBM_TEX_INDEX8);
	t->made = 1;
	for (l = 0; l < levels; l++)
		zero(t->level[l], tex_w(t, l) * tex_h(t, l) * 2u);
	return PBM_OK;
}

/* Level l + 1 from level l: each texel the 2x2 block above it, each
 * channel's sum plus 2 over 4 (a side of 1 takes its one texel twice).
 * See-through only if two or more of the four are: GLQuake averages alpha
 * too, and its alpha test passes over 0.666, three opaque of four. */
static void make_mip(struct pbm_texture *t, int l)
{
	uint32_t w = tex_w(t, l), h = tex_h(t, l);
	uint32_t w2 = tex_w(t, l + 1), h2 = tex_h(t, l + 1);
	uint32_t x, y;
	for (y = 0; y < h2; y++)
		for (x = 0; x < w2; x++) {
			uint32_t x0 = 2 * x, y0 = 2 * y;
			uint32_t x1 = x0 + 1 < w ? x0 + 1 : x0, y1 = y0 + 1 < h ? y0 + 1 : y0;
			struct rgba c[4], o;
			uint32_t opaque = 0;
			int i;
			c[0] = decode(t->keep, t->level[l][y0 * w + x0]);
			c[1] = decode(t->keep, t->level[l][y0 * w + x1]);
			c[2] = decode(t->keep, t->level[l][y1 * w + x0]);
			c[3] = decode(t->keep, t->level[l][y1 * w + x1]);
			o.r = o.g = o.b = 2;
			for (i = 0; i < 4; i++) {
				o.r += c[i].r;
				o.g += c[i].g;
				o.b += c[i].b;
				opaque += c[i].a == 255;
			}
			o.r >>= 2;
			o.g >>= 2;
			o.b >>= 2;
			o.a = opaque >= 3 ? 255 : 0;
			t->level[l + 1][y * w2 + x] = encode(t->keep, o);
		}
}

int pbm_texture_load(struct pbm *m, uint32_t id, int x, int y, int w, int h,
                     const uint8_t *data)
{
	struct pbm_texture *t;
	int i, j, l, fmt;
	if (id == 0 || id >= PBM_TEX_IDS || !m->tex[id].made)
		return PBM_E_ID;
	t = &m->tex[id];
	if (x < 0 || y < 0 || w < 0 || h < 0 || (uint32_t)(x + w) > tex_w(t, 0) ||
	    (uint32_t)(y + h) > tex_h(t, 0))
		return PBM_E_SIZE;
	fmt = t->format;
	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++) {
			struct rgba c;
			if (fmt == PBM_TEX_RGBA8888) {
				const uint8_t *p = data + 4 * (j * w + i);
				c.r = p[0];
				c.g = p[1];
				c.b = p[2];
				c.a = p[3];
			} else {
				uint32_t v = data[j * w + i];
				if (fmt == PBM_TEX_GREY8) {
					c.r = c.g = c.b = v;
				} else {
					c.r = m->palette[3 * v];
					c.g = m->palette[3 * v + 1];
					c.b = m->palette[3 * v + 2];
				}
				c.a = fmt == PBM_TEX_INDEX8 && t->alpha && v == 255 ? 0 : 255;
			}
			t->level[0][(uint32_t)(y + j) * tex_w(t, 0) + (uint32_t)(x + i)] =
			    encode(t->keep, c);
		}
	for (l = 0; l + 1 < t->levels; l++)
		make_mip(t, l);
	return PBM_OK;
}

/* --- state --- */

void pbm_frame(struct pbm *m)
{
	zero(&m->state, sizeof m->state);
	zero(m->unit, sizeof m->unit);
	m->depth_near = 0;
	m->depth_far = 0xffff;
}

void pbm_init(struct pbm *m, uint16_t *colour, uint16_t *depth, int w, int h,
              uint16_t *pool, uint32_t pool_words)
{
	zero(m, sizeof *m);
	m->colour = colour;
	m->depth = depth;
	m->w = w;
	m->h = h;
	m->pool = pool;
	m->pool_words = pool_words;
	pbm_frame(m);
}

void pbm_palette(struct pbm *m, const uint8_t *rgb)
{
	int i;
	for (i = 0; i < 768; i++)
		m->palette[i] = rgb[i];
}

void pbm_set_state(struct pbm *m, const struct pbm_state *s)
{
	m->state = *s;
}

void pbm_bind_texture(struct pbm *m, int unit, const struct pbm_bind *b)
{
	if (unit == 0 || unit == 1)
		m->unit[unit] = *b;
}

void pbm_depth_range(struct pbm *m, uint32_t near_d, uint32_t far_d)
{
	m->depth_near = near_d & 0xffffu;
	m->depth_far = far_d & 0xffffu;
}

void pbm_clear_colour(struct pbm *m, uint16_t colour)
{
	int32_t i, n = m->w * m->h;
	for (i = 0; i < n; i++)
		m->colour[i] = colour;
}

void pbm_clear_depth(struct pbm *m, uint16_t depth)
{
	int32_t i, n = m->w * m->h;
	for (i = 0; i < n; i++)
		m->depth[i] = depth;
}

/* --- sampling --- */

/* A texel position along one side, wrapped (the side is a power of two)
 * or held at the edges. */
static uint32_t place(int64_t u, uint32_t side, int clamp)
{
	if (clamp)
		return u < 0 ? 0 : u >= (int64_t)side ? side - 1 : (uint32_t)u;
	return (uint32_t)u & (side - 1);
}

/* The texel at s, t (frac fraction bits) of level l. */
static struct rgba sample(const struct pbm_texture *t, const struct pbm_bind *b,
                          int l, int64_t s, int64_t t_, int frac)
{
	uint32_t w = tex_w(t, l), h = tex_h(t, l);
	const uint16_t *tx = t->level[l];
	s = asr(s, l);
	t_ = asr(t_, l);
	if (!b->linear) {
		uint32_t u = place(asr(s, frac), w, b->clamp_s);
		uint32_t v = place(asr(t_, frac), h, b->clamp_t);
		return decode(t->keep, tx[v * w + u]);
	} else {
		/* Bilinear: the four texels round the point half a texel up
		 * and left, weighted by 4 bits of the fraction each way. */
		int64_t su = s - ((int64_t)1 << (frac - 1));
		int64_t sv = t_ - ((int64_t)1 << (frac - 1));
		uint32_t fu = (uint32_t)(asr(su, frac - 4) & 15);
		uint32_t fv = (uint32_t)(asr(sv, frac - 4) & 15);
		uint32_t u0 = place(asr(su, frac), w, b->clamp_s);
		uint32_t u1 = place(asr(su, frac) + 1, w, b->clamp_s);
		uint32_t v0 = place(asr(sv, frac), h, b->clamp_t);
		uint32_t v1 = place(asr(sv, frac) + 1, h, b->clamp_t);
		struct rgba c00 = decode(t->keep, tx[v0 * w + u0]);
		struct rgba c10 = decode(t->keep, tx[v0 * w + u1]);
		struct rgba c01 = decode(t->keep, tx[v1 * w + u0]);
		struct rgba c11 = decode(t->keep, tx[v1 * w + u1]);
		struct rgba o;
#define MIX(f) ((((c00.f * (16 - fu) + c10.f * fu) * (16 - fv)) + \
                 ((c01.f * (16 - fu) + c11.f * fu) * fv) + 128) >> 8)
		o.r = MIX(r);
		o.g = MIX(g);
		o.b = MIX(b);
		o.a = MIX(a);
#undef MIX
		return o;
	}
}

/* The mip level for a triangle: the largest l with texel area at least
 * 4^l times its pixel area (both in the same units), at most the last. */
static int pick_level(const struct pbm_texture *t, int64_t tex_area, int64_t px_area)
{
	int l = 0;
	if (tex_area < 0)
		tex_area = -tex_area;
	while (l + 1 < t->levels && tex_area >= px_area << (2 * (l + 1)))
		l++;
	return l;
}

/* --- triangles --- */

/* The edge function of a to b at p: twice the area of a, b, p, positive
 * when p is to the right of a to b on the screen (y down). */
static int64_t edge(int64_t ax, int64_t ay, int64_t bx, int64_t by, int64_t px, int64_t py)
{
	return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

/* The top-left rule: a pixel centre on an edge is the triangle's if the
 * edge is a top one (flat, the triangle below) or a left one (the
 * triangle to its right). Two triangles that share an edge walk it in
 * opposite directions, so exactly one of them takes such a pixel. */
static int takes_edge(int64_t ax, int64_t ay, int64_t bx, int64_t by)
{
	int64_t dy = by - ay, dx = bx - ax;
	return dy < 0 || (dy == 0 && dx > 0);
}

static uint32_t clamp255(int64_t v)
{
	return v < 0 ? 0 : v > 255 ? 255 : (uint32_t)v;
}

static int bound(const struct pbm *m, int unit)
{
	uint32_t id = m->unit[unit].id;
	return id != 0 && id < PBM_TEX_IDS && m->tex[id].made;
}

void pbm_triangle(struct pbm *m, const struct pbm_vertex *v0,
                  const struct pbm_vertex *v1, const struct pbm_vertex *v2)
{
	const struct pbm_state *st = &m->state;
	const struct pbm_vertex *v[3], *first = v0;
	int64_t area, x[3], y[3];
	int64_t minx, maxx, miny, maxy;
	int px0, px1, py0, py1, px, py, i, es, qs, lvl[2] = { 0, 0 };
	int take[3];
	uint32_t q[3], qmax;
	int64_t rda;
	int tex0 = st->tex0 && bound(m, 0), tex1 = st->tex1 && bound(m, 1);

	m->counts.tris++;
	area = edge(v0->x, v0->y, v1->x, v1->y, v2->x, v2->y);
	/* Positive: clockwise as you see it. */
	if (area == 0 || (st->cull == PBM_CULL_CW && area > 0) ||
	    (st->cull == PBM_CULL_CCW && area < 0)) {
		m->counts.culled++;
		return;
	}
	v[0] = v0;
	v[1] = area > 0 ? v1 : v2;
	v[2] = area > 0 ? v2 : v1;
	if (area < 0)
		area = -area;
	for (i = 0; i < 3; i++) {
		x[i] = v[i]->x;
		y[i] = v[i]->y;
	}
	/* Edge i is the one facing vertex i, from vertex i + 1 to i + 2. */
	for (i = 0; i < 3; i++)
		take[i] = takes_edge(x[(i + 1) % 3], y[(i + 1) % 3], x[(i + 2) % 3], y[(i + 2) % 3]);

	/* The weights' scales: the edge values cut to 24 bits (they are at
	 * most the area inside), the q's to 16, so a weight is 40 bits. */
	es = bitlen((uint64_t)area) - 24;
	if (es < 0)
		es = 0;
	qmax = v[0]->q;
	if (v[1]->q > qmax)
		qmax = v[1]->q;
	if (v[2]->q > qmax)
		qmax = v[2]->q;
	qs = bitlen(qmax) - 16;
	if (qs < 0)
		qs = 0;
	for (i = 0; i < 3; i++)
		q[i] = v[i]->q >> qs;
	/* For depth, a reciprocal of the area, once a triangle:
	 * floor(2^46 / (area >> es)), up to 2^46. */
	rda = (int64_t)(((uint64_t)1 << 46) / ((uint64_t)area >> es));
	{

		/* The mip levels, from the texel and pixel areas. Unit 0's
		 * coordinates are 12.4 (a 256th of a texel squared), as the
		 * screen's are; unit 1's are 10.6, 16 times finer squared. */
		if (tex0 && m->unit[0].mipmap)
			lvl[0] = pick_level(&m->tex[m->unit[0].id],
			                    edge(v[0]->s, v[0]->t, v[1]->s, v[1]->t, v[2]->s, v[2]->t),
			                    area);
		if (tex1 && m->unit[1].mipmap)
			lvl[1] = pick_level(&m->tex[m->unit[1].id],
			                    edge(v[0]->s2, v[0]->t2, v[1]->s2, v[1]->t2, v[2]->s2, v[2]->t2),
			                    area * 16);

		/* The pixels whose centres the triangle's box holds, on the
		 * surface. Centres are at 8 sixteenths past each whole pixel. */
		minx = maxx = x[0];
		miny = maxy = y[0];
		for (i = 1; i < 3; i++) {
			if (x[i] < minx) minx = x[i];
			if (x[i] > maxx) maxx = x[i];
			if (y[i] < miny) miny = y[i];
			if (y[i] > maxy) maxy = y[i];
		}
		px0 = (int)asr(minx - 8 + 15, 4);
		px1 = (int)asr(maxx - 8, 4);
		py0 = (int)asr(miny - 8 + 15, 4);
		py1 = (int)asr(maxy - 8, 4);
		if (px0 < 0) px0 = 0;
		if (py0 < 0) py0 = 0;
		if (px1 > m->w - 1) px1 = m->w - 1;
		if (py1 > m->h - 1) py1 = m->h - 1;

		for (py = py0; py <= py1; py++)
			for (px = px0; px <= px1; px++) {
				int64_t cx = (int64_t)px * 16 + 8, cy = (int64_t)py * 16 + 8;
				int64_t e[3], w[3], s, n, sh, s16, r;
				uint32_t d, qf, idx = (uint32_t)py * (uint32_t)m->w + (uint32_t)px;
				int64_t a_s, a_t, a_s2, a_t2;
				struct rgba c, vc, dst;
				int inside = 1;

				for (i = 0; i < 3; i++) {
					e[i] = edge(x[(i + 1) % 3], y[(i + 1) % 3],
					            x[(i + 2) % 3], y[(i + 2) % 3], cx, cy);
					if (e[i] < 0 || (e[i] == 0 && !take[i]))
						inside = 0;
				}
				if (!inside)
					continue;
				m->counts.covered++;

				/* Perspective: each vertex weighs its edge value
				 * times its q, and an attribute is the weighted mean.
				 * One reciprocal of the weights' sum, cut to 16 bits,
				 * serves every attribute. */
				s = 0;
				for (i = 0; i < 3; i++) {
					w[i] = (e[i] >> es) * (int64_t)q[i];
					s += w[i];
				}
				if (s == 0)
					continue;
				sh = bitlen((uint64_t)s) - 16;
				if (sh < 0)
					sh = 0;
				s16 = s >> sh;
				r = (int64_t)pbm_recip((uint32_t)s16);
#define MEAN(f) (n = w[0] * v[0]->f + w[1] * v[1]->f + w[2] * v[2]->f, \
                 asr(asr(n, (int)sh) * r, 36))

				/* Depth: q at the pixel, from the same sum. */
				{
					int k = 46 - (int)sh - qs;
					uint64_t qv = (uint64_t)s16 * (uint64_t)rda;
					qv = k >= 0 ? qv >> k : qv << -k;
					qf = qv > 0xffffffffu ? 0xffffffffu : (uint32_t)qv;
				}
				d = pbm_depth_of_q(qf);
				d = (uint32_t)((int64_t)m->depth_near +
				               asr((int64_t)d * ((int64_t)m->depth_far -
				                                 (int64_t)m->depth_near + 1), 16));
				if ((st->depth_func == PBM_DEPTH_LEQUAL && !(d <= m->depth[idx])) ||
				    (st->depth_func == PBM_DEPTH_GEQUAL && !(d >= m->depth[idx]))) {
					m->counts.depth_fail++;
					continue;
				}

				/* The vertex colour. */
				if (st->smooth) {
					vc.r = clamp255(asr(MEAN(r), 4));
					vc.g = clamp255(asr(MEAN(g), 4));
					vc.b = clamp255(asr(MEAN(b), 4));
					vc.a = clamp255(asr(MEAN(a), 4));
				} else {
					vc.r = first->r;
					vc.g = first->g;
					vc.b = first->b;
					vc.a = first->a;
				}

				/* Unit 0: the texel, alone or times the colour. */
				if (st->tex0) {
					if (tex0) {
						a_s = MEAN(s);   /* 12.8 texels */
						a_t = MEAN(t);
						c = sample(&m->tex[m->unit[0].id], &m->unit[0], lvl[0],
						           a_s, a_t, 8);
					} else {
						c.r = c.g = c.b = c.a = 255;
					}
					if (st->modulate) {
						c.r = c.r * (vc.r + 1) >> 8;
						c.g = c.g * (vc.g + 1) >> 8;
						c.b = c.b * (vc.b + 1) >> 8;
						c.a = c.a * (vc.a + 1) >> 8;
					}
				} else {
					c = vc;
				}
				/* Unit 1: darken by its grey g, times (256 - g) / 256
				 * (GLQuake keeps 255 - light in its lightmaps). */
				if (tex1) {
					struct rgba l;
					a_s2 = MEAN(s2);  /* 10.10 texels */
					a_t2 = MEAN(t2);
					l = sample(&m->tex[m->unit[1].id], &m->unit[1], lvl[1],
					           a_s2, a_t2, 10);
					c.r = c.r * (256 - l.r) >> 8;
					c.g = c.g * (256 - l.g) >> 8;
					c.b = c.b * (256 - l.b) >> 8;
				}
#undef MEAN
				if (st->alpha_test && c.a < 170) {
					m->counts.alpha_fail++;
					continue;
				}

				/* Blending: weights out of 256, so 255 is all. */
				if (st->blend != PBM_BLEND_OFF) {
					dst = decode(PBM_KEEP_565, m->colour[idx]);
					m->counts.blended++;
					if (st->blend == PBM_BLEND_ALPHA) {
						uint32_t wa = c.a + (c.a >> 7);
						c.r = (c.r * wa + dst.r * (256 - wa)) >> 8;
						c.g = (c.g * wa + dst.g * (256 - wa)) >> 8;
						c.b = (c.b * wa + dst.b * (256 - wa)) >> 8;
					} else if (st->blend == PBM_BLEND_MULINV) {
						c.r = dst.r * (256 - (c.r + (c.r >> 7))) >> 8;
						c.g = dst.g * (256 - (c.g + (c.g >> 7))) >> 8;
						c.b = dst.b * (256 - (c.b + (c.b >> 7))) >> 8;
					} else {
						c.r = clamp255((int64_t)c.r + dst.r);
						c.g = clamp255((int64_t)c.g + dst.g);
						c.b = clamp255((int64_t)c.b + dst.b);
					}
				}
				m->colour[idx] = pbm_565(c.r, c.g, c.b);
				if (st->depth_write)
					m->depth[idx] = (uint16_t)d;
				m->counts.drawn++;
			}
	}
}

void pbm_draw(struct pbm *m, int kind, const struct pbm_vertex *v, uint32_t n)
{
	uint32_t k;
	if (n < 3)
		return;
	if (kind == PBM_LIST) {
		for (k = 0; k + 2 < n; k += 3)
			pbm_triangle(m, &v[k], &v[k + 1], &v[k + 2]);
	} else if (kind == PBM_STRIP) {
		for (k = 0; k + 2 < n; k++)
			if (k & 1)
				pbm_triangle(m, &v[k + 1], &v[k], &v[k + 2]);
			else
				pbm_triangle(m, &v[k], &v[k + 1], &v[k + 2]);
	} else if (kind == PBM_FAN) {
		for (k = 0; k + 2 < n; k++)
			pbm_triangle(m, &v[0], &v[k + 1], &v[k + 2]);
	}
}
