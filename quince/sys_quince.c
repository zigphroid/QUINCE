/* quince/sys_quince.c: Quake's system layer on PectinOS (M1).
 *
 * QUINCE's own code. Quake asks a small, fixed set of things of the system
 * it runs on (quake/sys.h): files by handle, a clock, the console, and a
 * way to stop. id wrote one of these per platform (sys_linux.c, sys_win.c,
 * sys_dos.c); this is PectinOS's, on the SDK's calls. tests/sys_host.c is
 * the same layer on the PC, so the two builds share every other file.
 *
 * Paths: Quake builds them with '/', PectinOS takes '\' (FAT's, as DOS),
 * so every path is turned round here before it reaches the kernel.
 * Includes only C library headers (quince/libc/), not pectin.h, because
 * quakedef.h's names and pectin.h's meet (net_time): the SDK calls come
 * through quince/qsys.h's wrappers.
 */
#include "quakedef.h"
#include "qbench.h"

#include "qsys.h"

/* sdk/README.md, "Big files: 32 KB at a time". */
#define CHUNK 32768

qboolean isDedicated = false;

static void board_path(char *out, const char *path)
{
	int i;
	for (i = 0; path[i] && i < MAX_OSPATH - 1; i++)
		out[i] = path[i] == '/' ? '\\' : path[i];
	out[i] = 0;
}

int Sys_FileOpenRead(char *path, int *hndl)
{
	char p[MAX_OSPATH];
	int h, len;
	board_path(p, path);
	h = qsys_open(p, QSYS_READ);
	if (h < 0) {
		*hndl = -1;
		return -1;
	}
	len = qsys_lseek(h, 0, QSYS_END);
	qsys_lseek(h, 0, QSYS_SET);
	*hndl = h;
	return len;
}

int Sys_FileOpenWrite(char *path)
{
	char p[MAX_OSPATH];
	int h;
	board_path(p, path);
	h = qsys_open(p, QSYS_CREATE);
	if (h < 0)
		Sys_Error("Couldn't write %s", path);
	return h;
}

void Sys_FileClose(int h) { qsys_close(h); }
void Sys_FileSeek(int h, int pos) { qsys_lseek(h, pos, QSYS_SET); }

int Sys_FileRead(int h, void *dest, int n)
{
	byte *d = dest;
	int got = 0, r;
	while (got < n) {
		r = qsys_read(h, d + got, (unsigned)(n - got < CHUNK ? n - got : CHUNK));
		if (r <= 0)
			break;
		got += r;
	}
	return got;
}

int Sys_FileWrite(int h, void *data, int n)
{
	return qsys_write(h, data, (unsigned)n);
}

/* Quake only asks whether a file is there (-1: it isn't). */
int Sys_FileTime(char *path)
{
	char p[MAX_OSPATH];
	int h;
	board_path(p, path);
	h = qsys_open(p, QSYS_READ);
	if (h < 0)
		return -1;
	qsys_close(h);
	return 1;
}

void Sys_mkdir(char *path)
{
	char p[MAX_OSPATH];
	board_path(p, path);
	qsys_mkdir(p);   /* already there is fine */
}

void Sys_Error(char *fmt, ...)
{
	static char text[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	eprintf("Jam failed to set. (Quake: %s)\n", text);
	exit(1);
}

void Sys_Printf(char *fmt, ...)
{
	va_list ap;
	if (!qbench_verbose)
		return;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void Sys_Quit(void) { Host_Shutdown(); exit(0); }

/* The kernel's clock counts milliseconds since boot (ticks_ms): fine for
 * a timedemo of many seconds, coarse for one frame. */
double Sys_FloatTime(void)
{
	return (double)qsys_ticks_ms() / 1000.0;
}

char *Sys_ConsoleInput(void) { return NULL; }
void Sys_SendKeyEvents(void) { }
void Sys_LowFPPrecision(void) { }
void Sys_HighFPPrecision(void) { }
void Sys_MakeCodeWriteable(unsigned long start, unsigned long len) { (void)start; (void)len; }

/* The hunk comes from the PSRAM pool: quakedef.h's MINIMUM_MEMORY (5.75
 * MB) unless /MEM says otherwise, in KB. The rest of the pool, about 1.25
 * MB, is the texture buffers', the lightmaps' and the model slots'
 * (QUINCE: in id's files; LOG 2026-11-12). */
#define BOARD_MEM MINIMUM_MEMORY

/* QUINCE.c's main calls this for /TIMEDEMO and /SERVER, with the words
 * of the line in qbench_parse's order. */
int quince_bench(int argc, char **argv)
{
	static quakeparms_t parms;
	struct qbench_run run;
	if (qbench_parse(argc, argv, &run) != 0)
		return 2;
	parms.basedir = run.basedir;
	parms.memsize = run.memsize ? run.memsize : BOARD_MEM;
	/* What the pool holds before the hunk: the rest of Quake's PSRAM
	 * (textures, lightmaps, models) comes from what's left. */
	printf("quince: PSRAM pool %u KB free, hunk %d KB\n",
	       qsys_largest() >> 10, parms.memsize >> 10);
	parms.membase = qsys_alloc((unsigned)parms.memsize);
	if (!parms.membase) {
		eprintf("Headspace exceeded. (Quake's %d KB hunk)\n", parms.memsize >> 10);
		return 1;
	}
	qbench_main(&parms, &run, Sys_FloatTime);
	return 0;
}
