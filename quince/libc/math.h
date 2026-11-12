/* quince/libc/math.h: the math Quake calls, for id's files under CANNER.
 *
 * QUINCE's own (M1). The SDK's sdk/lib/math.c defines all of these but
 * asin and ceil, which quince/qlibc.c adds; this header stands in for the
 * SDK's math.h so the two can be declared together.
 *
 * The float ones the FPU does in one instruction (VSQRT, VABS, VRINTM,
 * VRINTP) are spelled as the compilers' built-ins, as the SDK's math.h
 * spells them (sdk/math.h, "The ones the FPU does in one instruction").
 * -ffreestanding stops GCC knowing the names, and CANNER only makes the
 * instruction for the built-in, so without these a sqrtf was a call to
 * sdk/lib/math.c's integer sqrtf: 885 samples of GCC's demo1 (M3, LOG
 * 2026-11-12). The host build (no __arm__) calls the C library's.
 */
#ifndef QUINCE_MATH_H
#define QUINCE_MATH_H
double sqrt(double x);
double fabs(double x);
double floor(double x);
double ceil(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double atan(double x);
double atan2(double y, double x);
double asin(double x);
double pow(double x, double y);
float sqrtf(float x);
float fabsf(float x);
float floorf(float x);
float ceilf(float x);
float sinf(float x);
float cosf(float x);
#if defined(__arm__)
#define sqrtf(x)    __builtin_sqrtf(x)
#define fabsf(x)    __builtin_fabsf(x)
#define floorf(x)   __builtin_floorf(x)
#define ceilf(x)    __builtin_ceilf(x)
#endif
#endif
