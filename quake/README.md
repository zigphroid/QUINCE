# quake/: id Software's Quake source

These files are id Software's WinQuake source, as released under the GNU
GPL on 1999-12-21, taken from `github.com/id-Software/Quake` at commit
`bf4ac424ce754894ac8f1dae6a3981954bc9852d` (its `WinQuake/` files are the
same as at the release commit, `0023db3`). The licence is `../LICENSE`.

They are the one exception to CLAUDE.md's hard rule 1 (Jacob, 2026-11-11,
`docs/LOG.md`). The rules for this directory:

- **Only the portable files came in.** The platform layers for DOS,
  Windows, Linux and Solaris (`sys_`, `vid_`, `gl_vid`, `snd_`, `in_`,
  `cd_` and `net_` files for those), the assembly (`.s`) and the project
  files were left out. Both renderers are here: GLQuake's (`gl_*`), which
  is the one the pixelboard's Glide-style acceleration would run, and the
  software one (`r_*`, `d_*`), for comparison. id's `sys_null.c`, `vid_null.c`, `snd_null.c`, `cd_null.c`,
  `in_null.c` and `net_none.c` are the starting points for QUINCE's own.
- **This commit is id's bytes, unchanged.** Every later change to a file
  here is marked in the file with a `QUINCE:` comment that says what and
  why, so `git diff` against this commit and a grep for `QUINCE:` both
  show all of it.
- **New code is ours and goes outside this directory** (`quince/`, `lib/`),
  under the same licence.
