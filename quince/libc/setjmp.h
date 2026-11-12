/* quince/libc/setjmp.h: setjmp and longjmp for GCC's board build (make gcc).
 *
 * QUINCE's own (M1). CANNER never reads this file: <setjmp.h> is one of
 * its own headers, found before any -I directory, and the pair is its own
 * code (apps/canner/README.md, "The rest of the language"). GCC's build
 * links no C library, so here the pair is GCC's builtins, which save and
 * restore the frame inline, with no library function behind them.
 *
 * The builtins' limits, which Quake's one use (host.c) keeps: longjmp's
 * value is always 1 (host.c passes 1), and longjmp is never called from
 * the function that called setjmp (Host_Error and Host_EndGame jump back
 * into _Host_Frame). The buffer is five words, as GCC's manual says.
 */
#ifndef QUINCE_SETJMP_H
#define QUINCE_SETJMP_H

typedef void *jmp_buf[5];

#define setjmp(env)      __builtin_setjmp(env)
#define longjmp(env, v)  __builtin_longjmp((env), 1)

#endif
