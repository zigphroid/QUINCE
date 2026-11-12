# QUINCE(1) - Quake, ported to PectinOS.

## Usage

    QUINCE [name]
    QUINCE /TIMEDEMO demo [/MEM:kb] [/V]
    QUINCE /SERVER map frames [/MEM:kb] [/V]
    QUINCE /SELFTEST

## What it does

QUINCE is id Software's Quake (the GPL source, GLQuake's renderer),
ported. It draws nothing yet: the renderer runs over a GL layer that
writes the commands the pixelboard's 3D engine will take (screen-space
triangles, their state and textures), counts them and drops them, so
QUINCE can time the game and what drawing will cost the board.

`/TIMEDEMO demo` plays a demo as fast as it goes and prints the frames,
the seconds, the frames a second and the time a frame, as Quake's own
`timedemo` works them out. Then, for each frame on average, the
triangles, vertices, primitives and GL calls the renderer made, and what
the pixelboard would be sent for them: its commands' words, of each
kind, and the bytes a second its link would carry at that rate, the
commands and the lightmaps' updates. The primitives dropped as off the
screen, the ones cut at the near plane or at the guard band (2,040
pixels out, past which PB's 12.4 positions can't reach), and any commands
that didn't fit a frame are counted too. The textures sent at load are
counted apart, sent once, and any reloaded during the run are added to
the rate.

`/SERVER map frames` loads a map with a player in it and runs that many
frames of a tenth of a second of game time each: the game's code, its
physics and the renderer. It prints the same lines, and then `state`:
a hash of where every entity ended up (its origin in eighths of a unit,
its angles in 256ths of a turn, its frame and its health) and how many
there are.
Two builds that print the same state played the same game.

Quake's files come from `J:\LARDER\QUINCE`: copy `PAK0.PAK` from your own
Quake (the shareware one is enough) to `J:\LARDER\QUINCE\ID1\PAK0.PAK`.

`/MEM:kb` sets the size of Quake's memory (its hunk) in KB, 5888 unless
given, which every shareware map fits; the rest of the PSRAM goes to
its texture buffers and lightmaps, so a bigger one leaves them too
little. `/V`
also prints Quake's console as it runs.

With no switch, QUINCE fills the screen and says hello: to `name`, or to
a jar when none is given.

`/SELFTEST` runs the checks the host test runs (`make test`), here on
the board's code, and prints how many passed.

## Keys

- `Esc` leaves the hello screen.

## Examples

    J:\>QUINCE /TIMEDEMO DEMO1
    J:\>QUINCE /SERVER E1M1 300

## See also

[[LARDER]], [[COOKBOOK]]
