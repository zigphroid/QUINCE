// tests/png.h: pictures out of the host tests (tests/png.c).
#ifndef QUINCE_PNG_H
#define QUINCE_PNG_H

#include <stddef.h>
#include <stdint.h>

// w x h pixels of RGB565, rows from the top, as an 8-bit RGB PNG. 0 if
// written. w at most 4,096.
int png_write_565(const char *path, const uint16_t *px, int w, int h);
// The CRC-32 PNG and zip use: crc 0 to start, then the last one's.
uint32_t png_crc32(uint32_t crc, const uint8_t *p, size_t n);

#endif
