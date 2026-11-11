#!/bin/bash
# Stop: a turn that leaves QUINCE's code changed doesn't end until `make
# test` passes. The state is the committed trees plus the uncommitted diff
# of the paths below; a state that already passed isn't run again, so a
# chat-only turn costs nothing. Blocking is exit 2 with the failure on
# stderr, which Claude sees. A second stop on the same failing state goes
# through, so Claude can say what it couldn't fix.
set -uo pipefail

input="$(cat)"
cd "$CLAUDE_PROJECT_DIR" || exit 0
paths=(quince lib tests tools Makefile PECTIN_REV)

state="$({
  for p in "${paths[@]}"; do git rev-parse -q "HEAD:$p" 2>/dev/null; done
  git diff HEAD -- "${paths[@]}"
  git ls-files -o --exclude-standard -- "${paths[@]}" | while IFS= read -r f; do
    echo "$f"; cat "$f"
  done
} | git hash-object --stdin)"

mkdir -p build
pass=build/.stop-test-pass
fail=build/.stop-test-fail
[ "$(cat "$pass" 2>/dev/null)" = "$state" ] && exit 0

if [ "$(cat "$fail" 2>/dev/null)" = "$state" ] \
   && echo "$input" | grep -q '"stop_hook_active": *true'; then
  exit 0
fi

root="$("$CLAUDE_PROJECT_DIR/.claude/hooks/pectin-root.sh")"
[ -z "$root" ] && exit 0        # no pectinos: nothing can build; not this turn's fault

log=build/stop-test.log
if PECTIN_ROOT="$root" make test >"$log" 2>&1; then
  echo "$state" > "$pass"
  exit 0
fi
echo "$state" > "$fail"
{
  echo "Stop hook: make test failed with these changes. Fix it before finishing,"
  echo "or say plainly what is still failing and why. Last lines of $log:"
  tail -25 "$log"
} >&2
exit 2
