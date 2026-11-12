/* tests/pbdump.c: QUINCE_PBDUMP=FILE on the PC, the GL layer's output
 * written to a file for the C model to draw (lib/pb3d.h, "the dump").
 *
 * Our own code (M2 PR 2). A sink for lib/pbgl.c: each call a record, in
 * the order the calls come. Little-endian, as the PC and the board are.
 */
#include <stdio.h>
#include <stdlib.h>
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

static void frame(const uint16_t *words, uint32_t n)
{
	uint32_t i;
	record(PB3D_REC_FRAME, n * 2);
	for (i = 0; i < n; i++)
		put16(words[i]);
}

static const struct pbgl_sink dump_sink = { palette, teximage, texsub, frame };

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
