/* lib/qvid.c: QUINCE's video layer for GLQuake over the null GL.
 *
 * Our own code (M1, docs/ROADMAP.md). On a PC or a Windows box this is
 * gl_vidnt.c's or gl_vidlinux.c's job: open a window, make a GL context,
 * turn the palette into the 32-bit colours textures are uploaded with.
 * Here there is no window and no context (lib/nullgl.c draws nothing), so
 * what's left is the state the renderer reads: the screen's size, the
 * palette tables, and the few switches GLQuake expects the video layer to
 * own. No kernel calls, so the host build links it too.
 */
#include "quakedef.h"
#include "nullgl.h"

/* The size the renderer thinks it draws at. 640x480 is GLQuake's usual
 * window, and what the vertex transform in nullgl.c projects onto; the
 * pixelboard's own size is a later milestone's question. */
#define QVID_WIDTH  640
#define QVID_HEIGHT 480

/* vid itself is gl_screen.c's. */
unsigned d_8to24table[256];
/* For 8-bit paletted cards only, so never read here: in PSRAM, out of the
 * program area (lib/qproto.h). */
QPSRAM unsigned char d_15to8table[65536];

int texture_mode = GL_LINEAR;
int texture_extension_number = 1;
float gldepthmin, gldepthmax;
qboolean gl_mtexable = false;    /* no multitexture: one pass a texture */
qboolean isPermedia = false;
const char *gl_vendor = "QUINCE";
const char *gl_renderer = "null GL (counts, draws nothing)";
const char *gl_version = "1.1";
const char *gl_extensions = "";

cvar_t gl_ztrick = {"gl_ztrick", "0", false, false, 0, NULL};

void VID_SetPalette(unsigned char *palette)
{
	int i;
	/* Textures go up as RGBA words, colour index 255 see-through, as the
	 * other GL layers do it. The 15-bit table is for 8-bit paletted
	 * cards, which this isn't: it stays zero. */
	for (i = 0; i < 256; i++) {
		unsigned r = palette[i * 3], g = palette[i * 3 + 1], b = palette[i * 3 + 2];
		d_8to24table[i] = (255u << 24) | (b << 16) | (g << 8) | r;
	}
	d_8to24table[255] &= 0x00ffffffu;
}

void VID_ShiftPalette(unsigned char *palette)
{
	(void)palette;   /* GLQuake blends a full-screen quad instead */
}

qboolean VID_Is8bit(void)
{
	return false;
}

void VID_Init(unsigned char *palette)
{
	Cvar_RegisterVariable(&gl_ztrick);
	vid.width = vid.conwidth = 320;
	vid.height = vid.conheight = 200;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0f / 240.0f);
	vid.numpages = 2;
	vid.maxwarpwidth = 320;
	vid.maxwarpheight = 200;
	/* The console and the 2D overlay draw into a 320x200 virtual screen;
	 * the 3D view uses GL_BeginRendering's size. Host_Init has loaded
	 * gfx/colormap.lmp before it calls this. */
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
	vid.recalc_refdef = 1;
	VID_SetPalette(palette);
}

void VID_Shutdown(void)
{
}

void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = QVID_WIDTH;
	*height = QVID_HEIGHT;
}

void GL_EndRendering(void)
{
	nullgl_end_frame();
}

/* net_vcr.c (recording a session's network for playback) isn't built. */
int VCR_Init(void)
{
	return 0;
}
