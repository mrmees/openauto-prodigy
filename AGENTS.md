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

Full design: `docs/archive/plans/2026-07-09-tiered-execution-codex-gate-design.md`.
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

After a plan's tasks are done, **the `openauto-prodigy` app target builds**,
and tests are green. (ctest does not compile `main.cpp` — a cached object file
masked an app-target break on 2026-07-09; always build the app target
explicitly before gating.)

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
