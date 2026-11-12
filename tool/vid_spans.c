/* vid_spans.c: a 640x480 screen in memory for the software renderer.
 * Our own code, shaped after id's vid_null.c (which is 320x200). Nothing is
 * shown; the spans are what we want. */
#include "quakedef.h"
#include "d_local.h"

viddef_t vid;
#define W 640
#define H 480
static byte vid_buffer[W * H];
static short zbuffer[W * H];
byte surfcache_base[8 * 1024 * 1024];	/* spandump.c reports offsets into it */
unsigned short d_8to16table[256];
unsigned d_8to24table[256];

void VID_SetPalette(unsigned char *palette) { }
void VID_ShiftPalette(unsigned char *palette) { }

void VID_Init(unsigned char *palette)
{
	int need;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.width = vid.conwidth = W;
	vid.height = vid.conheight = H;
	vid.aspect = ((float)H / (float)W) * (320.0 / 240.0);
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = W;
	d_pzbuffer = zbuffer;
	need = D_SurfaceCacheForRes(W, H);
	if (need > (int)sizeof surfcache_base)
		Sys_Error("surface cache %d too big", need);
	D_InitCaches(surfcache_base, need);
}

void VID_Shutdown(void) { }
void VID_Update(vrect_t *rects) { }
void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height) { }
void D_EndDirectRect(int x, int y, int width, int height) { }
