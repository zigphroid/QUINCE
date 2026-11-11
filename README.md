# pectin-rootstock

Start a program for [PectinOS](https://github.com/zigphroid/pectinos), the
DOS-like operating system for the Adafruit Fruit Jam, from a Claude
project, with nothing to install on your computer. Fruit trees grow on a
rootstock; your program grows on this one. Claude plants it, builds it,
runs it in SIMMER (PectinOS in QEMU, in the cloud) and shows you its
screen, and the board fetches it over WiFi.

This repo is written by pectinos's `make app-seed`. Fixes go to pectinos.

## 1. Your repo

On GitHub, **Use this template**, then **Create a new repository**. Make
it public if you want the board to fetch your program over WiFi (step 5);
a private one works too, with the other way there.

## 2. A Claude project

Make a new project in Claude and add your new repo, and only it: Claude
Code runs a repo's start hook only in a session that holds that one
repo. The hook fetches pectinos and builds CANNER and SIMMER for each new
cloud session (several minutes the first time).

- The project's cloud environment must reach github.com and Ubuntu's
  package mirrors (the hook installs Arm's GCC and QEMU's build packages
  with apt).
- Until PectinOS's first public release, pectinos is a private repo. If
  you have access to it, add `zigphroid/pectinos` to the project as
  well; Claude then runs the setup by hand in each new thread, as the
  `CLAUDE.md` says (before and after planting).

## 3. The project's instructions

Paste this into the project's instructions:

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

## 4. The first message

> Plant a game here. It's about (your idea).

Claude asks you for the name, whether it's a full-screen program or a
command, a line on what it does, and your name (for its `CLAUDE.md` and
licence). Then it plants it, runs it in SIMMER,
shows you the screen, and opens a pull request. Merge it, and start a new
thread for the program's first milestone: the planted `CLAUDE.md` takes
over from there, and `GETTING-STARTED.md` is your guide.

## 5. On your Fruit Jam

The board needs PectinOS as new as the program's pin, and the WiFi in
`J:\RECIPE.SYS`. From a release: BOOTSEL drag-and-drop of its UF2, and
its card zip unpacked on the SD card. Until PectinOS's first public
release (1.2), no release can run a planted program, so ask Claude for
PectinOS's UF2 built at the pin: it builds it in the cloud and gives it
to you, and you drag it onto the board the same way. Then either:

- **Over WiFi, from a public repo.** Claude runs `make dist`, which puts
  the built program in `dist/` with `PICKJAM.BAT`, and commits it in the
  pull request. Once it's merged, type the two lines `make dist` printed, once,
  to fetch `PICKJAM.BAT`, then `PICKJAM YOU/REPO NAME` each time after.
- **Through your computer.** Link your computer to the thread (the
  Claude app offers it) with a folder Claude may write in. Claude puts
  `NAME.JAM` there; copy it into `JAMS` on the SD card.
