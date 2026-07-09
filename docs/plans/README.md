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
- New feature ideas found mid-execution go to `docs/wishlist.md`, not into scope (**wishlist-then-promote**). Plans don't grow features mid-execution.

## What lives where (canonical docs)

| Question | Canonical source |
|---|---|
| Why does this feature exist / what's the priority? | `docs/roadmap-current.md` (delivery order wins on sequencing) |
| Hard constraints, commands, workflow, guardrails | root `AGENTS.md` — the single source of truth for agent instructions |
| Subsystem gotchas (Qt, AA protocol, QML, proto submodule) | the nested `AGENTS.md` nearest the code you're editing |
| Per-feature design rationale | the `*-design.md` this plan's header cites (may live in `docs/archive/plans/` once shipped) |
| Cross-cutting rails (API/JS/dashboards/overlays must compose) | root `AGENTS.md` § hard constraints; full history in `docs/archive/plans/2026-07-05-extensibility-architecture-design.md` |
| What previous sessions did / deviations | `docs/session-handoffs.md` |

Precedence when they disagree: **AGENTS.md constraints > design doc > plan detail.** If a plan step contradicts its design doc, stop and record the conflict in `docs/session-handoffs.md` rather than guessing.

## Picking up a plan

1. `git fetch` first, then check out the working branch (`dev` — single-branch workflow; PRs go to `main`).
2. Confirm the plan's `Status:` is ACTIVE. Read its **Global Constraints** and its design doc's executor-guidance section. These are the invariants; the tasks are just the route.
3. Design docs pin the commit they were grounded on. If `git log --oneline <pinned>..HEAD -- <files it cites>` shows changes, re-verify cited line numbers/signatures before editing — substrate drift is expected, silent misedits are not.
4. Execute tasks in order with `superpowers:subagent-driven-development` or `superpowers:executing-plans`. One task = one commit. Don't batch.

## Verification workflow (every task, no exceptions)

```bash
cd build && cmake .. && make -j$(nproc)      # local build (WSL2 Debian Trixie, Qt 6.8 system)
ctest --output-on-failure                     # full suite, all green
```

**ctest does NOT compile the app target** — always build `openauto-prodigy` too (`cmake --build . --target openauto-prodigy`) before claiming green; a broken `main.cpp` is invisible to the test suite.

Per-task: run the plan's targeted `ctest -R <test>` red→green cycle first (TDD is the norm), then the full suite before committing.

End of plan (or before any Pi deploy):

```bash
./cross-build.sh                              # Docker aarch64 cross-compile — NOT toolchain-pi4.cmake directly
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/
ssh matt@192.168.1.149 'sudo systemctl restart openauto-prodigy.service'
```

QML ships **inside the binary** (qt_add_qml_module + qmlcache) — QML/UI changes also require cross-build + binary rsync; a `git pull` on the Pi will NOT update the UI. Never claim a task done on a failing or skipped verification — report what actually happened (superpowers:verification-before-completion).

## Standing guardrails

The rails (frozen `proto/api/`, HF/AG roles, no-ofono, External-API rails, frozen numerics, submodule hands-off) live in root `AGENTS.md` — read them there before touching those areas.

## When things go sideways

- Bug or unexpected test failure → `superpowers:systematic-debugging` before any fix.
- A rail or invariant looks wrong for your case → stop, write the why in `docs/session-handoffs.md`, ask; don't silently deviate.
- Hardware-dependent step with no hardware (phone for live checks, Pi offline) → complete everything else, record the pending checklist item in the handoff, and say so plainly in your completion report.
- Finish a plan → append a handoff entry (what/why/status/verification results — match the existing format in `docs/session-handoffs.md`), flip the status header, and archive the plan.
