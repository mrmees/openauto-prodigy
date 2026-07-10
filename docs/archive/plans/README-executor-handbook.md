# Executor Handbook — Fable Sprint Plans

Status: COMPLETED 2026-07-09 (superseded by docs/plans/README.md)

**Audience:** any agent (or human) picking up an implementation plan from this directory. Read this once before your first task; every sprint plan assumes it.

## 1. What lives where (canonical docs)

| Question | Canonical source |
|---|---|
| Why does this feature exist / what's the priority? | `docs/roadmap-current.md` (delivery order) — the sprint produced *designs ahead of* that order; roadmap wins on sequencing |
| Cross-cutting rails (API/JS/dashboards/overlays must compose) | `docs/superpowers/specs/2026-07-05-extensibility-architecture-design.md` — **read §8 Executor Guidance before touching any of those four areas** |
| Per-feature design rationale | the `*-design.md` in `docs/superpowers/specs/` that your plan's header cites |
| The task list you execute | the plan in this directory |
| What previous sessions did / deviations | `docs/session-handoffs.md` |
| Build gotchas (Qt, D-Bus, PipeWire, Pi) | `CLAUDE.md` — the Gotchas section is load-bearing, not decorative |

Precedence when they disagree: CLAUDE.md constraints > design doc > plan detail. If a plan step contradicts its design doc, stop and record the conflict in `docs/session-handoffs.md` rather than guessing.

## 2. Picking up a plan

1. `git fetch` first, then check out the working branch the handoff log names (sprint output lives on `fable-design-sprint` until its PR merges).
2. Read the plan's **Global Constraints** and its design doc's **Executor Guidance** section. These are the invariants; the tasks are just the route.
3. Every design doc pins the commit it was grounded on. If `git log --oneline <pinned>..HEAD -- <files it cites>` shows changes, re-verify the cited line numbers/signatures before editing — substrate drift is expected, silent misedits are not.
4. Execute tasks in order with `superpowers:subagent-driven-development` or `superpowers:executing-plans`. One task = one commit. Don't batch.

## 3. Verification workflow (every task, no exceptions)

```bash
cd build && cmake .. && make -j$(nproc)      # local build (WSL2 Debian Trixie, Qt 6.8 system)
ctest --output-on-failure                     # full suite — currently ~88 tests, all green
```
Per-task: run the plan's targeted `ctest -R <test>` red→green cycle first (TDD is the norm), then the full suite before committing.

End of plan (or before any Pi deploy):
```bash
./cross-build.sh                              # Docker aarch64 cross-compile — NOT toolchain-pi4.cmake directly (fast default: app target only, ~4-6 min; use --full for all targets incl. ARM test binaries, ~20 min)
rsync -av build-pi/src/openauto-prodigy matt@<pi-ip>:~/openauto-prodigy/build/src/
ssh matt@<pi-ip> 'sudo systemctl restart openauto-prodigy.service'
```
Pi IP: `192.168.1.149` (static — the old `.152` address is retired), QML changes go via `git pull` on the Pi (not in the binary). Never claim a task done on a failing or skipped verification — report what actually happened (superpowers:verification-before-completion).

## 4. Standing guardrails

- **`libs/prodigy-oaa-protocol/` is hands-off** — community submodule. Note needed proto changes; never edit them here.
- **`proto/api/` is FROZEN additive-only** (since `875feaf`): field numbers never reused, messages never renamed, semantics never silently changed. New capability = new field + capability flag. Do not re-litigate Codex-reviewed decisions (e.g. the deleted answer/hangup capability special-case stays deleted).
- **HF/AG roles:** the Pi is the HFP Hands-Free (0x111e); the phone is the Audio Gateway. If you're registering profile 0x111f on the Pi, stop.
- **No ofono, no `provide-ofono`** — telephony goes through `org.pipewire.Telephony` directly.
- **Rail R1:** the External API binds providers/services, never EventBus topics, D-Bus paths, or AA protocol internals. **Rail R4:** all mutation through ActionRegistry or explicit invokables. **Rail R5/R3:** additive proto; the JS shim gets no capability the public API lacks.
- **Wishlist-then-promote:** new feature ideas go to `docs/wishlist.md`, not into scope. Plans don't grow features mid-execution.
- **Frozen numerics:** `ICallStateProvider` values (`Idle=0, Ringing=1, Active=2`), overlay z-bands (1000/2000/3000/3500/4000), `DashboardContributionKind` order, YAML placement field names. Append, never renumber.
- Commits during sprint execution are pre-approved; **push/PR still ask Matthew first**.

## 5. Cross-plan dependency map (execution order within the sprint output)

```
external-api-v1 (16 tasks)  ──┬─→ js-runtime impl plan (write it AFTER api lands — JS design §9)
hfp-call-audio (9 tasks) ─────┤   (its Task 8 and the API's Tasks 7/11 share the phone seam:
                              │    whichever lands SECOND applies the truthful-capability code)
multi-dashboards (7 tasks) ───┼─→ WebWidget descriptors arrive with the js-runtime package scanner
overlay-framework (4 tasks) ──┘   (independent of dashboards; both independent of API/HFP)
```
Safe parallel pairs: HFP ∥ dashboards, HFP ∥ overlays, API ∥ dashboards, API ∥ overlays. Do not run the API plan and the HFP plan concurrently in separate sessions — they both touch `IPhoneStateService` consumers.

## 6. When things go sideways

- Bug or unexpected test failure → `superpowers:systematic-debugging` before any fix.
- A rail or invariant looks wrong for your case → stop, write the why in `docs/session-handoffs.md`, ask; don't silently deviate.
- Hardware-dependent step with no hardware (phone for HFP live checks L1–L6, Pi offline) → complete everything else, record the pending checklist item in the handoff, and say so plainly in your completion report.
- Finish a plan → append a handoff entry (what/why/status/verification results — match the existing format in `docs/session-handoffs.md`).
