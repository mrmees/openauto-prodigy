# Agentic Workflow Simplification Plan

Status: COMPLETED 2026-07-25

**Goal:** Replace overlapping, unbounded agent workflows with one lean,
author-aware execution and review policy that preserves technical verification.

**Architecture:** A short global operating contract governs scope, delegation,
review budgets, and model cost across Codex and Claude. Repository instructions
retain Prodigy-specific constraints and commands while overriding generic skill
ceremony. A provider-agnostic review script records immutable SHAs and enforces
one initial pass plus one remediation pass.

**Tech stack:** Markdown agent instructions, Bash, git, jq, Codex CLI, Claude
advisor runtime.

## Global constraints

- Preserve the user's untracked AA display baseline.
- Do not alter application behavior or frozen protocol surfaces.
- Normal single-repository work gets one independent reviewer; Fable is
  reserved for major or multi-repository work.
- Reviews run to completion without a wall-clock timeout but are bounded by
  immutable scope and a two-pass gate.
- Only supported, reachable production failures block after remediation.
- Credential rotation remains manual; do not print or rewrite secrets.

## Task 1: Pin the review-gate contract with failing shell tests

**Files:**

- Create: `tests/test_review_gate.sh`
- Create: `scripts/review-gate.sh`
- Modify: `scripts/codex-review.sh`

**Acceptance criteria:**

- Pass one reviews the requested base through the captured HEAD.
- Pass two reviews only the first reviewed HEAD through the new HEAD.
- Repeating an already reviewed HEAD fails without launching a reviewer.
- Pass three fails without launching a reviewer.
- A changed base SHA is refused until an explicit user-authorized reset, after
  which it starts a fresh feature gate.
- The prompt requires a supported production entry point, reachable call chain,
  material impact, and evidence for blockers.
- Reviewer selection is independent of the author.

**Test command:** `bash tests/test_review_gate.sh`

**Out of scope:** validating external model quality or contacting a live model
from the test suite.

## Task 2: Replace repository workflow layering

**Files:**

- Modify: `AGENTS.md`
- Modify: `docs/plans/README.md`
- Modify: `scripts/README.md`

**Acceptance criteria:**

- Rules distinguish trivial, standard, and major work.
- Generic skills cannot add extra specs, plans, worktrees, subagents, or review
  gates beyond repository policy.
- Mandatory per-task subagents and per-task full builds are removed.
- The two-pass production-reachability review contract is explicit.
- Accepted hardware SHAs are protected from nonblocking review churn.

**Test command:** `bash tests/test_review_gate.sh && bash -n scripts/review-gate.sh scripts/codex-review.sh`

**Out of scope:** changing technical subsystem constraints or application build
commands.

## Task 3: Install the global operating contract

**Files:**

- Create/update: Linux and Windows global Codex `AGENTS.md`
- Create/update: Linux and Windows global Claude `CLAUDE.md`
- Modify: Linux and Windows Codex model defaults
- Modify: Windows Claude default model

**Acceptance criteria:**

- All four instruction files contain the same compact operating contract.
- Codex defaults use `high` reasoning rather than inherited `xhigh` or divergent
  platform settings.
- Claude defaults to Opus; Fable remains an explicit major-work choice.
- Secrets are neither printed nor modified.

**Test command:** compare policy-file hashes and query only the non-secret model
keys.

**Out of scope:** credential rotation and third-party plugin source changes.

## Task 4: Verify and document

**Files:**

- Modify: `docs/session-handoffs.md`
- Archive this plan under `docs/archive/plans/`

**Acceptance criteria:**

- Shell tests and syntax checks pass.
- Documentation links pass for tracked live documents.
- Review-gate behavior is exercised entirely with fake reviewers.
- The handoff records changes, rationale, status, next steps, and verification.
- The user-owned untracked baseline remains untouched.

**Test command:**

```bash
bash tests/test_review_gate.sh
bash -n scripts/review-gate.sh scripts/codex-review.sh
python3 scripts/check-doc-links.py --scope tracked-live
git diff --check
```

**Out of scope:** application compilation, Pi deployment, or hardware testing;
no application source or runtime behavior changes.
