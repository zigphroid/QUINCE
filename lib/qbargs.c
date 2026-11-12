/* lib/qbargs.c: the benchmark's words (lib/qbench.h), apart from Quake so
 * the host test and /SELFTEST check them without it (lib/selftest.c).
 * Our own code (M1). No libc: the same bytes on both sides. */
#include "qbench.h"

static int same(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

static unsigned length(const char *s)
{
	unsigned n = 0;
	while (s[n])
		n++;
	return n;
}

static void clear(struct qbench_run *run)
{
	run->kind = QBENCH_TIMEDEMO;
	run->basedir = 0;
	run->name = 0;
	run->frames = 0;
	run->memsize = 0;
}

int qbench_verbose;

static int to_int(const char *s, int *out)
{
	int v = 0;
	if (!*s)
		return -1;
	for (; *s; s++) {
		if (*s < '0' || *s > '9' || v > 9999999)
			return -1;   /* not a number, or past any count asked for */
		v = v * 10 + (*s - '0');
	}
	*out = v;
	return 0;
}

/* The shell hands a program its words in capitals (DEMO1, E1M1), and
 * Quake's pak names are lower case and compared exactly (COM_FindFile's
 * strcmp, common.c), so the name is lowered here. */
static char *lower(const char *s)
{
	static char buf[64];
	unsigned i;
	for (i = 0; s[i] && i < sizeof buf - 1; i++)
		buf[i] = s[i] >= 'A' && s[i] <= 'Z' ? s[i] - 'A' + 'a' : s[i];
	buf[i] = 0;
	return buf;
}

int qbench_parse(int argc, char **argv, struct qbench_run *run)
{
	char *words[4];
	int n = 0, i, kb;
	clear(run);
	for (i = 0; i < argc; i++) {
		if (same(argv[i], "-v"))
			qbench_verbose = 1;
		else if (same(argv[i], "-mem") && i + 1 < argc) {
			if (to_int(argv[++i], &kb) != 0 || kb <= 0 || kb > 65536)
				return -1;
			run->memsize = kb * 1024;
		} else if (n < 4)
			words[n++] = argv[i];
		else
			return -1;
	}
	if (n == 3 && same(words[1], "timedemo"))
		run->kind = QBENCH_TIMEDEMO;
	else if (n == 4 && same(words[1], "server")) {
		run->kind = QBENCH_SERVER;
		if (to_int(words[3], &run->frames) != 0 || run->frames <= 0)
			return -1;
	} else
		return -1;
	run->basedir = words[0];
	if (length(words[2]) >= 64)
		return -1;
	run->name = lower(words[2]);
	return 0;
}

