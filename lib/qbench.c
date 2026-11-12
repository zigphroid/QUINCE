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
 * second a PB link would carry at that rate (NULLGL_VERTEX_BYTES a vertex,
 * plus the textures uploaded during the run).
 */
#include "quakedef.h"
#include "nullgl.h"
#include "qbench.h"

int qbench_verbose;

static int to_int(const char *s, int *out)
{
	int v = 0;
	if (!*s)
		return -1;
	for (; *s; s++) {
		if (*s < '0' || *s > '9')
			return -1;
		v = v * 10 + (*s - '0');
	}
	*out = v;
	return 0;
}

int qbench_parse(int argc, char **argv, struct qbench_run *run)
{
	char *words[4];
	int n = 0, i, mb;
	memset(run, 0, sizeof *run);
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-v"))
			qbench_verbose = 1;
		else if (!strcmp(argv[i], "-mem") && i + 1 < argc) {
			if (to_int(argv[++i], &mb) != 0 || mb <= 0 || mb > 64)
				return -1;
			run->memsize = mb * 1024 * 1024;
		} else if (n < 4)
			words[n++] = argv[i];
		else
			return -1;
	}
	if (n == 3 && !strcmp(words[1], "timedemo"))
		run->kind = QBENCH_TIMEDEMO;
	else if (n == 4 && !strcmp(words[1], "server")) {
		run->kind = QBENCH_SERVER;
		if (to_int(words[3], &run->frames) != 0 || run->frames <= 0)
			return -1;
	} else
		return -1;
	run->basedir = words[0];
	run->name = words[2];
	return 0;
}

static void print_counts(const char *what, int frames, double seconds)
{
	double fps = seconds > 0 ? frames / seconds : 0;
	double per = frames > 0 ? 1.0 / frames : 0;
	double vbytes = (double)nullgl_total.verts * NULLGL_VERTEX_BYTES;
	double link = seconds > 0 ? (vbytes + nullgl_total.tex_bytes) / seconds : 0;
	printf("%s: %d frames %.2f seconds %.2f fps %.1f ms/frame\n",
	       what, frames, seconds, fps, frames > 0 ? seconds * 1000.0 / frames : 0.0);
	printf("%s: per frame %.0f tris %.0f verts %.0f prims %.0f gl calls %.0f binds\n",
	       what, nullgl_total.tris * per, nullgl_total.verts * per,
	       nullgl_total.prims * per, nullgl_total.calls * per, nullgl_total.binds * per);
	printf("%s: textures %u uploads %u bytes; PB link %.0f KB/s at this rate\n",
	       what, (unsigned)nullgl_total.tex_uploads, (unsigned)nullgl_total.tex_bytes,
	       link / 1024.0);
	printf("%s: totals %u tris %u verts %u texture bytes\n", what,
	       (unsigned)nullgl_total.tris, (unsigned)nullgl_total.verts,
	       (unsigned)nullgl_total.tex_bytes);
	printf("%s: checksum %.0f\n", what, (double)nullgl_checksum());
}

static void run_timedemo(struct qbench_run *run, double (*clock)(void))
{
	double old = clock(), now;
	int started = 0, frames;
	for (;;) {
		now = clock();
		Host_Frame((float)(now - old));
		old = now;
		if (cls.timedemo && !started) {
			started = 1;
			nullgl_reset();
		} else if (started && !cls.timedemo)
			break;
		if (!started && host_framecount > 1000)
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
	nullgl_reset();
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
