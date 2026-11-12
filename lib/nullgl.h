/* lib/nullgl.h: what lib/nullgl.c counts, for the timings to print.
 * Our own code (M1). */
#ifndef QUINCE_NULLGL_H
#define QUINCE_NULLGL_H

#include <stdint.h>

struct nullgl_counts {
	uint32_t frames;
	uint32_t calls;        /* every gl* call */
	uint32_t prims;        /* glBegin ... glEnd pairs */
	uint32_t verts;        /* glVertex* */
	uint32_t tris;         /* triangles those vertices make */
	uint32_t tex_uploads;  /* glTexImage2D: a texture, sent once */
	uint32_t tex_bytes;    /* the bytes those uploads carried */
	uint32_t sub_uploads;  /* glTexSubImage2D: GLQuake's lightmaps, changed
	                        * by dynamic lights while the game runs */
	uint32_t sub_bytes;    /* the bytes those updates carried */
	uint32_t binds;        /* glBindTexture */
};

/* Bytes one vertex would take on the PB link: x, y, z, 1/w, s, t as
 * floats and an RGBA colour, the shape a Glide-style card takes. A guess
 * until the link's own format exists (inferred, not measured). */
#define NULLGL_VERTEX_BYTES (6 * 4 + 4)

extern struct nullgl_counts nullgl_total;
/* Zero the counts (the timings start counting at the demo's first frame). */
void nullgl_reset(void);
/* Called once a frame by the video layer's GL_EndRendering. */
void nullgl_end_frame(void);
/* Sum of every transformed vertex's screen position, so the transform's
 * work can't be dropped by a compiler and two builds can be compared. */
float nullgl_checksum(void);

#endif
