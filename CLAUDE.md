# CLAUDE.md: QUINCE

QUINCE.JAM is a program for PectinOS, the DOS-like operating system for
the Adafruit Fruit Jam (RP2350). It's Jacob's, in its own repo, built
against a pectinos checkout (`PECTIN_ROOT`). What it does: Quake, ported to PectinOS.
It ports id Software's GPL Quake source (the WinQuake tree, `quake/`) to
the RP2350 and the pixelboard. Milestone 1 builds it with a renderer that
draws nothing and times it on the board, to find out whether the CPU and
the PSRAM behind the XIP cache can run Quake at all (`docs/ROADMAP.md`).

This is a **learning project**. Getting a working program matters, but
Jacob understanding *why* it works matters more: what the SDK and the
kernel do underneath a call, why the code has the shape it has, and what
the alternatives would have cost.

The PectinOS rules that apply to every program live in pectinos, so they
change in one place. This file links to them; read the parts a task
touches:

- `$PECTIN_ROOT/sdk/APP-CLAUDE.md`: the out-of-tree notes (the SDK's
  headers, its limits, the CANNER subset, tests, the big-file and
  saved-behind-another-burner rules). Read it at the start of a session.
- `$PECTIN_ROOT/docs/STYLE.md`: names, messages, code and docs.
- `$PECTIN_ROOT/docs/HUMAN-INTERFACE.md`: keys, screens, the prompt, what
  a program prints.
- `$PECTIN_ROOT/docs/LLM-INTERFACE.md` B3, its "Send any key" bullet: the
  bytes a terminal sends for each modified key, for simdrive and
  jamterm. The rest of that file is about pectinos's own project, not
  this one.

---

## How to work with Jacob

- **Plan in chat before each milestone, and wait for Jacob's OK** before
  writing more than a few lines of code. The plan says what will be built,
  the real tradeoffs, and which choices are Jacob's. A request that only
  asks for a plan ends in a plan, not a PR.
- **Explain as you go, in depth.** When you write a piece, say what the
  SDK, the kernel or the hardware is doing and why the code is shaped the
  way it is, pointing to the doc or source you mean. A few clear
  sentences beat a wall of text.
- **Surface tradeoffs, don't bury them.** When there are real
  alternatives, lay them out briefly, recommend one, and let Jacob
  choose.
- **Small, testable steps**, each ending in something Jacob can see
  run: on SIMMER, or on the board.
- **Log decisions** in `docs/LOG.md` (newest last): the date, the
  decision, the alternatives, and why. One short entry per real decision.
  `docs/ROADMAP.md` holds the milestones: update it in the PR that lands
  or reorders one.
- **Posts are short, factual and direct.** Lead with the answer or the
  question. Say what you checked, and say "inferred" when you didn't.
- **Keep memory.** Save what a fresh session would otherwise have to ask
  Jacob again (their preferences, decisions made in chat, where things
  stand) to project memory, one file per subject, in the turn you learn
  it. Not what the repo already records.

## Pull requests

- **There is no CI.** Before saying a PR is ready, run `make test` (the
  host suite), `make` (CANNER's subset is narrower than the host
  compiler's) and, when the change reaches the board's code, `make
  sim-smoke`. Say plainly what failed or was skipped. The Stop hook runs
  `make test` before a turn that changed code can end.
- **Every PR gets a separate, adversarial review** before it merges: code
  against the plan, test coverage (would a test fail if the change were
  wrong?), and every doc the change makes false, including ones the diff
  didn't touch. In a Claude project, that's a new reviewer thread. In
  plain Claude Code, it's a fresh session given the PR and nothing else.
  The reviewer sorts what it finds. Minor issues become GitHub issues on
  this repo labelled `later`. Bugs, test gaps and wrong docs go back to
  the implementer, and the PR is reviewed again after the fix.
- **Merging needs Jacob's OK, naming the PR number.** An earlier "merge
  them" doesn't carry over to a later PR. Anything outward-facing or hard
  to undo waits for them too.
- **Docs travel with behaviour.** A change to the keys, switches or
  settings updates `quince/QUINCE.md` (the COOKBOOK page: `make
  page-check`), `QUINCE.VRB` (the template) and `QUINCE.PRF` (settings)
  in the same PR.

## Hard rules

1. **We always write our own.** The program is our own C (C11,
   freestanding) plus the PectinOS SDK. No third-party libraries, no
   copied code. Reading references (standards, RFCs, datasheets, other
   programs' source) is encouraged; copying isn't. When code follows a
   reference closely, say so in a comment and name the source.
   **The one exception is id Software's Quake source** (GPL, 1999), which
   QUINCE ports: it lives in `quake/`, brought in unchanged at a named
   commit, and every change to it is marked `QUINCE:` in the file
   (`quake/README.md`; Jacob, 2026-11-11). Our own code stays outside
   `quake/`, and the repo is GPL-2.0-or-later to match.
2. **CANNER builds the program.** Keep every source inside CANNER's subset
   (`$PECTIN_ROOT/apps/canner/README.md`'s refusal table): no inline
   assembly, no bit-fields, no statement expressions, no
   `packed`/`weak` attributes, no VLAs. `lib/` takes no kernel calls and
   no `pectin.h`, so the same file builds for the host test.
3. **pectinos is read-only from here.** Never change the checkout, which
   would make builds that match no one else's. A pectinos bug or a gap in
   the SDK goes to zigphroid/pectinos as an issue, with a small case that
   shows it and the pinned commit. Work round it here meanwhile, and log
   the workaround.
4. **Fixtures yes, secrets never.** Test data may be committed under
   `tests/fixtures/` with a note on where it came from. Passwords, keys
   and account names never are: tests read them from the environment
   (`QUINCE_...`) and skip, saying why, when they're absent. On the card,
   secrets are `/SECRET` settings (`QUINCE.PRF`, `SEASON QUINCE`).
5. **The jam theme.** Messages and names follow `$PECTIN_ROOT/docs/STYLE.md`
   (`Headspace exceeded.`, `Seal broken. Discard contents.`, and so on),
   and the shared keys follow HUMAN-INTERFACE.md: Esc backs out or leaves,
   Ctrl+F finds, Ctrl+C copies.

## Testing, cheapest first

1. **`make test`**: the host suite under ASan and UBSan. `lib/selftest.c`
   holds the checks, so `QUINCE /SELFTEST` runs the same ones on the board.
   Put new checks there unless they need the host (fixtures, files).
   With `QUINCE_PAK` naming the shareware `id1/pak0.pak` (never
   committed), `make test` also runs `make test-quake`: Quake built for
   the PC (`-m32`, which needs `gcc-multilib`; it skips without one),
   its two benchmarks, and every shareware map loaded. The PC build
   rounds floats as the board does (SSE, nothing fused, the SDK's
   `math.c`), so e1m1's state and checksum are the board's, bit for bit.
   `make test-builtins` checks with CANNER that `math.h`'s `sqrtf` and
   the rounding functions are FPU instructions, not calls, and `make
   test-float` that the functions in `tests/float_hot.txt` call no more
   double code than it allows. A new `QUINCE:` float edit to a hot
   function goes on that list. `make test-builds` builds `make heat`
   and `make gcc` (GCC's skips without `arm-none-eabi-gcc`), without
   running them.
2. **`make sim-smoke`**: SIMMER, PectinOS in QEMU, with no window. It runs
   `/SELFTEST` on CANNER's code, then the program, then reads its screen
   back as text. The `simmer` skill has the recipes. `make sim-bench
   QUINCE_PAK=...` runs Quake's benchmarks there, CANNER's build, and
   holds the server's counts, state and checksum to the PC's.
3. **The board**, through a Claude Code session on the computer it's
   plugged into. Use the serial console, the screen viewer and the sound
   recorder (the `pectin-board` skill). Ask Jacob only for what a
   script can't judge (does it look right, does it sound right, how it
   feels, hands on the hardware), with exactly what to run and what they
   should see.

## Build

- `make preflight` → `build/QUINCE.CRT` checked as the store will check it
  (`$PECTIN_ROOT/tools/appcrate.py`): no FAIL and no SKIP before `make
  submit`, which opens the store's PR (GETTING-STARTED, "Into the store").
  `VERSION` goes up with each submission; it's the owner's call when.
- `make` → `build/QUINCE.JAM` · `make test` · `make page-check` ·
  `make sim-smoke` (for a screen program, `build/simsmoke/screen.png`
  too: attach it to show Jacob the screen from a cloud session) ·
  `make sim` (a window) · `make send` (to the board's card) · `make dist`
  (the .JAM and `PICKJAM.BAT` in `dist/`, committed, for a board that
  fetches it over the WiFi: GETTING-STARTED, "From a Claude project") ·
  `make os` (the pinned PectinOS onto the board) · `make pin` (move to
  the checkout's HEAD, in a PR of its own, below) · `make heat` (the
  cold code, by `quince/QUINCE.HOT`, a board run's THERMO heat file, out
  of the jar: `build/heat/`) · `make gcc` (GCC -O2 through the SDK's
  flags, to compare, the menus and the selftest run from PSRAM:
  `build/gcc/`). `sim-smoke`, `sim-bench` and `send`
  take either with `QUINCE_JAM=build/heat/QUINCE.JAM`.
- `PECTIN_REV` pins the pectinos commit the program builds and passes
  against. The board must run the same commit's PectinOS: an older kernel
  refuses a newer CANNER's .JAM (`Not a jar. (needs a newer PectinOS)`).
  Move the pin on purpose, in its own PR, with `make test`, `make
  sim-smoke` and a board run.

## Cloud sessions

The start hook (`.claude/hooks/session-start.sh`) gives each cloud
session pectinos at the pin, CANNER and SIMMER. A session holding more
than this repo (a project that also holds `zigphroid/pectinos`) runs no
repo's hooks, so if `make` can't find pectinos or `make sim-smoke` has
no SIMMER, run it by hand once, with a long timeout (30 minutes the
first time):
`CLAUDE_CODE_REMOTE=true CLAUDE_PROJECT_DIR="$PWD" .claude/hooks/session-start.sh`.
The Makefile finds a sibling `../pectinos` by itself.

## Gotchas (each one cost someone a bug)

- Check the build before believing a run: a failed compile leaves the old
  .JAM behind.
- The stack is 16 KB at most (the Makefile's `STACK`). Big structs and
  buffers go in `static` or on the heap, never in locals.
- `.bss` costs the program area (the TPA), which is smaller than it looks.
  Big buffers that are only needed for a while come from the heap.
- On the board, `mem_resize` never moves a block, so growing one can fail
  where the host's `realloc` wouldn't.
- At most `PECTIN_FILES_MAX` files are open at once. Read big files 32 KB
  at a time (`sdk/README.md`).
