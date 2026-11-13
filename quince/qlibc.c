/* quince/qlibc.c: the C library pieces id's Quake calls that the SDK
 * doesn't have (quince/libc/*.h declares them).
 *
 * QUINCE's own code (M1), written from the C standard's descriptions
 * (C11 §7.21 for stdio, §7.22 for stdlib, §7.24 for string, §7.12 for
 * math), only as far as Quake uses them. The board's build only: on the
 * PC, the PC's library does this.
 */
#include <pectin.h>
#include "libc/stdio.h"
#include "libc/stdlib.h"
#include "libc/string.h"
#include "libc/math.h"
#include "qsys.h"

/* ---- quince/qsys.h: pectin.h's inlines, as functions ---- */

int qsys_open(const char *path, int mode) { return open(path, mode); }
int qsys_read(int h, void *buf, unsigned n) { return read(h, buf, n); }
int qsys_write(int h, const void *buf, unsigned n) { return write(h, buf, n); }
int qsys_close(int h) { return close(h); }
int qsys_lseek(int h, int off, int whence) { return lseek(h, off, whence); }
int qsys_mkdir(const char *path) { return mkdir(path); }
int qsys_unlink(const char *path) { return unlink(path); }
void *qsys_alloc(unsigned n) { return mem_alloc(n); }
unsigned qsys_largest(void) { return mem_largest(); }
unsigned qsys_ticks_ms(void) { return ticks_ms(); }

/* ---- string.h ---- */

char *strcpy(char *dst, const char *src)
{
	char *d = dst;
	while ((*d++ = *src++) != 0)
		;
	return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
	size_t i = 0;
	for (; i < n && src[i]; i++)
		dst[i] = src[i];
	for (; i < n; i++)
		dst[i] = 0;
	return dst;
}

char *strcat(char *dst, const char *src)
{
	strcpy(dst + strlen(dst), src);
	return dst;
}

char *strstr(const char *hay, const char *needle)
{
	size_t n = strlen(needle);
	if (n == 0)
		return (char *)hay;
	for (; *hay; hay++)
		if (*hay == *needle && strncmp(hay, needle, n) == 0)
			return (char *)hay;
	return NULL;
}

char *strrchr(const char *s, int c)
{
	const char *last = NULL;
	for (;; s++) {
		if (*s == (char)c)
			last = s;
		if (!*s)
			return (char *)last;
	}
}

/* ---- stdlib.h ---- */

long strtol(const char *s, char **end, int base)
{
	const char *p = s;
	long v = 0;
	int neg = 0, any = 0, d;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;
	if (*p == '-' || *p == '+')
		neg = *p++ == '-';
	if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		p += 2;
		base = 16;
	} else if (base == 0)
		base = *p == '0' ? 8 : 10;
	for (;; p++) {
		if (*p >= '0' && *p <= '9') d = *p - '0';
		else if (*p >= 'a' && *p <= 'z') d = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'Z') d = *p - 'A' + 10;
		else break;
		if (d >= base)
			break;
		v = v * base + d;
		any = 1;
	}
	if (end)
		*end = (char *)(any ? p : s);
	return neg ? -v : v;
}

int atoi(const char *s)
{
	return (int)strtol(s, NULL, 10);
}

int abs(int x)
{
	return x < 0 ? -x : x;
}

void *malloc(size_t n)
{
	return mem_alloc((uint32_t)(n ? n : 1));
}

void free(void *p)
{
	if (p)
		mem_free(p);
}

/* ---- math.h: the two the SDK hasn't got ---- */

double ceil(double x)
{
	double f = floor(x);
	return f == x ? f : f + 1.0;
}

/* asin from atan2: sin y = x means tan y = x / sqrt(1 - x^2). */
double asin(double x)
{
	return atan2(x, sqrt(1.0 - x * x));
}

/* ---- stdio.h ---- */

#define QFILES 8

struct quince_file {
	int used;        /* 0: this slot is free (a handle can be 0, #7) */
	int handle;
	int eof;
	int writing;
	int have;        /* bytes in buf after pos */
	int pos;
	unsigned char buf[512];
};

static struct quince_file files[QFILES];

FILE *fopen(const char *path, const char *mode)
{
	/* Quake builds its paths with '/' (COM_FOpenFile, for a demo in the
	 * pak); PectinOS's are '\\', as quince/sys_quince.c's board_path. */
	char p[128];
	int i, h, how = PECTIN_READ;
	for (i = 0; path[i] && i < (int)sizeof p - 1; i++)
		p[i] = path[i] == '/' ? '\\' : path[i];
	p[i] = 0;
	if (mode[0] == 'w')
		how = PECTIN_CREATE;
	else if (mode[0] == 'a')
		how = PECTIN_APPEND;
	for (i = 0; i < QFILES; i++)
		if (!files[i].used)
			break;
	if (i == QFILES)
		return NULL;
	h = open(p, how);
	if (h < 0)
		return NULL;
	memset(&files[i], 0, sizeof files[i]);
	files[i].used = 1;
	files[i].handle = h;
	files[i].writing = how != PECTIN_READ;
	return &files[i];
}

int fclose(FILE *f)
{
	int r = close(f->handle);
	f->used = 0;
	return r < 0 ? EOF : 0;
}

/* What the read-ahead buffer holds is handed out first. */
size_t fread(void *buf, size_t size, size_t n, FILE *f)
{
	size_t want = size * n, got = 0;
	unsigned char *out = buf;
	int r;
	if (f->have > 0) {
		size_t take = (size_t)f->have < want ? (size_t)f->have : want;
		memcpy(out, f->buf + f->pos, take);
		f->pos += (int)take;
		f->have -= (int)take;
		got = take;
	}
	if (got < want) {
		r = read(f->handle, out + got, (uint32_t)(want - got));
		if (r > 0)
			got += (size_t)r;
		if (got < want)
			f->eof = 1;
	}
	return size ? got / size : 0;
}

size_t fwrite(const void *buf, size_t size, size_t n, FILE *f)
{
	int r = write(f->handle, buf, (uint32_t)(size * n));
	return r < 0 || size == 0 ? 0 : (size_t)r / size;
}

int fseek(FILE *f, long off, int whence)
{
	if (whence == SEEK_CUR)
		off -= f->have;   /* the read-ahead isn't read yet */
	f->have = f->pos = 0;
	f->eof = 0;
	return lseek(f->handle, (int32_t)off, whence) < 0 ? -1 : 0;
}

long ftell(FILE *f)
{
	int at = lseek(f->handle, 0, SEEK_CUR);
	return at < 0 ? -1 : at - f->have;
}

int fgetc(FILE *f)
{
	int r;
	if (f->have == 0) {
		r = read(f->handle, f->buf, sizeof f->buf);
		if (r <= 0) {
			f->eof = 1;
			return EOF;
		}
		f->have = r;
		f->pos = 0;
	}
	f->have--;
	return f->buf[f->pos++];
}

int fputc(int c, FILE *f)
{
	unsigned char b = (unsigned char)c;
	return write(f->handle, &b, 1) == 1 ? c : EOF;
}

int feof(FILE *f)
{
	return f->eof;
}

int fflush(FILE *f)
{
	(void)f;   /* nothing is buffered on the way out */
	return 0;
}

/* Quake's longest formatted line is a console line (MAXPRINTMSG, 4096 in
 * console.c) or a save file's; 4 KB of buffer for the static ones. In
 * PSRAM, as lib/qproto.h's QPSRAM: a printf a frame at most, against the
 * SRAM the GL layer's words window needs (M4 PR 4). */
#define FMT_MAX 4096
static __attribute__((section(".pbss"))) char fmt_buf[FMT_MAX];

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
	int n = vsnprintf(fmt_buf, FMT_MAX, fmt, ap);
	if (n > FMT_MAX - 1)
		n = FMT_MAX - 1;
	return write(f->handle, fmt_buf, (uint32_t)n) < 0 ? -1 : n;
}

int fprintf(FILE *f, const char *fmt, ...)
{
	va_list ap;
	int n;
	va_start(ap, fmt);
	n = vfprintf(f, fmt, ap);
	va_end(ap);
	return n;
}

/* C's sprintf has no size: Quake's callers size their buffers for what
 * they write, so the limit here is only a backstop. */
int vsprintf(char *buf, const char *fmt, va_list ap)
{
	return vsnprintf(buf, FMT_MAX, fmt, ap);
}

int sprintf(char *buf, const char *fmt, ...)
{
	va_list ap;
	int n;
	va_start(ap, fmt);
	n = vsnprintf(buf, FMT_MAX, fmt, ap);
	va_end(ap);
	return n;
}

static int is_blank(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* One word of the file, blanks before it skipped. */
static int scan_word(FILE *f, char *out, int max)
{
	int c, n = 0;
	do
		c = fgetc(f);
	while (c != EOF && is_blank(c));
	while (c != EOF && !is_blank(c)) {
		if (n < max - 1)
			out[n++] = (char)c;
		c = fgetc(f);
	}
	out[n] = 0;
	return n;
}

int fscanf(FILE *f, const char *fmt, ...)
{
	char word[256];
	va_list ap;
	int got = 0;
	va_start(ap, fmt);
	for (; *fmt; fmt++) {
		if (*fmt != '%')
			continue;   /* blanks and literals: Quake's are blanks */
		fmt++;
		if (!scan_word(f, word, sizeof word))
			break;
		if (*fmt == 's')
			strcpy(va_arg(ap, char *), word);
		else if (*fmt == 'i' || *fmt == 'd')
			*va_arg(ap, int *) = (int)strtol(word, NULL, *fmt == 'i' ? 0 : 10);
		else if (*fmt == 'f')
			*va_arg(ap, float *) = (float)atof(word);
		else
			break;
		got++;
	}
	va_end(ap);
	return got ? got : EOF;
}
