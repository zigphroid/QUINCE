#!/bin/sh
# Build the span dumper: id's WinQuake source as QUINCE imported it
# (QUINCE commit 19eee2d, quake/, before any QUINCE: change), id's software
# renderer, quake.patch (the hooks), and this folder's three files. Needs gcc -m32.
#   tool/build.sh QUINCE_CHECKOUT OUTDIR      -> OUTDIR/spans
set -e
Q=$1; O=$2; T=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$O/src"
git -C "$Q" archive 19eee2d quake | tar -x -C "$O/src"
cd "$O/src/quake"
patch -s -p1 < "$T/quake.patch"
cp "$T"/spandump.c "$T"/spandump.h "$T"/vid_spans.c "$T"/sys_spans.c .
SRC="cd_null.c chase.c cl_demo.c cl_input.c cl_main.c cl_parse.c cl_tent.c cmd.c
 common.c console.c crc.c cvar.c d_edge.c d_fill.c d_init.c d_modech.c d_part.c
 d_polyse.c d_scan.c d_sky.c d_sprite.c d_surf.c d_vars.c d_zpoint.c draw.c host.c
 host_cmd.c in_null.c keys.c mathlib.c menu.c model.c net_loop.c net_main.c
 net_none.c net_vcr.c nonintel.c pr_cmds.c pr_edict.c pr_exec.c r_aclip.c r_alias.c r_bsp.c
 r_draw.c r_edge.c r_efrag.c r_light.c r_main.c r_misc.c r_part.c r_sky.c
 r_sprite.c r_surf.c r_vars.c sbar.c screen.c snd_null.c sv_main.c sv_move.c
 sv_phys.c sv_user.c view.c wad.c world.c zone.c
 spandump.c vid_spans.c sys_spans.c"
# -U__i386__: id386 0, so the C span code runs, not id's x86 assembly.
# -m32: id's renderer keeps pointers in ints. SSE floats, nothing fused,
# as QUINCE's test-quake builds (the M33's rounding).
gcc -m32 -msse2 -mfpmath=sse -ffp-contract=off -U__i386__ -std=gnu89 -fcommon \
    -w -O2 -g -o "$O/spans" $SRC -lm
echo "built $O/spans"
