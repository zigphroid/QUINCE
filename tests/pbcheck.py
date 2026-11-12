#!/usr/bin/env python3
"""tests/pbcheck.py: read a QUINCE_PBDUMP file and check it holds to lib/pb3d.h.

Our own code (M2 PR 2). A second reader of the format, written apart from
the C that writes it, so a mistake in one shows as a disagreement:

    tests/pbcheck.py FILE [--last N]

Checks every record: the magic and version, each record's length, the
palette before any texture, each texture's size (powers of two up to
1,024) and its texels' bytes, each update inside a texture already made,
and each frame's words as whole commands: a known command, an argument
with no stray bits, a TRIS cut short by nothing, a texture bound that was
made, a CLIP on the 640x480 surface, a DEPTHRANGE with far at or above
near, and a TRIS whose vertices carry the coordinates of each unit
STATE has on. Prints a line of totals, and the
biggest frame; with --last N, a second line of the last N frames' words,
vertices and triangles, which are the timed window's, to hold against the
layer's own "sent totals" for the same run. Exits 1 on the first thing
wrong, saying what and where.
"""
import struct
import sys

MAGIC, VERSION = 0x33425051, 2
WIDTH, HEIGHT = 640, 480   # lib/qvid.c's surface
PALETTE, TEXIMAGE, TEXSUB, FRAME, TEXFREE = 1, 2, 3, 4, 5
INDEX8, RGBA8888, GREY8 = 0, 1, 2
DEPTH, STATE, TEXTURE, TRIS, DEPTHRANGE, CLIP = 15, 16, 17, 18, 19, 20


def vertex_words(fmt):
    n = 2
    if fmt & 0x04:
        n += 2      # q
    if fmt & 0x08:
        n += 2      # s, t
    if fmt & 0x10:
        n += 2      # s2, t2
    colour = fmt & 0x60
    n += {0x00: 0, 0x20: 1, 0x40: 2}[colour]
    return n


class Bad(Exception):
    pass


TEX0, TEX1 = 0x0080, 0x0100      # STATE's units
F_ST, F_ST2 = 0x08, 0x10          # TRIS's format bits for their coordinates


def check_frame(words, made, where):
    """The frame's commands, one by one. Returns (commands, vertices, tris)."""
    i, cmds, verts, tris = 0, 0, 0, 0
    state = 0   # a frame starts with every STATE bit 0
    while i < len(words):
        w = words[i]
        op, arg = w >> 8, w & 0xff
        if op == DEPTH:
            if arg & ~1:
                raise Bad("%s word %d: DEPTH with argument %#x" % (where, i, arg))
            n = 1
        elif op == STATE:
            if arg or i + 1 >= len(words) or words[i + 1] & ~0x0fff:
                raise Bad("%s word %d: a bad STATE" % (where, i))
            if words[i + 1] & 0x0003 == 3:
                raise Bad("%s word %d: STATE's depth test 3" % (where, i))
            state = words[i + 1]
            n = 2
        elif op == TEXTURE:
            if arg > 1 or i + 2 >= len(words) or words[i + 2] & ~0x000f:
                raise Bad("%s word %d: a bad TEXTURE" % (where, i))
            tid = words[i + 1]
            if tid and tid not in made:
                raise Bad("%s word %d: TEXTURE %d was never made" % (where, i, tid))
            n = 3
        elif op == DEPTHRANGE:
            if arg or i + 2 >= len(words):
                raise Bad("%s word %d: a bad DEPTHRANGE" % (where, i))
            if words[i + 2] < words[i + 1]:
                raise Bad("%s word %d: DEPTHRANGE's far %#x below its near %#x"
                          % (where, i, words[i + 2], words[i + 1]))
            n = 3
        elif op == CLIP:
            if arg or i + 4 >= len(words):
                raise Bad("%s word %d: a bad CLIP" % (where, i))
            x0, y0, x1, y1 = words[i + 1:i + 5]
            if not (x0 <= x1 <= WIDTH and y0 <= y1 <= HEIGHT):
                raise Bad("%s word %d: CLIP %d, %d to %d, %d isn't on the %dx%d surface"
                          % (where, i, x0, y0, x1, y1, WIDTH, HEIGHT))
            n = 5
        elif op == TRIS:
            kind, fmt = arg & 3, arg & 0x7c
            if arg & 0x80 or kind == 3 or arg & 0x60 == 0x60 or i + 1 >= len(words):
                raise Bad("%s word %d: TRIS argument %#x" % (where, i, arg))
            # A unit that is on reads its coordinates from every vertex
            # (pbgl sends them whenever it is: choose_format).
            if state & TEX0 and not fmt & F_ST:
                raise Bad("%s word %d: TRIS without s, t while TEX0 is on" % (where, i))
            if state & TEX1 and not fmt & F_ST2:
                raise Bad("%s word %d: TRIS without s2, t2 while TEX1 is on" % (where, i))
            nv = words[i + 1]
            if nv < 3:
                raise Bad("%s word %d: TRIS of %d vertices" % (where, i, nv))
            n = 2 + nv * vertex_words(fmt)
            if fmt & 0x04:
                # q, 1.31: never past the near plane's 1.0 by more than rounding
                vw = vertex_words(fmt)
                for k in range(nv):
                    lo, hi = words[i + 2 + k * vw + 2], words[i + 2 + k * vw + 3]
                    q = (hi << 16 | lo) / 2.0 ** 31
                    if q > 1.001:
                        raise Bad("%s word %d: vertex %d's q is %.4f, past the near plane"
                                  % (where, i, k, q))
            verts += nv
            tris += nv // 3 if kind == 0 else nv - 2
        else:
            raise Bad("%s word %d: unknown command %d" % (where, i, op))
        if i + n > len(words):
            raise Bad("%s word %d: command %d cut short" % (where, i, op))
        i += n
        cmds += 1
    return cmds, verts, tris


def main():
    path = sys.argv[1]
    last = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[2] == "--last" else 0
    per_frame = []
    data = open(path, "rb").read()
    if len(data) < 8 or struct.unpack_from("<II", data, 0) != (MAGIC, VERSION):
        raise Bad("%s: not a version %d dump" % (path, VERSION))
    at, palette, made = 8, False, {}
    frames = cmds = verts = tris = words = texbytes = subs = 0
    biggest = (0, 0)
    while at < len(data):
        if at + 8 > len(data):
            raise Bad("byte %d: a record header cut short" % at)
        rtype, zero, length = struct.unpack_from("<HHI", data, at)
        body = data[at + 8:at + 8 + length]
        where = "record at byte %d" % at
        if zero or len(body) != length:
            raise Bad("%s: header or length wrong" % where)
        if rtype == PALETTE:
            if length != 768:
                raise Bad("%s: a palette of %d bytes" % (where, length))
            palette = True
        elif rtype == TEXIMAGE:
            tid, fmt, flags, w, h = struct.unpack_from("<HBBHH", body, 0)
            if not palette and fmt == INDEX8:
                raise Bad("%s: texture %d before the palette" % (where, tid))
            if (not tid or fmt > 2 or flags & ~3 or w & (w - 1) or h & (h - 1)
                    or not 1 <= w <= 1024 or not 1 <= h <= 1024):
                raise Bad("%s: texture %d: format %d flags %#x %dx%d" % (where, tid, fmt, flags, w, h))
            if length != 8 + w * h * (4 if fmt == RGBA8888 else 1):
                raise Bad("%s: texture %d's texels are %d bytes" % (where, tid, length - 8))
            made[tid] = (fmt, w, h)
            texbytes += length - 8
        elif rtype == TEXSUB:
            tid, fmt, zero2, x, y, w, h = struct.unpack_from("<HBBHHHH", body, 0)
            if tid not in made or made[tid][0] != fmt or zero2:
                raise Bad("%s: an update to texture %d, never made as format %d" % (where, tid, fmt))
            _, tw, th = made[tid]
            if w == 0 or h == 0 or x + w > tw or y + h > th:
                raise Bad("%s: update %d,%d %dx%d outside texture %d (%dx%d)"
                          % (where, x, y, w, h, tid, tw, th))
            if length != 12 + w * h * (4 if fmt == RGBA8888 else 1):
                raise Bad("%s: the update's texels are %d bytes" % (where, length - 12))
            subs += length - 12
        elif rtype == FRAME:
            if length & 1:
                raise Bad("%s: a frame of %d bytes" % (where, length))
            fw = struct.unpack_from("<%dH" % (length // 2), body, 0)
            c, v, t = check_frame(fw, made, "frame %d" % frames)
            cmds, verts, tris, words = cmds + c, verts + v, tris + t, words + len(fw)
            per_frame.append((len(fw), v, t))
            if len(fw) > biggest[0]:
                biggest = (len(fw), frames)
            frames += 1
        elif rtype == TEXFREE:
            made.pop(struct.unpack_from("<H", body, 0)[0], None)
        else:
            raise Bad("%s: record type %d" % (where, rtype))
        at += 8 + length + (length & 1)
    print("pbcheck: %d frames, %d words, %d commands, %d vertices, %d triangles, "
          "%d textures (%d bytes), %d bytes of updates; biggest frame %d words (frame %d)"
          % (frames, words, cmds, verts, tris, len(made), texbytes, subs, biggest[0], biggest[1]))
    if last:
        tail = per_frame[-last:]
        print("pbcheck: the last %d frames: %d words, %d vertices, %d triangles"
              % (len(tail), sum(f[0] for f in tail), sum(f[1] for f in tail),
                 sum(f[2] for f in tail)))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Bad as e:
        print("pbcheck: FAIL: %s" % e)
        sys.exit(1)
