/* tests/fpu_builtins.c: make test-builtins compiles this with CANNER -S
 * through quince/libc/math.h, and fails if any of these is a call rather
 * than its FPU instruction (M3, LOG 2026-11-12; roundf since #32). */
#include <math.h>

float q_sqrtf(float x) { return sqrtf(x); }
float q_fabsf(float x) { return fabsf(x); }
float q_floorf(float x) { return floorf(x); }
float q_ceilf(float x) { return ceilf(x); }
/* lib/pb3d.c's fix16 and pb3d_fix_q round by the built-in itself, the
 * M33's VRINTA, in the GL layer's hottest path (#32). quince/libc/math.h
 * has no roundf, so it is the built-in's spelling that is checked. */
float q_roundf(float x) { return __builtin_roundf(x); }

int main(void)
{
	return (int)(q_sqrtf(4.0f) + q_fabsf(-1.0f) + q_floorf(1.5f) + q_ceilf(1.5f) +
	             q_roundf(0.5f));
}
