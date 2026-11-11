#!/bin/bash
# Prints the pectinos checkout the program builds against, or nothing: an
# explicit PECTIN_ROOT, else ~/pectinos (the Makefile's default), else a
# sibling of this repo (where the session-start hook clones it in a cloud
# session). Shared by the hooks, which don't all see the session's
# environment.
for d in "${PECTIN_ROOT:-}" "$HOME/pectinos" "$(dirname "$CLAUDE_PROJECT_DIR")/pectinos"; do
  if [ -n "$d" ] && git -C "$d" rev-parse HEAD >/dev/null 2>&1; then
    echo "$d"
    exit 0
  fi
done
