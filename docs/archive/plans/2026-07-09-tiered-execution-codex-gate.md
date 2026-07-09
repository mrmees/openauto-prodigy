# Tiered Execution + Codex Review Gate Implementation Plan

Status: COMPLETED 2026-07-09

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the pre-push Codex review gate script and document the tiered execution workflow (tier tags, Definition of Ready, Opus→Codex→Fable escalation) in AGENTS.md, per the approved spec `docs/superpowers/specs/2026-07-09-tiered-execution-codex-gate-design.md`.

**Architecture:** One bash script (`scripts/codex-review.sh`) captures a git diff range, builds a prompt file, and invokes the Codex CLI read-only with the verdict written to a gitignored `reviews/` directory. Workflow conventions (tiering, escalation, gate-then-push) live as documentation in AGENTS.md with a pointer from CLAUDE.md — no new agent files, no new directories beyond `reviews/`.

**Tech Stack:** Bash, git, OpenAI Codex CLI (`codex` at `/home/matt/.local/bin/codex`), shellcheck.

## Global Constraints

- **Codex invocation pattern (from workspace CLAUDE.md standing rule):** prompt piped via stdin from a file (`- < promptfile`), verdict captured via `-o <file>`. NEVER pass the prompt as a command-line argument.
- **Review gate sandbox is `-s read-only`.** Only the escalation ladder (documented, not scripted, in this plan) uses `-s workspace-write`.
- **Exit codes are contract:** 0 = success or nothing to review; 1 = usage/repo/ref error; 2 = codex CLI not installed; 4 = codex exec failed. AGENTS.md documents these; do not renumber.
- **This repo lives on a 9p mount (`/mnt/e`)** — the executable bit is unreliable. The script is always invoked as `bash scripts/codex-review.sh`, never `./scripts/codex-review.sh`. Do not add a chmod step.
- **Commit per task on `develop`. Do NOT `git push`** — pushing is gated on Task 3's review outcome and Matthew's explicit go-ahead.
- Scratch/test files go in the session scratchpad directory, not the repo and not `/tmp` root.

---

### Task 1: `scripts/codex-review.sh` + `reviews/` gitignore

**Tier:** opus

**Files:**
- Create: `scripts/codex-review.sh`
- Modify: `.gitignore` (append `reviews/`)
- Test: scratchpad test harness `<scratchpad>/codex-review-tests/run-tests.sh` (NOT committed)

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: `scripts/codex-review.sh` with the exit-code contract 0/1/2/4 described in Global Constraints; verdict file path pattern `reviews/<YYYY-MM-DD-HHMMSS>-codex-review.md` and diff copy `reviews/<YYYY-MM-DD-HHMMSS>-diff.txt`. Task 2's AGENTS.md text and Task 3's live run rely on exactly these.

- [ ] **Step 1: Write the test harness (failing first)**

Create `<scratchpad>/codex-review-tests/run-tests.sh` with the content below, substituting `<scratchpad>` with the absolute session scratchpad path:

```bash
#!/usr/bin/env bash
# run-tests.sh — dry-run matrix for scripts/codex-review.sh
# Uses a fake `codex` shim so no real Codex calls are made.
set -u

SCRIPT="/mnt/e/claude/personal/openautopro/openauto-prodigy/scripts/codex-review.sh"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/codex-review-test.XXXXXX")"
PASS=0; FAIL=0

check() { # name expected_exit actual_exit
  if [ "$2" -eq "$3" ]; then PASS=$((PASS+1)); echo "PASS: $1 (exit $3)";
  else FAIL=$((FAIL+1)); echo "FAIL: $1 (expected exit $2, got $3)"; fi
}

# --- fake codex shim: consumes stdin, honors -o, exits 0 ---
mkdir -p "$WORK/fakebin"
cat > "$WORK/fakebin/codex" <<'SHIM'
#!/usr/bin/env bash
out=""
while [ $# -gt 0 ]; do
  case "$1" in
    -o) out="$2"; shift 2 ;;
    *) shift ;;
  esac
done
prompt="$(cat)"
[ -n "$out" ] && printf 'LGTM — no issues found. (fake codex; prompt bytes: %s)\n' "${#prompt}" > "$out"
SHIM
chmod +x "$WORK/fakebin/codex"

# --- failing codex shim: exits 1 ---
mkdir -p "$WORK/failbin"
printf '#!/usr/bin/env bash\nexit 1\n' > "$WORK/failbin/codex"
chmod +x "$WORK/failbin/codex"

# --- scratch repo (no upstream yet) ---
git init -q "$WORK/repo"
cd "$WORK/repo"
git config user.email test@test.local
git config user.name test
echo "line one" > a.txt
git add a.txt && git commit -qm "c1"

# 1: not inside a git repo -> exit 1
cd "$WORK"
bash "$SCRIPT" >/dev/null 2>&1; check "not-a-repo" 1 $?

# 2: bad base ref -> exit 1
cd "$WORK/repo"
bash "$SCRIPT" nosuchref >/dev/null 2>&1; check "bad-base-ref" 1 $?

# 3: clean repo, no upstream -> empty diff -> exit 0
bash "$SCRIPT" >/dev/null 2>&1; check "empty-diff" 0 $?

# 4: uncommitted changes + fake codex -> exit 0 + review file created
echo "line two" >> a.txt
PATH="$WORK/fakebin:$PATH" bash "$SCRIPT" >/dev/null 2>&1; check "uncommitted-fake-codex" 0 $?
ls reviews/*-codex-review.md >/dev/null 2>&1; check "review-file-created" 0 $?
ls reviews/*-diff.txt >/dev/null 2>&1; check "diff-file-created" 0 $?

# 5: codex missing (PATH stripped to system dirs) -> exit 2
PATH=/usr/bin:/bin bash "$SCRIPT" >/dev/null 2>&1; check "codex-missing" 2 $?

# 6: codex exec fails -> exit 4
PATH="$WORK/failbin:$PATH" bash "$SCRIPT" >/dev/null 2>&1; check "codex-exec-fails" 4 $?

# 7: explicit base ref -> exit 0
git add a.txt && git commit -qm "c2"
PATH="$WORK/fakebin:$PATH" bash "$SCRIPT" HEAD~1 >/dev/null 2>&1; check "explicit-base" 0 $?

# 8: upstream configured, unpushed commit -> default range review -> exit 0
git init -q --bare "$WORK/origin.git"
git remote add origin "$WORK/origin.git"
git push -q -u origin "$(git symbolic-ref --short HEAD)"
echo "line three" >> a.txt
git add a.txt && git commit -qm "c3"
PATH="$WORK/fakebin:$PATH" bash "$SCRIPT" >/dev/null 2>&1; check "upstream-range" 0 $?

echo ""
echo "Results: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
```

- [ ] **Step 2: Run harness to verify it fails**

Run: `bash <scratchpad>/codex-review-tests/run-tests.sh`
Expected: FAIL across the board (script does not exist yet — every `bash "$SCRIPT"` exits 127; harness prints `Results: 0 pass, ...` and exits non-zero). This proves the harness detects a broken/missing script.

- [ ] **Step 3: Write the script**

Create `scripts/codex-review.sh` with exactly this content:

```bash
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
```

- [ ] **Step 4: shellcheck**

Run: `shellcheck scripts/codex-review.sh` (if not installed: `sudo apt-get install -y shellcheck`, permitted by autonomy rules)
Expected: no errors. Info/style notes are acceptable only if they don't change behavior; fix any warning-level findings.

- [ ] **Step 5: Run the harness to verify it passes**

Run: `bash <scratchpad>/codex-review-tests/run-tests.sh`
Expected: `Results: 10 pass, 0 fail`, exit 0. (10 checks: 8 numbered cases plus the two file-created assertions in case 4.)

- [ ] **Step 6: Sanity-run in the real repo without touching Codex**

Run: `cd /mnt/e/claude/personal/openautopro/openauto-prodigy && bash scripts/codex-review.sh HEAD`
Expected: `Nothing to review — empty diff for scope: Range review: ...` and exit 0. This proves the script runs on the 9p mount. Do NOT run it with no argument here — that would launch a real multi-minute Codex review of the unpushed range, which is Task 3's job.

- [ ] **Step 7: Gitignore `reviews/`**

Append to `.gitignore`:

```
reviews/
```

Verify: `git check-ignore -v reviews/anything.md` prints the `.gitignore` rule.

- [ ] **Step 8: Commit**

```bash
git add scripts/codex-review.sh .gitignore
git commit -m "feat(workflow): add pre-push Codex review gate script

codex-review.sh reviews @{upstream}..HEAD (or an explicit base ref) via
the Codex CLI in a read-only sandbox: prompt via stdin file, verdict via
-o into gitignored reviews/. Exit contract: 0 ok/empty, 1 usage, 2 codex
missing, 4 codex failed.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: Document the workflow — AGENTS.md + CLAUDE.md pointer

**Tier:** sonnet

**Files:**
- Modify: `AGENTS.md` (full-file replacement, content below)
- Modify: `CLAUDE.md` (insert one paragraph in the `## Workflow` section)

**Interfaces:**
- Consumes: `scripts/codex-review.sh` and its exit codes 0/1/2/4 from Task 1 (referenced verbatim in the text below).
- Produces: AGENTS.md section heading `## Tiered Execution Workflow` (Task 3's handoff entry and future sessions reference it by this exact name).

- [ ] **Step 1: Replace AGENTS.md**

Read `AGENTS.md` first (Write requires it). Replace the entire file with:

```markdown
# AGENTS.md

## Project Management Loop

For behavior-changing work in this repository:

1. Check alignment with `docs/project-vision.md` before implementation.
2. Update `docs/roadmap-current.md` when priorities or sequencing change.
3. Before claiming completion, run:
   - `cd build && cmake --build . -j$(nproc)` (local build)
   - `ctest --output-on-failure` (test suite)
4. Append a handoff entry to `docs/session-handoffs.md` including:
   - what changed
   - why
   - status
   - next 1-3 steps
   - verification commands/results

## Tiered Execution Workflow

Full design: `docs/superpowers/specs/2026-07-09-tiered-execution-codex-gate-design.md`.
Planning stays interactive in the main (Fable) session via superpowers
brainstorming + writing-plans; the conventions below govern how plans are
tagged, executed, and reviewed.

### Tier tags (plan time)

Every task in a superpowers plan carries a `Tier:` field:

| Tier | Model | Used for |
|------|-------|----------|
| `opus` | Opus | Default for implementation: real code, logic, cross-module changes |
| `sonnet` | Sonnet | Test scaffolding, mocks/fixtures, mechanical pattern-following edits, doc updates |
| `main` | Fable (main session) | Protocol-critical or judgment-heavy work (AA protocol internals, threading) |

**Definition of Ready** — do not dispatch a task unless: exact files are
named; acceptance criteria are testable ("returns 422 on empty id", not
"works"); scope is bounded with an explicit out-of-scope line; a test command
is given; no open questions remain. A task that fails this checklist goes back
to the human as a question, not to a worker.

**Small-work bypass:** trivial single-file fixes skip tiering entirely; the
main session just does them.

### Dispatch (execution time)

Dispatch each task as a subagent with the tier's model pinned (Agent tool
`model` parameter). Workers inherit the repo CLAUDE.md automatically. Workers
own the build/fix/test loop and report **synthesized results only**: files
changed (one line each), test command + pass/fail counts, deviations. Raw logs
stay out of the main session's context. Workers commit per task; nobody pushes
mid-execution.

### Escalation ladder: Opus → Codex (GPT-5.5) → Fable

1. **Opus worker** — owns the task. Two focused attempts at green, then
   escalate instead of grinding.
2. **Codex** — gets a prompt file containing the task (files, acceptance
   criteria, test command) plus the worker's failure report, with write
   access so it can apply the fix:

   ```bash
   codex exec -C <repo-abs-path> -s workspace-write -o <scratchpad>/codex-task-verdict.txt - < <scratchpad>/codex-task-prompt.txt
   ```

   Afterwards the main session reads the verdict file AND `git diff`, verifies
   the result against the task's acceptance criteria, and commits if it
   passes. Codex is trusted to write, not to self-certify.
3. **Fable (main session)** — takes the task over directly if Codex also
   fails.

### Review gate (per feature, pre-push)

After a plan's tasks are done and tests are green:

1. Run `bash scripts/codex-review.sh` — reviews `@{upstream}..HEAD` in a
   read-only sandbox and saves structured P1/P2/P3 findings to `reviews/`
   (gitignored). Explicit range: `bash scripts/codex-review.sh <base-ref>`.
   Exit codes: 0 ok/empty, 1 usage, 2 codex not installed, 4 codex failed.
   On exit 2/4 the gate degrades to a Fable-only review — note that in the
   session-handoffs entry; the gate never silently passes.
2. The main Fable session adjudicates **every** finding: confirmed → fix
   (inline if small, Opus worker if substantial); dismissed → stated reason.
   No silent drops. Substantial fixes trigger one re-run of the gate on the
   new range.
3. Record the adjudication summary (confirmed/dismissed counts, blockers
   fixed) in the session-handoffs entry. Only then push — with the user's
   go-ahead.

## Cross-Compile Verification

For changes that affect Pi deployment:
- `./cross-build.sh` (Docker cross-compile for aarch64)
- Deploy and test on Pi when hardware-dependent

## Scope Note

This local file defines repo-specific workflow expectations.
Platform-level safety and skill instructions still apply.
```

- [ ] **Step 2: Add the CLAUDE.md pointer**

Read `CLAUDE.md`, find the `## Workflow` section. It currently begins:

```markdown
## Workflow

This project follows a structured workflow. See `AGENTS.md` for the full loop.
```

Edit to:

```markdown
## Workflow

This project follows a structured workflow. See `AGENTS.md` for the full loop.
Plan tasks carry tier tags (`opus`/`sonnet`/`main`) with an Opus→Codex→Fable
escalation ladder, and every feature passes the pre-push Codex review gate
(`bash scripts/codex-review.sh`) — see AGENTS.md § Tiered Execution Workflow.
```

If the section text differs from the above, insert the same new sentence
immediately after the sentence that references `AGENTS.md`.

- [ ] **Step 3: Verify docs are consistent**

Run: `grep -n "codex-review.sh" AGENTS.md CLAUDE.md && grep -n "Tiered Execution Workflow" AGENTS.md CLAUDE.md`
Expected: both files reference the script; AGENTS.md contains the section heading; CLAUDE.md references it by name.

- [ ] **Step 4: Commit**

```bash
git add AGENTS.md CLAUDE.md
git commit -m "docs(workflow): document tiered execution + Codex gate in AGENTS.md

Tier tags (opus/sonnet/main) + Definition of Ready at plan time, model-
pinned dispatch, opus->codex->fable escalation ladder, and the per-feature
pre-push review gate. CLAUDE.md gets a pointer.

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: Shakedown run — gate the pending develop push

**Tier:** main

**Files:**
- Create: `reviews/<timestamp>-codex-review.md` + `reviews/<timestamp>-diff.txt` (gitignored artifacts)
- Modify: `docs/session-handoffs.md` (append adjudication entry); any files needing fixes for confirmed findings

**Interfaces:**
- Consumes: `scripts/codex-review.sh` (Task 1); AGENTS.md gate procedure (Task 2).
- Produces: adjudicated verdict on the entire unpushed develop range; the push itself (only after Matthew's go-ahead).

This task runs in the main Fable session by design — adjudication is the
main session's job per AGENTS.md. It uses the real Codex CLI and real money/
quota; it is also the review the pending develop push was already gated on.

- [ ] **Step 1: Pre-flight**

Run: `git status --porcelain && git log --oneline @{upstream}..HEAD | wc -l`
Expected: clean working tree; ~39 unpushed commits (36 pre-existing + spec + Tasks 1-2). If the tree is dirty, stop and resolve before reviewing.

- [ ] **Step 2: Run the gate**

Run: `bash scripts/codex-review.sh`
Expected: "Sending diff to Codex for review (Pre-push review: origin/develop..HEAD ...)"; the command may take several minutes (the Bash tool auto-backgrounds it as a harness-tracked job). On completion: `Verdict saved to: reviews/<timestamp>-codex-review.md`, exit 0. If exit 2/4: follow the degrade path in AGENTS.md (Fable-only review, note it in the handoff entry) and continue from Step 3 using that review instead.

- [ ] **Step 3: Adjudicate**

Read the verdict file. For every finding, produce an explicit ruling in the working notes: `CONFIRMED — <fix summary>` or `DISMISSED — <reason>`. No finding may be skipped.

- [ ] **Step 4: Fix confirmed findings**

Fix inline for small changes; dispatch an Opus worker for substantial ones (per the Definition of Ready — if a finding can't be turned into a ready task, it needs discussion with Matthew first). After fixes:

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: build succeeds, 88 tests pass.

Commit fixes:

```bash
git add -A ':!reviews'
git commit -m "fix: address confirmed findings from Codex pre-push review

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

If the fix diff is substantial (multiple source files or behavior changes), re-run the gate once: `bash scripts/codex-review.sh` and repeat Steps 3-4 for new findings. Small fixes don't loop.

- [ ] **Step 5: Record the adjudication in session-handoffs**

Append an entry to `docs/session-handoffs.md` following its existing entry format: what changed (gate implemented + shakedown run), why, adjudication summary (N findings: X confirmed/fixed, Y dismissed with reasons), status, next steps, verification commands/results.

```bash
git add docs/session-handoffs.md
git commit -m "docs: session handoff — Codex gate shakedown on develop range

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Report and gate the push on Matthew**

Report the adjudication summary to Matthew and ask for the push go-ahead. Push ONLY after explicit approval, and immediately after (no parallel work in between, per the commit/push race rule):

```bash
git push origin develop
```

---

## Self-Review Notes

- **Spec coverage:** script interface/behavior/exit codes → Task 1; `reviews/` gitignore → Task 1 Step 7; AGENTS.md + CLAUDE.md + escalation ladder documentation → Task 2; shellcheck + dry-run matrix → Task 1 Steps 4-5; first live run on the unpushed develop range → Task 3. Tier tags/DoR are conventions documented in Task 2 (spec's out-of-scope note: no skill changes). No gaps found.
- **Consistency:** exit codes 0/1/2/4 identical in Global Constraints, script header, harness expectations, and AGENTS.md text. Review-file naming `<timestamp>-codex-review.md` consistent between script and harness assertions.
- **Placeholders:** `<scratchpad>` (session scratchpad path) and `<timestamp>`/`<base-ref>`/`<repo-abs-path>` are runtime values, not unfinished content; all code blocks are complete.
