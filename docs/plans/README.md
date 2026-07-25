# Plans — Conventions & Executor Guidance

**Audience:** any agent (or human) picking up a design or implementation plan from this directory. Read this once before your first task; every plan assumes it.

## Plan conventions

- Every plan/design file carries a `Status:` header near the top. Vocabulary:
  - `ACTIVE` — current guidance; safe to execute.
  - `COMPLETED <YYYY-MM-DD>` — done; kept for history.
  - `PARKED — <reason>` — intentionally on hold; do not execute without re-triage.
  - `ABANDONED — <reason>` — dead end; never execute.
- **Only ACTIVE files are current guidance.** Everything else is context.
- Completion flips the header to `COMPLETED <date>` and moves the file to `docs/archive/plans/` **in the same commit**.
- New plans and specs (from brainstorming/writing-plans or any other process) are saved HERE in `docs/plans/` — nowhere else.
- New user-facing capability ideas found mid-execution go to `docs/wishlist.md` (**wishlist-then-promote**).
- Concrete technical findings go to `docs/engineering-backlog.md`; they must be re-researched against current code before promotion.
- Unconfirmed milestone or hardware observations go to `docs/validation-current.md` until validated.
- None of these buckets grows an active plan's scope.

## What lives where (canonical docs)

| Question | Canonical source |
|---|---|
| Why does this feature exist / what's the priority? | `docs/roadmap-current.md` (delivery order wins on sequencing) |
| Hard constraints, commands, workflow, guardrails | root `AGENTS.md` — the single source of truth for agent instructions |
| Subsystem gotchas (Qt, AA protocol, QML, proto submodule) | the nested `AGENTS.md` nearest the code you're editing |
| Per-feature design rationale | the `*-design.md` this plan's header cites (may live in `docs/archive/plans/` once shipped) |
| Cross-cutting rails (API/JS/dashboards/overlays must compose) | root `AGENTS.md` § hard constraints; full history in `docs/archive/plans/2026-07-05-extensibility-architecture-design.md` |
| What previous sessions did / deviations | `docs/session-handoffs.md` |
| Which user-facing capabilities are not yet promoted? | `docs/wishlist.md` |
| Which technical leads need fresh research? | `docs/engineering-backlog.md` |
| Which current milestone observations need hardware validation? | `docs/validation-current.md` |

Precedence when they disagree: **AGENTS.md constraints > design doc > plan detail.** If a plan step contradicts its design doc, stop and record the conflict in `docs/session-handoffs.md` rather than guessing.

## Picking up a plan

1. `git fetch` first, then check out the working branch (`dev` — single-branch workflow; PRs go to `main`).
2. Confirm the plan's `Status:` is ACTIVE. Read its **Global Constraints** and its design doc's executor-guidance section. These are the invariants; the tasks are just the route.
3. Design docs pin the commit they were grounded on. If `git log --oneline <pinned>..HEAD -- <files it cites>` shows changes, re-verify cited line numbers/signatures before editing — substrate drift is expected, silent misedits are not.
4. Execute tasks in order, inline by default. Use subagents only for genuinely
   independent, non-overlapping work or when the user asks. Prefer one coherent
   commit per task, but do not split mechanical edits merely to create commits.

## Verification workflow

Run the narrow test that proves each task while iterating. Run the complete
gate once on the final tree, selected by the changed surface:

- Docs/tooling only: targeted tests, syntax/lint, relevant doc-link checks, and
  `git diff --check`. Do not compile the application.
- C++/QML/CMake/runtime config: native build, explicit `openauto-prodigy`
  target, and `ctest --output-on-failure` from the Linux-filesystem build dir.
- Pi artifact, embedded QML, or target-only behavior: add `./cross-build.sh`.
- Hardware behavior: add the relevant live check when hardware is available.

**ctest does NOT compile `main.cpp`**. Application changes always require the
explicit `openauto-prodigy` target before claiming green.

QML ships **inside the binary** (qt_add_qml_module + qmlcache), so a Pi QML
test requires cross-build and binary deployment. A `git pull` does not update
the UI. Never claim completion from stale or inapplicable verification.

## Standing guardrails

The rails (frozen `proto/api/`, HF/AG roles, no-ofono, External-API rails, frozen numerics, submodule hands-off) live in root `AGENTS.md` — read them there before touching those areas.

## When things go sideways

- Bug or unexpected test failure → investigate root cause before any fix.
- A rail or invariant looks wrong for your case → stop, write the why in `docs/session-handoffs.md`, ask; don't silently deviate.
- Hardware-dependent step with no hardware (phone for live checks, Pi offline) → complete everything else, record the pending checklist item in the handoff, and say so plainly in your completion report.
- Finish a plan → append a handoff entry (what/why/status/verification results — match the existing format in `docs/session-handoffs.md`), flip the status header, and archive the plan.
