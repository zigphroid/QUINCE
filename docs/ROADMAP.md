# QUINCE roadmap

Each milestone ends in something that runs on the board. Plan each one in
chat before its code; update this file in the PR that lands or reorders a
milestone.

## M0: frame (done 2026-11-11)

Scaffolded by pectinos's `make new-app` at bafa5bf: `QUINCE.JAM` builds,
its host test and `/SELFTEST` pass, `make sim-smoke` runs it on SIMMER.

## M1: Quake with nothing drawn, timed on the board

Planned 2026-11-11. Can the RP2350 run Quake's game at all, before any
drawing? id's WinQuake source (`quake/`) built by CANNER with the
renderer, video, sound and CD as stubs and the network as loopback only,
its hunk a `mem_alloc` block in PSRAM. Two timings on the board, each with
THERMO's profile:

1. `timedemo demo1`: the client side (demo playback).
2. A server benchmark: e1m1 loaded and a fixed number of server frames run,
   which times QuakeC and the physics that a demo skips.

A GCC `-O2` build is timed beside CANNER's, for measuring only. The
pixelboard's side (drawing) is a later milestone.

- PR 1: id's source in `quake/`, unchanged, and the licence (GPL-2.0 or
  later) and rule 1's exception for it.
- PR 2: the null-renderer build, on the PC first (a timedemo against
  `pak0.pak` when `QUINCE_PAK` names one, else skipped), then CANNER's,
  run on SIMMER.
- PR 3: the board run, with `id1/pak0.pak` on the card: the numbers and
  the profiles, and what they say about feasibility.
