/* spandump.c: write the spans of chosen frames, and a count of every frame.
 *
 * Our own code (QUINCE, for pectinos's memory bench). Two environment
 * variables drive it, both read once:
 *   SPANS_COUNT=FILE   one line a frame: frame, spans and pixels by kind
 *   SPANS_FRAMES=a,b,c with SPANS_OUT=PREFIX: each listed frame's spans
 *                      in PREFIX-fNNNN.txt (format in ../README.md)
 * Frames are numbered by sys_spans.c's main (spandump_frameno): from 1,
 * the timedemo's first frame or the map's first frame after it loads;
 * loading frames are not counted.
 *
 * The 1/z and s/t values follow d_scan.c's D_DrawZSpans and D_DrawSpans8
 * line for line (id's formulas, so the numbers are the ones the renderer
 * writes and reads).
 */
#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"
#include "spandump.h"

static const char *kname[] = { "tex", "z", "sky", "turb", "solid" };
static FILE *countf, *framef;
static int inited, want[4096], nwant;
static const char *outprefix;
static int cur_frame = -1;
static long nspans[5], npix[5];
/* SPANS_CHECK=1: the z values the spans say, rebuilt pixel by pixel as
 * D_DrawZSpans writes them, then held to the real zbuffer at frame end
 * (models and particles, drawn after the spans, change some pixels). */
static short shadow[MAXWIDTH * MAXHEIGHT];
static unsigned char shadow_set[MAXWIDTH * MAXHEIGHT];
extern byte surfcache_base[];

static void init(void)
{
	const char *c = getenv("SPANS_COUNT"), *f = getenv("SPANS_FRAMES");
	inited = 1;
	if (c && *c && !(countf = fopen(c, "w")))
		Sys_Error("can't write %s", c);
	outprefix = getenv("SPANS_OUT");
	while (f && *f && nwant < 4096) {
		want[nwant++] = atoi(f);
		f = strchr(f, ',');
		if (f) f++;
	}
}

int spandump_frameno = -1;	/* set by main: the frame being drawn, or -1 */
static int frame_now(void) { return spandump_frameno; }

static int wanted(int fr)
{
	int i;
	for (i = 0; i < nwant; i++)
		if (want[i] == fr)
			return 1;
	return 0;
}

static void start_frame(int fr)
{
	char name[512];
	cur_frame = fr;
	memset(nspans, 0, sizeof nspans);
	memset(npix, 0, sizeof npix);
	if (outprefix && wanted(fr)) {
		snprintf(name, sizeof name, "%s-f%04d.txt", outprefix, fr);
		if (!(framef = fopen(name, "w")))
			Sys_Error("can't write %s", name);
		fprintf(framef, "# frame %d screen %dx%d view %d %d %d %d warp %d zwidth %d\n",
		        fr, vid.width, vid.height, r_refdef.vrect.x, r_refdef.vrect.y,
		        r_refdef.vrect.width, r_refdef.vrect.height, r_dowarp, d_zwidth);
		fprintf(framef, "# kind surf u v count izi izistep [cache cachew cacheh s0 t0 s1 t1]\n");
	}
}

void spandump_frame_end(void)
{
	int k;
	if (cur_frame < 0)
		return;
	if (countf) {
		fprintf(countf, "%d", cur_frame);
		for (k = 0; k < 5; k++)
			fprintf(countf, " %ld %ld", nspans[k], npix[k]);
		fprintf(countf, "\n");
	}
	if (getenv("SPANS_CHECK")) {
		long same = 0, diff = 0, unset = 0, i;
		for (i = 0; i < (long)vid.width * vid.height; i++) {
			if (!shadow_set[i]) { unset++; continue; }
			if (shadow[i] == d_pzbuffer[i]) same++;
			else { if (getenv("SPANS_CHECK")[0] == '2' && diff < 40) printf("  %ld,%ld span %d zbuf %d\n", i % vid.width, i / vid.width, shadow[i], d_pzbuffer[i]); diff++; }
		}
		printf("check frame %d: %ld pixels match the zbuffer, %ld differ, %ld no span\n",
		       cur_frame, same, diff, unset);
		memset(shadow_set, 0, sizeof shadow_set);
	}
	if (framef) {
		fprintf(framef, "# end");
		for (k = 0; k < 5; k++)
			fprintf(framef, " %s %ld spans %ld px", kname[k], nspans[k], npix[k]);
		fprintf(framef, "\n");
		fclose(framef);
		framef = NULL;
	}
	cur_frame = -1;
}

/* d_scan.c's D_DrawSpans8: s and t in 16.16 texels at pixel (u, v), clamped. */
static void st_at(int u, int v, int *s, int *t)
{
	float du = (float)u, dv = (float)v;
	float sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
	float tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
	float zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
	float z = (float)0x10000 / zi;
	*s = (int)(sdivz * z) + sadjust;
	if (*s > bbextents) *s = bbextents; else if (*s < 0) *s = 0;
	*t = (int)(tdivz * z) + tadjust;
	if (*t > bbextentt) *t = bbextentt; else if (*t < 0) *t = 0;
}

void spandump(int kind, int surf, espan_t *pspan, void *cache, int w, int h)
{
	int fr;
	if (!inited)
		init();
	fr = frame_now();
	if (fr < 1)
		return;
	if (fr != cur_frame) {
		spandump_frame_end();
		start_frame(fr);
	}
	/* d_scan.c's D_DrawZSpans: izistep from d_zistepu, izi at the span's start. */
	int izistep = (int)(d_zistepu * 0x8000 * 0x10000);
	do {
		float du = (float)pspan->u, dv = (float)pspan->v;
		double zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		int izi = (int)(zi * 0x8000 * 0x10000);
		if (kind == SD_Z && getenv("SPANS_CHECK")) {
			int i, z = izi, o = d_zwidth * pspan->v + pspan->u;
			for (i = 0; i < pspan->count; i++, z += izistep) {
				shadow[o + i] = (short)(z >> 16);
				shadow_set[o + i] = 1;
			}
		}
		nspans[kind]++;
		npix[kind] += pspan->count;
		if (!framef)
			continue;
		fprintf(framef, "%s %d %d %d %d %d %d", kname[kind], surf, pspan->u, pspan->v,
		        pspan->count, izi, izistep);
		if (kind == SD_TEX) {
			int s0, t0, s1, t1;
			st_at(pspan->u, pspan->v, &s0, &t0);
			st_at(pspan->u + pspan->count - 1, pspan->v, &s1, &t1);
			fprintf(framef, " %ld %d %d %d %d %d %d",
			        (long)((byte *)cache - surfcache_base), w, h, s0, t0, s1, t1);
		}
		fputc('\n', framef);
	} while ((pspan = pspan->pnext) != NULL);
}
