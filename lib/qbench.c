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
 *   (QuakeC, physics), the client, and the renderer over the null GL.
 *
 * Beside each timing, lib/nullgl.c's counts per frame, and the bytes per
 * second a PB link would carry at that rate: NULLGL_VERTEX_BYTES a vertex,
 * and the lightmaps' updates (glTexSubImage2D). Textures are counted apart:
 * the PB holds them all in its own memory, so each crosses the link once
 * (Jacob, 2026-11-12).
 */
#include "quakedef.h"
#include "nullgl.h"
#include "qbench.h"

/* The textures uploaded from Host_Init to the timed window's start: what
 * the PB is sent once, at load (kept by take_load before the reset). */
static uint32_t load_uploads, load_bytes;

static void take_load(void)
{
	load_uploads = nullgl_total.tex_uploads;
	load_bytes = nullgl_total.tex_bytes;
	nullgl_reset();
}

static void print_counts(const char *what, int frames, double seconds)
{
	double fps = seconds > 0 ? frames / seconds : 0;
	double per = frames > 0 ? 1.0 / frames : 0;
	double vbytes = (double)nullgl_total.verts * NULLGL_VERTEX_BYTES;
	/* What crosses the PB link while the game runs: the vertices, the
	 * lightmaps' updates and any texture reloaded. A texture crosses it
	 * once, at load, since the PB keeps every one in its own 512 MB
	 * (Jacob, 2026-11-12). */
	double vrate = seconds > 0 ? vbytes / seconds : 0;
	double lrate = seconds > 0 ? (nullgl_total.sub_bytes + nullgl_total.tex_bytes) / seconds : 0;
	printf("%s: %d frames %.2f seconds %.2f fps %.1f ms/frame\n",
	       what, frames, seconds, fps, frames > 0 ? seconds * 1000.0 / frames : 0.0);
	printf("%s: per frame %.0f tris %.0f verts %.0f prims %.0f gl calls %.0f binds\n",
	       what, nullgl_total.tris * per, nullgl_total.verts * per,
	       nullgl_total.prims * per, nullgl_total.calls * per, nullgl_total.binds * per);
	printf("%s: PB link %.0f KB/s at this rate: vertices %.0f KB/s, texture updates %.0f KB/s\n",
	       what, (vrate + lrate) / 1024.0, vrate / 1024.0, lrate / 1024.0);
	printf("%s: lightmap updates %u, %u bytes\n",
	       what, (unsigned)nullgl_total.sub_uploads, (unsigned)nullgl_total.sub_bytes);
	printf("%s: at load, textures %u uploads %u bytes, sent once\n",
	       what, (unsigned)load_uploads, (unsigned)load_bytes);
	/* A texture uploaded inside the window is one Quake's cache let go
	 * and loaded again, which a tight hunk costs: link traffic too. */
	printf("%s: during the run, textures %u uploads %u bytes (reloaded)\n",
	       what, (unsigned)nullgl_total.tex_uploads, (unsigned)nullgl_total.tex_bytes);
	printf("%s: totals %u tris %u verts %u lightmap bytes %u texture bytes\n", what,
	       (unsigned)nullgl_total.tris, (unsigned)nullgl_total.verts,
	       (unsigned)nullgl_total.sub_bytes, (unsigned)nullgl_total.tex_bytes);
	printf("%s: checksum %.0f\n", what, (double)nullgl_checksum());
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
