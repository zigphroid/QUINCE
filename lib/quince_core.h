// QUINCE's core: the part worth testing. Plain C, no kernel calls, no
// pectin.h, so the same file builds for the board (CANNER) and for the
// host test (cc, under the sanitizers). Grow the program here; keep the
// screen and the files in quince/QUINCE.c.
#ifndef QUINCE_CORE_H
#define QUINCE_CORE_H

// Writes "Hello from WHO." into buf, never more than size bytes, always
// NUL-ended (when size > 0), and returns buf. WHO is "a jar" when who is
// NULL or empty.
char *quince_greeting(char *buf, unsigned size, const char *who);

// The checks: the host test runs them (make test), and `QUINCE /SELFTEST`
// runs the same ones on the board and on SIMMER, under CANNER's code. A
// check that passes on the host and fails there is a compiler bug or code
// that leans on something the host gives for free; either is worth
// knowing. Calls fail(what, got, want) for each one that fails, counts the
// ones it ran into *ran, and returns how many failed.
int quince_selftest(void (*fail)(const char *what, const char *got, const char *want),
                    int *ran);

#endif
