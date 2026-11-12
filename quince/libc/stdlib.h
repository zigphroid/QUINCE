/* quince/libc/stdlib.h: QUINCE's own (M1), for id's files under CANNER.
 * exit, atof and strtod are the SDK's (sdk/lib/pectin.c); the rest are quince/qlibc.c's. */
#ifndef QUINCE_STDLIB_H
#define QUINCE_STDLIB_H
/* No <pectin.h> here: it names things Quake names too (net_time), so
 * id's files see only the C library's own names. */
#include <stddef.h>

void exit(int code);
double atof(const char *s);
double strtod(const char *s, char **end);

#define RAND_MAX 0x7fff
int rand(void);
void srand(unsigned seed);
int atoi(const char *s);
long strtol(const char *s, char **end, int base);
int abs(int x);
/* From the program's PSRAM pool (mem_alloc), not a heap of its own. */
void *malloc(size_t n);
void free(void *p);
#endif
