/* lib/qbench.c: M1's two timings (docs/ROADMAP.md).
 *
 * Our own code. Both runs start Quake the way its own command line would
 * (+timedemo, +map) and then drive Host_Frame, the one call a system
 * layer makes each frame, measuring around it:
 *
 * - timedemo: Quake times itself. The numbers printed are the ones
 *   CL_FinishTimeDemo works out (cl_demo.c): frames from the second, and
 *   seconds of Quake's realtime, which is the system clock.
 * - server: a map loaded with its client connected over loopback, then
 *   FRAMES calls of Host_Frame(0.1), a tenth of a second of game time
 *   each, timed by the clock around them. Each frame runs the server
 *   (QuakeC, physics), the client, and the renderer over the GL layer.
 *
 * Beside each timing, what lib/pbgl.c sent PB (M2): its commands' words
 * per frame and of what kind, and the bytes per second the PB link would
 * carry at that rate, the commands and the lightmaps' updates. Textures
 * are counted apart: PB keeps them all in its own memory, so each crosses
 * the link once, at load (Jacob, 2026-11-12). What GLQuake asked for, as
 * M1 counted it, is on its own lines, so M1's numbers can be held beside
 * these.
 */
#include "quakedef.h"
#include "pbgl.h"
#include "qbench.h"

/* The textures uploaded from Host_Init to the timed window's start: what
 * the PB is sent once, at load (kept by take_load before the reset). */
static uint32_t load_uploads, load_bytes, load_rgba;

static void take_load(void)
{
	load_uploads = pbgl_total.tex_uploads;
	load_bytes = pbgl_total.tex_bytes;
	load_rgba = pbgl_total.tex_rgba;
	pbgl_reset();
}

static void print_counts(const char *what, int frames, double seconds)
{
	const struct pbgl_counts *c = &pbgl_total;
	double fps = seconds > 0 ? frames / seconds : 0;
	double per = frames > 0 ? 1.0 / frames : 0;
	double cbytes = (double)c->words * 2;
	/* What crosses the PB link while the game runs: the commands, the
	 * lightmaps' updates and any texture made again. */
	double crate = seconds > 0 ? cbytes / seconds : 0;
	double lrate = seconds > 0 ? (c->sub_bytes + c->tex_bytes) / seconds : 0;
	printf("%s: %d frames %.2f seconds %.2f fps %.1f ms/frame\n",
	       what, frames, seconds, fps, frames > 0 ? seconds * 1000.0 / frames : 0.0);
	printf("%s: GL per frame %.0f tris %.0f verts %.0f prims %.0f gl calls %.0f binds\n",
	       what, c->tris * per, c->verts * per, c->prims * per, c->calls * per, c->binds * per);
	printf("%s: PB per frame %.0f words (%.1f KB): %.0f TRIS with %.0f verts and %.0f tris, "
	       "%.0f STATE, %.0f TEXTURE, %.0f DEPTH and DEPTHRANGE\n",
	       what, c->words * per, cbytes * per / 1024.0, c->tris_cmds * per, c->sent_verts * per,
	       c->sent_tris * per, c->state_cmds * per, c->texture_cmds * per, c->other_cmds * per);
	printf("%s: PB link %.0f KB/s at this rate: commands %.0f KB/s (vertices %.0f), "
	       "texture updates %.0f KB/s\n",
	       what, (crate + lrate) / 1024.0, crate / 1024.0,
	       seconds > 0 ? c->vertex_words * 2.0 / seconds / 1024.0 : 0.0, lrate / 1024.0);
	printf("%s: culled %u primitives, cut %u into %u fans, split %u, dropped %u commands, "
	       "biggest frame %u words\n",
	       what, (unsigned)c->culled, (unsigned)c->clipped, (unsigned)c->clip_tris,
	       (unsigned)c->split, (unsigned)c->dropped, (unsigned)c->max_frame_words);
	printf("%s: lightmap updates %u, %u bytes (GLQuake made %u)\n",
	       what, (unsigned)c->sub_uploads, (unsigned)c->sub_bytes, (unsigned)c->sub_calls);
	printf("%s: at load, textures %u uploads %u bytes (%u as RGBA), sent once\n",
	       what, (unsigned)load_uploads, (unsigned)load_bytes, (unsigned)load_rgba);
	/* A texture made inside the window is one Quake's cache let go and
	 * loaded again, which a tight hunk costs: link traffic too. */
	printf("%s: during the run, textures %u uploads %u bytes (reloaded)\n",
	       what, (unsigned)c->tex_uploads, (unsigned)c->tex_bytes);
	printf("%s: totals %u tris %u verts %u lightmap bytes %u texture bytes\n", what,
	       (unsigned)c->tris, (unsigned)c->verts, (unsigned)c->sub_bytes, (unsigned)c->tex_bytes);
	printf("%s: sent totals %u words %u verts %u tris\n", what,
	       (unsigned)c->words, (unsigned)c->sent_verts, (unsigned)c->sent_tris);
	printf("%s: checksum %u\n", what, (unsigned)pbgl_checksum());
}

static void run_timedemo(struct qbench_run *run, double (*clock)(void))
{
	double first = clock(), old = first, now;
	int started = 0, frames;
	for (;;) {
		now = clock();
		Host_Frame((float)(now - old));
		old = now;
		if (cls.timedemo && !started) {
			started = 1;
			take_load();
		} else if (started && !cls.timedemo)
			break;
		/* By the clock, not by host_framecount, which a demo that never
		 * opened leaves where it was: five minutes is past any load. */
		if (!started && now - first > 300)
			Sys_Error("timedemo %s never started", run->name);
	}
	/* CL_FinishTimeDemo's own sums, from what it leaves behind. */
	frames = host_framecount - cls.td_startframe - 1;
	print_counts("timedemo", frames, realtime - cls.td_starttime);
}

static void run_server(struct qbench_run *run, double (*clock)(void))
{
	double start, end;
	int i;
	for (i = 0; !(sv.active && cls.signon == SIGNONS); i++) {
		if (i > 1000)
			Sys_Error("map %s never finished loading", run->name);
		Host_Frame(0.1f);
	}
	take_load();
	start = clock();
	for (i = 0; i < run->frames; i++)
		Host_Frame(0.1f);
	end = clock();
	print_counts("server", run->frames, end - start);
}

void qbench_main(void *vparms, struct qbench_run *run, double (*clock)(void))
{
	quakeparms_t *parms = vparms;
	static char *argv[8];
	int argc = 0;
	argv[argc++] = "quince";
	argv[argc++] = "-basedir";
	argv[argc++] = run->basedir;
	argv[argc++] = run->kind == QBENCH_TIMEDEMO ? "+timedemo" : "+map";
	argv[argc++] = run->name;
	parms->argc = argc;
	parms->argv = argv;
	COM_InitArgv(parms->argc, parms->argv);
	Host_Init(parms);
	if (run->kind == QBENCH_TIMEDEMO)
		run_timedemo(run, clock);
	else
		run_server(run, clock);
	Host_Shutdown();
}
