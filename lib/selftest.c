// The checks QUINCE /SELFTEST and the host test share (quince_core.h). A
// new check goes in its own function, listed in checks[]. No libc here:
// the same bytes must mean the same thing on both sides, so even the
// string compare is our own.
#include "quince_core.h"
#include "qbench.h"
#include "nullgl.h"
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

static void (*const checks[])(void) = { greeting, bench_words, null_gl };

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
