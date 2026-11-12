/* lib/qvid.c: QUINCE's video layer for GLQuake over the GL layer to PB.
 *
 * Our own code (M1, M2: docs/ROADMAP.md). On a PC or a Windows box this is
 * gl_vidnt.c's or gl_vidlinux.c's job: open a window, make a GL context,
 * find the multitexture extension. Here the GL is lib/pbgl.c, which
 * writes PB's commands, so what's left is its memory, the state the
 * renderer reads (the screen's size, the palette), and the switches
 * GLQuake expects the video layer to own. No kernel calls, so the host
 * build links it too: the memory is malloc's, the PSRAM pool on the board
 * (quince/qlibc.c).
 */
#include "quakedef.h"
#include "pbgl.h"

/* The size the renderer draws at: PB's 640x480 (PB-ASKS, decision 2). */
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
/* Two textures a pixel (PB-ASKS, decision 3): GLQuake draws a wall and its
 * lightmap in one pass through the SGIS calls (gl_rsurf.c), pointed at
 * lib/pbgl.c in VID_Init. */
qboolean gl_mtexable = false;
qboolean isPermedia = false;
const char *gl_vendor = "QUINCE";
const char *gl_renderer = "PB's 3D commands (counted, not yet drawn)";
const char *gl_version = "1.1";
const char *gl_extensions = "";

cvar_t gl_ztrick = {"gl_ztrick", "0", false, false, 0, NULL};

/* What lib/pbgl.c works in (lib/pbgl.h). A frame's commands: demo1's
 * biggest is 77,401 words on the PC, an explosion's particles (each a
 * list triangle of 30 words), against 16,400 for its average frame
 * (LOG 2026-11-12, "M2 PR 2"), so 128 K words, 256 KB; a frame past it
 * loses commands, counted as dropped. The
 * textures by GL's name: GLQuake numbers them from 1 as it makes them,
 * about 1,200 for a shareware map. The scratch is a 256x256 texture,
 * gl_max_size. */
#define QVID_WORDS    (128u * 1024u)
#define QVID_TEXTURES 4096u
#define QVID_SCRATCH  (256u * 256u)

/* Where the commands go: nowhere, until PB has a 3D service. The PC's
 * QUINCE_PBDUMP sets one (tests/pbdump.c). */
const struct pbgl_sink *qvid_sink;

static void init_gl(void)
{
	static struct pbgl_mem mem;
	mem.words = malloc(QVID_WORDS * sizeof(uint16_t));
	mem.nwords = QVID_WORDS;
	mem.textures = malloc(QVID_TEXTURES * PBGL_TEXTURE_BYTES);
	mem.max_textures = QVID_TEXTURES;
	mem.scratch = malloc(QVID_SCRATCH);
	mem.nscratch = QVID_SCRATCH;
	if (!mem.words || !mem.textures || !mem.scratch)
		Sys_Error("VID_Init: no memory for the GL layer");
	pbgl_init(&mem, QVID_WIDTH, QVID_HEIGHT, qvid_sink);
	/* The state id's GL_Init (gl_vidnt.c, gl_vidlinux.c) sets once, which
	 * the renderer counts on and never sets itself: textures on, the
	 * alpha test on, flat shading, the front faces culled (Quake winds
	 * them the other way), and textures replacing the colour. Without it
	 * the 2D overlay's first frames (the console, the loading plaque) go
	 * to PB as white untextured quads, which the triangle engine's model
	 * showed (docs/PB-ASKS.md step 2, PR 2). GL_Init's glAlphaFunc,
	 * glClearColor and glPolygonMode are left out: PB's alpha test and
	 * clear are fixed, so lib/pbgl.c ignores them. */
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glShadeModel(GL_FLAT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	gl_mtexable = true;
	qglMTexCoord2fSGIS = pbgl_mtexcoord;
	qglSelectTextureSGIS = pbgl_select_texture;
}

void VID_SetPalette(unsigned char *palette)
{
	int i;
	pbgl_palette(palette);
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
	init_gl();
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
	pbgl_end_frame();
}

/* net_vcr.c (recording a session's network for playback) isn't built. */
int VCR_Init(void)
{
	return 0;
}
