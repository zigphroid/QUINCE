/* lib/GL/gl.h: QUINCE's GL, the declarations GLQuake's renderer
 * (quake/gl_*.c) uses, and no more.
 *
 * Our own header, written for M1 (docs/ROADMAP.md): lib/pbgl.c defines
 * these functions (M2), turning them into the pixelboard's 3D commands.
 * The enum values are QUINCE's own numbering, not OpenGL's: nothing reads
 * them but pbgl.c, which only needs them distinct (bits for the two
 * buffer masks), and small enough that glTexEnvf's float holds one
 * exactly.
 */
#ifndef QUINCE_GL_H
#define QUINCE_GL_H

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

#define APIENTRY

#define GL_ALPHA                          0x100
#define GL_ALPHA_TEST                     0x101
#define GL_BACK                           0x102
#define GL_BLEND                          0x103
#define GL_COLOR_INDEX                    0x104
#define GL_COLOR_INDEX8_EXT               0x105
#define GL_CULL_FACE                      0x106
#define GL_DEPTH_TEST                     0x107
#define GL_FASTEST                        0x108
#define GL_FLAT                           0x109
#define GL_FOG                            0x10a
#define GL_FOG_COLOR                      0x10b
#define GL_FOG_END                        0x10c
#define GL_FOG_MODE                       0x10d
#define GL_FRONT                          0x10e
#define GL_GEQUAL                         0x10f
#define GL_INTENSITY                      0x110
#define GL_LEQUAL                         0x111
#define GL_LINEAR                         0x112
#define GL_LINEAR_MIPMAP_LINEAR           0x113
#define GL_LINEAR_MIPMAP_NEAREST          0x114
#define GL_LINES                          0x115
#define GL_LUMINANCE                      0x116
#define GL_MODELVIEW                      0x117
#define GL_MODELVIEW_MATRIX               0x118
#define GL_MODULATE                       0x119
#define GL_NEAREST                        0x11a
#define GL_NEAREST_MIPMAP_LINEAR          0x11b
#define GL_NEAREST_MIPMAP_NEAREST         0x11c
#define GL_NICEST                         0x11d
#define GL_ONE                            0x11e
#define GL_ONE_MINUS_SRC_ALPHA            0x11f
#define GL_ONE_MINUS_SRC_COLOR            0x120
#define GL_PERSPECTIVE_CORRECTION_HINT    0x121
#define GL_POLYGON                        0x122
#define GL_PROJECTION                     0x123
#define GL_QUADS                          0x124
#define GL_REPLACE                        0x125
#define GL_RGB                            0x126
#define GL_RGBA                           0x127
#define GL_RGBA4                          0x128
#define GL_SMOOTH                         0x129
#define GL_SRC_ALPHA                      0x12a
#define GL_TEXTURE_2D                     0x12b
#define GL_TEXTURE_ENV                    0x12c
#define GL_TEXTURE_ENV_MODE               0x12d
#define GL_TEXTURE_MAG_FILTER             0x12e
#define GL_TEXTURE_MIN_FILTER             0x12f
#define GL_TRIANGLES                      0x130
#define GL_TRIANGLE_FAN                   0x131
#define GL_TRIANGLE_STRIP                 0x132
#define GL_UNSIGNED_BYTE                  0x133
#define GL_ZERO                           0x134
#define GL_COLOR_BUFFER_BIT               0x001
#define GL_DEPTH_BUFFER_BIT               0x002

void glBegin(GLenum mode);
void glEnd(void);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex3fv(const GLfloat *v);
void glTexCoord2f(GLfloat s, GLfloat t);
void glTexCoord2fv(const GLfloat *v);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor4fv(const GLfloat *v);
void glColor3ubv(const unsigned char *v);
void glColor4ubv(const unsigned char *v);

void glMatrixMode(GLenum mode);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat *m);
void glPushMatrix(void);
void glPopMatrix(void);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
void glDepthRange(GLclampd n, GLclampd f);
void glGetFloatv(GLenum what, GLfloat *out);

void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glBlendFunc(GLenum s, GLenum d);
void glDepthFunc(GLenum f);
void glDepthMask(GLboolean on);
void glCullFace(GLenum face);
void glShadeModel(GLenum mode);
void glHint(GLenum what, GLenum mode);
void glDrawBuffer(GLenum buf);
void glReadBuffer(GLenum buf);
void glClear(GLbitfield mask);
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void glFinish(void);
void glFogi(GLenum what, GLint v);
void glFogf(GLenum what, GLfloat v);
void glFogfv(GLenum what, const GLfloat *v);
void glAlphaFunc(GLenum f, GLclampf ref);
void glPolygonMode(GLenum face, GLenum mode);
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, GLvoid *px);

void glBindTexture(GLenum target, GLuint tex);
void glTexEnvf(GLenum target, GLenum what, GLfloat v);
void glTexParameterf(GLenum target, GLenum what, GLfloat v);
void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h,
                  GLint border, GLenum fmt, GLenum type, const GLvoid *px);
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,
                     GLenum fmt, GLenum type, const GLvoid *px);

#endif
