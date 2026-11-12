/* spandump.h: record the spans Quake's software renderer draws.
 * Our own code (QUINCE, for pectinos's memory bench). Called from
 * d_edge.c's D_DrawSurfaces after each span-drawing call (d_edge.patch). */
#ifndef SPANDUMP_H
#define SPANDUMP_H
enum { SD_TEX, SD_Z, SD_SKY, SD_TURB, SD_SOLID };
/* surf: the surface's index in surfaces[]; cache: the texels the spans
 * read (surface cache block or turbulent texture), w and h its size. */
void spandump(int kind, int surf, espan_t *spans, void *cache, int w, int h);
void spandump_frame_end(void);
extern int spandump_frameno;
#endif
