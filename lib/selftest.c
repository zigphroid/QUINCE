// The checks QUINCE /SELFTEST and the host test share (quince_core.h). A
// new check goes in its own function, listed in checks[]. No libc here:
// the same bytes must mean the same thing on both sides, so even the
// string compare is our own.
#include "quince_core.h"
#include "qbench.h"
#include "nullgl.h"
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

// The null GL (lib/nullgl.c): a vertex's screen position, as its
// checksum sums them (x plus y), to the nearest pixel.
static long screen(float x, float y, float z)
{
    float before = nullgl_checksum();
    glBegin(GL_LINES);   // a primitive that makes no triangles
    glVertex3f(x, y, z);
    glEnd();
    float d = nullgl_checksum() - before;
    return (long)(d + (d < 0 ? -0.5f : 0.5f));
}

static long tris(GLenum mode, int n)
{
    uint32_t before = nullgl_total.tris;
    glBegin(mode);
    for (int i = 0; i < n; i++)
        glVertex2f(0.0f, 0.0f);
    glEnd();
    return (long)(nullgl_total.tris - before);
}

static void null_gl(void)
{
    nullgl_reset();
    glViewport(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 480, 0, -1, 1);   // GL_Set2D's: y down, as the screen
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // x 10, and y 20 from the top is 460 from the bottom: 470.
    check_num("glOrtho: (10, 20) on the screen", screen(10, 20, 0), 470);
    glPushMatrix();
    glTranslatef(100, 0, 0);
    check_num("glTranslatef moves it", screen(10, 20, 0), 570);
    glPopMatrix();
    check_num("glPopMatrix puts it back", screen(10, 20, 0), 470);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1, 1, -1, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW);
    // On the axis: the middle, (320, 240).
    check_num("glFrustum: the axis at the middle", screen(0, 0, -1), 560);
    // (0.5, -0.5) at depth 2 divides by w = 2: x 400, y 180.
    check_num("glFrustum: the divide by w", screen(0.5f, -0.5f, -2), 580);

    check_num("GL_TRIANGLES", tris(GL_TRIANGLES, 6), 2);
    check_num("GL_QUADS", tris(GL_QUADS, 8), 4);
    check_num("GL_TRIANGLE_FAN", tris(GL_TRIANGLE_FAN, 5), 3);
    check_num("GL_TRIANGLE_STRIP", tris(GL_TRIANGLE_STRIP, 6), 4);
    check_num("GL_POLYGON", tris(GL_POLYGON, 7), 5);
    check_num("GL_POLYGON of two", tris(GL_POLYGON, 2), 0);
    check_num("GL_LINES", tris(GL_LINES, 4), 0);

    nullgl_reset();
    glTexImage2D(GL_TEXTURE_2D, 0, 4, 16, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
    check_num("a texture's bytes", (long)nullgl_total.tex_bytes, 16 * 8 * 4);
    check_num("a lightmap update's bytes, apart", (long)nullgl_total.sub_bytes, 4 * 4);
    check_num("each call counted", (long)nullgl_total.calls, 2);
    nullgl_reset();
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

static void (*const checks[])(void) = { greeting, bench_words, null_gl, pb3d_fixed,
                                         pb3d_commands };

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
