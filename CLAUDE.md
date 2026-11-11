# CLAUDE.md: pectin-rootstock (not planted yet)

This repo is a **rootstock**: the start of a program for PectinOS, the
DOS-like operating system for the Adafruit Fruit Jam (RP2350), before it
has a name. The `.rootstock` file says it hasn't been planted. Your first
job in it is to plant the owner's program, in a cloud session, with no
computer of theirs involved. pectinos's `tools/newapp.py --here` writes
the program into this checkout: its source, host test, SIMMER smoke test,
COOKBOOK page, a `CLAUDE.md` that replaces this one, and the hooks. Once
it's merged, every later session works from that `CLAUDE.md`.

The owner is new to PectinOS. Say what each step does and why, briefly,
and keep the posts short and direct.

## 1. The pectinos checkout

Everything builds against a pectinos checkout, `PECTIN_ROOT`: the SDK,
CANNER (the C compiler that builds every program), SIMMER (PectinOS in
QEMU) and the tools.

- Run the start hook by hand once before planting, even when
  `PECTIN_ROOT` is already set: a session holding several repos (a
  project that also holds `zigphroid/pectinos`) runs no repo's hooks, and
  where it did run, running it again is quick.
  `CLAUDE_CODE_REMOTE=true CLAUDE_PROJECT_DIR="$PWD" .claude/hooks/session-start.sh`
  It finds pectinos (`$PECTIN_ROOT`, `~/pectinos` or a sibling
  `../pectinos`) or clones it, moves it to the release to plant at, and
  builds CANNER, then SIMMER: several minutes the first time. Give it a
  long timeout (30 minutes). Its last lines say where pectinos is
  (`PECTIN_ROOT=...`): `export PECTIN_ROOT=...` in each command after,
  since running it by hand doesn't set it for the session.
- If the start hook says it couldn't clone `zigphroid/pectinos`: either
  the cloud environment's network doesn't reach github.com (README.md,
  step 2, says what it needs), or pectinos is still private (it is until
  its first public release). Tell the owner which, with what to change:
  for a private pectinos, adding `zigphroid/pectinos` to this Claude
  project, which needs access to it.

The hook leaves pectinos at the newest release tag that has the
rootstock in it, or, until there is one, where the checkout is (a fresh
clone's main), and says which. That commit becomes the program's pin
(`PECTIN_REV`), and the board must run PectinOS from that release or
newer. Planted at a commit that isn't a release, no released PectinOS
will run the program (`Not a jar. (needs a newer PectinOS)`): the board
needs PectinOS built at the pin (4, below).

## 2. Ask the owner, once

In one message, with a suggestion for each:

- **The name**: 1 to 8 letters and digits, a letter first. It's the
  `.JAM` file's name and what they type at the `J:\>` prompt. `newapp.py`
  refuses a built-in command's name or an alias's.
- **Screen or command**: `screen`, a full-screen program (a game is one),
  or `cli`, a command that prints and returns.
- **What it does**, in one line.
- **Their name**, as the new `CLAUDE.md` and the licence will give it.

## 3. Plant it

```sh
python3 "$PECTIN_ROOT/tools/newapp.py" --here --dir . --name NAME \
    --owner "THEIR NAME" --kind screen --what "WHAT IT DOES"
```

It writes the program over this rootstock, builds it with CANNER, runs
its host test and page check, and commits on the current branch. A
failure there is a pectinos bug: report it to the owner with the output.

Then `make sim-smoke` if the hook said SIMMER is ready. For a screen
program it saves `build/simsmoke/screen.png`, the program as the board
draws it: attach it so the owner sees their program running.

## 4. Hand it over

Push the branch and open a pull request into `main`. In the reply, say
what was made (the table in the new `GETTING-STARTED.md`, "Where things
are", is the tour), and that once they merge it, a **new** thread starts
from the planted `CLAUDE.md`, which plans the program's first milestone
with them. Point them at `GETTING-STARTED.md`'s "From a Claude project"
for getting it onto their Jam. If the hook said no release can plant
yet, say so plainly: their board needs PectinOS built at the pin, which
you can build here (`make -C "$PECTIN_ROOT"` writes
`build/pectinos.uf2`, the first-install image) and give them, through
their linked computer or attached to your reply; they drag it onto the
`RP2350` drive in BOOTSEL mode (hold BUTTON1, tap reset).

Don't start designing or writing the program itself here: the planted
`CLAUDE.md` has the way of working for that (plan first, the owner's
OK, reviews), and this session started from this one.
