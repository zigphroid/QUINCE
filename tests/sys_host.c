/* tests/sys_host.c: Quake's system layer on the PC, for M1's host run.
 *
 * Our own code (M1, docs/ROADMAP.md). What id's sys_linux.c does for a
 * real game (files, the clock, the console, quitting) done the simplest
 * way the PC allows, so the same Quake objects that go on the board can be
 * run and timed here first. quince/sys_quince.c is the board's.
 *
 *   build/quake_host BASEDIR timedemo DEMO
 *   build/quake_host BASEDIR server MAP FRAMES
 *
 * BASEDIR holds id1/pak0.pak. Prints the results (qbench.c). With
 * QUINCE_PBDUMP=FILE set, also writes what the GL layer sends PB to FILE
 * (tests/pbdump.c), for the C model.
 */
#define _POSIX_C_SOURCE 200809L
#include "quakedef.h"
#include "qbench.h"
#include "pbgl.h"
#include "pbdump.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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
	if (!f) {
		*hndl = -1;
		return -1;
	}
	h = new_handle();
	handles[h] = f;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	*hndl = h;
	return (int)len;
}

int Sys_FileOpenWrite(char *path)
{
	FILE *f = fopen(path, "wb");
	int h;
	if (!f)
		Sys_Error("Couldn't write %s: %s", path, strerror(errno));
	h = new_handle();
	handles[h] = f;
	return h;
}

void Sys_FileClose(int h) { fclose(handles[h]); handles[h] = NULL; }
void Sys_FileSeek(int h, int pos) { fseek(handles[h], pos, SEEK_SET); }
int Sys_FileRead(int h, void *dest, int n) { return (int)fread(dest, 1, (size_t)n, handles[h]); }
int Sys_FileWrite(int h, void *data, int n) { return (int)fwrite(data, 1, (size_t)n, handles[h]); }

int Sys_FileTime(char *path)
{
	struct stat st;
	return stat(path, &st) == 0 ? (int)st.st_mtime : -1;
}

void Sys_mkdir(char *path) { mkdir(path, 0777); }

void Sys_Error(char *fmt, ...)
{
	va_list ap;
	fputs("Sys_Error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
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

double Sys_FloatTime(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

char *Sys_ConsoleInput(void) { return NULL; }
void Sys_SendKeyEvents(void) { }
void Sys_LowFPPrecision(void) { }
void Sys_HighFPPrecision(void) { }
void Sys_MakeCodeWriteable(unsigned long start, unsigned long len) { (void)start; (void)len; }

/* The hunk: Quake asks for one block at startup and carves everything
 * from it. The board's comes from the PSRAM pool; here, malloc, the same
 * size as the board's, so the two runs send the same frames. */
#define HOST_MEM MINIMUM_MEMORY

int main(int argc, char **argv)
{
	static quakeparms_t parms;
	struct qbench_run run;
	if (qbench_parse(argc - 1, argv + 1, &run) != 0) {
		fprintf(stderr, "usage: quake_host BASEDIR timedemo DEMO | BASEDIR server MAP FRAMES [-v]\n");
		return 2;
	}
	if (getenv("QUINCE_PBDUMP") && *getenv("QUINCE_PBDUMP"))
		qvid_sink = pbdump_open(getenv("QUINCE_PBDUMP"));
	parms.basedir = run.basedir;
	parms.memsize = run.memsize ? run.memsize : HOST_MEM;
	parms.membase = malloc((size_t)parms.memsize);
	if (!parms.membase)
		Sys_Error("no memory for the hunk");
	qbench_main(&parms, &run, Sys_FloatTime);
	return 0;
}
