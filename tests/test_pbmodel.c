// tests/test_pbmodel.c: the triangle engine's C model (lib/pbmodel.c)
// held to its rules (docs/PB3D.md), then a scene drawn with every rule at
// once, written to build/pbmodel/scene.png and held to its CRC.
//
// A host test, not /SELFTEST: the model isn't in QUINCE.JAM (PB draws,
// not the Jam), and a scene needs a 640x480 picture and a file.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pbmodel.h"
#include "png.h"

static int ran, failed;

static void check(const char *what, long got, long want)
{
    ran++;
    if (got != want) {
        failed++;
        printf("FAIL: %s: got %ld, want %ld\n", what, got, want);
    }
}

static void check_true(const char *what, int ok)
{
    check(what, ok != 0, 1);
}

// The model, with the surface and pool on the heap at their exact sizes,
// so ASan sees a write past either.
static struct pbm m;
static uint16_t *colour, *depth, *pool;

static void model(int w, int h)
{
    free(colour);
    free(depth);
    free(pool);
    colour = malloc(sizeof *colour * (size_t)w * (size_t)h);
    depth = malloc(sizeof *depth * (size_t)w * (size_t)h);
    pool = malloc(sizeof *pool * (size_t)(1u << 20));
    pbm_init(&m, colour, depth, w, h, pool, 1u << 20);
    pbm_clear_colour(&m, 0);
    pbm_clear_depth(&m, 0xffff);
}

static struct pbm_state state(void)
{
    struct pbm_state s;
    memset(&s, 0, sizeof s);
    return s;
}

// A vertex from pixels, q as a fraction and texels, rounded as the GL
// layer rounds (lib/pb3d.h's pb3d_fix_*).
static struct pbm_vertex vtx(double x, double y, double q, double s, double t)
{
    struct pbm_vertex v;
    memset(&v, 0, sizeof v);
    v.x = (int32_t)lround(x * 16);
    v.y = (int32_t)lround(y * 16);
    v.q = (uint32_t)llround(q * 2147483648.0);
    v.s = (int32_t)lround(s * 16);
    v.t = (int32_t)lround(t * 16);
    v.r = v.g = v.b = v.a = 255;
    return v;
}

static struct pbm_vertex rgba(struct pbm_vertex v, int r, int g, int b, int a)
{
    v.r = (uint8_t)r;
    v.g = (uint8_t)g;
    v.b = (uint8_t)b;
    v.a = (uint8_t)a;
    return v;
}

static struct pbm_vertex st2(struct pbm_vertex v, double s2, double t2)
{
    v.s2 = (int32_t)lround(s2 * 64);
    v.t2 = (int32_t)lround(t2 * 64);
    return v;
}

// An axis-aligned rectangle as a fan, s and t across it.
static void rect(double x0, double y0, double x1, double y1, double q,
                 double s0, double t0, double s1, double t1)
{
    struct pbm_vertex v[4];
    v[0] = vtx(x0, y0, q, s0, t0);
    v[1] = vtx(x1, y0, q, s1, t0);
    v[2] = vtx(x1, y1, q, s1, t1);
    v[3] = vtx(x0, y1, q, s0, t1);
    pbm_draw(&m, PBM_FAN, v, 4);
}

static uint32_t lcg = 12345;
static int rnd(int n)
{
    lcg = lcg * 1103515245u + 12345u;
    return (int)((lcg >> 16) % (uint32_t)n);
}

static int r5(uint16_t v) { return v >> 11; }

// --- coverage ---

// Two triangles a cell over a jittered grid, and a fan round a point: with
// every triangle adding 1 to each channel of the pixels it takes, each
// pixel must end at exactly 1. A gap leaves a 0, a pixel two triangles
// both take a 2.
static void partition(void)
{
    enum { W = 61, H = 47, N = 9 };
    static struct pbm_vertex g[N + 1][N + 1];
    struct pbm_state s = state();
    struct pbm_vertex fan[12];
    int i, j, k, bad = 0;

    model(W, H);
    s.blend = PBM_BLEND_ADD;
    pbm_set_state(&m, &s);
    for (j = 0; j <= N; j++)
        for (i = 0; i <= N; i++) {
            double x = -3 + i * (W + 6) / (double)N, y = -3 + j * (H + 6) / (double)N;
            if (i > 0 && i < N)
                x += (rnd(64) - 32) / 16.0;
            if (j > 0 && j < N)
                y += (rnd(64) - 32) / 16.0;
            g[j][i] = rgba(vtx(x, y, 1, 0, 0), 8, 4, 8, 255);
        }
    // A vertex exactly on a pixel centre and edges through centres too.
    g[4][4].x = 30 * 16 + 8;
    g[4][4].y = 22 * 16 + 8;
    g[4][5].y = g[4][4].y;
    g[5][4].x = g[4][4].x;
    for (j = 0; j < N; j++)
        for (i = 0; i < N; i++) {
            if ((i + j) & 1) {
                pbm_triangle(&m, &g[j][i], &g[j][i + 1], &g[j + 1][i + 1]);
                pbm_triangle(&m, &g[j][i], &g[j + 1][i + 1], &g[j + 1][i]);
            } else {
                pbm_triangle(&m, &g[j][i], &g[j][i + 1], &g[j + 1][i]);
                pbm_triangle(&m, &g[j][i + 1], &g[j + 1][i + 1], &g[j + 1][i]);
            }
        }
    for (k = 0; k < W * H; k++)
        bad += r5(colour[k]) != 1;
    check("the grid takes every pixel once", bad, 0);

    // A fan in an empty picture, its rim outside it: once again.
    pbm_clear_colour(&m, 0);
    fan[0] = rgba(vtx(25.5, 20.5, 1, 0, 0), 8, 4, 8, 255);
    for (k = 0; k < 11; k++) {
        double a = k * 2 * 3.14159265358979 / 10;
        fan[k + 1] = rgba(vtx(25.5 + 90 * cos(a), 20.5 + 90 * sin(a), 1, 0, 0), 8, 4, 8, 255);
    }
    fan[11] = fan[1];
    pbm_draw(&m, PBM_FAN, fan, 12);
    bad = 0;
    for (k = 0; k < W * H; k++)
        bad += r5(colour[k]) != 1;
    check("a fan round a pixel centre takes every pixel once", bad, 0);
}

// The top-left rule: a square with its edges through pixel centres takes
// the centres on its top and left edges, not those on its right and bottom.
static void top_left(void)
{
    struct pbm_state s = state();
    model(4, 4);
    pbm_set_state(&m, &s);
    pbm_clear_colour(&m, 0);
    rect(0.5, 0.5, 2.5, 2.5, 1, 0, 0, 0, 0);
    check("pixels taken", (long)m.counts.drawn, 4);
    check("top left's centre (on two edges)", colour[0], 0xffff);
    check("on the top edge", colour[1], 0xffff);
    check("on the right edge", colour[2], 0);
    check("on the bottom edge", colour[2 * 4 + 1], 0);
}

static void culling(void)
{
    struct pbm_state s = state();
    struct pbm_vertex a = vtx(0, 0, 1, 0, 0), b = vtx(8, 0, 1, 0, 0),
                      c = vtx(0, 8, 1, 0, 0), d = vtx(8, 8, 1, 0, 0);
    struct pbm_vertex strip[4];

    model(8, 8);
    s.cull = PBM_CULL_CW;
    pbm_set_state(&m, &s);
    pbm_triangle(&m, &a, &b, &c);   // clockwise on the screen (y down)
    check("CULL_CW drops a clockwise triangle", (long)m.counts.culled, 1);
    pbm_triangle(&m, &a, &c, &b);
    check("and draws a counter-clockwise one", (long)m.counts.drawn > 0, 1);
    s.cull = PBM_CULL_CCW;
    pbm_set_state(&m, &s);
    m.counts.culled = 0;
    pbm_triangle(&m, &a, &c, &b);
    check("CULL_CCW drops a counter-clockwise one", (long)m.counts.culled, 1);
    // A strip of two, its first clockwise: its second winds the same way.
    strip[0] = a;
    strip[1] = b;
    strip[2] = c;
    strip[3] = d;
    m.counts.culled = 0;
    pbm_draw(&m, PBM_STRIP, strip, 4);
    check("a strip's odd triangle winds as its even one", (long)m.counts.culled, 0);
    pbm_triangle(&m, &a, &a, &b);
    check("no area: nothing", (long)m.counts.culled, 1);
}

// A triangle far past the surface on every side: every pixel once, and
// nothing written outside (ASan).
static void guard_band(void)
{
    struct pbm_state s = state();
    model(37, 29);
    pbm_set_state(&m, &s);
    rect(-2000, -2000, 2000, 2000, 1, 0, 0, 0, 0);
    check("covered", (long)m.counts.covered, 37 * 29);
}

// --- depth ---

static void depth_format(void)
{
    uint32_t q, last = 0, d;
    int ok = 1;
    check("q 1.0 is nearest", (long)pbm_depth_of_q(0x80000000u), 0);
    check("past 1.0 too", (long)pbm_depth_of_q(0xffffffffu), 0);
    check("q 0.5 starts the first octave", (long)pbm_depth_of_q(0x40000000u), 0x0fff);
    check("q just under 0.5 the second", (long)pbm_depth_of_q(0x3fffffffu), 0x1000);
    check("q 2^-16 is the farthest", (long)pbm_depth_of_q(1u << 15), 0xffff);
    check("q 0", (long)pbm_depth_of_q(0), 0xffff);
    // Never nearer as q falls, and a 4,096th of the way at each octave.
    for (q = 0x80000000u; q > (1u << 14); q -= q >> 9) {
        d = pbm_depth_of_q(q);
        if (d < last)
            ok = 0;
        last = d;
    }
    check_true("depth keeps q's order", ok);
    check("one step in the mantissa", (long)(pbm_depth_of_q(0x40000000u) -
                                             pbm_depth_of_q(0x40000000u + (1u << 18))), 1);
}

static void depth_test(void)
{
    struct pbm_state s = state();
    uint16_t near_c = pbm_565(0, 255, 0), far_c = pbm_565(255, 0, 0);
    struct pbm_vertex nv[3], fv[3];
    int i;

    model(16, 16);
    for (i = 0; i < 3; i++) {
        double x = i == 1 ? 40 : -8, y = i == 2 ? 40 : -8;
        nv[i] = rgba(vtx(x, y, 0.5, 0, 0), 0, 255, 0, 255);
        fv[i] = rgba(vtx(x, y, 0.25, 0, 0), 255, 0, 0, 255);
    }
    s.depth_func = PBM_DEPTH_LEQUAL;
    s.depth_write = 1;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_LIST, fv, 3);
    pbm_draw(&m, PBM_LIST, nv, 3);
    check("LEQUAL: the near one over the far", colour[5 * 16 + 5], near_c);
    // Within a step: q at a pixel is good to about S16's 16 bits.
    check("its depth written", labs((long)depth[5 * 16 + 5] -
                                    (long)pbm_depth_of_q(0x40000000u)) <= 1, 1);
    pbm_draw(&m, PBM_LIST, fv, 3);
    check("LEQUAL: the far one after it fails", colour[5 * 16 + 5], near_c);
    pbm_draw(&m, PBM_LIST, nv, 3);
    check("LEQUAL: the same depth passes", (long)m.counts.depth_fail, 256);

    pbm_clear_depth(&m, 0);
    s.depth_func = PBM_DEPTH_GEQUAL;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_LIST, nv, 3);
    pbm_draw(&m, PBM_LIST, fv, 3);
    check("GEQUAL: the far one wins", colour[5 * 16 + 5], far_c);

    pbm_clear_depth(&m, 0x1234);
    s.depth_func = PBM_DEPTH_ALWAYS;
    s.depth_write = 0;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_LIST, nv, 3);
    check("ALWAYS draws", colour[5 * 16 + 5], near_c);
    check("no write leaves depth", depth[5 * 16 + 5], 0x1234);

    // The view weapon's range: 0 to 0.3 of it, so a weapon four times
    // farther than a wall still sits in front of it.
    pbm_clear_depth(&m, 0xffff);
    s.depth_func = PBM_DEPTH_LEQUAL;
    s.depth_write = 1;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_LIST, nv, 3);   // the wall, q 0.5
    for (i = 0; i < 3; i++)
        fv[i].q = 0x10000000u;       // q 0.125
    pbm_depth_range(&m, 0, 0x4ccc);
    pbm_draw(&m, PBM_LIST, fv, 3);
    check("the weapon over the wall", colour[5 * 16 + 5], far_c);
    check("its depth scaled", labs((long)depth[5 * 16 + 5] -
                                   (long)((pbm_depth_of_q(0x10000000u) * 0x4ccdu) >> 16)) <= 1, 1);
}

// --- textures ---

static const uint8_t *grey_palette(void)
{
    static uint8_t p[768];
    int i;
    for (i = 0; i < 256; i++) {
        p[3 * i] = (uint8_t)i;
        p[3 * i + 1] = (uint8_t)(255 - i);
        p[3 * i + 2] = (uint8_t)(i * 7);
    }
    return p;
}

static void bind(int unit, uint32_t id, int clamp, int linear, int mipmap)
{
    struct pbm_bind b;
    b.id = id;
    b.clamp_s = b.clamp_t = clamp;
    b.linear = linear;
    b.mipmap = mipmap;
    pbm_bind_texture(&m, unit, &b);
}

// A channel through 5 bits and back, as 1555 keeps it.
static uint32_t five(uint32_t v)
{
    return (v >> 3) << 3 | (v >> 3) >> 2;
}

static void palette_and_alpha(void)
{
    const uint8_t *p = grey_palette();
    static const uint8_t idx[4] = { 3, 200, 255, 17 };
    struct pbm_state s = state();
    int i;

    model(2, 2);
    pbm_palette(&m, p);
    check("made", pbm_texture(&m, 7, PBM_TEX_INDEX8, 1, 1, 0, 0), PBM_OK);
    check("loaded", pbm_texture_load(&m, 7, 0, 0, 2, 2, idx), PBM_OK);
    s.tex0 = 1;
    pbm_set_state(&m, &s);
    bind(0, 7, 0, 0, 0);
    rect(0, 0, 2, 2, 1, 0, 0, 2, 2);
    for (i = 0; i < 4; i++)
        check("an index through the palette, kept 565", colour[i],
              pbm_565(p[3 * idx[i]], p[3 * idx[i] + 1], p[3 * idx[i] + 2]));
    check("255 is a colour without alpha", colour[2], pbm_565(255, 0, (255 * 7) & 255));

    // With alpha, kept 1555; 255 is see-through, and the alpha test drops it.
    pbm_clear_colour(&m, 0x1111);
    check("made with alpha", pbm_texture(&m, 8, PBM_TEX_INDEX8, 1, 1, 0, 1), PBM_OK);
    pbm_texture_load(&m, 8, 0, 0, 2, 2, idx);
    s.alpha_test = 1;
    pbm_set_state(&m, &s);
    bind(0, 8, 0, 0, 0);
    rect(0, 0, 2, 2, 1, 0, 0, 2, 2);
    check("255 dropped", colour[2], 0x1111);
    check("alpha_fail", (long)m.counts.alpha_fail, 1);
    check("kept 1555: 5 bits of green", colour[1],
          pbm_565(five(p[600]), five(p[601]), five(p[602])));

    // RGBA8888: alpha under 128 is see-through.
    {
        static const uint8_t px[16] = { 10, 20, 30, 127, 40, 50, 60, 128,
                                        0, 0, 0, 0, 255, 255, 255, 255 };
        pbm_clear_colour(&m, 0x2222);
        pbm_texture(&m, 9, PBM_TEX_RGBA8888, 1, 1, 0, 0);
        pbm_texture_load(&m, 9, 0, 0, 2, 2, px);
        bind(0, 9, 0, 0, 0);
        rect(0, 0, 2, 2, 1, 0, 0, 2, 2);
        check("alpha 127 dropped", colour[0], 0x2222);
        check("alpha 128 kept", colour[1] != 0x2222, 1);
    }

    // The alpha test passes 170 and up (GLQuake's GREATER 0.666): an
    // opaque texel times a vertex alpha of 169 or 170.
    {
        struct pbm_vertex v[4];
        int k, a;
        for (a = 169; a <= 170; a++) {
            pbm_clear_colour(&m, 0x3333);
            m.counts.alpha_fail = 0;
            s.modulate = 1;
            pbm_set_state(&m, &s);
            bind(0, 7, 0, 0, 0);
            for (k = 0; k < 4; k++)
                v[k] = rgba(vtx(k == 1 || k == 2 ? 2 : 0, k >= 2 ? 2 : 0, 1, k == 1 || k == 2 ? 2 : 0,
                                k >= 2 ? 2 : 0), 255, 255, 255, a);
            // The texel's 255 times (a + 1) / 256 is a again.
            pbm_draw(&m, PBM_FAN, v, 4);
            check(a == 169 ? "alpha 169 dropped" : "alpha 170 kept",
                  (long)m.counts.alpha_fail, a == 169 ? 4 : 0);
        }
    }

    // A mip of an alpha texture is see-through when two or more of its four are.
    {
        static const uint8_t one[4] = { 255, 1, 1, 1 }, two[4] = { 255, 255, 1, 1 };
        pbm_texture(&m, 10, PBM_TEX_INDEX8, 1, 1, 1, 1);
        pbm_texture_load(&m, 10, 0, 0, 2, 2, one);
        check("one of four see-through: opaque", m.tex[10].level[1][0] >> 15, 1);
        pbm_texture_load(&m, 10, 0, 0, 2, 2, two);
        check("two of four: see-through", m.tex[10].level[1][0] >> 15, 0);
    }
}

static void mips(void)
{
    static uint8_t lv0[16];
    struct pbm_state s = state();
    const struct pbm_texture *t;
    int i;

    model(16, 16);
    // A checker of 0 and 201: level 1 is 101 everywhere (402 + 2 over 4:
    // the 2 rounds), level 2 too.
    for (i = 0; i < 16; i++)
        lv0[i] = (uint8_t)(((i & 1) ^ ((i >> 2) & 1)) ? 201 : 0);
    pbm_texture(&m, 3, PBM_TEX_GREY8, 2, 2, 1, 0);
    pbm_texture_load(&m, 3, 0, 0, 4, 4, lv0);
    t = &m.tex[3];
    check("levels to 1x1", t->levels, 3);
    check("level 1 averages 2x2, plus 2 over 4", t->level[1][0], (201 + 201 + 2) / 4);
    check("level 2", t->level[2][0], (4 * 101 + 2) / 4);
    // A sub-rectangle loaded makes the mips again.
    {
        static const uint8_t four[4] = { 40, 40, 40, 40 };
        pbm_texture_load(&m, 3, 0, 0, 2, 2, four);
        check("level 1 after the sub-load", t->level[1][0], (4 * 40 + 2) / 4);
        check("level 0 beside it unchanged", t->level[0][3], 201);
    }
    pbm_texture_load(&m, 3, 0, 0, 4, 4, lv0);

    // The level by area: 8x8 texels on 4x4 pixels is 4 texels a pixel,
    // level 1; on 5x5 pixels, 2.56, level 0.
    s.tex0 = 1;
    pbm_set_state(&m, &s);
    bind(0, 3, 0, 0, 1);
    rect(0, 0, 4, 4, 1, 0, 0, 8, 8);
    check("4 texels a pixel: level 1", colour[0], pbm_565(100, 100, 100));
    rect(8, 8, 13, 13, 1, 0, 0, 8, 8);
    check("2.56 a pixel: level 0", colour[8 * 16 + 8] == pbm_565(0, 0, 0) ||
                                   colour[8 * 16 + 8] == pbm_565(200, 200, 200), 1);
    bind(0, 3, 0, 0, 0);
    rect(0, 0, 4, 4, 1, 0, 0, 8, 8);
    check("without MIPMAP: level 0", colour[0], pbm_565(0, 0, 0));
}

static void wrap_and_filter(void)
{
    static const uint8_t row[4] = { 0, 50, 100, 150 };
    static const uint8_t two[2] = { 0, 160 };
    struct pbm_state s = state();
    int x, ok = 1;

    model(32, 1);
    pbm_texture(&m, 4, PBM_TEX_GREY8, 2, 0, 0, 0);
    pbm_texture_load(&m, 4, 0, 0, 4, 1, row);
    s.tex0 = 1;
    pbm_set_state(&m, &s);
    // Pixel 0's centre at texel 5.5.
    bind(0, 4, 0, 0, 0);
    rect(0, 0, 1, 1, 1, 5, 0, 6, 1);
    check("repeat: texel 5 is texel 1", colour[0], pbm_565(50, 50, 50));
    bind(0, 4, 1, 0, 0);
    rect(0, 0, 1, 1, 1, 5, 0, 6, 1);
    check("clamp: texel 5 is texel 3", colour[0], pbm_565(150, 150, 150));
    rect(0, 0, 1, 1, 1, -3, 0, -2, 1);
    check("clamp: texel -3 is texel 0", colour[0], 0);

    // Bilinear across two texels, 0 and 160, held at the ends: 0 up to
    // texel 0's centre, 160 from texel 1's, and in between a step a
    // sixteenth of a texel, never back.
    pbm_texture(&m, 5, PBM_TEX_GREY8, 1, 0, 0, 0);
    pbm_texture_load(&m, 5, 0, 0, 2, 1, two);
    bind(0, 5, 1, 1, 0);
    rect(0, 0, 32, 1, 1, 0, 0, 2, 1);   // a 16th of a texel a pixel
    check("left of texel 0's centre", colour[3], 0);
    check("right of texel 1's centre", colour[28], pbm_565(160, 160, 160));
    // Pixel 15's centre is at s 0.96875, 7.5 sixteenths past texel 0's
    // centre: the fraction's 4 bits are 7, so 160 * 7 / 16 = 70.
    check("between: 7 sixteenths", colour[15], pbm_565(70, 70, 70));
    for (x = 1; x < 32; x++)
        if (r5(colour[x]) < r5(colour[x - 1]))
            ok = 0;
    check_true("never back", ok);
}

// A floor running away from the eye: q falls from 1 on the left to 0.25
// on the right, s runs 0-64 texels. Each pixel's texel must be the
// perspective-correct one, sum(l q s) / sum(l q) of its barycentric l,
// except within a 16th of a texel of a boundary, where the 12.4
// coordinates' rounding may fall either way.
static void perspective(void)
{
    static uint8_t ramp[64];
    struct pbm_state s = state();
    struct pbm_vertex v[3];
    int i, x, y, bad = 0, tried = 0;

    model(64, 16);
    for (i = 0; i < 64; i++)
        ramp[i] = (uint8_t)(i * 4);
    pbm_texture(&m, 6, PBM_TEX_GREY8, 6, 0, 0, 0);
    pbm_texture_load(&m, 6, 0, 0, 64, 1, ramp);
    s.tex0 = 1;
    pbm_set_state(&m, &s);
    bind(0, 6, 1, 0, 0);
    v[0] = vtx(0, 0, 1, 0, 0);
    v[1] = vtx(64, 0, 0.25, 64, 0);
    v[2] = vtx(0, 64, 1, 0, 0);
    pbm_draw(&m, PBM_LIST, v, 3);
    for (y = 0; y < 16; y++)
        for (x = 0; x < 64 - y; x++) {
            double px = x + 0.5, py = y + 0.5;
            // Barycentric: l1 along x, l2 along y, l0 the rest.
            double l1 = px / 64, l2 = py / 64, l0 = 1 - l1 - l2;
            double want = (l1 * 0.25 * 64) / (l0 * 1 + l1 * 0.25 + l2 * 1);
            double frac = want - floor(want);
            uint16_t c = colour[y * 64 + x];
            if (l0 <= 0 || frac < 1 / 16.0 || frac > 15 / 16.0)
                continue;
            tried++;
            if (c != pbm_565((uint32_t)floor(want) * 4, (uint32_t)floor(want) * 4,
                             (uint32_t)floor(want) * 4))
                bad++;
        }
    check("perspective-correct texels", bad, 0);
    check_true("pixels tried", tried > 400);
}

// --- combine and blend ---

static void combine(void)
{
    static const uint8_t white = 255, lm = 64;
    struct pbm_state s = state();
    struct pbm_vertex v[4];
    int i;

    model(2, 2);
    pbm_texture(&m, 1, PBM_TEX_GREY8, 0, 0, 0, 0);
    pbm_texture_load(&m, 1, 0, 0, 1, 1, &white);
    pbm_texture(&m, 2, PBM_TEX_GREY8, 0, 0, 0, 0);
    pbm_texture_load(&m, 2, 0, 0, 1, 1, &lm);
    bind(0, 1, 0, 0, 0);
    bind(1, 2, 0, 1, 0);
    v[0] = rgba(vtx(0, 0, 1, 0, 0), 128, 64, 32, 255);
    v[1] = rgba(vtx(2, 0, 1, 1, 0), 128, 64, 32, 255);
    v[2] = rgba(vtx(2, 2, 1, 1, 1), 128, 64, 32, 255);
    v[3] = rgba(vtx(0, 2, 1, 0, 1), 128, 64, 32, 255);

    s.tex0 = 1;
    s.modulate = 1;
    s.smooth = 1;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("modulate: 255 x 128 is 128", colour[0], pbm_565(128, 64, 32));
    s.modulate = 0;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("replace: the texel", colour[0], 0xffff);
    s.tex0 = 0;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("unit 0 off: the colour", colour[0], pbm_565(128, 64, 32));
    s.tex0 = 1;
    s.tex1 = 1;
    pbm_set_state(&m, &s);
    for (i = 0; i < 4; i++)
        v[i] = st2(v[i], 0.5, 0.5);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("the lightmap: 255 x (256 - 64) / 256", colour[0], pbm_565(191, 191, 191));
    // Flat shading takes the triangle's first vertex.
    v[0] = rgba(v[0], 0, 0, 255, 255);
    s.tex0 = s.tex1 = 0;
    s.smooth = 0;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("flat: the first vertex", colour[3], pbm_565(0, 0, 255));
}

static void blends(void)
{
    struct pbm_state s = state();
    struct pbm_vertex v[4];
    uint16_t dst = pbm_565(200, 100, 40);
    // dst as 8 bits a channel, after 565.
    uint32_t dr = 206, dg = 101, db = 41;
    int i;

    model(2, 2);
    for (i = 0; i < 4; i++)
        v[i] = rgba(vtx(i == 1 || i == 2 ? 2 : 0, i >= 2 ? 2 : 0, 1, 0, 0), 64, 128, 255, 128);
    s.smooth = 1;
    s.blend = PBM_BLEND_ALPHA;
    pbm_set_state(&m, &s);
    pbm_clear_colour(&m, dst);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("alpha 128: weights 129 and 127", colour[0],
          pbm_565((64 * 129 + dr * 127) >> 8, (128 * 129 + dg * 127) >> 8,
                  (255 * 129 + db * 127) >> 8));
    s.blend = PBM_BLEND_MULINV;
    pbm_set_state(&m, &s);
    pbm_clear_colour(&m, dst);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("mulinv: dst (256 - w(src))", colour[0],
          pbm_565((dr * (256 - 64)) >> 8, (dg * (256 - 129)) >> 8, (db * 0) >> 8));
    s.blend = PBM_BLEND_ADD;
    pbm_set_state(&m, &s);
    pbm_clear_colour(&m, dst);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("add, held at 255", colour[0], pbm_565(255, 229, 255));
    check("blended counted", (long)m.counts.blended, 12);
    for (i = 0; i < 4; i++)
        v[i].a = 255;
    s.blend = PBM_BLEND_ALPHA;
    pbm_set_state(&m, &s);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("alpha 255 is the source", colour[0], pbm_565(64, 128, 255));
    for (i = 0; i < 4; i++)
        v[i].a = 0;
    pbm_clear_colour(&m, dst);
    pbm_draw(&m, PBM_FAN, v, 4);
    check("alpha 0 is the picture", colour[0], dst);
}

// --- the scene ---

// A point seen by a camera at the origin looking down +z, y up, 90
// degrees across: the screen position, q as near / z (near 4), and its
// texel positions.
static struct pbm_vertex eye(double x, double y, double z, double s, double t)
{
    return vtx(320 + 320 * x / z, 240 - 320 * y / z, 4 / z, s, t);
}

// A quad in the world, as a fan, its lightmap over all of it (16x16
// texels, the centres of the edge ones at its corners).
static void quad(const double p[4][3], const double st[4][2], const uint8_t *rgb)
{
    static const double l[4][2] = { { 0.5, 0.5 }, { 15.5, 0.5 }, { 15.5, 15.5 }, { 0.5, 15.5 } };
    struct pbm_vertex v[4];
    int i;
    for (i = 0; i < 4; i++) {
        v[i] = st2(eye(p[i][0], p[i][1], p[i][2], st[i][0], st[i][1]), l[i][0], l[i][1]);
        if (rgb)
            v[i] = rgba(v[i], rgb[3 * i], rgb[3 * i + 1], rgb[3 * i + 2], 255);
    }
    pbm_draw(&m, PBM_FAN, v, 4);
}

// Every rule at once, the way Quake uses them: a corridor in perspective,
// its brick mipmapped and lit by a lightmap; a box sunk into the floor
// (depth); water on the floor (alpha blend); a light flash (add); a HUD
// picture with see-through texels (alpha test); and the weapon in its own
// depth range, in front of all of it.
static void scene(const char *png)
{
    static uint8_t brick[64 * 64], light[16 * 16], hud[16 * 16];
    static uint8_t pal[768];
    static const double floor_[4][3] = { { -40, -32, 512 }, { 40, -32, 512 }, { 40, -32, 8 }, { -40, -32, 8 } };
    static const double floor_st[4][2] = { { 0, 504 }, { 80, 504 }, { 80, 0 }, { 0, 0 } };
    static const double left[4][3] = { { -40, -32, 512 }, { -40, -32, 8 }, { -40, 64, 8 }, { -40, 64, 512 } };
    static const double right[4][3] = { { 40, -32, 8 }, { 40, -32, 512 }, { 40, 64, 512 }, { 40, 64, 8 } };
    static const double side_st[4][2] = { { 0, 96 }, { 504, 96 }, { 504, 0 }, { 0, 0 } };
    static const double back[4][3] = { { -40, 64, 512 }, { 40, 64, 512 }, { 40, -32, 512 }, { -40, -32, 512 } };
    static const double back_st[4][2] = { { 0, 0 }, { 80, 0 }, { 80, 96 }, { 0, 96 } };
    static const double front[4][3] = { { 0, -8, 100 }, { 24, -8, 100 }, { 24, -40, 100 }, { 0, -40, 100 } };
    static const double side[4][3] = { { 0, -8, 124 }, { 0, -8, 100 }, { 0, -40, 100 }, { 0, -40, 124 } };
    static const double top[4][3] = { { 0, -8, 124 }, { 24, -8, 124 }, { 24, -8, 100 }, { 0, -8, 100 } };
    static const double water[4][3] = { { -40, -31, 20 }, { 0, -31, 20 }, { 0, -31, 80 }, { -40, -31, 80 } };
    static const double gun[4][3] = { { 6, -14, 9 }, { 10, -14, 9 }, { 9, -9, 16 }, { 7, -9, 16 } };
    static const double none[4][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
    static const uint8_t box_front[12] = { 255, 200, 0, 220, 160, 0, 160, 80, 0, 200, 120, 0 };
    static const uint8_t box_side[12] = { 120, 90, 0, 150, 110, 0, 90, 50, 0, 70, 40, 0 };
    static const uint8_t box_top[12] = { 255, 240, 120, 255, 240, 120, 255, 220, 80, 255, 220, 80 };
    static const uint8_t blue[12] = { 40, 80, 200, 40, 120, 220, 40, 80, 200, 20, 60, 160 };
    static const uint8_t steel[12] = { 90, 90, 100, 60, 60, 70, 200, 200, 210, 150, 150, 160 };
    struct pbm_state s;
    struct pbm_vertex v[14];
    int i, x, y;
    uint32_t crc;

    model(640, 480);
    for (i = 0; i < 256; i++) {
        pal[3 * i] = (uint8_t)(i < 128 ? i * 2 : 255);
        pal[3 * i + 1] = (uint8_t)(i < 128 ? i : (i - 128) * 2);
        pal[3 * i + 2] = (uint8_t)(i < 192 ? i / 3 : (i - 192) * 4);
    }
    pbm_palette(&m, pal);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int mortar = (y % 16) < 2 || ((x + (y / 16 % 2) * 16) % 32) < 2;
            brick[y * 64 + x] = (uint8_t)(mortar ? 60 : 100 + ((x * 7 + y * 13) % 20));
        }
    // The lightmap: 255 - light, as GLQuake keeps it; a light near one corner.
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            double d = sqrt((x - 4.0) * (x - 4.0) + (y - 3.0) * (y - 3.0));
            light[y * 16 + x] = (uint8_t)(d > 14 ? 220 : 220 * d / 14);
        }
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            int ring = abs((x - 8) * (x - 8) + (y - 8) * (y - 8) - 30) < 10;
            hud[y * 16 + x] = (uint8_t)(ring ? 200 : 255);
        }
    pbm_texture(&m, 1, PBM_TEX_INDEX8, 6, 6, 1, 0);
    pbm_texture_load(&m, 1, 0, 0, 64, 64, brick);
    pbm_texture(&m, 2, PBM_TEX_GREY8, 4, 4, 0, 0);
    pbm_texture_load(&m, 2, 0, 0, 16, 16, light);
    pbm_texture(&m, 3, PBM_TEX_INDEX8, 4, 4, 0, 1);
    pbm_texture_load(&m, 3, 0, 0, 16, 16, hud);

    pbm_clear_colour(&m, pbm_565(20, 20, 40));
    pbm_clear_depth(&m, 0xffff);
    s = state();
    s.depth_func = PBM_DEPTH_LEQUAL;
    s.depth_write = 1;
    s.tex0 = s.tex1 = 1;
    s.cull = PBM_CULL_CCW;   // the corridor's faces wind clockwise toward the eye
    pbm_set_state(&m, &s);
    bind(0, 1, 0, 0, 1);
    bind(1, 2, 1, 1, 0);
    quad(floor_, floor_st, 0);
    quad(left, side_st, 0);
    quad(right, side_st, 0);
    quad(back, back_st, 0);

    // The box, gouraud and untextured.
    s.tex0 = s.tex1 = 0;
    s.smooth = 1;
    pbm_set_state(&m, &s);
    quad(front, none, box_front);
    quad(side, none, box_side);
    quad(top, none, box_top);

    // Water: half over the floor, depth tested but not written.
    s.blend = PBM_BLEND_ALPHA;
    s.depth_write = 0;
    s.cull = PBM_CULL_NONE;
    pbm_set_state(&m, &s);
    for (i = 0; i < 4; i++)
        v[i] = rgba(eye(water[i][0], water[i][1], water[i][2], 0, 0),
                    blue[3 * i], blue[3 * i + 1], blue[3 * i + 2], 128);
    pbm_draw(&m, PBM_FAN, v, 4);

    // A light flash on the screen, added: bright in the middle, nothing
    // at the rim.
    s.blend = PBM_BLEND_ADD;
    s.depth_func = PBM_DEPTH_ALWAYS;
    pbm_set_state(&m, &s);
    // (A 12-gon from a table, not cos and sin: libm's last bits differ
    // between the Mac and Linux, and the CRC must not.)
    v[0] = rgba(vtx(500, 150, 1, 0, 0), 140, 90, 20, 255);
    for (i = 0; i < 12; i++) {
        static const double c[12] = { 50, 43.25, 25, 0, -25, -43.25, -50, -43.25, -25, 0, 25, 43.25 };
        v[i + 1] = rgba(vtx(500 + c[i], 150 + c[(i + 9) % 12], 1, 0, 0), 0, 0, 0, 255);
    }
    v[13] = v[1];
    pbm_draw(&m, PBM_FAN, v, 14);

    // The weapon, in 0 to 0.3 of the range.
    s = state();
    s.depth_func = PBM_DEPTH_LEQUAL;
    s.depth_write = 1;
    s.smooth = 1;
    pbm_set_state(&m, &s);
    pbm_depth_range(&m, 0, 0x4ccc);
    quad(gun, none, steel);
    pbm_depth_range(&m, 0, 0xffff);

    // The HUD: a picture 4x, its 255s see-through.
    s = state();
    s.tex0 = 1;
    s.alpha_test = 1;
    pbm_set_state(&m, &s);
    bind(0, 3, 1, 0, 0);
    rect(16, 400, 80, 464, 1, 0, 0, 16, 16);

    if (png && png_write_565(png, colour, 640, 480) != 0)
        printf("FAIL: couldn't write %s\n", png);
    crc = png_crc32(0, (const uint8_t *)colour, sizeof *colour * 640u * 480u);
    // Change this only on purpose, with the picture looked at: it holds
    // every rule at once (docs/PB3D.md, "Changing the rules").
    check("the scene's CRC", (long)crc, (long)0xd8cd7614u);
    printf("test_pbmodel: scene %08x, %u triangles, %u culled, %u pixels covered, %u drawn (%s)\n",
           crc, m.counts.tris, m.counts.culled, m.counts.covered, m.counts.drawn,
           png ? png : "no picture");
}

int main(int argc, char **argv)
{
    partition();
    top_left();
    culling();
    guard_band();
    depth_format();
    depth_test();
    palette_and_alpha();
    mips();
    wrap_and_filter();
    perspective();
    combine();
    blends();
    scene(argc > 1 ? argv[1] : 0);
    free(colour);
    free(depth);
    free(pool);
    if (failed) {
        printf("test_pbmodel: %d of %d checks failed\n", failed, ran);
        return 1;
    }
    printf("test_pbmodel: %d checks OK\n", ran);
    return 0;
}
