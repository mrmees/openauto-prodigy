#!/usr/bin/env bash
# codex-review.sh — pre-push second-opinion review via the OpenAI Codex CLI.
#
# Part of the tiered workflow (see AGENTS.md and
# docs/superpowers/specs/2026-07-09-tiered-execution-codex-gate-design.md):
# after a plan's execution completes, this sends the unpushed range to Codex
# (a different model family) for structured P1/P2/P3 findings. The main Fable
# session adjudicates every finding; the push happens only after.
#
# Usage:
#   bash scripts/codex-review.sh              # review @{upstream}..HEAD;
#                                             # falls back to uncommitted
#                                             # changes if no upstream is set
#   bash scripts/codex-review.sh <base-ref>   # review <base-ref>..HEAD
#
# Exit codes:
#   0  verdict saved, or nothing to review
#   1  usage / not a git repo / bad base ref
#   2  codex CLI not installed (gate degrades to Fable-only review — note it
#      in the session-handoffs entry)
#   4  codex exec failed (if auth, run: codex login)

set -euo pipefail

# --- pre-flight: repo ---
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "ERROR: not inside a git repository." >&2
  exit 1
fi
REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

# --- args + scope resolution ---
MODE=""
RANGE=""
SCOPE_DESC=""
if [ "${1:-}" != "" ]; then
  if ! git rev-parse --verify --quiet "$1^{commit}" >/dev/null; then
    echo "ERROR: '$1' is not a valid commit reference." >&2
    echo "Usage: bash scripts/codex-review.sh [<base-ref>]" >&2
    exit 1
  fi
  MODE="range"
  RANGE="$1..HEAD"
  SCOPE_DESC="Range review: $(git rev-parse --short "$1")..$(git rev-parse --short HEAD)"
elif git rev-parse --verify --quiet '@{upstream}' >/dev/null 2>&1; then
  MODE="range"
  RANGE='@{upstream}..HEAD'
  SCOPE_DESC="Pre-push review: $(git rev-parse --abbrev-ref '@{upstream}')..HEAD ($(git rev-parse --short '@{upstream}')..$(git rev-parse --short HEAD))"
else
  MODE="uncommitted"
  SCOPE_DESC="Review of uncommitted changes (no upstream configured)"
fi

# --- capture diff ---
if [ "$MODE" = "range" ]; then
  DIFF="$(git diff "$RANGE")"
  FILES="$(git diff --name-only "$RANGE")"
else
  DIFF="$(git diff HEAD)"
  FILES="$(git diff --name-only HEAD)"
fi

if [ -z "$DIFF" ]; then
  echo "Nothing to review — empty diff for scope: $SCOPE_DESC"
  exit 0
fi

# --- pre-flight: codex ---
if ! command -v codex >/dev/null 2>&1; then
  echo "NOTE: codex CLI not found on PATH — skipping the Codex review gate." >&2
  echo "      The gate degrades to a Fable-only review; record that in the" >&2
  echo "      session-handoffs entry. (Install codex + 'codex login' to enable.)" >&2
  exit 2
fi

# --- artifacts ---
REVIEWS_DIR="$REPO_ROOT/reviews"
mkdir -p "$REVIEWS_DIR"
TIMESTAMP="$(date +%Y-%m-%d-%H%M%S)"
DIFF_FILE="$REVIEWS_DIR/$TIMESTAMP-diff.txt"
REVIEW_FILE="$REVIEWS_DIR/$TIMESTAMP-codex-review.md"
PROMPT_FILE="$(mktemp "${TMPDIR:-/tmp}/codex-review-prompt.XXXXXX")"
trap 'rm -f "$PROMPT_FILE"' EXIT

printf '%s\n' "$DIFF" > "$DIFF_FILE"

# --- build prompt (heredoc expands the variables once; diff content is not
# --- re-evaluated by the shell) ---
cat > "$PROMPT_FILE" <<EOF
Review this code change made by another AI coding agent.

$SCOPE_DESC

Focus on: logic bugs, regressions, hidden edge cases, bad assumptions, missing
validation, concurrency/threading issues, performance problems, security
risks, and missing or weak tests.

Return findings as a structured list with severity:
  P1 = must fix, P2 = should fix, P3 = nice to have.
For each finding include: file, line/area, severity, issue description, and a
suggested fix. If nothing is wrong, say "LGTM — no issues found."

Changed files:
$FILES

Git diff:
$DIFF
EOF

# --- call codex: read-only sandbox (the gate advises, it never edits);
# --- prompt via stdin file, verdict via -o (never prompt-as-argument) ---
echo "Sending diff to Codex for review ($SCOPE_DESC)..." >&2
if ! codex exec -C "$REPO_ROOT" -s read-only -o "$REVIEW_FILE" - < "$PROMPT_FILE"; then
  echo "ERROR: codex exec failed. If this is an auth issue, run: codex login" >&2
  exit 4
fi

echo ""
echo "Verdict saved to: $REVIEW_FILE"
echo "Diff saved to:    $DIFF_FILE"
