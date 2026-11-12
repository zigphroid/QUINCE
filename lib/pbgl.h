/* lib/pbgl.h: QUINCE's GL layer to the pixelboard (lib/pbgl.c), what it
 * counts, and where its commands go.
 *
 * Our own code (M2, docs/ROADMAP.md; docs/PB-ASKS.md step 1). The layer
 * takes GLQuake's GL calls and writes PB's 3D commands (lib/pb3d.h) a
 * frame at a time. PB's 3D service doesn't exist yet, so on the board the
 * commands are counted and dropped; on the PC a sink can keep them
 * (tests/pbdump.c writes them to a file for the C model).
 */
#ifndef QUINCE_PBGL_H
#define QUINCE_PBGL_H

#include <stdint.h>

struct pbgl_counts {
	/* What GLQuake asked for, as M1's null GL counted it. */
	uint32_t frames;
	uint32_t calls;        /* every gl* call */
	uint32_t prims;        /* glBegin ... glEnd pairs */
	uint32_t verts;        /* glVertex* */
	uint32_t tris;         /* triangles those vertices make */
	uint32_t binds;        /* glBindTexture */
	/* What PB would be sent. */
	uint32_t words;        /* drawing commands' words, every frame's */
	uint32_t vertex_words; /* the part of words that is vertices */
	uint32_t tris_cmds;    /* TRIS commands */
	uint32_t sent_verts;   /* vertices in them */
	uint32_t sent_tris;    /* triangles they make */
	uint32_t state_cmds;   /* STATE */
	uint32_t texture_cmds; /* TEXTURE */
	uint32_t other_cmds;   /* DEPTH, DEPTHRANGE and CLIP */
	uint32_t culled;       /* primitives wholly off the screen or behind the eye */
	uint32_t clipped;      /* primitives cut at the near plane or the guard band */
	uint32_t clip_tris;    /* triangles those cuts made, sent as fans */
	uint32_t dropped;      /* commands that didn't fit the frame's buffer */
	uint32_t split;        /* primitives too long for one, sent as several */
	uint32_t max_frame_words; /* the biggest frame */
	uint32_t tex_uploads;  /* textures made: sent once, at load */
	uint32_t tex_bytes;    /* their texels' bytes, as sent */
	uint32_t tex_rgba;     /* of those, textures sent as RGBA */
	uint32_t sub_calls;    /* glTexSubImage2D: GLQuake's lightmap updates */
	uint32_t sub_uploads;  /* the updates sent: a band of rows per block a frame */
	uint32_t sub_bytes;    /* their bytes */
	uint32_t ignored;      /* uploads the layer drops: mips, which PB builds */
};

/* Where the layer's output goes. Any of them may be 0. A texture call
 * goes as it happens; a frame's commands at its end, so a texture made or
 * changed during a frame comes before that frame's record. */
struct pbgl_sink {
	void (*palette)(const uint8_t *rgb);   /* 768 bytes */
	void (*teximage)(uint32_t id, uint32_t format, uint32_t flags, uint32_t w,
	                 uint32_t h, const void *texels);
	void (*texsub)(uint32_t id, uint32_t format, uint32_t x, uint32_t y, uint32_t w,
	               uint32_t h, const void *texels);
	void (*frame)(const uint16_t *words, uint32_t n);
};

/* The memory the layer works in, from the caller, since lib/ takes no
 * kernel calls: words for a frame's commands, a table of the textures
 * (PBGL_TEXTURE_BYTES each, for GL's names 0 to max_textures - 1; a name
 * past it is drawn as no texture), and scratch for a texture's texels on
 * their way to PB (the biggest texture GLQuake makes, 256x256 under its
 * gl_max_size). */
struct pbgl_mem {
	uint16_t *words;
	uint32_t nwords;
	uint8_t *textures;
	uint32_t max_textures;
	uint8_t *scratch;
	uint32_t nscratch;
};
#define PBGL_TEXTURE_BYTES 2u

extern struct pbgl_counts pbgl_total;

/* Before any GL call: the memory, the drawing surface's size (the
 * picture PB draws into: 640x480) and the sink. */
void pbgl_init(const struct pbgl_mem *mem, int width, int height, const struct pbgl_sink *sink);
/* Zero the counts (the timings count from the demo's first frame). */
void pbgl_reset(void);
/* Called once a frame by the video layer's GL_EndRendering. */
void pbgl_end_frame(void);
/* A sum of every frame's words, so two builds can be compared. */
uint32_t pbgl_checksum(void);
/* The palette, as VID_SetPalette has it: r, g, b for 256 indices. */
void pbgl_palette(const uint8_t *rgb);

/* GL_Upload8's texture (quake/gl_draw.c, QUINCE:): w x h bytes of
 * palette indices into the bound texture, resampled to powers of two of
 * at most max_side (gl_max_size) a side, as GL_Upload32 did in RGBA. */
void pbgl_upload8(const uint8_t *data, int w, int h, int mipmap, int alpha, int max_side);

/* The SGIS multitexture calls (glquake.h's qglMTexCoord2fSGIS and
 * qglSelectTextureSGIS point at them), unit by GLQuake's TEXTURE0_SGIS
 * and TEXTURE1_SGIS. */
#define PBGL_TEXTURE0 0x835Eu
#define PBGL_TEXTURE1 0x835Fu
void pbgl_mtexcoord(unsigned int unit, float s, float t);
void pbgl_select_texture(unsigned int unit);

/* lib/qvid.c's: the sink it gives pbgl_init, 0 on the board. The PC's
 * main sets it before Quake starts (tests/pbdump.c). */
extern const struct pbgl_sink *qvid_sink;

#endif
