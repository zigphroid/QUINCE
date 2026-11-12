# Getting started with QUINCE

This is for you, Jacob, not for Claude: how to get from this new repo to
a program running on your Fruit Jam, with Claude working on it the way
PectinOS itself is built. `CLAUDE.md` is the other half, which Claude reads.

Planted from pectin-rootstock in a Claude project, with no computer of
your own in it? Then "From a Claude project", just below, is your path,
and sections 1 to 3 are for when you want to build at home too.

## From a Claude project

Each new cloud session fetches pectinos and builds CANNER and SIMMER
(the start hook), so Claude builds QUINCE, tests it, and runs it on
SIMMER with no help. For a full-screen program, `make sim-smoke` saves
`build/simsmoke/screen.png`, QUINCE as the board draws it, and Claude can
show it to you in the thread.

Onto your Fruit Jam, two ways:

- **Over the WiFi**, from a public repo: ask for `make dist`. It puts
  `QUINCE.JAM` and `PICKJAM.BAT` in `dist/`, to commit in a pull request,
  and prints the lines to type on the board. Once it's merged (GitHub's
  copy can lag a few minutes), the first time:

  ```
  CD J:\JAMS
  PICK https://github.com/YOU/REPO/raw/main/dist/PICKJAM.BAT PICKJAM.BAT
  ```

  (PICK follows GitHub's redirect to the file.) The prompt takes one
  screen row, so for a long `YOU/REPO` `make dist` prints the line
  without `https://`; that one's first step is plain http, which anyone on
  your network could answer with a file of their own, so type it only on
  a network you trust, and `TASTE PICKJAM.BAT` after. `PICKJAM` itself
  is https all the way, and replaces the program only with a whole
  download.

  and each time after, `PICKJAM YOU/REPO QUINCE` (the name in capitals,
  as `dist/` has it). PICK needs the WiFi: `WIFI_SSID` and `WIFI_PASS`
  in `J:\RECIPE.SYS`, `MARKET` to check.
- **Through your computer**: link it to the thread in the Claude app,
  with a folder Claude may write in. Claude puts `QUINCE.JAM` there;
  copy it into `JAMS` on the SD card.

The board runs PectinOS from a release as new as `PECTIN_REV`, or newer:
an older one refuses the program with `Not a jar. (needs a newer
PectinOS)`. Pinned to a commit that isn't a release (no release could
plant it yet), it needs PectinOS built at the pin: ask Claude, which
builds `build/pectinos.uf2` in the cloud (`make -C $PECTIN_ROOT`) and
gives it to you; drag it onto the `RP2350` drive in BOOTSEL mode.

## 1. Your computer

- `make`, Python 3, a C compiler (`cc`: on a Mac, Xcode's command-line
  tools), and Arm's GNU toolchain (`arm-none-eabi-gcc`).
- `python3 -m pip install pyserial` for the board's console
  (`make send`, `jamterm.py`).
- [`tio`](https://github.com/tio/tio) to type at the board yourself
  (`tio /dev/cu.usbmodemNNNN`). Close it before `make send`.
- The pectinos checkout, at the commit this repo pins. Do this on every
  computer you build on, from this repo's directory:

  ```sh
  git clone https://github.com/zigphroid/pectinos ~/pectinos
  git -C ~/pectinos checkout $(cat PECTIN_REV)
  ```

  Somewhere other than `~/pectinos` works too: set `PECTIN_ROOT` to it.
  Neither the checkout nor this repo can sit in a path with a space in
  it (`~/My Projects`), because make splits paths at spaces. The Makefile
  stops and says so if one does.
  The checkout is read-only. You never change it, you only move the pin
  (`make pin`, below).
- SIMMER, which is PectinOS in QEMU: run `make -C ~/pectinos sim-qemu`
  once. It takes several minutes, and `~/pectinos/docs/SIMMER.md` lists
  what it needs. After that, `make sim` shows QUINCE in a window and
  `make sim-smoke` tests it with no window.

Check it all works:

```sh
make test && make && make sim-smoke
```

## 2. The board running PectinOS

You need a Fruit Jam, a USB-C cable, an HDMI monitor, a USB keyboard and a
microSD card (FAT32, MBR). A debug probe is optional. pectinos's README,
"Quick start" sections 1, 4 and 5, has the details:
<https://github.com/zigphroid/pectinos#quick-start>. Skip its build and
flash steps: this repo does those for you.

Put the board in BOOTSEL mode: hold BUTTON1, tap reset, let go. Then, from
this repo:

```sh
make os      # builds PectinOS at the commit PECTIN_REV pins and loads it
make send    # puts QUINCE.JAM on the card over USB-C
```

`make os` uses `picotool`. If you don't have it, drag
`~/pectinos/build/pectinos.uf2` onto the `RP2350` drive instead. Then type
`QUINCE` at the board's prompt.

Keep the board on this pin. A PectinOS older than the CANNER that built
`QUINCE.JAM` refuses it with `Not a jar. (needs a newer PectinOS)`. When
you move the pin, run `make os` again.

## 3. GitHub

Create an empty repo on GitHub (no README or licence; this one has them),
then:

```sh
git remote add origin git@github.com:YOU/quince.git
git push -u origin main
```

## 4. Claude

These steps make Claude work on QUINCE the way it works on PectinOS. It
plans first, explains in depth, has every PR reviewed by a separate
session, and merges only when you say so.

**A Claude project with only this repo.** Claude Code runs a repo's hooks
only in a session that holds that one repo, so don't add pectinos,
unless it's private and you have access (it is until its first public
release): then the start hook can't clone it, you add
`zigphroid/pectinos` to the project, and each session runs the hook by
hand (CLAUDE.md, "Cloud sessions"). This
repo's start hook clones pectinos at the pin, builds CANNER and SIMMER, and
sets `PECTIN_ROOT` for each new cloud session, so there's no setup script
to write. The cloud environment's network must reach github.com (pectinos,
and QEMU through its GitHub mirrors) and Ubuntu's package mirrors (the hook
installs Arm's GCC and QEMU's build packages with apt). The first session
takes several minutes to build QEMU. If its start hook says SIMMER setup
failed, `make test` and `make` still work, but `make sim-smoke` won't.

**The project's instructions.** Paste this into the project's instructions:

> Work follows CLAUDE.md in the repo. Plan each milestone in chat and wait
> for my OK before writing its code. When an implementation thread believes
> its PR is ready to merge, it says so in its thread, and a separate
> reviewer thread is started for that PR. The reviewer checks that the
> code does what the plan says, that test coverage is good (run `make
> test`, `make` and `make sim-smoke`; there is no CI), and that the
> documentation (README, docs/, CLAUDE.md, the COOKBOOK page, LOG.md, code
> comments) matches what the PR does. Minor issues are filed as GitHub
> issues on this repo labelled `later`. Bugs, test gaps or inaccurate docs
> go back to the implementation thread to fix, then the PR is reviewed
> again. I merge only after the reviewer passes it, and only on an OK that
> names the PR number. Problems in PectinOS or its SDK are filed as issues
> on zigphroid/pectinos, never patched in the checkout.

**The board.** A cloud session can't reach the board's USB (it can
reach its WiFi: "From a Claude project", above). For board work,
start a Claude Code session on the computer the Jam is plugged into, in
this repo's checkout there. That can be a Remote Control session started
from the project, or `claude` in a terminal. The `pectin-board` skill
(`.claude/skills/`) tells it how to send, run, look and listen. It asks
you only for what needs a person.

**The first message.** Something like:

> Read CLAUDE.md and $PECTIN_ROOT/sdk/APP-CLAUDE.md. I want QUINCE to
> (what it should do). Let's plan M1 together. Tell me the choices that
> are mine.

## 5. Where things are

| | |
|---|---|
| `quince/QUINCE.c` | the program: the screen, the files, the board's side |
| `quince/QUINCE.VRB` | its argument template (what the shell accepts) |
| `quince/QUINCE.md` | its COOKBOOK page (`COOKBOOK QUINCE` on the board) |
| `quince/sys_quince.c` | Quake's system layer on the board: files, the clock, the hunk, `/TIMEDEMO` and `/SERVER` |
| `quince/qlibc.c`, `quince/libc/` | the C library pieces Quake needs that the SDK hasn't got (`FILE`, `sprintf`, `malloc` on `mem_alloc`) |
| `quake/` | id Software's Quake source, GPL, with QUINCE's changes marked `QUINCE:` (`quake/README.md`) |
| `lib/` | the part worth testing: no kernel calls, so it builds on the host too |
| `lib/pbgl.c`, `lib/GL/` | the GL layer under GLQuake's renderer: it writes the pixelboard's 3D commands (`lib/pb3d.h`, `lib/pb3d.c`), counted and not yet drawn |
| `lib/qbench.c`, `lib/qbargs.c` | the two benchmarks, and their words |
| `lib/selftest.c` | the checks, run by `make test` and by `QUINCE /SELFTEST` |
| `lib/pbmodel.c` | the C model of PB's triangle engine, the spec its gateware is held to (`docs/PB3D.md`); not in the .JAM |
| `tests/` | the host test's runner, and host-only checks |
| `tests/test_pbmodel.c`, `tests/png.c` | the model held to its rules, and a scene it draws: `build/pbmodel/scene.png` |
| `tests/sys_host.c`, `tests/test_quake.py` | `make test-quake`: Quake on the PC (32-bit, `-m32`), against `QUINCE_PAK` |
| `tests/fpu_builtins.c` | `make test-builtins`: `math.h`'s `sqrtf` and the rest are FPU instructions under CANNER |
| `tools/simsmoke.py` | `make sim-smoke`: SIMMER, driven by pectinos's `simdrive.py` |
| `tools/simbench.py` | `make sim-bench`: the benchmarks on SIMMER, against `QUINCE_PAK` |
| `tools/PICKJAM.BAT` | the board's side of `make dist`, copied into `dist/` |
| `dist/` | `make dist`'s output, committed: what the board's `PICKJAM` fetches |
| `docs/PB-ASKS.md`, `docs/PB3D.md` | what Quake asks of PB, and the rules PB draws a triangle by |
| `docs/ROADMAP.md`, `docs/LOG.md` | the milestones, and the decisions with their reasons |
| `.claude/` | the hooks (setup, and `make test` before a turn ends), the skills |
| `PECTIN_REV` | the pectinos commit this builds against |
| `VERSION` | QUINCE's version, one line: the store's crate takes it |

The SDK's tour is `~/pectinos/sdk/README.md`. The model programs to read are
`~/pectinos/apps/examples/larder.c` (full screen) and
`~/pectinos/apps/TEMPLATE/` (command line). Questions about PectinOS go to
its repo's issues.


## 6. Into the store

When QUINCE is ready for others, it can go in the store, the
`zigphroid/pectin-crates` repo, so that `GROCER FETCH QUINCE` on anyone's
board installs it. The repo has to be public on GitHub, with every
commit pushed.

```sh
make preflight   # builds build/QUINCE.CRT and checks it as the store will
make submit      # opens the store's pull request
```

`make preflight` runs `make test`, `make sim-smoke` and `make page-check`
(a run that fails is a FAIL), builds the crate
again from a fresh clone of the commit, and checks it:
- against the store as it is now: the name, the version, the crates it needs;
- the card paths in the source: its own are `J:\LARDER\QUINCE`,
  `J:\KEEP\QUINCE` and `J:\RECIPES\QUINCE`;
- the page (`make page-check`), the settings, the API and the size;
- that the commit is pushed and public;
- that nothing in it looks like a password.

Each line is OK, WARN, FAIL or SKIP. `make submit` takes a run with no
FAIL and no SKIP.

`make submit` needs the `gh` command, signed in (`gh auth login`), to
fork the store and open the pull request. Without it, it leaves the
branch ready and prints the steps. `make submit DRY_RUN=1` goes as far
as the branch and pushes nothing, even past a FAIL: a way to try it
before the repo is public. The store's owner reads the source
and merges, and the next publish puts QUINCE in the index.

Raise `VERSION` for each new submission: the store takes only a newer one.
Files the program ships go in `larder/` (they install as
`J:\LARDER\QUINCE\`). Settings to start from go in `seeds/` as
`seeds/QUINCE.RCP` (they install as `J:\RECIPES\QUINCE.RCP`, only where
there isn't one already). `CRATE_NEEDS := SDK` in the Makefile names
crates it needs.
