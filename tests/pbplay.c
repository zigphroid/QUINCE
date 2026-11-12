// tests/pbplay.c: a QUINCE_PBDUMP file drawn by the triangle engine's C
// model, the way PB would draw it.
//
// Our own code (docs/PB-ASKS.md step 2, PR 2). The dump is lib/pb3d.h's:
// the palette, the texture calls and each frame's commands, in the order
// the GL layer sent them (tests/pbdump.c writes it). Every record goes to
// the model through lib/pbdecode.c; each frame drawn is a CRC-32 of its
// 640x480 picture, and the frames asked for are written as PNGs.
//
//   pbplay DUMP [-o DIR] [-only] [-lod tri|q|exact|tri3] [-bias H] [-linear] [-scale N] [-raw DIR] [FRAME...]
//
// FRAME numbers count from 0. Without -only every frame is drawn, so each
// frame meets the picture the one before it left, as on PB; with it only
// the frames named are (the textures still all go in), which is quicker
// for a long demo. It prints each named frame's CRC, then a line of
// totals: frames, the CRC of every drawn frame's CRC in turn, and the
// pixels a frame covered, drew and blended (the fill budget's numbers).
// Its exit is 1 if a record or a command was refused.
//
// The mip study (lib/pbmodel.h's struct pbm_study, LOG 2026-11-12): -lod
// picks how a pixel's level is chosen, -bias H adds H half levels to it
// (-1 rounds down, where the rule rounds), -linear reads unit 0
// bilinear, and -scale N draws N times as wide and high, each picture
// written the mean of each N x N block (a supersampled reference); -raw DIR writes each named frame as DIR/frameNNNN.rgb (8
// bits a channel) and .dep (the 16-bit depth, at 1x only). The totals then
// say how many reads changed level, the texels each frame read, and the
// misses of an 8 KB direct-mapped texture cache of 32-byte lines (16
// texels in a row of a level, as the pool holds them): a stand-in for PB's.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pb3d.h"
#include "pbdecode.h"
#include "pbmodel.h"
#include "png.h"

enum { W = 640, H = 480, POOL_WORDS = 16u << 20, MAX_NAMED = 4096, LINES = 256 };

static struct pbm m;

// The texel reads: a bit a pool word for the frame's distinct texels, and
// the cache's tags.
static uint8_t *seen;
static uint32_t tags[LINES];
static unsigned long long distinct, misses, reads;

static void texel(void *ctx, uint32_t i)
{
    uint32_t line = i >> 4, slot = line % LINES;
    (void)ctx;
    reads++;
    if (!(seen[i >> 3] & (1u << (i & 7)))) {
        seen[i >> 3] |= (uint8_t)(1u << (i & 7));
        distinct++;
    }
    if (tags[slot] != line + 1) {
        tags[slot] = line + 1;
        misses++;
    }
}

static void write_raw(const char *dir, long frame, const uint16_t *c, const uint16_t *d, int k)
{
    char name[512];
    FILE *f;
    int x, y, i, j;
    static uint8_t row[W * 3];
    snprintf(name, sizeof name, "%s/frame%04ld.rgb", dir, frame);
    f = fopen(name, "wb");
    if (!f)
        return;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            unsigned r = 0, g = 0, b = 0;
            for (j = 0; j < k; j++)
                for (i = 0; i < k; i++) {
                    uint16_t v = c[(y * k + j) * W * k + x * k + i];
                    r += (v >> 11) * 255u / 31u;
                    g += (v >> 5 & 63) * 255u / 63u;
                    b += (v & 31) * 255u / 31u;
                }
            row[x * 3] = (uint8_t)((r + k * k / 2) / (k * k));
            row[x * 3 + 1] = (uint8_t)((g + k * k / 2) / (k * k));
            row[x * 3 + 2] = (uint8_t)((b + k * k / 2) / (k * k));
        }
        fwrite(row, 1, sizeof row, f);
    }
    fclose(f);
    if (k == 1) {
        snprintf(name, sizeof name, "%s/frame%04ld.dep", dir, frame);
        f = fopen(name, "wb");
        if (f) {
            fwrite(d, 2, W * H, f);
            fclose(f);
        }
    }
}

// A picture of N x N blocks' means, for -scale's PNG.
static void shrink(uint16_t *out, const uint16_t *c, int k)
{
    int x, y, i, j;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            unsigned r = 0, g = 0, b = 0;
            for (j = 0; j < k; j++)
                for (i = 0; i < k; i++) {
                    uint16_t v = c[(y * k + j) * W * k + x * k + i];
                    r += v >> 11;
                    g += v >> 5 & 63;
                    b += v & 31;
                }
            out[y * W + x] = (uint16_t)((r + k * k / 2) / (k * k) << 11 |
                                        (g + k * k / 2) / (k * k) << 5 | (b + k * k / 2) / (k * k));
        }
}

static uint32_t get16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t get32(const uint8_t *p) { return get16(p) | get16(p + 2) << 16; }

int main(int argc, char **argv)
{
    const char *path = 0, *dir = 0, *raw = 0;
    int lod = PBM_LOD_TRI, sc = 1, study = 0, bias = 0, linear0 = 0;
    static uint16_t small[W * H];
    long named[MAX_NAMED];
    int nnamed = 0, only = 0, i, bad = 0;
    FILE *f;
    uint8_t head[8];
    uint8_t *rec = 0;
    uint16_t *words = 0;
    size_t cap = 0;
    uint16_t *colour, *depth;
    uint16_t *pool = malloc(sizeof *pool * POOL_WORDS);
    long frame = 0, drawn = 0;
    uint32_t all = 0;
    unsigned long long covered = 0, painted = 0, blended = 0, tris = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            dir = argv[++i];
        else if (!strcmp(argv[i], "-only"))
            only = 1;
        else if (!strcmp(argv[i], "-raw") && i + 1 < argc)
            raw = argv[++i];
        else if (!strcmp(argv[i], "-linear"))
            linear0 = study = 1;
        else if (!strcmp(argv[i], "-bias") && i + 1 < argc)
            bias = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-scale") && i + 1 < argc)
            sc = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-lod") && i + 1 < argc) {
            const char *l = argv[++i];
            study = 1;
            lod = !strcmp(l, "q") ? PBM_LOD_Q : !strcmp(l, "exact") ? PBM_LOD_EXACT :
                  !strcmp(l, "tri3") ? PBM_LOD_TRILINEAR : PBM_LOD_TRI;
        }
        else if (!path)
            path = argv[i];
        else if (nnamed < MAX_NAMED)
            named[nnamed++] = strtol(argv[i], 0, 10);
    }
    if (sc < 1 || sc > 4)
        sc = 1;
    colour = malloc(sizeof *colour * W * H * sc * sc);
    depth = malloc(sizeof *depth * W * H * sc * sc);
    seen = calloc(POOL_WORDS / 8, 1);
    if (!path || !pool || !colour || !depth || !seen) {
        fprintf(stderr, "usage: pbplay DUMP [-o DIR] [-only] [-lod tri|q|exact|tri3] [-scale N] [-raw DIR] [FRAME...]\n");
        return 2;
    }
    f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return 2;
    }
    if (fread(head, 1, 8, f) != 8 || get32(head) != PB3D_DUMP_MAGIC ||
        get32(head + 4) != PB3D_DUMP_VERSION) {
        fprintf(stderr, "pbplay: %s isn't a version %u dump\n", path, PB3D_DUMP_VERSION);
        return 2;
    }
    pbm_init(&m, colour, depth, W * sc, H * sc, pool, POOL_WORDS);
    m.study.lod = lod;
    m.study.scale = sc;
    m.study.texel = texel;
    m.study.bias = bias;
    m.study.linear0 = linear0;
    pbm_clear_colour(&m, 0);
    pbm_clear_depth(&m, 0xffff);

    while (fread(head, 1, PB3D_REC_HEADER, f) == PB3D_REC_HEADER) {
        uint32_t type = get16(head), len = get32(head + 4), padded = len + (len & 1u);
        int e = 0;
        if (padded > cap) {
            cap = padded;
            rec = realloc(rec, cap);
            words = realloc(words, cap);
        }
        if (fread(rec, 1, padded, f) != padded) {
            fprintf(stderr, "pbplay: the dump ends inside a record\n");
            bad = 1;
            break;
        }
        if (type == PB3D_REC_PALETTE && len == 768) {
            pbm_palette(&m, rec);
        } else if (type == PB3D_REC_TEXIMAGE && len >= PB3D_TEXIMAGE_HEADER) {
            e = pbd_teximage(&m, get16(rec), rec[2], rec[3], get16(rec + 4), get16(rec + 6),
                             rec + PB3D_TEXIMAGE_HEADER);
        } else if (type == PB3D_REC_TEXSUB && len >= PB3D_TEXSUB_HEADER) {
            e = pbd_texsub(&m, get16(rec), get16(rec + 4), get16(rec + 6), get16(rec + 8),
                           get16(rec + 10), rec + PB3D_TEXSUB_HEADER);
        } else if (type == PB3D_REC_TEXFREE && len >= 2) {
            pbd_texfree(&m, get16(rec));
        } else if (type == PB3D_REC_FRAME) {
            int want = 0, k;
            for (k = 0; k < nnamed; k++)
                want |= named[k] == frame;
            if (!only || want) {
                uint32_t n = len / 2, crc;
                for (k = 0; k < (int)n; k++)
                    words[k] = (uint16_t)get16(rec + 2 * k);
                memset(&m.counts, 0, sizeof m.counts);
                memset(seen, 0, POOL_WORDS / 8);
                if (pbd_frame(&m, words, n) < 0) {
                    fprintf(stderr, "pbplay: frame %ld: a command PB would refuse\n", frame);
                    bad = 1;
                }
                crc = png_crc32(0, (const uint8_t *)colour, sizeof *colour * W * H * sc * sc);
                all = png_crc32(all, (const uint8_t *)&crc, 4);
                covered += m.counts.covered;
                painted += m.counts.drawn;
                blended += m.counts.blended;
                tris += m.counts.tris;
                drawn++;
                if (want) {
                    printf("pbplay: frame %ld %08x, %u triangles, %u covered, %u drawn, %u blended\n",
                           frame, crc, m.counts.tris, m.counts.covered, m.counts.drawn,
                           m.counts.blended);
                    if (raw)
                        write_raw(raw, frame, colour, depth, sc);
                    if (dir) {
                        char name[512];
                        snprintf(name, sizeof name, "%s/frame%04ld.png", dir, frame);
                        if (sc > 1)
                            shrink(small, colour, sc);
                        if (png_write_565(name, sc > 1 ? small : colour, W, H) != 0) {
                            fprintf(stderr, "pbplay: couldn't write %s\n", name);
                            bad = 1;
                        }
                    }
                }
            }
            frame++;
        } else {
            fprintf(stderr, "pbplay: a record of type %u and %u bytes, not one of lib/pb3d.h's\n",
                    type, len);
            bad = 1;
        }
        if (e != PBM_OK) {
            fprintf(stderr, "pbplay: frame %ld: a texture record refused (%d)\n", frame, e);
            bad = 1;
        }
    }
    fclose(f);
    if (study || sc > 1)
        printf("pbplay: study lod %d scale %d: %.2f%% of mip reads moved level; a frame %.0f texel reads, "
               "%.0f distinct texels, %.0f cache misses (%.0f KB)\n",
               lod, sc, m.study.mip_reads ? 100.0 * (double)m.study.level_moved / (double)m.study.mip_reads : 0,
               drawn ? (double)reads / drawn : 0, drawn ? (double)distinct / drawn : 0,
               drawn ? (double)misses / drawn : 0, drawn ? (double)misses * 32 / drawn / 1024 : 0);
    printf("pbplay: %ld frames, %ld drawn, all %08x; a frame %.0f triangles, %.2f screens covered, "
           "%.2f drawn, %.2f blended; textures %u KB\n",
           frame, drawn, all, drawn ? (double)tris / drawn : 0,
           drawn ? (double)covered / drawn / (W * H * sc * sc) : 0, drawn ? (double)painted / drawn / (W * H * sc * sc) : 0,
           drawn ? (double)blended / drawn / (W * H * sc * sc) : 0, m.pool_used * 2u / 1024u);
    free(rec);
    free(words);
    free(pool);
    free(colour);
    free(depth);
    free(seen);
    return bad;
}
