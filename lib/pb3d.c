/* lib/pb3d.c: PB's 3D commands written into a buffer (lib/pb3d.h).
 *
 * Our own code (docs/PB-ASKS.md, step 1). No kernel calls and no libc,
 * so the host test and QUINCE /SELFTEST check the same bytes. The buffer
 * is the caller's: the GL layer keeps a frame's worth on the heap, since
 * a static one would cost the program area.
 */
#include "pb3d.h"

/* --- the conversions --- */

/* v times scale to the nearest whole step, halves away from zero, held
 * at lo and hi. The tests come before the cast, since casting a float
 * out of the integer's range is undefined in C (C11 6.3.1.4). */
static int32_t fix(float v, float scale, int32_t lo, int32_t hi)
{
	if (!(v == v))
		return 0;            /* NaN */
	v *= scale;
	if (v <= (float)lo)
		return lo;
	if (v >= (float)hi)
		return hi;
	return (int32_t)(v < 0.0f ? v - 0.5f : v + 0.5f);
}

int16_t pb3d_fix_12_4(float v)
{
	return (int16_t)fix(v, 16.0f, -32768, 32767);
}

int16_t pb3d_fix_10_6(float v)
{
	return (int16_t)fix(v, 64.0f, -32768, 32767);
}

/* q is 0 or more (behind the near plane the GL layer has clipped), so
 * the 1.31 value is unsigned. A float holds 24 bits, so the low bits of
 * a big q are 0; PB's depth takes 16 of them and its perspective fewer. */
uint32_t pb3d_fix_q(float q)
{
	float v;
	if (!(q == q) || q <= 0.0f)
		return 0;
	v = q * 2147483648.0f;
	if (v >= 4294967040.0f)  /* the largest float under 2^32 */
		return 0xffffffffu;
	return (uint32_t)(v + 0.5f);
}

/* --- the length of a command --- */

uint32_t pb3d_cmd_words(const uint16_t *w, uint32_t left)
{
	uint32_t op, arg, n, need;
	if (left == 0)
		return 0;
	op = PB3D_OP(w[0]);
	arg = PB3D_ARG(w[0]);
	switch (op) {
	case PB3D_DEPTH:
		if (arg & ~PB3D_DEPTH_CLEAR)
			return 0;
		need = PB3D_DEPTH_WORDS;
		break;
	case PB3D_STATE:
		if (arg)
			return 0;
		need = PB3D_STATE_WORDS;
		break;
	case PB3D_TEXTURE:
		if (arg > 1)
			return 0;
		need = PB3D_TEXTURE_WORDS;
		break;
	case PB3D_DEPTHRANGE:
		if (arg)
			return 0;
		need = PB3D_DEPTHRANGE_WORDS;
		break;
	case PB3D_TRIS:
		if ((arg & ~PB3D_ARG_ALL) || (arg & PB3D_KIND_MASK) == 3u ||
		    (arg & PB3D_F_COLOUR_MASK) == PB3D_F_COLOUR_MASK)
			return 0;
		if (left < PB3D_TRIS_WORDS)
			return 0;
		n = w[1];
		need = PB3D_TRIS_WORDS + n * PB3D_VERTEX_WORDS(arg);
		break;
	default:
		return 0;
	}
	return need <= left ? need : 0;
}

/* --- the encoder --- */

void pb3d_init(struct pb3d_buf *b, uint16_t *words, uint32_t cap)
{
	b->w = words;
	b->cap = cap;
	b->dropped = 0;
	pb3d_clear(b);
}

void pb3d_clear(struct pb3d_buf *b)
{
	b->n = 0;
	b->tris_at = 0;
	b->tris_left = 0;
	b->tris_format = 0;
}

/* Room for k more words, or count the command dropped. A command is
 * written whole or not at all, so a reader never meets half of one. */
static int room(struct pb3d_buf *b, uint32_t k)
{
	if (b->tris_left == 0 && b->cap - b->n >= k)
		return 1;
	b->dropped++;
	return 0;
}

static void put(struct pb3d_buf *b, uint32_t v)
{
	b->w[b->n++] = (uint16_t)v;
}

static void put32(struct pb3d_buf *b, uint32_t v)
{
	put(b, v & 0xffffu);
	put(b, v >> 16);
}

void pb3d_depth(struct pb3d_buf *b, int clear)
{
	if (room(b, PB3D_DEPTH_WORDS))
		put(b, PB3D_WORD(PB3D_DEPTH, clear ? PB3D_DEPTH_CLEAR : 0));
}

void pb3d_state(struct pb3d_buf *b, uint32_t bits)
{
	if (room(b, PB3D_STATE_WORDS)) {
		put(b, PB3D_WORD(PB3D_STATE, 0));
		put(b, bits & PB3D_S_ALL);
	}
}

void pb3d_texture(struct pb3d_buf *b, uint32_t unit, uint32_t id, uint32_t how)
{
	if (room(b, PB3D_TEXTURE_WORDS)) {
		put(b, PB3D_WORD(PB3D_TEXTURE, unit & 1u));
		put(b, id & 0xffffu);
		put(b, how & PB3D_T_ALL);
	}
}

void pb3d_depthrange(struct pb3d_buf *b, uint32_t near, uint32_t far)
{
	if (room(b, PB3D_DEPTHRANGE_WORDS)) {
		put(b, PB3D_WORD(PB3D_DEPTHRANGE, 0));
		put(b, near & 0xffffu);
		put(b, far & 0xffffu);
	}
}

int pb3d_tris(struct pb3d_buf *b, uint32_t kind, uint32_t format, uint32_t n)
{
	uint32_t arg = (kind & PB3D_KIND_MASK) | (format & PB3D_FORMAT_MASK);
	uint32_t vw = PB3D_VERTEX_WORDS(format);
	if ((arg & PB3D_KIND_MASK) == 3u || (arg & PB3D_F_COLOUR_MASK) == PB3D_F_COLOUR_MASK ||
	    n < 3 || n > 0xffffu) {
		b->dropped++;
		return -1;
	}
	/* n * vw can't overflow: n is under 2^16 and vw at most 11. */
	if (!room(b, PB3D_TRIS_WORDS + n * vw))
		return -1;
	put(b, PB3D_WORD(PB3D_TRIS, arg));
	b->tris_at = b->n;
	put(b, n);
	b->tris_left = n;
	b->tris_format = (uint8_t)format;
	return 0;
}

void pb3d_vertex(struct pb3d_buf *b, const struct pb3d_vertex *v)
{
	uint32_t f = b->tris_format;
	if (b->tris_left == 0)
		return;   /* no TRIS open: its pb3d_tris failed, or n are done */
	put(b, (uint16_t)pb3d_fix_12_4(v->x));
	put(b, (uint16_t)pb3d_fix_12_4(v->y));
	if (f & PB3D_F_Q)
		put32(b, pb3d_fix_q(v->q));
	if (f & PB3D_F_ST) {
		put(b, (uint16_t)pb3d_fix_12_4(v->s));
		put(b, (uint16_t)pb3d_fix_12_4(v->t));
	}
	if (f & PB3D_F_ST2) {
		put(b, (uint16_t)pb3d_fix_10_6(v->s2));
		put(b, (uint16_t)pb3d_fix_10_6(v->t2));
	}
	if ((f & PB3D_F_COLOUR_MASK) == PB3D_F_GREY)
		put(b, v->r);
	else if ((f & PB3D_F_COLOUR_MASK) == PB3D_F_RGBA) {
		put(b, (uint32_t)v->r | (uint32_t)v->g << 8);
		put(b, (uint32_t)v->b | (uint32_t)v->a << 8);
	}
	b->tris_left--;
}
