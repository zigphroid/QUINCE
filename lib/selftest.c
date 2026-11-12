// The checks QUINCE /SELFTEST and the host test share (quince_core.h). A
// new check goes in its own function, listed in checks[]. No libc here:
// the same bytes must mean the same thing on both sides, so even the
// string compare is our own.
#include "quince_core.h"
#include "qbench.h"
#include "pbgl.h"
#include "pb3d.h"
#include "GL/gl.h"

static void (*report)(const char *what, const char *got, const char *want);
static int ran, failed;

static int same(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void check_str(const char *what, const char *got, const char *want)
{
    ran++;
    if (!same(got, want)) {
        failed++;
        report(what, got, want);
    }
}

static void greeting(void)
{
    char buf[80], small[12];

    check_str("a name", quince_greeting(buf, sizeof buf, "JAM"), "Hello from JAM.");
    check_str("an empty name", quince_greeting(buf, sizeof buf, ""), "Hello from a jar.");
    check_str("no name", quince_greeting(buf, sizeof buf, 0), "Hello from a jar.");
    check_str("a tight buffer cuts cleanly", quince_greeting(small, sizeof small, "JAM"),
              "Hello from ");
}

// A number as text, for check_num's report.
static const char *num(char *buf, long v)
{
    char t[24];
    int n = 0, i = 0;
    unsigned long u = v < 0 ? 0ul - (unsigned long)v : (unsigned long)v;
    do
        t[n++] = (char)('0' + u % 10);
    while ((u /= 10) && n < 22);
    if (v < 0)
        buf[i++] = '-';
    while (n)
        buf[i++] = t[--n];
    buf[i] = 0;
    return buf;
}

static void check_num(const char *what, long got, long want)
{
    char g[24], w[24];
    ran++;
    if (got != want) {
        failed++;
        report(what, num(g, got), num(w, want));
    }
}

// The benchmark's words (lib/qbargs.c), as QUINCE.c and the PC's main
// hand them over.
static int parse(struct qbench_run *run, const char *a, const char *b, const char *c,
                 const char *d, const char *e, const char *f)
{
    char *w[6];
    int n = 0;
    const char *in[6] = { a, b, c, d, e, f };
    for (int i = 0; i < 6 && in[i]; i++)
        w[n++] = (char *)in[i];
    return qbench_parse(n, w, run);
}

static void bench_words(void)
{
    struct qbench_run r;

    check_num("timedemo parses", parse(&r, "Q", "timedemo", "DEMO1", 0, 0, 0), 0);
    check_num("timedemo's kind", r.kind, QBENCH_TIMEDEMO);
    check_str("the demo's name, lowered for the pak", r.name, "demo1");
    check_str("the base directory", r.basedir, "Q");
    check_num("no -mem, the default hunk", r.memsize, 0);
    check_num("server parses", parse(&r, "Q", "server", "E1M1", "300", 0, 0), 0);
    check_num("server's kind", r.kind, QBENCH_SERVER);
    check_str("the map's name, lowered", r.name, "e1m1");
    check_num("server's frames", r.frames, 300);
    check_num("-mem in KB", parse(&r, "Q", "server", "e1m1", "5", "-mem", "6144"), 0);
    check_num("-mem's bytes", r.memsize, 6144 * 1024);
    check_num("-mem 0 refused", parse(&r, "Q", "timedemo", "d", "-mem", "0", 0), -1);
    check_num("-mem past 64 MB refused", parse(&r, "Q", "timedemo", "d", "-mem", "65537", 0), -1);
    check_num("-mem 64 MB taken", parse(&r, "Q", "timedemo", "d", "-mem", "65536", 0), 0);
    check_num("-mem with no number", parse(&r, "Q", "timedemo", "d", "-mem", 0, 0), -1);
    check_num("-mem with letters", parse(&r, "Q", "timedemo", "d", "-mem", "5k", 0), -1);
    check_num("a huge number refused", parse(&r, "Q", "server", "m", "99999999999", 0, 0), -1);
    check_num("server with no frames", parse(&r, "Q", "server", "e1m1", 0, 0, 0), -1);
    check_num("server with 0 frames", parse(&r, "Q", "server", "e1m1", "0", 0, 0), -1);
    check_num("an unknown benchmark", parse(&r, "Q", "bench", "e1m1", 0, 0, 0), -1);
    check_num("too many words", parse(&r, "Q", "timedemo", "d", "x", "y", 0), -1);
    check_num("a name too long",
              parse(&r, "Q", "timedemo",
                    "a123456789b123456789c123456789d123456789e123456789f123456789g123", 0, 0, 0),
              -1);
}

// PB's 3D commands (lib/pb3d.c): the words they are, as the C model
// (lib/pbmodel.c) and later PB will read them.
static void check_words(const char *what, const uint16_t *got, uint32_t n,
                        const uint16_t *want, uint32_t m)
{
    char g[24], w[24];
    uint32_t i = 0;
    ran++;
    while (i < n && i < m && got[i] == want[i])
        i++;
    if (i == n && i == m)
        return;
    failed++;
    // Where they part: the word's place and the two values, or the lengths.
    if (i < n && i < m) {
        num(g, (long)got[i] + 100000L * (long)(i + 1));
        num(w, (long)want[i] + 100000L * (long)(i + 1));
    } else {
        num(g, (long)n);
        num(w, (long)m);
    }
    report(what, g, w);
}

static void pb3d_fixed(void)
{
    volatile float zero = 0.0f;   // so no compiler folds 0 / 0 away
    uint32_t q;
    check_num("12.4: one pixel", pb3d_fix_12_4(1.0f), 16);
    check_num("12.4: a half step rounds away from 0", pb3d_fix_12_4(0.03125f), 1);
    check_num("12.4: and below 0", pb3d_fix_12_4(-1.03125f), -17);
    check_num("12.4: just under a half step", pb3d_fix_12_4(0.03f), 0);
    // The float just under a half step: adding a half rounds the sum up
    // to a whole one, which M2's fix got wrong; roundf doesn't.
    check_num("12.4: the float just under a half step", pb3d_fix_12_4(0.49999997f * 0.0625f), 0);
    check_num("12.4: held at the guard band's right", pb3d_fix_12_4(3000.0f), 32767);
    check_num("12.4: and its left", pb3d_fix_12_4(-3000.0f), -32768);
    check_num("12.4: NaN is 0", pb3d_fix_12_4(zero / zero), 0);
    check_num("10.6: a texel and a half", pb3d_fix_10_6(1.5f), 96);
    check_num("10.6: held at 512", pb3d_fix_10_6(600.0f), 32767);
    q = pb3d_fix_q(1.0f);
    check_num("q: 1.0 at the near plane, high half", (long)(q >> 16), 0x8000);
    check_num("q: and low half", (long)(q & 0xffffu), 0);
    q = pb3d_fix_q(0.25f);
    check_num("q: a quarter", (long)(q >> 16), 0x2000);
    check_num("q: 0 is 0", (long)pb3d_fix_q(0.0f), 0);
    check_num("q: behind the eye is 0", (long)pb3d_fix_q(-1.0f), 0);
    q = pb3d_fix_q(3.0f);
    check_num("q: held at the top, high half", (long)(q >> 16), 0xffff);
    check_num("q: and low half", (long)(q & 0xffffu), 0xffff);
    // q * 2^31 in [2^23, 2^24) has an ulp of 1, so M2's v + 0.5 was a tie
    // there and rounded odd values up to even; roundf leaves them.
    check_num("q: an odd value past 2^23 stays odd",
              (long)pb3d_fix_q(8388609.0f / 2147483648.0f), 8388609);
}

static void pb3d_commands(void)
{
    static uint16_t w[64];
    struct pb3d_buf b;
    struct pb3d_vertex v[3] = {
        { 10.0f, 20.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0 },
        { 30.5f, 20.0f, 0.5f, 64.0f, 0.0f, 4.0f, 0.0f, 0, 0, 0, 0 },
        { -5.0f, 40.25f, 0.25f, 0.0f, 32.0f, 0.0f, 2.5f, 0, 0, 0, 0 },
    };
    // A world fan: depth cleared, the state, both textures, three vertices.
    static const uint16_t world[] = {
        0x0f01,                     // DEPTH, clear
        0x1000, 0x01c5,             // STATE: lequal, write, modulate, both units
        0x1100, 7, 0x0008,          // TEXTURE 0: id 7, mipmapped
        0x1101, 300, 0x0007,        // TEXTURE 1: id 300, clamped, bilinear
        0x121e, 3,                  // TRIS: fan, q | st | st2
        160, 320, 0x0000, 0x8000, 0, 0, 0, 0,
        488, 320, 0x0000, 0x4000, 1024, 0, 256, 0,
        0xffb0, 644, 0x0000, 0x2000, 0, 512, 0, 160,
    };
    // A model's strip vertex, grey, and a particle's list, RGBA.
    static const uint16_t model[] = {
        0x122d, 3,                  // TRIS: strip, q | st | grey
        16, 16, 0, 0x8000, 0, 0, 200,
        16, 16, 0, 0x8000, 0, 0, 200,
        16, 16, 0, 0x8000, 0, 0, 200,
        0x1300, 0x0100, 0xfe00,     // DEPTHRANGE: 0x100 to 0xfe00
        0x124c, 3,                  // TRIS: list, q | st | rgba
        16, 16, 0, 0x8000, 0, 0, 0x2211, 0x4433,
        16, 16, 0, 0x8000, 0, 0, 0x2211, 0x4433,
        16, 16, 0, 0x8000, 0, 0, 0x2211, 0x4433,
    };
    struct pb3d_vertex one = { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 200, 0, 0, 0 };
    struct pb3d_vertex col = { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0x11, 0x22, 0x33, 0x44 };

    pb3d_init(&b, w, 64);
    pb3d_depth(&b, 1);
    pb3d_state(&b, PB3D_S_DEPTH_LEQUAL | PB3D_S_DEPTH_WRITE | PB3D_S_MODULATE |
                   PB3D_S_TEX0 | PB3D_S_TEX1);
    pb3d_texture(&b, 0, 7, PB3D_T_MIPMAP);
    pb3d_texture(&b, 1, 300, PB3D_T_CLAMP_S | PB3D_T_CLAMP_T | PB3D_T_LINEAR);
    check_num("a world fan opens", pb3d_tris(&b, PB3D_KIND_FAN,
                                             PB3D_F_Q | PB3D_F_ST | PB3D_F_ST2, 3), 0);
    for (int i = 0; i < 3; i++)
        pb3d_vertex(&b, &v[i]);
    check_words("a world fan's words", b.w, b.n, world, sizeof world / sizeof world[0]);
    check_num("nothing dropped", (long)b.dropped, 0);
    check_num("each word read back as one command",
              (long)(pb3d_cmd_words(w, b.n) + pb3d_cmd_words(w + 1, b.n - 1) +
                     pb3d_cmd_words(w + 3, b.n - 3) + pb3d_cmd_words(w + 6, b.n - 6) +
                     pb3d_cmd_words(w + 9, b.n - 9)),
              (long)b.n);

    pb3d_clear(&b);
    pb3d_tris(&b, PB3D_KIND_STRIP, PB3D_F_Q | PB3D_F_ST | PB3D_F_GREY, 3);
    for (int i = 0; i < 3; i++)
        pb3d_vertex(&b, &one);
    pb3d_depthrange(&b, 0x100, 0xfe00);
    pb3d_tris(&b, PB3D_KIND_LIST, PB3D_F_Q | PB3D_F_ST | PB3D_F_RGBA, 3);
    for (int i = 0; i < 3; i++)
        pb3d_vertex(&b, &col);
    check_words("grey and RGBA vertices", b.w, b.n, model, sizeof model / sizeof model[0]);

    check_num("a world vertex is 8 words", PB3D_VERTEX_WORDS(PB3D_F_Q | PB3D_F_ST | PB3D_F_ST2), 8);
    check_num("a model's is 7", PB3D_VERTEX_WORDS(PB3D_F_Q | PB3D_F_ST | PB3D_F_GREY), 7);
    check_num("a HUD picture's is 4", PB3D_VERTEX_WORDS(PB3D_F_ST), 4);
    check_num("a flat colour's is 4", PB3D_VERTEX_WORDS(PB3D_F_RGBA), 4);

    // Commands the reader refuses.
    static const uint16_t bad_op[] = { 0x0e00 }, bad_kind[] = { 0x1203, 0 },
        bad_colour[] = { 0x1260, 0 }, short_tris[] = { 0x1208, 1, 0, 0, 0, 0 },
        bad_unit[] = { 0x1102, 1, 0 }, bad_depth[] = { 0x0f02 }, empty[] = { 0x1208, 0 },
        state[] = { 0x1000, 0 };
    check_num("POLYLINE isn't ours", (long)pb3d_cmd_words(bad_op, 1), 0);
    check_num("kind 3 refused", (long)pb3d_cmd_words(bad_kind, 2), 0);
    check_num("colour 0x60 refused", (long)pb3d_cmd_words(bad_colour, 2), 0);
    check_num("a TRIS cut short refused", (long)pb3d_cmd_words(short_tris, 5), 0);
    check_num("and whole, taken", (long)pb3d_cmd_words(short_tris, 6), 6);
    check_num("unit 2 refused", (long)pb3d_cmd_words(bad_unit, 3), 0);
    check_num("a DEPTH bit past clear refused", (long)pb3d_cmd_words(bad_depth, 1), 0);
    check_num("a TRIS of none is two words", (long)pb3d_cmd_words(empty, 2), 2);
    check_num("a STATE cut short refused", (long)pb3d_cmd_words(state, 1), 0);
    check_num("and whole, two words", (long)pb3d_cmd_words(state, 2), 2);
    static const uint16_t clip[] = { 0x1400, 0, 0, 640, 365 }, bad_clip[] = { 0x1401, 0, 0, 1, 1 };
    pb3d_init(&b, w, 64);
    pb3d_clip(&b, 0, 0, 640, 365);
    check_words("CLIP's words", b.w, b.n, clip, 5);
    check_num("a CLIP is five words", (long)pb3d_cmd_words(clip, 5), 5);
    check_num("a CLIP cut short refused", (long)pb3d_cmd_words(clip, 4), 0);
    check_num("a CLIP with an argument refused", (long)pb3d_cmd_words(bad_clip, 5), 0);

    // A full buffer: whole commands or none.
    pb3d_init(&b, w, 5);
    pb3d_state(&b, 0);
    pb3d_texture(&b, 0, 1, 0);
    pb3d_depth(&b, 0);
    check_num("a command past the end is dropped", (long)b.dropped, 1);
    check_num("and the buffer holds the two that fit", (long)b.n, 5);
    pb3d_init(&b, w, 64);
    check_num("a TRIS of two refused", pb3d_tris(&b, PB3D_KIND_FAN, PB3D_F_ST, 2), -1);
    pb3d_vertex(&b, &one);
    check_num("and its vertices write nothing", (long)b.n, 0);
    check_num("a TRIS too big for the room refused",
              pb3d_tris(&b, PB3D_KIND_LIST, PB3D_F_ST, 20), -1);
    check_num("so two dropped, nothing written", (long)(b.dropped * 100 + b.n), 200);
    pb3d_tris(&b, PB3D_KIND_FAN, PB3D_F_ST, 3);
    pb3d_state(&b, 0);
    check_num("a command inside an open TRIS is dropped", (long)b.dropped, 3);
}

// The GL layer (lib/pbgl.c): what PB is sent for GL calls, read back
// from a sink that keeps the last frame and the last texture.
// In PSRAM on the board, as lib/qproto.h's QPSRAM (which needs id's
// types, so it isn't included here): 4 KB only the selftest uses, which
// GCC's build, all its code in SRAM, couldn't spare (LOG 2026-11-12).
#if defined(__CANNER__) || defined(__arm__)
#define SELFTEST_PSRAM __attribute__((section(".pbss")))
#else
#define SELFTEST_PSRAM
#endif
static SELFTEST_PSRAM uint16_t gl_words[1024], gl_frame[1024];
static uint8_t gl_textures[16 * PBGL_TEXTURE_BYTES], gl_scratch[256], gl_tex[256];
static uint32_t gl_seq, gl_tex_seq, gl_nsubs, gl_sub_seq;
static uint32_t gl_nframe, gl_tex_id, gl_tex_format, gl_tex_flags, gl_tex_w, gl_tex_h;

static void gl_keep_frame(const uint16_t *w, uint32_t n)
{
    gl_nframe = n;
    for (uint32_t i = 0; i < n && i < 1024; i++)
        gl_frame[i] = w[i];
}

static void gl_keep_tex(uint32_t id, uint32_t format, uint32_t flags, uint32_t w, uint32_t h,
                        const void *texels)
{
    const uint8_t *t = texels;
    uint32_t n = w * h * (format == PB3D_TEX_RGBA8888 ? 4u : 1u);
    gl_tex_seq = ++gl_seq;
    gl_tex_id = id;
    gl_tex_format = format;
    gl_tex_flags = flags;
    gl_tex_w = w;
    gl_tex_h = h;
    for (uint32_t i = 0; i < n && i < sizeof gl_tex; i++)
        gl_tex[i] = t[i];
}

// The texture updates sent, in order with the textures made: gl_seq
// numbers each call, so a check can say which went first.
static uint32_t gl_sub_id, gl_sub_y, gl_sub_h, gl_sub_first, gl_sub_last;

static void gl_keep_sub(uint32_t id, uint32_t format, uint32_t x, uint32_t y, uint32_t w,
                        uint32_t h, const void *texels)
{
    const uint8_t *t = texels;
    (void)format; (void)x;
    gl_nsubs++;
    gl_sub_seq = ++gl_seq;
    gl_sub_id = id;
    gl_sub_y = y;
    gl_sub_h = h;
    gl_sub_first = t[0];
    gl_sub_last = t[w * h - 1];
}

static const struct pbgl_sink gl_sink = { 0, gl_keep_tex, gl_keep_sub, gl_keep_frame };

static void gl_start(uint32_t nwords)
{
    struct pbgl_mem m;
    m.words = gl_words;
    m.nwords = nwords;
    m.textures = gl_textures;
    m.max_textures = 16;
    m.scratch = gl_scratch;
    m.nscratch = sizeof gl_scratch;
    pbgl_init(&m, 640, 480, &gl_sink);
    gl_nframe = 0;
    gl_nsubs = 0;
}

// The frame's words from word k, against want's n.
static void gl_check(const char *what, uint32_t k, const uint16_t *want, uint32_t n)
{
    pbgl_end_frame();
    // The n words from k, or all there are if fewer.
    check_words(what, gl_frame + (gl_nframe >= k ? k : 0),
                gl_nframe >= k + n ? n : gl_nframe >= k ? gl_nframe - k : 0, want, n);
}

static void ortho(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 480, 0, -1, 1);   // GL_Set2D's: y down, as the screen
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void frustum(void)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1, 1, -1, 1, 1, 10);   // near 1: q = 1 / w
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void gl_screen(void)
{
    gl_start(1024);
    ortho();
    glBegin(GL_TRIANGLES);
    glVertex2f(10, 20);
    glVertex2f(30.5f, 20);
    glPushMatrix();               // the matrix is taken as each vertex comes
    glTranslatef(100, 0, 0);
    glVertex2f(10, 40);
    glPopMatrix();
    glEnd();
    // Untextured, white: the state is smooth shading alone, and no colour.
    static const uint16_t flat[] = {
        0x1000, 0x0800,               // STATE: smooth
        0x1200, 3,                    // TRIS: a list, x and y only
        160, 320, 488, 320, 1760, 640,
    };
    gl_check("glOrtho: pixels on the surface", 0, flat, sizeof flat / sizeof flat[0]);
    check_num("a frame counted", (long)pbgl_total.frames, 1);

    frustum();
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, 0, -2);           // on the axis: the middle, w 2, q a half
    glVertex3f(1, -1, -2);          // x 480, y 360 (y down)
    glVertex3f(-1, 1, -4);          // w 4: x 240, y 180, q a quarter
    glEnd();
    static const uint16_t persp[] = {
        0x1000, 0x0800,
        0x1226, 3,                    // TRIS: fan, q, grey
        5120, 3840, 0, 0x4000, 128,
        7680, 5760, 0, 0x4000, 128,
        3840, 2880, 0, 0x2000, 128,
    };
    gl_check("glFrustum: the divide, q and a grey colour", 0, persp,
             sizeof persp / sizeof persp[0]);
}

static void gl_prims(void)
{
    static const uint16_t quads[] = { 0x1202, 4 };   // TRIS: a fan of 4, x and y
    gl_start(1024);
    ortho();
    glBegin(GL_QUADS);
    for (int i = 0; i < 8; i++)
        glVertex2f((float)(i & 1) * 10, (float)(i >> 1 & 1) * 10);
    glEnd();
    gl_check("GL_QUADS: each quad a fan", 2, quads, 2);
    check_num("two TRIS for two quads", (long)pbgl_total.tris_cmds, 2);
    check_num("and GL's count, as M1's", (long)pbgl_total.tris, 4);

    glBegin(GL_POLYGON);
    glVertex2f(0, 0);
    glVertex2f(10, 0);
    glEnd();
    pbgl_end_frame();
    check_num("a polygon of two sends nothing", (long)gl_nframe, 0);

    // The same state twice: sent once.
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++)
        glVertex2f((float)i, (float)(i & 1) * 10);
    glEnd();
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 3; i++)
        glVertex2f((float)i, 5);
    glEnd();
    pbgl_end_frame();
    check_num("state sent only when it changes", (long)pbgl_total.state_cmds, 2);
    check_num("and again in the next frame", (long)gl_frame[0], 0x1000);

    // Culled: wholly left of the screen, or behind the eye.
    pbgl_reset();
    glBegin(GL_TRIANGLES);
    glVertex2f(-50, 0); glVertex2f(-10, 0); glVertex2f(-20, 30);
    glEnd();
    frustum();
    glBegin(GL_TRIANGLES);
    glVertex3f(0, 0, 1); glVertex3f(1, 0, 2); glVertex3f(0, 1, 3);
    glEnd();
    pbgl_end_frame();
    check_num("off the screen or behind the eye: culled", (long)pbgl_total.culled, 2);
    check_num("and nothing sent", (long)gl_nframe, 0);

    // A fan longer than the layer keeps: sent in pieces.
    ortho();
    pbgl_reset();
    glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i < 300; i++)
        glVertex2f((float)(i % 50), (float)(i / 50));
    glEnd();
    check_num("a fan of 300 is split once", (long)pbgl_total.split, 1);
    check_num("its triangles all sent", (long)pbgl_total.sent_tris, 298);
    pbgl_end_frame();
}

static void gl_clip(void)
{
    gl_start(1024);
    frustum();
    // A triangle with one corner behind the near plane (z -0.5 is w 0.5,
    // nearer than 1): cut there into a fan of four, every q at most 1.
    glBegin(GL_TRIANGLES);
    glVertex3f(-1, -1, -2);
    glVertex3f(1, -1, -2);
    glVertex3f(0, 0, -0.5f);
    glEnd();
    pbgl_end_frame();
    check_num("cut at the near plane", (long)pbgl_total.clipped, 1);
    check_num("into one fan", (long)pbgl_total.clip_tris, 1);
    check_num("of four vertices", (long)(gl_frame[2] == 0x1206 ? gl_frame[3] : 0), 4);
    long top = 0;
    for (int i = 0; i < 4; i++) {
        long q = gl_frame[4 + 4 * i + 3];
        if (q > top)
            top = q;
    }
    check_num("no q past the near plane's", top, 0x8000);

    // Past the guard band: x out at 3,000 pixels is brought back in.
    pbgl_reset();
    ortho();
    glBegin(GL_TRIANGLES);
    glVertex2f(0, 0); glVertex2f(3000, 0); glVertex2f(0, 100);
    glEnd();
    pbgl_end_frame();
    check_num("cut at the guard band", (long)pbgl_total.clipped, 1);
    long right = 0;
    for (uint32_t i = 4; i + 1 < gl_nframe; i += 2)
        if ((int16_t)gl_frame[i] > right)
            right = (int16_t)gl_frame[i];
    check_num("no x past 2,040 pixels", right, 2040 * 16);
}

static void gl_state(void)
{
    gl_start(1024);
    ortho();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glShadeModel(GL_FLAT);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, 0); glVertex2f(1, 0); glVertex2f(0, 1);
    glEnd();
    pbgl_end_frame();
    check_num("depth first: a DEPTH without a clear", (long)gl_frame[0], 0x0f00);
    check_num("then lequal, write, front faces culled as CCW", (long)gl_frame[2],
              PB3D_S_DEPTH_LEQUAL | PB3D_S_DEPTH_WRITE | PB3D_S_CULL_CCW);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthMask(0);
    glDepthRange(0, 0.3);
    glDisable(GL_CULL_FACE);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, 0); glVertex2f(1, 0); glVertex2f(0, 1);
    glEnd();
    static const uint16_t weapon[] = {
        0x0f01,                       // DEPTH, cleared
        0x1000, 0x0001,               // STATE: lequal, no write
        0x1300, 0x0000, 19661,        // DEPTHRANGE: 0 to 0.3 of 65,535
    };
    gl_check("the view weapon's depth range", 0, weapon, sizeof weapon / sizeof weapon[0]);
}

// GL's viewport as PB's CLIP: y counted from the bottom turned to rows
// from the top, sent before the TRIS that needs it, and only on a change.
static void tri(void)
{
    glBegin(GL_TRIANGLES);
    glVertex2f(0, 0); glVertex2f(1, 0); glVertex2f(0, 1);
    glEnd();
}

static void gl_viewport(void)
{
    gl_start(1024);
    ortho();
    tri();
    pbgl_end_frame();
    check_num("the whole surface sends no CLIP", (long)gl_nframe, 2 + 2 + 6);
    glViewport(0, 115, 640, 365);   // demo1's 3D view, above the status bar
    ortho();
    tri();
    glViewport(0, 0, 640, 480);     // GL_Set2D's, for the HUD
    tri();
    static const uint16_t view[] = {
        0x1000, 0x0800,               // STATE: smooth
        0x1400, 0, 0, 640, 365,       // CLIP: rows 0 to 364
        0x1200, 3, 0, 0, 16, 0, 0, 12,  // y: 1 of 480 is 365/480 of a pixel
        0x1400, 0, 0, 640, 480,       // and back to the whole surface
    };
    gl_check("the 3D view's CLIP, then the HUD's", 0, view, sizeof view / sizeof view[0]);
    tri();
    pbgl_end_frame();
    check_num("the next frame starts whole: no CLIP", (long)gl_nframe, 2 + 2 + 6);
    // A frame that ends with the 3D view's viewport: PB starts the next
    // one whole, so its first TRIS needs the CLIP again.
    glViewport(0, 115, 640, 365);
    tri();
    pbgl_end_frame();
    tri();
    static const uint16_t again[] = { 0x1000, 0x0800, 0x1400, 0, 0, 640, 365 };
    gl_check("each frame sends its CLIP afresh", 0, again, sizeof again / sizeof again[0]);
    glViewport(-20, -10, 700, 100); // past the edges: rows 390 to 489, held to 479
    tri();
    static const uint16_t held[] = { 0x1000, 0x0800, 0x1400, 0, 390, 640, 480 };
    gl_check("a viewport held to the surface", 0, held, sizeof held / sizeof held[0]);
}

static void gl_textures_check(void)
{
    static const uint8_t three_by_two[] = { 1, 2, 3, 4, 5, 6 };
    static const uint8_t four_by_two[] = { 1, 2, 2, 3, 4, 5, 5, 6 };
    uint8_t pal[768];
    gl_start(1024);
    for (int i = 0; i < 768; i++)
        pal[i] = (uint8_t)(i / 3);   // index i is grey i
    pbgl_palette(pal);
    glBindTexture(GL_TEXTURE_2D, 5);
    pbgl_upload8(three_by_two, 3, 2, 1, 0, 256);
    check_num("indices: id", (long)gl_tex_id, 5);
    check_num("indices: as they are", (long)gl_tex_format, PB3D_TEX_INDEX8);
    check_num("to a power of two", (long)(gl_tex_w * 100 + gl_tex_h), 402);
    int same = 1;
    for (int i = 0; i < 8; i++)
        same &= gl_tex[i] == four_by_two[i];
    check_num("its texels, point sampled as id's GL_ResampleTexture", same, 1);
    check_num("mipmapped, no alpha", (long)gl_tex_flags, PB3D_TEXF_MIPMAP);
    // Rows too: 3 x 3 to 4 x 4 takes rows 0, 0, 1 and 2, as columns
    // 0, 1, 1 and 2 (#24: 3 x 2 to 4 x 2 keeps every row, so a resample
    // that ignored the row step passed it).
    // A fourth row past the texture, so a resample that took rows 0 to 3
    // reads 10s there and fails on its values rather than off the end.
    static const uint8_t three_by_three[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10 };
    static const uint8_t four_by_four[] = { 1, 2, 2, 3, 1, 2, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9 };
    pbgl_upload8(three_by_three, 3, 3, 0, 0, 256);
    check_num("3 x 3 to 4 x 4", (long)(gl_tex_w * 100 + gl_tex_h), 404);
    same = 1;
    for (int i = 0; i < 16; i++)
        same &= gl_tex[i] == four_by_four[i];
    check_num("its rows, the nearest one down", same, 1);
    static const uint8_t see[] = { 0, 255, 7, 9 };
    pbgl_upload8(see, 2, 2, 0, 1, 256);
    check_num("index 255 with alpha: see-through", (long)gl_tex_flags, PB3D_TEXF_ALPHA);
    pbgl_upload8(see + 2, 2, 1, 0, 1, 256);
    check_num("alpha with no 255: solid", (long)gl_tex_flags, 0);

    // RGBA of palette colours goes as indices; a colour that isn't one,
    // as RGBA.
    static const uint8_t rgba[] = { 9, 9, 9, 255, 40, 40, 40, 255, 0, 0, 0, 0, 1, 1, 1, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, 4, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    check_num("RGBA of palette colours: indices", (long)gl_tex_format, PB3D_TEX_INDEX8);
    check_num("with alpha 0 as 255", (long)(gl_tex[0] * 1000000L + gl_tex[1] * 1000L + gl_tex[2]),
              9 * 1000000L + 40 * 1000L + 255);
    check_num("and see-through", (long)gl_tex_flags, PB3D_TEXF_ALPHA);
    static const uint8_t dot[] = { 255, 255, 255, 200, 1, 2, 3, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, 4, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, dot);
    check_num("not palette colours: RGBA", (long)gl_tex_format, PB3D_TEX_RGBA8888);
    // Opaque white is index 255's colour in this palette, but 255 is the
    // see-through index, so white can't go as an index (#24).
    static const uint8_t white[] = { 255, 255, 255, 255, 9, 9, 9, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, 4, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    check_num("index 255's colour, opaque: RGBA", (long)gl_tex_format, PB3D_TEX_RGBA8888);

    // A world polygon: s, t scaled to the texture's texels with whole
    // textures taken off; the lightmap's by its own size.
    pbgl_upload8(four_by_two, 4, 2, 1, 0, 256);   // texture 5: 4 x 2
    frustum();
    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    pbgl_end_frame();
    glBegin(GL_POLYGON);
    glTexCoord2f(3.0f, -1.5f); glVertex3f(0, 0, -2);
    glTexCoord2f(3.5f, -1.0f); glVertex3f(0.1f, 0, -2);
    glTexCoord2f(3.25f, 0.0f); glVertex3f(0, 0.1f, -2);
    glEnd();
    pbgl_end_frame();
    // STATE (2 words), TEXTURE (3), TRIS (2), then the first vertex's x,
    // y and q (4) before its s and t.
    check_num("s from the first copy: 3.0 is 0", (long)(int16_t)gl_frame[11], 0);
    check_num("t: -1.5 is half of 2 texels from -2", (long)(int16_t)gl_frame[12], 16);
    check_num("s 3.5 is 2 texels", (long)(int16_t)gl_frame[11 + 6], 32);
    check_num("mipmapped textures say so", (long)gl_frame[4], PB3D_T_MIPMAP);
}

static void gl_full(void)
{
    gl_start(30);
    ortho();
    for (int k = 0; k < 4; k++) {
        glBegin(GL_TRIANGLES);
        glVertex2f(0, 0); glVertex2f(1, 0); glVertex2f(0, 1);
        glEnd();
    }
    pbgl_end_frame();
    check_num("a frame past its words drops whole commands", (long)pbgl_total.dropped, 1);
    check_num("and keeps the rest", (long)gl_nframe, 2 + 8 * 3);
}

// Where the kept frame's TRIS number k starts, or -1: read back command
// by command, as PB would.
static long gl_find_tris(uint32_t k)
{
    uint32_t i = 0;
    while (i < gl_nframe) {
        uint32_t n = pb3d_cmd_words(gl_frame + i, gl_nframe - i);
        if (n == 0)
            return -1;
        if (gl_frame[i] >> 8 == PB3D_TRIS && k-- == 0)
            return (long)i;
        i += n;
    }
    return -1;
}

// The kept frame's words from at, against want's m (no words if at is -1).
static void gl_check_at(const char *what, long at, const uint16_t *want, uint32_t m)
{
    uint32_t left = at < 0 ? 0 : gl_nframe - (uint32_t)at;
    check_words(what, gl_frame + (at < 0 ? 0 : at), left < m ? left : m, want, m);
}

// Twice the signed area of TRIS k's polygon on the screen (y down), from
// its vertices' x and y, each vw words.
static long gl_area(uint32_t k, uint32_t vw)
{
    long at = gl_find_tris(k), sum = 0;
    if (at < 0)
        return 0;
    uint32_t n = gl_frame[at + 1];
    for (uint32_t i = 0; i < n; i++) {
        const uint16_t *a = gl_frame + at + 2 + i * vw;
        const uint16_t *b = gl_frame + at + 2 + ((i + 1) % n) * vw;
        sum += (long)(int16_t)a[0] * (int16_t)b[1] - (long)(int16_t)b[0] * (int16_t)a[1];
    }
    return sum / 256;   // 12.4 squared, back to pixels
}

static void gl_cuts(void)
{
    static const unsigned char black[3] = { 0, 0, 0 }, bright[3] = { 200, 100, 50 };
    gl_start(1024);
    ortho();
    // Both units on, colour modulated: a cut vertex carries s, t, s2, t2
    // and RGBA, each taken halfway, where the guard band cuts at 2,040
    // pixels the edges to a corner 4,080 out.
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_TEXTURE_2D);
    pbgl_select_texture(PBGL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    pbgl_select_texture(PBGL_TEXTURE0);
    glBegin(GL_TRIANGLES);
    glColor3ubv(black);
    pbgl_mtexcoord(PBGL_TEXTURE0, 0, 0); pbgl_mtexcoord(PBGL_TEXTURE1, 0, 0);
    glVertex2f(0, 0);
    glColor3ubv(bright);
    pbgl_mtexcoord(PBGL_TEXTURE0, 2, 4); pbgl_mtexcoord(PBGL_TEXTURE1, 8, 1);
    glVertex2f(4080, 0);
    glColor3ubv(black);
    pbgl_mtexcoord(PBGL_TEXTURE0, 0, 0); pbgl_mtexcoord(PBGL_TEXTURE1, 0, 0);
    glVertex2f(0, 100);
    glEnd();
    pbgl_end_frame();
    // The fan: the first corner, the two cuts, the last corner. Each cut
    // is x 2,040, s 1, t 2 (12.4), s2 4, t2 a half (10.6), and colour
    // 100, 50, 25, opaque.
    static const uint16_t cut_vertices[] = {
        0x125a, 4,                                       // TRIS: fan, s t, s2 t2, RGBA
        0, 0, 0, 0, 0, 0, 0x0000, 0xff00,
        2040 * 16, 0, 16, 32, 256, 32, 0x3264, 0xff19,
        2040 * 16, 50 * 16, 16, 32, 256, 32, 0x3264, 0xff19,
        0, 100 * 16, 0, 0, 0, 0, 0x0000, 0xff00,
    };
    long at = gl_find_tris(0);
    gl_check_at("a guard-band cut: each cut vertex's s, t, s2, t2 and colour", at,
                cut_vertices, sizeof cut_vertices / sizeof cut_vertices[0]);

    // In y, both ways: past the bottom at 4,000 and the top at -3,000.
    gl_start(1024);
    ortho();
    glBegin(GL_TRIANGLES);
    glVertex2f(0, -3000); glVertex2f(0, 4000); glVertex2f(100, 0);
    glEnd();
    pbgl_end_frame();
    check_num("cut at the guard band in y", (long)pbgl_total.clipped, 1);
    long low = 0, high = 0;
    at = gl_find_tris(0);
    for (uint32_t i = 0; at >= 0 && i < gl_frame[at + 1]; i++) {
        long y = (int16_t)gl_frame[at + 2 + 2 * i + 1];
        if (y < low) low = y;
        if (y > high) high = y;
    }
    check_num("no y past 2,040 pixels down", high, 2040 * 16);
    check_num("nor up", low, -2040 * 16);

    // A strip whose last vertex is behind the eye: its first triangle
    // goes whole, as a list; its second, odd, is cut into a fan. Both
    // wind as GL's strip does (the odd one as 2, 1, 3), so on the screen
    // they face the same way.
    gl_start(1024);
    frustum();
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3f(-1, -1, -2);
    glVertex3f(1, -1, -2);
    glVertex3f(-1, 1, -2);
    glVertex3f(1, 1, -0.5f);
    glEnd();
    pbgl_end_frame();
    at = gl_find_tris(1);
    check_num("a strip over the near plane: a list, then a fan",
              (long)gl_frame[gl_find_tris(0) < 0 ? 0 : gl_find_tris(0)] * 0x10000L +
              (at < 0 ? 0 : gl_frame[at]), 0x1204L * 0x10000L + 0x1206);
    long first = gl_area(0, 4), second = gl_area(1, 4);
    check_num("the whole triangle winds clockwise as seen", first < 0, 1);
    check_num("and the cut one the same way", second < 0, 1);
}

// a, b, c and d one after another in out's 96 bytes (lib/ has no
// snprintf), for a check's name.
static const char *join(char *out, const char *a, const char *b, const char *c, const char *d)
{
    const char *part[4] = { a, b, c, d };
    unsigned k = 0;
    for (int i = 0; i < 4; i++)
        for (const char *p = part[i]; *p && k < 95; p++)
            out[k++] = *p;
    out[k] = 0;
    return out;
}

// The guard band at each of its four edges, 2,040 pixels from the
// surface's corner whatever the viewport: a vertex 10 pixels inside is
// sent as it is, and one 10 pixels out is cut exactly at the edge, with
// the inside one kept. The viewport is (x, row, w, h) in PB's rows, and
// the projection makes GL's units its pixels; the triangles' other
// corners are near (cx, cy). Each plane's own factor,
// in outcode_of and the cut both, is the only one that passes all four.
static void gl_guard_at(const char *where, int vx, int vrow, int vw, int vh, float cx, float cy)
{
    static const float dir[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
    static const char *const edge[4] = { "left", "right", "top", "bottom" };
    char what[96];
    for (int e = 0; e < 4; e++) {
        float dx = dir[e][0], dy = dir[e][1];
        // A point 10 pixels inside the guard's edge (2,030 from the
        // corner) and one 10 outside (2,050), level with the viewport's
        // centre.
        float in_x = dx != 0 ? dx * 2030.0f : cx, in_y = dy != 0 ? dy * 2030.0f : cy;
        float out_x = dx != 0 ? dx * 2050.0f : cx, out_y = dy != 0 ? dy * 2050.0f : cy;
        for (int pass = 0; pass < 2; pass++) {
            gl_start(1024);
            glViewport(vx, 480 - (vrow + vh), vw, vh);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(vx, vx + vw, vrow + vh, vrow, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glBegin(GL_TRIANGLES);
            if (pass == 1)
                glVertex2f(out_x, out_y);
            glVertex2f(in_x + (dx != 0 ? 0 : 40), in_y + (dy != 0 ? 0 : 40));
            glVertex2f(cx + 20 * dy, cy + 20 * dx);
            if (pass == 0)
                glVertex2f(cx - 20 * dy, cy - 20 * dx);
            glEnd();
            pbgl_end_frame();
            long at = gl_find_tris(0), far = 0, kept = 0;
            uint32_t n = at < 0 ? 0 : gl_frame[at + 1];
            int axis = dx != 0 ? 0 : 1;
            long sign = (long)(dx + dy);
            for (uint32_t i = 0; i < n; i++) {
                long c = (int16_t)gl_frame[at + 2 + 2 * i + axis] * sign;
                if (c > far)
                    far = c;
                if (c == 2030 * 16)
                    kept++;
            }
            check_num(join(what, where, pass ? ": 10 pixels outside the guard's " :
                                               ": 10 pixels inside the guard's ",
                           edge[e], pass ? " is cut there" : " is sent whole"),
                      (long)pbgl_total.clipped, pass);
            check_num(join(what, where, ": and at its ", edge[e],
                           pass ? " the polygon ends at the edge" : " the vertex is where it was"),
                      far, pass ? 2040 * 16 : 2030 * 16);
            check_num(join(what, where, ": the inside vertex at the ", edge[e], " is kept"),
                      kept, 1);
        }
    }
}

static void gl_guard(void)
{
    gl_guard_at("the whole surface", 0, 0, 640, 480, 320, 240);
    gl_guard_at("demo1's 3D view", 0, 0, 640, 365, 320, 182.5f);  // glViewport(0, 115, 640, 365)
    // A viewport past the guard band (none GLQuake makes): x 2,050 is on
    // its screen and still outside the guard, so every test is made.
    gl_guard_at("a viewport at 1,800", 1800, 0, 640, 480, 1900, 240);
    // And one whose right guard factor is between 0.5 and 1
    // ((2,040 - 1,820) / 320, about 0.69): the screen's planes alone
    // would pass x 2,050, so guard_wider must be off for it (#34).
    gl_guard_at("a viewport at 1,500", 1500, 0, 640, 480, 1820, 240);
}

static void gl_subs(void)
{
    static uint8_t lightmap[4 * 8];
    for (int i = 0; i < 32; i++)
        lightmap[i] = (uint8_t)(i + 1);   // row r is 4r + 1 to 4r + 4
    gl_start(1024);
    glBindTexture(GL_TEXTURE_2D, 3);
    glTexImage2D(GL_TEXTURE_2D, 0, 1, 4, 8, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, lightmap);
    // Three updates to one block in a frame, as GLQuake's one pass makes
    // them: rows 2, 5 and 6, then 1. Sent once, at the frame's end, as
    // the band from row 1 to row 6.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 2, 4, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, lightmap + 8);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 5, 4, 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, lightmap + 20);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, 4, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, lightmap + 4);
    check_num("updates held to the frame's end", (long)gl_nsubs, 0);
    pbgl_end_frame();
    check_num("then sent as one", (long)gl_nsubs, 1);
    check_num("to the block", (long)gl_sub_id, 3);
    check_num("rows 1 to 6: from row 1", (long)gl_sub_y, 1);
    check_num("six rows", (long)gl_sub_h, 6);
    check_num("their texels, row 1's first to row 6's last",
              (long)(gl_sub_first * 100 + gl_sub_last), 5 * 100 + 28);
    check_num("three calls, one update, 24 bytes",
              (long)(pbgl_total.sub_calls * 10000 + pbgl_total.sub_uploads * 100 +
                     pbgl_total.sub_bytes), 3 * 10000 + 100 + 24);
    // An update held when the block is made again goes first.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 3, 4, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, lightmap + 12);
    glTexImage2D(GL_TEXTURE_2D, 0, 1, 4, 8, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, lightmap);
    check_num("an update held is sent before its texture is made again",
              (long)(gl_nsubs == 2 && gl_sub_seq < gl_tex_seq), 1);
    pbgl_end_frame();
    check_num("and not again at the frame's end", (long)gl_nsubs, 2);
}

static void gl_split(void)
{
    // A fan of 300 goes as 256, then the first vertex, the 256th, and
    // the 44 after: 46. A strip as 256, then its 255th and 256th and the
    // 44 after. Vertex i is at (i mod 50, i / 50).
    static const uint16_t fan[] = { 0x1202, 46, 0, 0, 5 * 16, 5 * 16, 6 * 16, 5 * 16 };
    static const uint16_t strip[] = { 0x1201, 46, 4 * 16, 5 * 16, 5 * 16, 5 * 16, 6 * 16, 5 * 16 };
    GLenum modes[2] = { GL_TRIANGLE_FAN, GL_TRIANGLE_STRIP };
    for (int m = 0; m < 2; m++) {
        gl_start(1024);
        ortho();
        glBegin(modes[m]);
        for (int i = 0; i < 300; i++)
            glVertex2f((float)(i % 50), (float)(i / 50));
        glEnd();
        pbgl_end_frame();
        long first = gl_find_tris(0), at = gl_find_tris(1);
        check_num(m ? "a strip of 300: first piece 256" : "a fan of 300: first piece 256",
                  first < 0 ? 0 : (long)gl_frame[first + 1], 256);
        gl_check_at(m ? "and the strip's second piece goes on from its last two"
                      : "and the fan's second piece from its first and last",
                    at, m ? strip : fan, 8);
    }
}

static void (*const checks[])(void) = { greeting, bench_words, pb3d_fixed, pb3d_commands,
                                         gl_screen, gl_prims, gl_clip, gl_state, gl_viewport,
                                         gl_textures_check, gl_full, gl_cuts, gl_guard, gl_subs,
                                         gl_split };

int quince_selftest(void (*fail)(const char *what, const char *got, const char *want),
                    int *count)
{
    report = fail;
    ran = failed = 0;
    for (unsigned i = 0; i < sizeof checks / sizeof checks[0]; i++)
        checks[i]();
    *count = ran;
    return failed;
}
