#!/usr/bin/env bash

# Compatibility wrapper for Claude-authored work that needs an independent
# Codex review. New agent workflows should call review-gate.sh --author ... so
# the reviewer family is selected automatically.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)

if (($# > 1)); then
    echo "Usage: bash scripts/codex-review.sh [<base-ref>]" >&2
    exit 1
fi

ARGS=(--reviewer codex)
if (($# == 1)); then
    ARGS+=(--base "$1")
fi

exec "$SCRIPT_DIR/review-gate.sh" "${ARGS[@]}"
