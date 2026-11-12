/* quince/libc/string.h: the C library's string functions, for id's files.
 *
 * QUINCE's own (M1). The SDK has most of them (kernel/lib/string.c); quince/qlibc.c has the rest Quake calls. Only for
 * CANNER's build: the PC's build uses the PC's own headers.
 */
#ifndef QUINCE_STRING_H
#define QUINCE_STRING_H
/* No <pectin.h> here: it names things Quake names too (net_time), so
 * id's files see only the C library's own names. */
#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int strcasecmp(const char *a, const char *b);
char *strchr(const char *s, int c);

char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strstr(const char *hay, const char *needle);
char *strrchr(const char *s, int c);
#endif
