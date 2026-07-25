#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
GATE="$REPO_ROOT/scripts/review-gate.sh"
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

assert_eq()
{
    local expected=$1
    local actual=$2
    local label=$3
    [[ "$actual" == "$expected" ]] ||
        fail "$label: expected '$expected', got '$actual'"
}

assert_contains()
{
    local file=$1
    local text=$2
    local label=$3
    rg -Fq -- "$text" "$file" || fail "$label: missing '$text' in $file"
}

assert_exit()
{
    local expected=$1
    shift
    set +e
    "$@" >"$TEST_ROOT/command.out" 2>"$TEST_ROOT/command.err"
    local actual=$?
    set -e
    assert_eq "$expected" "$actual" "exit status"
}

commit_file()
{
    local name=$1
    local contents=$2
    printf '%s\n' "$contents" >"$name"
    git add "$name"
    git commit -q -m "test: update $name"
    git rev-parse HEAD
}

mkdir -p "$TEST_ROOT/bin"
cat >"$TEST_ROOT/bin/codex" <<'FAKE_CODEX'
#!/usr/bin/env bash
set -euo pipefail
printf 'codex %s\n' "$*" >>"$FAKE_REVIEW_LOG"
output=""
while (($#)); do
    if [[ "$1" == "-o" || "$1" == "--output-last-message" ]]; then
        output=$2
        shift 2
        continue
    fi
    shift
done
cat >"$FAKE_PROMPT_LOG"
printf '{"findings":[]}\n' >"$output"
FAKE_CODEX
chmod +x "$TEST_ROOT/bin/codex"

cat >"$TEST_ROOT/bin/fake-claude-review" <<'FAKE_CLAUDE'
#!/usr/bin/env bash
set -euo pipefail
printf 'claude %s\n' "$*" >>"$FAKE_REVIEW_LOG"
printf '%s\n' '{"jobId":"test","status":"completed","result":{"findings":[]}}'
FAKE_CLAUDE
chmod +x "$TEST_ROOT/bin/fake-claude-review"

export PATH="$TEST_ROOT/bin:$PATH"
export FAKE_REVIEW_LOG="$TEST_ROOT/reviewer.log"
export FAKE_PROMPT_LOG="$TEST_ROOT/prompt.txt"
export REVIEW_GATE_CLAUDE_RUNNER="$TEST_ROOT/bin/fake-claude-review"

mkdir "$TEST_ROOT/repo"
cd "$TEST_ROOT/repo"
git init -q -b dev
git config user.name "Review Gate Test"
git config user.email "review-gate@example.invalid"

base=$(commit_file base.txt base)
head_one=$(commit_file feature.txt feature)

# Claude-authored work receives an independent Codex review.
bash "$GATE" --author claude --base "$base"
state="$TEST_ROOT/repo/reviews/review-gate-dev.json"
assert_eq "1" "$(jq -r '.pass' "$state")" "initial pass"
assert_eq "$base" "$(jq -r '.base_sha' "$state")" "initial base"
assert_eq "$head_one" "$(jq -r '.reviewed_head' "$state")" "initial head"
assert_eq "codex" "$(jq -r '.reviewer' "$state")" "Claude author reviewer"
assert_contains "$FAKE_PROMPT_LOG" "Supported production entry point" "reachability contract"
assert_contains "$FAKE_PROMPT_LOG" "$base..$head_one" "immutable initial range"
assert_contains "$FAKE_PROMPT_LOG" "Only BLOCKER findings block publication" "blocking contract"
initial_calls=$(wc -l <"$FAKE_REVIEW_LOG")

# Re-running an unchanged tree is forbidden and does not launch a reviewer.
assert_exit 6 bash "$GATE" --author claude --base "$base"
assert_eq "$initial_calls" "$(wc -l <"$FAKE_REVIEW_LOG")" "same-head reviewer calls"

# The only remediation pass reviews the first reviewed HEAD through new HEAD.
head_two=$(commit_file fix.txt fix)
bash "$GATE" --author claude --base "$base"
assert_eq "2" "$(jq -r '.pass' "$state")" "remediation pass"
assert_eq "$head_one" "$(jq -r '.effective_base' "$state")" "remediation base"
assert_eq "$head_two" "$(jq -r '.reviewed_head' "$state")" "remediation head"
assert_contains "$FAKE_PROMPT_LOG" "$head_one..$head_two" "immutable remediation range"
two_pass_calls=$(wc -l <"$FAKE_REVIEW_LOG")

# A third pass is a hard stop and never launches a reviewer.
head_three=$(commit_file speculative.txt speculative)
assert_exit 5 bash "$GATE" --author claude --base "$base"
assert_eq "$two_pass_calls" "$(wc -l <"$FAKE_REVIEW_LOG")" "third-pass reviewer calls"

# Advancing the feature base starts a new gate. Codex-authored normal work
# routes to Opus, not another Codex reviewer.
bash "$GATE" --author codex --base "$head_two"
assert_eq "1" "$(jq -r '.pass' "$state")" "advanced-base pass"
assert_eq "$head_two" "$(jq -r '.base_sha' "$state")" "advanced feature base"
assert_eq "$head_three" "$(jq -r '.reviewed_head' "$state")" "advanced feature head"
assert_eq "opus" "$(jq -r '.reviewer' "$state")" "Codex author reviewer"
assert_contains "$FAKE_REVIEW_LOG" "--model opus" "Opus routing"
assert_contains "$FAKE_REVIEW_LOG" "--effort high" "bounded review effort"
assert_contains "$FAKE_REVIEW_LOG" "--no-timeout" "no wall-clock timeout"

echo "PASS: review gate enforces immutable author-aware two-pass reviews"
