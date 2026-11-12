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
 * to a 16-bit field. The tests come before the cast, since casting a
 * float out of the integer's range is undefined in C (C11 6.3.1.4).
 * roundf is the M33's one VRINTA, which rounds halves away from zero
 * (M3's built-ins); adding a half and cutting, as M2 first did, took
 * four branches and got 0.49999997 wrong (it rounds the sum up to 1).
 * M2's board run had this at 7% of the program (LOG 2026-11-12). */
static uint16_t fix16(float v, float scale)
{
	if (!(v == v))
		return 0;            /* NaN */
	v *= scale;
	if (v <= -32768.0f)
		return 0x8000u;
	if (v >= 32767.0f)
		return 0x7fffu;
	return (uint16_t)(int32_t)__builtin_roundf(v);
}

int16_t pb3d_fix_12_4(float v)
{
	return (int16_t)fix16(v, 16.0f);
}

int16_t pb3d_fix_10_6(float v)
{
	return (int16_t)fix16(v, 64.0f);
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
	/* Not v + 0.5: for v in [2^23, 2^24) a float's step is 1, so the sum
	 * is a tie, rounded to even, and an odd v would come out one high.
	 * roundf rounds v itself (VRINTA). */
	return (uint32_t)__builtin_roundf(v);
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
	case PB3D_CLIP:
		if (arg)
			return 0;
		need = PB3D_CLIP_WORDS;
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

void pb3d_clip(struct pb3d_buf *b, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
	if (room(b, PB3D_CLIP_WORDS)) {
		put(b, PB3D_WORD(PB3D_CLIP, 0));
		put(b, x0 & 0xffffu);
		put(b, y0 & 0xffffu);
		put(b, x1 & 0xffffu);
		put(b, y1 & 0xffffu);
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
	put(b, fix16(v->x, 16.0f));
	put(b, fix16(v->y, 16.0f));
	if (f & PB3D_F_Q)
		put32(b, pb3d_fix_q(v->q));
	if (f & PB3D_F_ST) {
		put(b, fix16(v->s, 16.0f));
		put(b, fix16(v->t, 16.0f));
	}
	if (f & PB3D_F_ST2) {
		put(b, fix16(v->s2, 64.0f));
		put(b, fix16(v->t2, 64.0f));
	}
	if ((f & PB3D_F_COLOUR_MASK) == PB3D_F_GREY)
		put(b, v->r);
	else if ((f & PB3D_F_COLOUR_MASK) == PB3D_F_RGBA) {
		put(b, (uint32_t)v->r | (uint32_t)v->g << 8);
		put(b, (uint32_t)v->b | (uint32_t)v->a << 8);
	}
	b->tris_left--;
}
