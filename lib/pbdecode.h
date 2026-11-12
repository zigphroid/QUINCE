/* lib/pbdecode.h: PB's 3D commands (lib/pb3d.h) read into the triangle
 * engine's C model (lib/pbmodel.h), as PB's gateware is to read them.
 *
 * Our own code (docs/PB-ASKS.md step 2, PR 2). Like the model, no libc
 * and no kernel calls, and not in QUINCE.JAM.
 */
#ifndef QUINCE_PBDECODE_H
#define QUINCE_PBDECODE_H

#include <stdint.h>
#include "pbmodel.h"

/* One frame's drawing commands, the words between START and SHOW: the
 * model's state goes back to a frame's start (pbm_frame), then each
 * command is carried out in turn. Returns the number of commands, or -1
 * if a command isn't one of lib/pb3d.h's or runs past the end (PB's
 * pb3d_cmd_words gives 0): the commands before it are drawn, it and the
 * rest of the frame are not. */
int pbd_frame(struct pbm *m, const uint16_t *w, uint32_t n);

/* The texture calls (the dump's PB3D_REC_* records): make id from w x h
 * texels of format PB3D_TEX_* with PB3D_TEXF_* flags; change a rectangle
 * of one already made. PBM_OK, or the model's error (PBM_E_SIZE for a
 * side that isn't a power of two). */
int pbd_teximage(struct pbm *m, uint32_t id, uint32_t format, uint32_t flags,
                 uint32_t w, uint32_t h, const uint8_t *texels);
int pbd_texsub(struct pbm *m, uint32_t id, uint32_t x, uint32_t y, uint32_t w,
               uint32_t h, const uint8_t *texels);
void pbd_texfree(struct pbm *m, uint32_t id);

#endif
