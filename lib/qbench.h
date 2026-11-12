/* lib/qbench.h: M1's two timings, shared by the PC (tests/sys_host.c) and
 * the board (quince/sys_quince.c). Our own code. */
#ifndef QUINCE_QBENCH_H
#define QUINCE_QBENCH_H

enum qbench_kind { QBENCH_TIMEDEMO, QBENCH_SERVER };

struct qbench_run {
	enum qbench_kind kind;
	char *basedir;
	char *name;     /* the demo, or the map */
	int frames;     /* the server benchmark's frame count */
	int memsize;    /* the hunk's bytes; 0 for the caller's default */
};

extern int qbench_verbose;   /* print Quake's console to the console too */

/* Read "BASEDIR timedemo DEMO" or "BASEDIR server MAP FRAMES", and an
 * optional -v and -mem KB (the hunk). 0, or -1 for words that aren't one. */
int qbench_parse(int argc, char **argv, struct qbench_run *run);

/* Host_Init, then run the benchmark and print its line. parms holds the
 * hunk; clock is the system layer's Sys_FloatTime. */
struct quakeparms_s;
void qbench_main(void *parms, struct qbench_run *run, double (*clock)(void));

#endif
