/* sys_spans.c: Quake's system layer on the PC for the span dump.
 * Our own code, the same shape as QUINCE's tests/sys_host.c: files by
 * stdio, a fixed clock (1/72 s a call, so every run draws the same
 * frames), and a main that runs one timedemo and stops when it ends.
 *   spans BASEDIR DEMO          a timedemo (demo1, demo2, demo3)
 *   spans BASEDIR map MAP N     MAP loaded, then N frames of 1/72 s each
 * (BASEDIR holds id1/pak0.pak)
 */
#include "quakedef.h"
#include <sys/stat.h>
#include "r_local.h"
#include "d_local.h"
#include "spandump.h"

qboolean isDedicated = false;
#define MAX_HANDLES 16
static FILE *handles[MAX_HANDLES];

static int new_handle(void)
{
	int i;
	for (i = 1; i < MAX_HANDLES; i++)
		if (!handles[i])
			return i;
	Sys_Error("out of file handles");
	return -1;
}
int Sys_FileOpenRead(char *path, int *hndl)
{
	FILE *f = fopen(path, "rb");
	long len;
	int h;
	if (!f) { *hndl = -1; return -1; }
	h = new_handle();
	handles[h] = f;
	fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
	*hndl = h;
	return (int)len;
}
int Sys_FileOpenWrite(char *path)
{
	FILE *f = fopen(path, "wb");
	int h;
	if (!f) Sys_Error("Couldn't write %s", path);
	h = new_handle();
	handles[h] = f;
	return h;
}
void Sys_FileClose(int h) { fclose(handles[h]); handles[h] = NULL; }
void Sys_FileSeek(int h, int pos) { fseek(handles[h], pos, SEEK_SET); }
int Sys_FileRead(int h, void *d, int n) { return (int)fread(d, 1, n, handles[h]); }
int Sys_FileWrite(int h, void *d, int n) { return (int)fwrite(d, 1, n, handles[h]); }
int Sys_FileTime(char *path) { struct stat st; return stat(path, &st) == 0 ? (int)st.st_mtime : -1; }
void Sys_mkdir(char *path) { mkdir(path, 0777); }
void Sys_Error(char *fmt, ...)
{
	va_list ap;
	fputs("Sys_Error: ", stderr);
	va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
	fputc('\n', stderr);
	exit(1);
}
void Sys_Printf(char *fmt, ...)
{
	va_list ap;
	if (!getenv("SPANS_VERBOSE")) return;
	va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
}
void Sys_Quit(void) { exit(0); }
static long calls;
double Sys_FloatTime(void) { return (double)calls++ / 72.0; }
char *Sys_ConsoleInput(void) { return NULL; }
void Sys_SendKeyEvents(void) { }
void Sys_LowFPPrecision(void) { }
void Sys_HighFPPrecision(void) { }
void Sys_MakeCodeWriteable(unsigned long s, unsigned long l) { }

int main(int argc, char **argv)
{
	static quakeparms_t parms;
	static char *qargv[16];
	double old, now;
	int n = 0, frames = 0, ismap;
	ismap = argc == 5 && !strcmp(argv[2], "map");
	if (argc != 3 && !ismap) {
		fprintf(stderr, "usage: spans BASEDIR DEMO | BASEDIR map MAP FRAMES\n");
		return 2;
	}
	qargv[n++] = "spans";
	qargv[n++] = ismap ? "+map" : "+timedemo";
	qargv[n++] = ismap ? argv[3] : argv[2];
	/* SPANS_NOMODELS=1: no entities or view model, so the
	 * zbuffer holds what the spans wrote, and particles (for SPANS_CHECK). */
	if (getenv("SPANS_NOMODELS")) {
		qargv[n++] = "+r_drawentities"; qargv[n++] = "0";
		qargv[n++] = "+r_drawviewmodel"; qargv[n++] = "0";
	}
	parms.basedir = argv[1];
	parms.argc = n;
	parms.argv = qargv;
	COM_InitArgv(parms.argc, parms.argv);
	parms.memsize = 32 * 1024 * 1024;
	parms.membase = malloc(parms.memsize);
	Host_Init(&parms);
	old = Sys_FloatTime();
	for (;;) {
		/* Count a frame once the timedemo runs or the map is entered. */
		int on = ismap ? (sv.active && cls.signon == SIGNONS) : cls.timedemo;
		if (frames && !on)
			break;
		spandump_frameno = on ? ++frames : -1;
		if (ismap && frames > atoi(argv[4]))
			break;
		now = Sys_FloatTime();
		Host_Frame((float)(now - old));
		old = now;
		spandump_frame_end();
		if (!frames && calls > 72 * 300)
			Sys_Error("never started");
	}
	printf("%s: %d frames counted\n", ismap ? argv[3] : argv[2], frames - 1);
	return 0;
}
