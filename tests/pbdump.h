/* tests/pbdump.h: the PC's QUINCE_PBDUMP sink (tests/pbdump.c). */
#ifndef QUINCE_PBDUMP_H
#define QUINCE_PBDUMP_H
#include "pbgl.h"
/* Open path for writing and return a sink that writes there. */
const struct pbgl_sink *pbdump_open(const char *path);
#endif
