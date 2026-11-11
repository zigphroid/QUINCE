#!/bin/bash
# Cloud sessions only: give the program the pectinos checkout it builds against
# (the SDK, CANNER, SIMMER; the Makefile's PECTIN_ROOT) at the commit
# PECTIN_REV pins, and build the tools, so `make test`, `make` and `make
# sim-smoke` work from the first turn. A computer of your own keeps its own
# ~/pectinos and is left alone.
#
# Claude Code runs a repo's hooks only in a session started on that one
# repo, so keep this repo alone in its Claude project: pectinos is cloned
# here, read-only, and never needs to be in the session.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

hooks="$CLAUDE_PROJECT_DIR/.claude/hooks"
pin="$(cat "$CLAUDE_PROJECT_DIR/PECTIN_REV" 2>/dev/null || true)"
root="$("$hooks/pectin-root.sh")"
if [ -z "$root" ]; then
  root="$(dirname "$CLAUDE_PROJECT_DIR")/pectinos"
  if ! git clone -q https://github.com/zigphroid/pectinos "$root" >&2; then
    echo "session-start: could not clone zigphroid/pectinos; make and make test need it" >&2
    echo "session-start: the cloud environment's network must reach github.com, and" >&2
    echo "session-start: until its first public release pectinos is private: add" >&2
    echo "session-start: zigphroid/pectinos to the Claude project, if you have access" >&2
    exit 0
  fi
fi

# A rootstock (pectin-rootstock, not planted yet) has no pin: it takes the
# newest release that can plant it (one with tools/newapp/seed), so the
# program it plants is pinned to a PectinOS its owner's board can run. With
# none yet, it plants at the checkout's commit (a fresh clone: main's newest).
if [ -z "$pin" ] && [ -f "$CLAUDE_PROJECT_DIR/.rootstock" ]; then
  # A project's own pectinos checkout can be a shallow clone with no tags,
  # which takes the tags only shallow (a plain --tags fetch is refused
  # there); a full clone takes them as they are.
  if [ "$(git -C "$root" rev-parse --is-shallow-repository)" = true ]; then
    git -C "$root" fetch -q --depth 1 origin 'refs/tags/v*:refs/tags/v*' >/dev/null 2>&1 || true
  else
    git -C "$root" fetch -q --tags origin >/dev/null 2>&1 || true
  fi
  for t in $(git -C "$root" tag -l 'v*' --sort=-v:refname); do
    if git -C "$root" cat-file -e "$t:tools/newapp/seed" 2>/dev/null; then
      pin="$(git -C "$root" rev-parse "$t^{commit}")"
      echo "session-start: a rootstock: planting at pectinos's release $t" >&2
      break
    fi
  done
  if [ -z "$pin" ]; then
    echo "session-start: a rootstock, and no pectinos release can plant it yet: planting at its current commit," >&2
    echo "session-start: which no released PectinOS will run; the board needs PectinOS built at it (build/pectinos.uf2)" >&2
  fi
fi

# Move to the pin only when nothing would be lost: a clean worktree, and
# --detach, so no branch moves. A dirty checkout stays where it is, with a
# warning (and pectinos is read-only from here: nothing should dirty it).
if [ -n "$pin" ] && [ "$(git -C "$root" rev-parse HEAD)" != "$pin" ]; then
  if [ -z "$(git -C "$root" status --porcelain)" ]; then
    if git -C "$root" cat-file -e "$pin^{commit}" 2>/dev/null \
       || git -C "$root" fetch -q origin "$pin" >&2; then
      git -C "$root" checkout -q --detach "$pin" >&2
    else
      # A pin that never reached GitHub (scaffolded from an unpushed
      # commit): build at whatever is here, and say so.
      echo "session-start: pectinos has no commit ${pin:0:7}; left at $(git -C "$root" rev-parse --short HEAD)" >&2
    fi
  else
    echo "session-start: $root has local changes; left at $(git -C "$root" rev-parse --short HEAD), not the pin ${pin:0:7}" >&2
  fi
fi

if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
  echo "export PECTIN_ROOT=\"$root\"" >> "$CLAUDE_ENV_FILE"
fi

# What `make` needs from pectinos (a host cc builds them).
make -C "$root" canner build/host/verbusage build/host/prefcheck build/host/jarcard >&2
echo "session-start: PECTIN_ROOT=$root ($(git -C "$root" rev-parse --short HEAD)), CANNER built" >&2

# SIMMER, for `make sim-smoke`: Arm's GCC for its kernel, QEMU's build
# packages ($PECTIN_ROOT/docs/SIMMER.md), then QEMU with PectinOS's machine
# (several minutes, once: the container cache keeps it) and the kernel. A
# failure here costs SIMMER, not the session.
simmer() {
  local need=() p
  command -v arm-none-eabi-gcc >/dev/null 2>&1 || need+=(gcc-arm-none-eabi)
  for p in ninja-build pkg-config libglib2.0-dev libpixman-1-dev python3-venv \
           libgnutls28-dev libsdl2-dev; do
    dpkg -s "$p" >/dev/null 2>&1 || need+=("$p")
  done
  # Each step's failure is the function's: called as `if simmer`, bash
  # ignores set -e inside it, so a failed QEMU build would otherwise fall
  # through to sim-build and report SIMMER ready with no QEMU.
  if [ ${#need[@]} -gt 0 ]; then
    apt-get update -q >&2 && DEBIAN_FRONTEND=noninteractive apt-get install -y -q "${need[@]}" >&2 \
      || return 1
  fi
  # QEMU and its subprojects come from gitlab.com, which some cloud
  # environments refuse; QEMU's own mirrors on GitHub hold the same trees.
  if [ ! -x "$root/simmer/qemu/bin/qemu-system-arm" ]; then
    GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=url.https://github.com/qemu/.insteadOf \
      GIT_CONFIG_VALUE_0=https://gitlab.com/qemu-project/ make -C "$root" sim-qemu >&2 || return 1
  fi
  make -C "$root" sim-build >&2 || return 1
  [ -x "$root/simmer/qemu/bin/qemu-system-arm" ]
}
if simmer; then
  echo "session-start: SIMMER ready" >&2
else
  echo "session-start: SIMMER setup failed; make sim-smoke won't run (make test and make still do)" >&2
fi
