/* tests/pbdump.c: QUINCE_PBDUMP=FILE on the PC, the GL layer's output
 * written to a file for the C model to draw (lib/pb3d.h, "the dump").
 *
 * Our own code (M2 PR 2). A sink for lib/pbgl.c: each call a record, in
 * the order the calls come, but a frame's windows of words, which are
 * one record at its end. Little-endian, as the PC and the board are.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pbgl.h"
#include "pb3d.h"
#include "pbdump.h"

static FILE *out;

static void put16(uint32_t v)
{
	fputc((int)(v & 0xff), out);
	fputc((int)(v >> 8 & 0xff), out);
}

static void put32(uint32_t v)
{
	put16(v & 0xffff);
	put16(v >> 16);
}

static void record(uint32_t type, uint32_t len)
{
	put16(type);
	put16(0);
	put32(len);
}

static void pad(uint32_t len)
{
	if (len & 1u)
		fputc(0, out);
}

static uint32_t texel_bytes(uint32_t format)
{
	return format == PB3D_TEX_RGBA8888 ? 4u : 1u;
}

static void palette(const uint8_t *rgb)
{
	record(PB3D_REC_PALETTE, 768);
	fwrite(rgb, 1, 768, out);
}

static void teximage(uint32_t id, uint32_t format, uint32_t flags, uint32_t w, uint32_t h,
                     const void *texels)
{
	uint32_t n = w * h * texel_bytes(format);
	record(PB3D_REC_TEXIMAGE, PB3D_TEXIMAGE_HEADER + n);
	put16(id);
	fputc((int)format, out);
	fputc((int)flags, out);
	put16(w);
	put16(h);
	fwrite(texels, 1, n, out);
	pad(n);
}

static void texsub(uint32_t id, uint32_t format, uint32_t x, uint32_t y, uint32_t w,
                   uint32_t h, const void *texels)
{
	uint32_t n = w * h * texel_bytes(format);
	record(PB3D_REC_TEXSUB, PB3D_TEXSUB_HEADER + n);
	put16(id);
	fputc((int)format, out);
	fputc(0, out);
	put16(x);
	put16(y);
	put16(w);
	put16(h);
	fwrite(texels, 1, n, out);
	pad(n);
}

/* The frame comes a window at a time (M4 PR 4) and the file keeps a
 * record a frame, so the windows are gathered here until it ends. */
static uint16_t *fw;
static uint32_t fn, fcap;

static void words(const uint16_t *w, uint32_t n)
{
	if (fn + n > fcap) {
		uint32_t cap = fcap ? fcap : 16384u;
		while (cap < fn + n)
			cap *= 2;
		fw = realloc(fw, cap * sizeof *fw);
		if (!fw) {
			fprintf(stderr, "QUINCE_PBDUMP: no memory for a frame\n");
			exit(1);
		}
		fcap = cap;
	}
	memcpy(fw + fn, w, n * sizeof *w);
	fn += n;
}

static void frame(void)
{
	uint32_t i;
	record(PB3D_REC_FRAME, fn * 2);
	for (i = 0; i < fn; i++)
		put16(fw[i]);
	fn = 0;
}

static const struct pbgl_sink dump_sink = { palette, teximage, texsub, words, frame };

static void close_dump(void)
{
	if (out && fclose(out) != 0)
		fprintf(stderr, "QUINCE_PBDUMP: couldn't finish the file\n");
	out = 0;
}

const struct pbgl_sink *pbdump_open(const char *path)
{
	out = fopen(path, "wb");
	if (!out) {
		perror(path);
		exit(1);
	}
	put32(PB3D_DUMP_MAGIC);
	put32(PB3D_DUMP_VERSION);
	atexit(close_dump);
	return &dump_sink;
}
