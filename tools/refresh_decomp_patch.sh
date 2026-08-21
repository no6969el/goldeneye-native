#!/bin/sh
# Regenerate patches/decomp-host-port.patch from a decomp working tree.
#
# WHY THIS IS A SCRIPT AND NOT A NOTE
#
# The patch is the ONLY record of what this port changes in n64decomp/007 --
# Hard Rule 1 in VENDORING.md says the decomp source is never vendored, so if
# the patch is stale the repository silently describes a port that no longer
# exists. It went stale exactly once, by being overwritten with an older copy
# during a routine tree sync, and nothing caught it because a stale patch still
# applies cleanly and still builds -- it just builds the wrong thing.
#
# Run this before every commit that touches the decomp.
#
#     tools/refresh_decomp_patch.sh /path/to/n64decomp-007
set -e
DECOMP="${1:?usage: refresh_decomp_patch.sh <decomp-root>}"
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$HERE/patches/decomp-host-port.patch"

before=$(wc -l < "$OUT" 2>/dev/null || echo 0)
git -C "$DECOMP" diff > "$OUT"
after=$(wc -l < "$OUT")

echo "patches/decomp-host-port.patch: $before -> $after lines"
if [ "$after" -lt "$before" ]; then
    echo "WARNING: the patch SHRANK. That is either a real revert or a stale" >&2
    echo "  decomp checkout. Check before committing." >&2
fi
