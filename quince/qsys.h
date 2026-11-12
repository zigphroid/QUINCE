/* quince/qsys.h: the SDK calls sys_quince.c and id's files need, as real
 * functions (QUINCE's own, M1).
 *
 * pectin.h's file, memory and clock calls are static inlines (each one a
 * single kernel call, sdk/pectin.h), and a file that includes quakedef.h
 * can't include pectin.h too: some names are in both (net_time). So
 * quince/qlibc.c, which includes pectin.h, wraps the ones needed here.
 */
#ifndef QUINCE_QSYS_H
#define QUINCE_QSYS_H

#define QSYS_READ   0   /* PECTIN_READ */
#define QSYS_CREATE 1   /* PECTIN_CREATE */
#define QSYS_SET    0   /* PECTIN_SEEK_SET */
#define QSYS_END    2   /* PECTIN_SEEK_END */

int qsys_open(const char *path, int mode);
int qsys_read(int h, void *buf, unsigned n);
int qsys_write(int h, const void *buf, unsigned n);
int qsys_close(int h);
int qsys_lseek(int h, int off, int whence);
int qsys_mkdir(const char *path);
int qsys_unlink(const char *path);
void *qsys_alloc(unsigned n);
unsigned qsys_largest(void);   /* the biggest block the pool could give now */
unsigned qsys_ticks_ms(void);
int eprintf(const char *fmt, ...);

#endif
