#!/usr/bin/env bash

# One bounded, author-aware review gate per feature. A successful initial pass
# permits one remediation pass. The gate refuses unchanged reruns and pass 3.

set -euo pipefail

usage()
{
    cat <<'EOF'
Usage:
  review-gate.sh --author codex|claude [--major] [--base <ref>] [--accepted <ref>]
  review-gate.sh --reviewer codex|opus|fable [--base <ref>] [--accepted <ref>]
  review-gate.sh --reset

Normal routing is author-aware: Codex work is reviewed by Opus, while Claude
work is reviewed by Codex. --major routes Codex-authored work to Fable.

Exit codes:
  0 review completed or empty diff
  1 usage or invalid repository/ref
  2 reviewer runtime unavailable
  4 reviewer failed or returned invalid JSON
  5 third review pass refused
  6 current HEAD was already reviewed
  7 history or HEAD changed during review
EOF
}

AUTHOR=""
REVIEWER=""
BASE_REF=""
ACCEPTED_REF=""
MAJOR=0
RESET=0

while (($#)); do
    case "$1" in
        --author)
            [[ $# -ge 2 ]] || { usage >&2; exit 1; }
            AUTHOR=$2
            shift 2
            ;;
        --reviewer)
            [[ $# -ge 2 ]] || { usage >&2; exit 1; }
            REVIEWER=$2
            shift 2
            ;;
        --base)
            [[ $# -ge 2 ]] || { usage >&2; exit 1; }
            BASE_REF=$2
            shift 2
            ;;
        --accepted)
            [[ $# -ge 2 ]] || { usage >&2; exit 1; }
            ACCEPTED_REF=$2
            shift 2
            ;;
        --major)
            MAJOR=1
            shift
            ;;
        --reset)
            RESET=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            if [[ -z "$BASE_REF" && "$1" != -* ]]; then
                BASE_REF=$1
                shift
            else
                echo "ERROR: unknown argument '$1'" >&2
                usage >&2
                exit 1
            fi
            ;;
    esac
done

if ! REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null); then
    echo "ERROR: not inside a git repository" >&2
    exit 1
fi
cd "$REPO_ROOT"

HEAD_SHA=$(git rev-parse HEAD)
BRANCH=$(git branch --show-current)
if [[ -z "$BRANCH" ]]; then
    BRANCH="detached-${HEAD_SHA:0:12}"
fi
STATE_KEY=$(printf '%s' "$BRANCH" | tr '/:' '__')
REVIEWS_DIR="$REPO_ROOT/reviews"
STATE_FILE="$REVIEWS_DIR/review-gate-${STATE_KEY}.json"
mkdir -p "$REVIEWS_DIR"

if ((RESET)); then
    rm -f "$STATE_FILE"
    echo "Review gate reset for $BRANCH"
    exit 0
fi

if [[ -z "$REVIEWER" ]]; then
    case "$AUTHOR" in
        codex)
            if ((MAJOR)); then
                REVIEWER="fable"
            else
                REVIEWER="opus"
            fi
            ;;
        claude|opus|fable)
            REVIEWER="codex"
            ;;
        *)
            echo "ERROR: provide --author codex|claude or --reviewer codex|opus|fable" >&2
            exit 1
            ;;
    esac
fi

case "$REVIEWER" in
    codex|opus|fable) ;;
    *)
        echo "ERROR: unsupported reviewer '$REVIEWER'" >&2
        exit 1
        ;;
esac

if [[ -z "$BASE_REF" ]]; then
    if git rev-parse --verify --quiet '@{upstream}^{commit}' >/dev/null; then
        BASE_REF='@{upstream}'
    else
        echo "ERROR: no upstream is configured; provide --base <ref>" >&2
        exit 1
    fi
fi

if ! BASE_SHA=$(git rev-parse --verify "$BASE_REF^{commit}" 2>/dev/null); then
    echo "ERROR: invalid base ref '$BASE_REF'" >&2
    exit 1
fi
if ! git merge-base --is-ancestor "$BASE_SHA" "$HEAD_SHA"; then
    echo "ERROR: base $BASE_SHA is not an ancestor of HEAD $HEAD_SHA" >&2
    exit 1
fi

ACCEPTED_SHA=""
if [[ -n "$ACCEPTED_REF" ]]; then
    if ! ACCEPTED_SHA=$(git rev-parse --verify "$ACCEPTED_REF^{commit}" 2>/dev/null); then
        echo "ERROR: invalid accepted ref '$ACCEPTED_REF'" >&2
        exit 1
    fi
fi

PASS=1
EFFECTIVE_BASE=$BASE_SHA
if [[ -f "$STATE_FILE" ]]; then
    OLD_BASE=$(jq -r '.base_sha // empty' "$STATE_FILE")
    OLD_HEAD=$(jq -r '.reviewed_head // empty' "$STATE_FILE")
    OLD_PASS=$(jq -r '.pass // 0' "$STATE_FILE")
    if [[ "$OLD_BASE" == "$BASE_SHA" ]]; then
        if [[ "$OLD_HEAD" == "$HEAD_SHA" ]]; then
            echo "ERROR: HEAD $HEAD_SHA was already reviewed; refusing a duplicate pass" >&2
            exit 6
        fi
        if ! git merge-base --is-ancestor "$OLD_HEAD" "$HEAD_SHA"; then
            echo "ERROR: reviewed history changed from $OLD_HEAD to $HEAD_SHA" >&2
            exit 7
        fi
        if ((OLD_PASS >= 2)); then
            echo "ERROR: review pass 3 refused for base $BASE_SHA" >&2
            echo "Only a user-authorized gate reset or an advanced feature base may continue." >&2
            exit 5
        fi
        PASS=$((OLD_PASS + 1))
        EFFECTIVE_BASE=$OLD_HEAD
    fi
fi

RANGE="$EFFECTIVE_BASE..$HEAD_SHA"
DIFF=$(git diff --no-ext-diff "$RANGE")
FILES=$(git diff --name-only "$RANGE")
if [[ -z "$DIFF" ]]; then
    echo "Nothing to review for immutable range $RANGE"
    exit 0
fi

TIMESTAMP=$(date +%Y-%m-%d-%H%M%S)
DIFF_FILE="$REVIEWS_DIR/${TIMESTAMP}-${REVIEWER}-pass${PASS}-diff.txt"
PROMPT_FILE="$REVIEWS_DIR/${TIMESTAMP}-${REVIEWER}-pass${PASS}-prompt.txt"
VERDICT_FILE="$REVIEWS_DIR/${TIMESTAMP}-${REVIEWER}-pass${PASS}-review.json"
RAW_FILE="$REVIEWS_DIR/${TIMESTAMP}-${REVIEWER}-pass${PASS}-raw.log"
SCHEMA_FILE=$(mktemp "${TMPDIR:-/tmp}/review-gate-schema.XXXXXX.json")
VERDICT_TMP=$(mktemp "${TMPDIR:-/tmp}/review-gate-verdict.XXXXXX.json")
STATE_TMP=$(mktemp "${TMPDIR:-/tmp}/review-gate-state.XXXXXX.json")
trap 'rm -f "$SCHEMA_FILE" "$VERDICT_TMP" "$STATE_TMP"' EXIT

printf '%s\n' "$DIFF" >"$DIFF_FILE"

PASS_CONTEXT="This is the initial feature review."
if ((PASS == 2)); then
    PASS_CONTEXT="This is the single remediation review. Review only the remediation delta and whether it introduces a supported production regression. Do not begin a fresh whole-feature audit."
fi

ACCEPTED_CONTEXT="No hardware-accepted SHA was declared."
if [[ -n "$ACCEPTED_SHA" ]]; then
    ACCEPTED_CONTEXT="Hardware-accepted SHA: $ACCEPTED_SHA. Findings that do not establish a production BLOCKER must not cause churn in the accepted tree."
fi

cat >"$PROMPT_FILE" <<EOF
You are the single independent publication reviewer for this feature.

Immutable review range: $RANGE
Review pass: $PASS of 2
Reviewer: $REVIEWER
$PASS_CONTEXT
$ACCEPTED_CONTEXT

Review the changed behavior against supported production use. You may inspect
unchanged code only to trace direct callers and contracts. Do not broaden this
into an unrelated repository audit.

Only BLOCKER findings block publication. A BLOCKER must include all of:
1. Supported production entry point.
2. Reachable production call chain.
3. Material user-visible, data-integrity, security, or safety impact.
4. Concrete evidence or a reproducible scenario.

Public-but-unused internals, unsupported direct mutation, test-only observers,
and hypothetical future consumers are not blockers. Record them as MINOR
research notes when they are genuinely useful. Do not demand implementation.

Use MAJOR for a supported production defect that should be fixed or explicitly
deferred, but which does not prevent publication. Use MINOR for nonblocking
quality notes and research leads. Findings must be caused by this immutable
delta. Return an empty findings array when there is no supported defect.

Return JSON only:
{"findings":[{"severity":"BLOCKER|MAJOR|MINOR","title":"...","fact":"Include production entry, call chain, impact, and evidence","recommendation":"..."}]}

Changed files:
$FILES

Git diff:
$DIFF
EOF

cat >"$SCHEMA_FILE" <<'EOF'
{
  "type": "object",
  "additionalProperties": false,
  "required": ["findings"],
  "properties": {
    "findings": {
      "type": "array",
      "items": {
        "type": "object",
        "additionalProperties": false,
        "required": ["severity", "title", "fact", "recommendation"],
        "properties": {
          "severity": {"type": "string", "enum": ["BLOCKER", "MAJOR", "MINOR"]},
          "title": {"type": "string"},
          "fact": {"type": "string"},
          "recommendation": {"type": "string"}
        }
      }
    }
  }
}
EOF

START_SECONDS=$(date +%s)
case "$REVIEWER" in
    codex)
        if ! command -v codex >/dev/null 2>&1; then
            echo "ERROR: Codex CLI is unavailable" >&2
            exit 2
        fi
        if ! codex exec --ephemeral --ignore-user-config \
            -m gpt-5.6-sol -c 'model_reasoning_effort="high"' \
            -C "$REPO_ROOT" -s read-only --output-schema "$SCHEMA_FILE" \
            -o "$VERDICT_TMP" - <"$PROMPT_FILE" >"$RAW_FILE" 2>&1; then
            echo "ERROR: Codex review failed; see $RAW_FILE" >&2
            exit 4
        fi
        ;;
    opus|fable)
        CLAUDE_ARGS=(review --base "$EFFECTIVE_BASE" --effort high
            --no-turn-limit --no-timeout --no-background-fallback --json)
        if [[ "$REVIEWER" == "opus" ]]; then
            CLAUDE_ARGS+=(--model opus)
        else
            CLAUDE_ARGS+=(--model claude-fable-5)
        fi
        # The advisor runtime already embeds the immutable git diff. Keep the
        # focus argument short so large reviews never hit argv size limits.
        CLAUDE_FOCUS="Pass $PASS of 2 for immutable range $RANGE. $PASS_CONTEXT Only BLOCKER findings block publication. A BLOCKER requires a supported production entry point, reachable call chain, material impact, and concrete evidence. Unsupported direct mutation, test-only observers, and hypothetical future consumers are nonblocking research notes. Do not broaden into an unrelated repository audit."
        CLAUDE_ARGS+=("$CLAUDE_FOCUS")

        if [[ -n "${REVIEW_GATE_CLAUDE_RUNNER:-}" ]]; then
            CLAUDE_COMMAND=("$REVIEW_GATE_CLAUDE_RUNNER")
        else
            CODEX_ROOT=${CODEX_HOME:-$HOME/.codex}
            mapfile -t COMPANIONS < <(printf '%s\n' \
                "$CODEX_ROOT"/plugins/cache/claude-plugin-codex/claude-code-advisor/*/scripts/claude-companion.mjs | sort -V)
            COMPANION=""
            for candidate in "${COMPANIONS[@]}"; do
                [[ -f "$candidate" ]] && COMPANION=$candidate
            done
            if [[ -z "$COMPANION" ]] || ! command -v node >/dev/null 2>&1; then
                echo "ERROR: Claude advisor runtime is unavailable" >&2
                exit 2
            fi
            CLAUDE_COMMAND=(node "$COMPANION")
        fi

        if ! "${CLAUDE_COMMAND[@]}" "${CLAUDE_ARGS[@]}" >"$RAW_FILE" 2>&1; then
            echo "ERROR: $REVIEWER review failed; see $RAW_FILE" >&2
            exit 4
        fi
        if ! jq -e '.status == "completed" and (.result.findings | type == "array")' \
            "$RAW_FILE" >/dev/null 2>&1; then
            echo "ERROR: $REVIEWER returned invalid review JSON; see $RAW_FILE" >&2
            exit 4
        fi
        jq '.result' "$RAW_FILE" >"$VERDICT_TMP"
        ;;
esac

if ! jq -e '
    type == "object" and
    (.findings | type == "array") and
    all(.findings[];
        (.severity == "BLOCKER" or .severity == "MAJOR" or .severity == "MINOR") and
        (.title | type == "string") and
        (.fact | type == "string") and
        (.recommendation | type == "string"))
' "$VERDICT_TMP" >/dev/null 2>&1; then
    echo "ERROR: reviewer returned invalid verdict JSON" >&2
    exit 4
fi

CURRENT_HEAD=$(git rev-parse HEAD)
if [[ "$CURRENT_HEAD" != "$HEAD_SHA" ]]; then
    echo "ERROR: HEAD changed during review ($HEAD_SHA -> $CURRENT_HEAD); verdict discarded" >&2
    exit 7
fi

cp "$VERDICT_TMP" "$VERDICT_FILE"
DURATION_SECONDS=$(($(date +%s) - START_SECONDS))
REVIEWED_AT=$(date -u +%Y-%m-%dT%H:%M:%SZ)
jq -n \
    --arg branch "$BRANCH" \
    --arg author "${AUTHOR:-unspecified}" \
    --arg reviewer "$REVIEWER" \
    --arg base_sha "$BASE_SHA" \
    --arg effective_base "$EFFECTIVE_BASE" \
    --arg reviewed_head "$HEAD_SHA" \
    --arg accepted_sha "$ACCEPTED_SHA" \
    --arg reviewed_at "$REVIEWED_AT" \
    --arg verdict_file "$VERDICT_FILE" \
    --argjson pass "$PASS" \
    --argjson duration_seconds "$DURATION_SECONDS" \
    '{
      branch: $branch,
      author: $author,
      reviewer: $reviewer,
      effort: "high",
      pass: $pass,
      base_sha: $base_sha,
      effective_base: $effective_base,
      reviewed_head: $reviewed_head,
      accepted_sha: $accepted_sha,
      reviewed_at: $reviewed_at,
      duration_seconds: $duration_seconds,
      verdict_file: $verdict_file
    }' >"$STATE_TMP"
mv "$STATE_TMP" "$STATE_FILE"

BLOCKERS=$(jq '[.findings[] | select(.severity == "BLOCKER")] | length' "$VERDICT_FILE")
MAJORS=$(jq '[.findings[] | select(.severity == "MAJOR")] | length' "$VERDICT_FILE")
MINORS=$(jq '[.findings[] | select(.severity == "MINOR")] | length' "$VERDICT_FILE")

echo "Review pass $PASS of 2 completed on immutable range $RANGE"
echo "Reviewer: $REVIEWER (effort high, ${DURATION_SECONDS}s)"
echo "Findings: BLOCKER=$BLOCKERS MAJOR=$MAJORS MINOR=$MINORS"
echo "Verdict: $VERDICT_FILE"
echo "State: $STATE_FILE"
