# Tiered Plan Execution + Codex Review Gate — Design

**Date:** 2026-07-09
**Status:** Approved design, pending implementation plan
**Source of concepts:** https://github.com/orionmilos0-jpg/fabletieredworkflow (evaluated 2026-07-09; concepts adopted selectively, nothing installed from the package)

## Motivation

Two problems, both confirmed by Matthew:

1. **Fable usage limits.** The main session runs on Fable and currently does
   planning, implementation, and the token-heavy build/fix loop itself. The
   bulk of that middle work does not need Fable.
2. **Review rigor is ad-hoc.** Codex (GPT-5.5 via the OpenAI Codex CLI) already
   reviews substantial changes before pushing (e.g. the bench-commit review
   gating the current develop push), but each time it is improvised.

## What we adopt / what we don't

Adopted (as concepts, integrated into the existing superpowers workflow):

- Model-tiered execution: implementation on Opus, mechanical work on Sonnet,
  Fable reserved for planning, adjudication, and escalation.
- Definition of Ready gate on dispatchable tasks.
- A `scripts/codex-review.sh` pre-push review gate with structured P1/P2/P3
  findings, adjudicated by Fable.
- Codex as a real escalation tier for stuck implementation work.

Explicitly NOT adopted (duplicates what we already have, or ceremony we don't
want):

- `handoffs/` directory and handoff-package template (superpowers plans already
  serve this role).
- `RUN-STATE.md` shared run log (`docs/session-handoffs.md` covers it).
- Autonomous `architect` subagent (superpowers brainstorming + writing-plans is
  interactive by design; Matthew stays in the planning loop).
- `/tier` slash command, standing `.claude/agents/*.md` files, their
  `install.sh`.

## Design

### 1. Tier tags at plan time

Every task in a superpowers plan gets a `Tier:` field:

| Tier | Model | Used for |
|------|-------|----------|
| `opus` | Opus | Default for implementation: real code, logic, cross-module changes |
| `sonnet` | Sonnet | Test scaffolding, mocks/fixtures, mechanical pattern-following edits, doc updates |
| `main` | Fable (main session) | Protocol-critical or judgment-heavy work (AA protocol internals, threading) where the plan itself anticipates design calls |

**Definition of Ready** — a task may not be dispatched to a subagent unless:

- [ ] Exact files are named
- [ ] Acceptance criteria are testable ("returns 422 on empty id", not "works")
- [ ] Scope is bounded, with an explicit out-of-scope line
- [ ] Test command is given
- [ ] No open questions remain

A task that cannot pass this checklist is surfaced to Matthew as a question,
not dispatched. Rationale: an ambiguous spec makes a cheaper model confidently
burn tokens down the wrong path, wiping out the savings.

**Small-work bypass:** trivial single-file fixes skip tiering entirely; the
main session just does them. Tiering pays off on multi-step plans only.

### 2. Dispatch at execution time

Subagent-driven execution as already practiced, with the model pinned per tier
tag via the Agent tool's `model` parameter. No standing agent definition files:
subagents inherit the repo `CLAUDE.md` (gotchas, build commands, conventions)
automatically.

Workers own the build/fix/test loop and report back **synthesized results
only**: files changed (one line each), test command + pass/fail counts,
deviations from the task, anything deferred. Raw logs never enter the main
session's context.

**Commit discipline unchanged:** workers commit per task as today. Pushing is
gated on the review gate (section 4). Pushes happen only immediately after a
review passes — never mid-execution while workers may be committing in
parallel.

### 3. Escalation ladder: Opus → Codex (GPT-5.5) → Fable

1. **Opus worker** — owns the task. If the build/tests won't go green after
   two focused attempts, or the worker reports blocked, escalate.
2. **Codex** — receives a prompt file containing the task (files, acceptance
   criteria, test command) plus the Opus worker's failure report. Run with
   write access using the known-reliable invocation:

   ```bash
   codex exec -C <repo-abs-path> -s workspace-write -o <scratchpad>/codex-task-verdict.txt - < <scratchpad>/codex-task-prompt.txt
   ```

   After Codex finishes, the main session (Fable) reads the verdict file AND
   `git diff`, and verifies the result against the task's acceptance criteria
   before moving on. Codex is trusted to write, not to self-certify. If the
   work passes verification, the main session commits it.
3. **Fable (main session)** — takes the task over directly if Codex also
   fails.

This ladder replaces any separate read-only "stuck consult" convention.

### 4. Codex review gate — `scripts/codex-review.sh`

Fires **once per feature, pre-push** (after a plan's execution completes and
tests are green), reviewing the whole unpushed range.

**Interface:**

- `bash scripts/codex-review.sh` — default scope `@{upstream}..HEAD` (the
  unpushed range). If no upstream is configured, falls back to uncommitted
  changes (`git diff HEAD`).
- `bash scripts/codex-review.sh <base-ref>` — explicit range `<base-ref>..HEAD`.

**Behavior:**

- Validates it is inside a git repo and (if given) that `<base-ref>` resolves;
  errors exit 1.
- Empty diff → message + exit 0.
- `codex` CLI not on PATH → note + exit 2 (distinct code; the workflow then
  degrades to a Fable-only review with a note in the session-handoffs entry —
  the gate never silently passes).
- Builds a prompt file containing: review instructions (structured findings
  with P1 = must fix / P2 = should fix / P3 = nice to have; each finding lists
  file, line/area, severity, issue, suggested fix; "LGTM" if clean), the
  changed-file list, and the full diff.
- Invokes Codex **read-only** — the gate never writes; only the escalation
  ladder does:

  ```bash
  codex exec -C "$REPO_ROOT" -s read-only -o "reviews/<timestamp>-codex-review.md" - < "$PROMPT_FILE"
  ```

  Prompt via stdin file, verdict via `-o` — never the prompt as a command-line
  argument (shell-quoting fragility with large diffs).
- `codex exec` failure → error + exit 4 (suggest `codex login` for auth
  issues).
- Saves `reviews/<timestamp>-diff.txt` alongside the verdict; prints the
  verdict path. `reviews/` is gitignored (transient artifacts, not repo
  history).

**Adjudication (Fable, main session):** every Codex finding gets an explicit
ruling — confirmed (→ fixed: inline if small, routed to an Opus worker if
substantial) or dismissed (→ stated reason). No silent drops. Substantial
fixes trigger one re-run of the gate on the new range; small fixes don't loop.
The adjudication summary (confirmed/dismissed counts, blockers fixed) goes in
the session-handoffs entry. Then, and only then, push.

### 5. Documentation and housekeeping changes

- **`AGENTS.md`** — single source of truth for the updated loop: tier tags +
  Definition of Ready at plan time, tiered dispatch, escalation ladder,
  gate-then-push. Includes the exact Codex invocation patterns for both the
  escalation tier (workspace-write) and the review gate (read-only).
- **`CLAUDE.md`** (repo-level) — two-line pointer to the AGENTS.md workflow
  section.
- **`.gitignore`** — add `reviews/`.

## Failure modes

| Failure | Handling |
|---------|----------|
| Codex CLI missing/broken at gate time | Exit 2/4; Fable-only review + note in handoff entry; never silently passes |
| Codex escalation produces a bad diff | Fable verification against acceptance criteria catches it; ladder proceeds to Fable takeover |
| Ambiguous plan task | Definition of Ready blocks dispatch; question surfaced to Matthew |
| Worker burns attempts without progress | Two-attempt cap forces escalation instead of grinding |
| Push sweeps up parallel unreviewed commits | Push only immediately after a passing review; never mid-execution |

## Verification

- **Script:** shellcheck-clean; manual dry runs for each path (no arg with
  upstream, explicit base ref, empty diff, codex absent via PATH manipulation).
- **First live run:** the pending unpushed develop range (~36 commits) — the
  review that push was already gated on becomes the gate's shakedown cruise.
- **Workflow:** first multi-step plan after adoption runs tiered dispatch
  end-to-end; session-handoffs entry records how it went.

## Out of scope

- No changes to the superpowers skills themselves (tier tags and DoR are
  conventions applied when writing plans in this repo, documented in
  AGENTS.md).
- No CI integration of the gate (local pre-push discipline only, for now).
- No automation of the push itself.
