/* lib/nullgl.c: QUINCE's null GL, under GLQuake's renderer (quake/gl_*.c).
 *
 * Our own code (M1, docs/ROADMAP.md, LOG 2026-11-12). Nothing is drawn.
 * What a Glide-style card leaves to the CPU is still done, so the timings
 * include it: the modelview and projection matrices are kept as OpenGL
 * defines them, and every vertex is taken through both and the
 * perspective divide to the screen, as a MiniGL over Glide had to do on
 * the CPU. Clipping isn't done (a card's guard band would take most of it;
 * inferred). Everything a card would be sent is counted: vertices,
 * triangles, texture uploads and their bytes.
 *
 * The matrix rules follow the OpenGL 1.1 specification, section 2.10
 * ("Coordinate Transformations"): column-major 4x4 matrices, each call
 * multiplying the current matrix on the right.
 */
#include "GL/gl.h"
#include "nullgl.h"
#include <math.h>
#include <string.h>

struct nullgl_counts nullgl_total;

#define STACK_DEPTH 8
typedef struct { float m[16]; } mat4;

static mat4 modelview[STACK_DEPTH], projection[STACK_DEPTH];
static int mv_top, pr_top;
static GLenum matrix_mode = GL_MODELVIEW;
static mat4 mvp;            /* projection x modelview, remade when stale */
static int mvp_stale = 1;
static float view_x, view_y, view_w = 640, view_h = 480;
static GLenum prim_mode;
static uint32_t prim_verts;
static float sum;

static mat4 *current(void)
{
	return matrix_mode == GL_PROJECTION ? &projection[pr_top] : &modelview[mv_top];
}

static void identity(mat4 *a)
{
	memset(a, 0, sizeof *a);
	a->m[0] = a->m[5] = a->m[10] = a->m[15] = 1.0f;
}

/* out = a x b, column-major: element (row r, column c) is m[c * 4 + r]. */
static void multiply(mat4 *out, const mat4 *a, const mat4 *b)
{
	mat4 t;
	int r, c, k;
	for (c = 0; c < 4; c++)
		for (r = 0; r < 4; r++) {
			float s = 0.0f;
			for (k = 0; k < 4; k++)
				s += a->m[k * 4 + r] * b->m[c * 4 + k];
			t.m[c * 4 + r] = s;
		}
	*out = t;
}

static void apply(const mat4 *b)
{
	mat4 *a = current();
	multiply(a, a, b);
	mvp_stale = 1;
}

static void count_call(void)
{
	nullgl_total.calls++;
}

void nullgl_reset(void)
{
	memset(&nullgl_total, 0, sizeof nullgl_total);
	sum = 0.0f;
}

void nullgl_end_frame(void)
{
	nullgl_total.frames++;
}

float nullgl_checksum(void)
{
	return sum;
}

/* --- matrices --- */

void glMatrixMode(GLenum mode) { count_call(); matrix_mode = mode; }
void glLoadIdentity(void) { count_call(); identity(current()); mvp_stale = 1; }

void glLoadMatrixf(const GLfloat *m)
{
	count_call();
	memcpy(current()->m, m, sizeof(float) * 16);
	mvp_stale = 1;
}

void glPushMatrix(void)
{
	count_call();
	if (matrix_mode == GL_PROJECTION) {
		if (pr_top + 1 < STACK_DEPTH) { projection[pr_top + 1] = projection[pr_top]; pr_top++; }
	} else if (mv_top + 1 < STACK_DEPTH) {
		modelview[mv_top + 1] = modelview[mv_top];
		mv_top++;
	}
}

void glPopMatrix(void)
{
	count_call();
	if (matrix_mode == GL_PROJECTION) { if (pr_top > 0) pr_top--; }
	else if (mv_top > 0) mv_top--;
	mvp_stale = 1;
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	mat4 t;
	count_call();
	identity(&t);
	t.m[12] = x; t.m[13] = y; t.m[14] = z;
	apply(&t);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	mat4 t;
	count_call();
	identity(&t);
	t.m[0] = x; t.m[5] = y; t.m[10] = z;
	apply(&t);
}

/* The rotation of the specification's glRotate: angle in degrees about the
 * axis (x, y, z), normalised first. */
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	mat4 t;
	float len = sqrtf(x * x + y * y + z * z), a, c, s, d;
	count_call();
	if (len == 0.0f)
		return;
	x /= len; y /= len; z /= len;
	a = angle * (3.14159265f / 180.0f);
	c = cosf(a); s = sinf(a); d = 1.0f - c;
	identity(&t);
	t.m[0] = x * x * d + c;     t.m[4] = x * y * d - z * s; t.m[8]  = x * z * d + y * s;
	t.m[1] = y * x * d + z * s; t.m[5] = y * y * d + c;     t.m[9]  = y * z * d - x * s;
	t.m[2] = x * z * d - y * s; t.m[6] = y * z * d + x * s; t.m[10] = z * z * d + c;
	apply(&t);
}

void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	mat4 p;
	count_call();
	memset(&p, 0, sizeof p);
	p.m[0] = (float)(2 * n / (r - l));
	p.m[5] = (float)(2 * n / (t - b));
	p.m[8] = (float)((r + l) / (r - l));
	p.m[9] = (float)((t + b) / (t - b));
	p.m[10] = (float)(-(f + n) / (f - n));
	p.m[11] = -1.0f;
	p.m[14] = (float)(-2 * f * n / (f - n));
	apply(&p);
}

void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	mat4 p;
	count_call();
	identity(&p);
	p.m[0] = (float)(2 / (r - l));
	p.m[5] = (float)(2 / (t - b));
	p.m[10] = (float)(-2 / (f - n));
	p.m[12] = (float)(-(r + l) / (r - l));
	p.m[13] = (float)(-(t + b) / (t - b));
	p.m[14] = (float)(-(f + n) / (f - n));
	apply(&p);
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
	count_call();
	view_x = (float)x; view_y = (float)y; view_w = (float)w; view_h = (float)h;
}

void glGetFloatv(GLenum what, GLfloat *out)
{
	count_call();
	if (what == GL_MODELVIEW_MATRIX)
		memcpy(out, modelview[mv_top].m, sizeof(float) * 16);
}

void glDepthRange(GLclampd n, GLclampd f) { count_call(); (void)n; (void)f; }

/* --- vertices: the work that stays on the CPU --- */

/* Triangles a primitive of n vertices makes, as a card would be sent them. */
static uint32_t triangles(GLenum mode, uint32_t n)
{
	if (mode == GL_TRIANGLES)
		return n / 3;
	if (mode == GL_QUADS)
		return n / 4 * 2;
	if (mode == GL_TRIANGLE_FAN || mode == GL_TRIANGLE_STRIP || mode == GL_POLYGON)
		return n >= 3 ? n - 2 : 0;
	return 0;   /* GL_LINES: GLQuake's only lines are debugging ones */
}

void glBegin(GLenum mode)
{
	count_call();
	prim_mode = mode;
	prim_verts = 0;
}

void glEnd(void)
{
	count_call();
	nullgl_total.prims++;
	nullgl_total.tris += triangles(prim_mode, prim_verts);
}

static void vertex(float x, float y, float z)
{
	const float *m;
	float cx, cy, cw, iw;
	count_call();
	if (mvp_stale) {
		multiply(&mvp, &projection[pr_top], &modelview[mv_top]);
		mvp_stale = 0;
	}
	m = mvp.m;
	cx = m[0] * x + m[4] * y + m[8] * z + m[12];
	cy = m[1] * x + m[5] * y + m[9] * z + m[13];
	cw = m[3] * x + m[7] * y + m[11] * z + m[15];
	/* z isn't needed for the count; a card takes 1/w for its depth and
	 * perspective correction, which is the divide here. */
	iw = cw != 0.0f ? 1.0f / cw : 0.0f;
	sum += view_x + (cx * iw + 1.0f) * 0.5f * view_w
	     + view_y + (cy * iw + 1.0f) * 0.5f * view_h;
	prim_verts++;
	nullgl_total.verts++;
}

void glVertex2f(GLfloat x, GLfloat y) { vertex(x, y, 0.0f); }
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { vertex(x, y, z); }
void glVertex3fv(const GLfloat *v) { vertex(v[0], v[1], v[2]); }

/* Texture coordinates and colours go with the vertex in the bytes counted
 * (NULLGL_VERTEX_BYTES); setting them costs the call. */
void glTexCoord2f(GLfloat s, GLfloat t) { count_call(); (void)s; (void)t; }
void glTexCoord2fv(const GLfloat *v) { count_call(); (void)v; }
void glColor3f(GLfloat r, GLfloat g, GLfloat b) { count_call(); (void)r; (void)g; (void)b; }
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { count_call(); (void)r; (void)g; (void)b; (void)a; }
void glColor4fv(const GLfloat *v) { count_call(); (void)v; }
void glColor3ubv(const unsigned char *v) { count_call(); (void)v; }
void glColor4ubv(const unsigned char *v) { count_call(); (void)v; }

/* --- textures --- */

static uint32_t texel_bytes(GLenum fmt)
{
	if (fmt == GL_RGBA)
		return 4;
	if (fmt == GL_RGB)
		return 3;
	return 1;   /* GL_ALPHA, GL_LUMINANCE, GL_INTENSITY, GL_COLOR_INDEX */
}

void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h,
                  GLint border, GLenum fmt, GLenum type, const GLvoid *px)
{
	count_call();
	(void)target; (void)level; (void)internal; (void)border; (void)type; (void)px;
	nullgl_total.tex_uploads++;
	nullgl_total.tex_bytes += (uint32_t)w * (uint32_t)h * texel_bytes(fmt);
}

void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,
                     GLenum fmt, GLenum type, const GLvoid *px)
{
	count_call();
	(void)target; (void)level; (void)x; (void)y; (void)type; (void)px;
	nullgl_total.tex_uploads++;
	nullgl_total.tex_bytes += (uint32_t)w * (uint32_t)h * texel_bytes(fmt);
}

void glBindTexture(GLenum target, GLuint tex) { count_call(); (void)target; (void)tex; nullgl_total.binds++; }
void glTexEnvf(GLenum t, GLenum w, GLfloat v) { count_call(); (void)t; (void)w; (void)v; }
void glTexParameterf(GLenum t, GLenum w, GLfloat v) { count_call(); (void)t; (void)w; (void)v; }

/* --- state a card keeps: a call each, nothing else --- */

void glEnable(GLenum cap) { count_call(); (void)cap; }
void glDisable(GLenum cap) { count_call(); (void)cap; }
void glBlendFunc(GLenum s, GLenum d) { count_call(); (void)s; (void)d; }
void glDepthFunc(GLenum f) { count_call(); (void)f; }
void glDepthMask(GLboolean on) { count_call(); (void)on; }
void glCullFace(GLenum face) { count_call(); (void)face; }
void glShadeModel(GLenum mode) { count_call(); (void)mode; }
void glHint(GLenum what, GLenum mode) { count_call(); (void)what; (void)mode; }
void glDrawBuffer(GLenum buf) { count_call(); (void)buf; }
void glReadBuffer(GLenum buf) { count_call(); (void)buf; }
void glClear(GLbitfield mask) { count_call(); (void)mask; }
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { count_call(); (void)r; (void)g; (void)b; (void)a; }
void glFinish(void) { count_call(); }
void glFogi(GLenum w, GLint v) { count_call(); (void)w; (void)v; }
void glFogf(GLenum w, GLfloat v) { count_call(); (void)w; (void)v; }
void glFogfv(GLenum w, const GLfloat *v) { count_call(); (void)w; (void)v; }
void glAlphaFunc(GLenum f, GLclampf ref) { count_call(); (void)f; (void)ref; }
void glPolygonMode(GLenum face, GLenum mode) { count_call(); (void)face; (void)mode; }

/* Screenshots read the frame back; there is none, so they get black. */
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, GLvoid *px)
{
	count_call();
	(void)x; (void)y; (void)type;
	memset(px, 0, (size_t)w * (size_t)h * texel_bytes(fmt));
}
