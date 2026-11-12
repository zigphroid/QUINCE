/* quince/libc/stdio.h: a small FILE over PectinOS handles, for id's files.
 *
 * QUINCE's own (M1). The SDK deliberately has no FILE* (sdk/README.md,
 * "What is deliberately not here"), but Quake reads and writes its
 * config, its saves and its demos through one, so quince/qlibc.c gives it
 * the part Quake uses: no buffering of its own beyond what a getc needs,
 * since Quake reads big files through Sys_FileRead instead.
 */
#ifndef QUINCE_STDIO_H
#define QUINCE_STDIO_H
/* No <pectin.h> here: it names things Quake names too (net_time), so
 * id's files see only the C library's own names. */
#include <stddef.h>
#include <stdarg.h>

/* The SDK's (sdk/lib/pectin.c, kernel/lib/format.c). */
int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int puts(const char *s);
int snprintf(char *buf, unsigned size, const char *fmt, ...);
int vsnprintf(char *buf, unsigned size, const char *fmt, va_list ap);

typedef struct quince_file FILE;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
size_t fread(void *buf, size_t size, size_t n, FILE *f);
size_t fwrite(const void *buf, size_t size, size_t n, FILE *f);
int fseek(FILE *f, long off, int whence);
long ftell(FILE *f);
int fgetc(FILE *f);
#define getc(f) fgetc(f)
int fputc(int c, FILE *f);
int feof(FILE *f);
int fflush(FILE *f);
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
/* Only what Quake's loadgame reads: %s, %i, %d and %f, and blanks. */
int fscanf(FILE *f, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list ap);
#endif
