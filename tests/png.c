// tests/png.c: an RGB565 picture written as a PNG, for the host tests.
//
// Our own code, from the PNG specification (W3C, 2nd edition, 2003: the
// chunk layout, section 5; IHDR, 11.2.2; the CRC, annex D) and zlib's and
// deflate's (RFC 1950, RFC 1951 section 3.2.4: stored blocks). Nothing is
// compressed: every block is stored, so the file is about the picture's
// size in RGB, and the code stays small. Any viewer opens it.
#include "png.h"

#include <stdio.h>

static uint32_t crc_table[256];

static void crc_init(void)
{
    uint32_t n, c;
    int k;
    if (crc_table[1])
        return;
    for (n = 0; n < 256; n++) {
        c = n;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

uint32_t png_crc32(uint32_t crc, const uint8_t *p, size_t n)
{
    crc_init();
    crc = ~crc;
    while (n--)
        crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

// A chunk being written: its type and data go through the CRC as they go
// out, so the data can come in pieces.
struct chunk {
    FILE *f;
    uint32_t crc;
};

static void chunk_start(struct chunk *c, FILE *f, const char *type, uint32_t len)
{
    uint8_t b[4];
    c->f = f;
    put32(b, len);
    fwrite(b, 1, 4, f);
    fwrite(type, 1, 4, f);
    c->crc = png_crc32(0, (const uint8_t *)type, 4);
}

static void chunk_data(struct chunk *c, const uint8_t *p, size_t n)
{
    fwrite(p, 1, n, c->f);
    c->crc = png_crc32(c->crc, p, n);
}

static void chunk_end(struct chunk *c)
{
    uint8_t b[4];
    put32(b, c->crc);
    fwrite(b, 1, 4, c->f);
}

int png_write_565(const char *path, const uint16_t *px, int w, int h)
{
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    FILE *f = fopen(path, "wb");
    struct chunk c;
    uint8_t b[13], row[1 + 3 * 4096];
    uint32_t raw = (uint32_t)h * (1u + 3u * (uint32_t)w);
    uint32_t blocks = (raw + 65534) / 65535;
    uint32_t a1 = 1, a2 = 0, left = 65535, in_block = 0;
    int x, y;

    if (!f || w < 1 || w > 4096 || h < 1)
        return -1;
    fwrite(sig, 1, 8, f);

    put32(b, (uint32_t)w);
    put32(b + 4, (uint32_t)h);
    b[8] = 8;   // bits a channel
    b[9] = 2;   // RGB
    b[10] = b[11] = b[12] = 0;
    chunk_start(&c, f, "IHDR", 13);
    chunk_data(&c, b, 13);
    chunk_end(&c);

    // zlib's two bytes, the stored blocks (5 bytes of header each), Adler-32.
    chunk_start(&c, f, "IDAT", 2 + raw + 5 * blocks + 4);
    b[0] = 0x78;
    b[1] = 0x01;
    chunk_data(&c, b, 2);
    for (y = 0; y < h; y++) {
        size_t n = 0, i;
        row[n++] = 0;   // no filter
        for (x = 0; x < w; x++) {
            uint16_t v = px[(size_t)y * (size_t)w + (size_t)x];
            uint32_t r5 = v >> 11, g6 = (v >> 5) & 63, b5 = v & 31;
            row[n++] = (uint8_t)(r5 << 3 | r5 >> 2);
            row[n++] = (uint8_t)(g6 << 2 | g6 >> 4);
            row[n++] = (uint8_t)(b5 << 3 | b5 >> 2);
        }
        for (i = 0; i < n; i++) {
            if (in_block == 0) {
                uint32_t len = raw < 65535 ? raw : 65535;
                left = len;
                b[0] = raw == len ? 1 : 0;   // the last block
                b[1] = (uint8_t)len;
                b[2] = (uint8_t)(len >> 8);
                b[3] = (uint8_t)~len;
                b[4] = (uint8_t)(~len >> 8);
                chunk_data(&c, b, 5);
                in_block = 1;
            }
            chunk_data(&c, &row[i], 1);
            a1 = (a1 + row[i]) % 65521;
            a2 = (a2 + a1) % 65521;
            raw--;
            if (--left == 0)
                in_block = 0;
        }
    }
    put32(b, a2 << 16 | a1);
    chunk_data(&c, b, 4);
    chunk_end(&c);

    chunk_start(&c, f, "IEND", 0);
    chunk_end(&c);
    return fclose(f) == 0 ? 0 : -1;
}
