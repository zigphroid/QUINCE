/* lib/qrand.c: Quake's rand and srand, on the board and on the PC alike.
 * Our own code (M1).
 *
 * A 32-bit linear congruential generator, the constants from Knuth's
 * TAOCP vol. 2 §3.3.4's table (Numerical Recipes' pair); the high bits are
 * the good ones, so the 15 returned come from there. Quake only needs it
 * to look random, and to be the same from run to run. Both builds take
 * this one, not the PC's libc's, so the server benchmark makes the same
 * random choices on both (QuakeC's random(), the particles) and its
 * counts can be held to each other (tests/test_quake.py, tools/simbench.py).
 */
#include <stdint.h>
#include <stdlib.h>

static uint32_t rand_state = 1;

int rand(void)
{
	rand_state = rand_state * 1664525u + 1013904223u;
	return (int)((rand_state >> 16) & 0x7fff);
}

void srand(unsigned seed)
{
	rand_state = seed;
}
