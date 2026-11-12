/* lib/qproto.h: prototypes for id's functions that id's files call before
 * any declaration of them.
 *
 * QUINCE's own (M1). C89 let a call declare a function by itself (an
 * "implicit declaration", returning int); C99 dropped that, and CANNER
 * refuses it ("C needs a declaration before the call"). Each line here is
 * the function's definition's own signature, from the file named, so the
 * calls are checked against it. quakedef.h includes this last (QUINCE:).
 */
#ifndef QUINCE_QPROTO_H
#define QUINCE_QPROTO_H

/* QPSRAM: a zeroed static that goes in PSRAM on the board, out of the 384
 * KB program area, as pectin.h's PECTIN_PSRAM does (id's files can't
 * include pectin.h; quince/qsys.h says why). Those come from the kernel's
 * 1 MB heap, so only the middling arrays use it (QUINCE: in each file);
 * the biggest come from the PSRAM pool through malloc instead. Nothing on
 * the PC. */
#ifdef __CANNER__
#define QPSRAM __attribute__((section(".pbss")))
#else
#define QPSRAM
#endif

/* chase.c calls world.c's */
qboolean SV_RecursiveHullCheck(hull_t *hull, int num, float p1f, float p2f,
                               vec3_t p1, vec3_t p2, trace_t *trace);

#ifdef GLQUAKE
/* gl_warp.c */
void EmitBothSkyLayers(msurface_t *fa);
void EmitSkyPolys(msurface_t *fa);
void EmitWaterPolys(msurface_t *fa);
void GL_SubdivideSurface(msurface_t *fa);
void R_DrawSkyChain(msurface_t *s);
/* gl_rsurf.c */
void GL_BuildLightmaps(void);
qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_DrawBrushModel(entity_t *e);
void R_DrawWaterSurfaces(void);
void R_DrawWorld(void);
void R_RenderBrushPoly(msurface_t *fa);
/* gl_draw.c */
int GL_LoadPicTexture(qpic_t *pic);
void GL_Set2D(void);
void GL_Upload8_EXT(byte *data, int width, int height, qboolean mipmap, qboolean alpha);
/* gl_mesh.c */
void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *hdr);
/* gl_rlight.c */
void R_AnimateLight(void);
int R_LightPoint(vec3_t p);
void R_MarkLights(dlight_t *light, int bit, mnode_t *node);
void R_RenderDlights(void);
/* gl_rmain.c */
void R_RotateForEntity(entity_t *e);
/* gl_refrag.c */
void R_StoreEfrags(efrag_t **ppefrag);
/* r_part.c */
void R_ClearParticles(void);
void R_DrawParticles(void);
void R_InitParticles(void);
/* lib/qvid.c */
qboolean VID_Is8bit(void);
#endif

/* mathlib.c */
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);
/* view.c */
void V_CalcBlend(void);

#endif
