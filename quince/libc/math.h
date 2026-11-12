/* quince/libc/math.h: the math Quake calls, for id's files under CANNER.
 *
 * QUINCE's own (M1). The SDK's sdk/lib/math.c defines all of these but
 * asin and ceil, which quince/qlibc.c adds; this header stands in for the
 * SDK's math.h so the two can be declared together.
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
#endif
