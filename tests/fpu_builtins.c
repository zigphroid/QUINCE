/* tests/fpu_builtins.c: make test-builtins compiles this with CANNER -S
 * through quince/libc/math.h, and fails if any of these is a call rather
 * than its FPU instruction (M3, LOG 2026-11-12). */
#include <math.h>

float q_sqrtf(float x) { return sqrtf(x); }
float q_fabsf(float x) { return fabsf(x); }
float q_floorf(float x) { return floorf(x); }
float q_ceilf(float x) { return ceilf(x); }

int main(void)
{
	return (int)(q_sqrtf(4.0f) + q_fabsf(-1.0f) + q_floorf(1.5f) + q_ceilf(1.5f));
}
