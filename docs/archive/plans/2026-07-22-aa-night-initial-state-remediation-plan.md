# Android Auto Night Initial-State Remediation — Implementation Plan

Status: COMPLETED 2026-07-22

**Design (read first):**
`docs/archive/plans/2026-07-22-aa-night-initial-state-remediation-design.md`
**Approved by Matthew:** 2026-07-22
**Grounded against:** `origin/main` at `e81c0686`
**Branch:** `agent/aa-night-initial-state-remediation`
**Publication:** one bounded commit at a time; no push until all gates,
adjudication, approved live validation, and closure are complete

## Goal

Deliver the authoritative day/night value in Android Auto's first NIGHT_DATA
indication, including when the value was established before channel open or
subscription, without changing other sensors or protocol definitions.

## Global Constraints

- Read root `AGENTS.md`, `src/AGENTS.md`, `src/core/aa/AGENTS.md`, and
  `libs/prodigy-oaa-protocol/AGENTS.md` before implementation.
- Preserve wireless-only AA, HFP Hands-Free role, no-ofono, frozen API schema,
  External API rails, and frozen numerics.
- Never modify `libs/prodigy-oaa-protocol/proto/`.
- Prefer handler-owned state retention over an orchestrator timing workaround.
- Update current behavior documentation in the implementation commit.
- One bounded task and commit at a time. Nobody pushes mid-execution.
- Do not deploy, modify live configuration, or restart services without
  Matthew's explicit approval at the live-validation gate.
- Preserve the Pi checkout's unrelated dirty state; never pull, reset, clean,
  or overwrite unrelated Pi files.
- New discoveries remain out of scope and follow wishlist-then-promote.
- Do not edit archived plans or old handoff entries. Closure prepends exactly
  one handoff and archives this design and plan together.

---

## Task 1 — Retain and seed authoritative night state

**Tier:** main
**Commit:** `fix(aa): preserve night state for initial sensor delivery`

### Files

- Modify: `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/SensorChannelHandler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/SensorChannelHandler.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `tests/test_sensor_channel_handler.cpp`
- Modify: `docs/architecture.md`

**Out of scope:** other remaining P1s, night-source redesign, provider
implementation redesign, session teardown redesign, protobuf/API changes, and
unrelated sensor semantics.

### Acceptance

- `pushNightMode()` always retains the supplied value.
- No night indication is sent before channel open plus NIGHT_DATA subscription.
- NIGHT_DATA subscription sends the retained value.
- Initial day and initial night payloads decode with the expected `is_night`.
- A pre-open update and an open-but-unsubscribed update are retained without
  premature transmission.
- Later day/night transitions decode correctly.
- Close/reopen/resubscribe sends the latest retained value.
- Driving-status and parking-brake initial payloads decode to their unchanged
  defaults.
- The orchestrator explicitly seeds from `NightModeProvider::isNight()` before
  `session_->start()`.
- No `.proto`, frozen API, transport, HFP, ofono, audio-routing, or QML change
  occurs.

### Focused verification

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_sensor_channel_handler \
  test_night_mode test_oaa_integration test_aa_orchestrator -j$(nproc)
ctest -R '^(test_sensor_channel_handler|test_night_mode|test_oaa_integration|test_aa_orchestrator)$' \
  --output-on-failure

cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

---

## Task 2 — Repository verification and review

**Tier:** main
**Depends on:** Task 1 committed
**Commit:** none unless an adjudicated fix is required

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
python3 scripts/check-doc-links.py
git diff --check
git diff --name-only origin/main...HEAD
bash scripts/codex-review.sh origin/main
```

Adjudicate every review finding. Confirmed findings are fixed in their own
bounded commit; dismissals carry a reason. A substantial confirmed fix triggers
the permitted single review-gate rerun.

**Out of scope:** expanding review findings into unrelated audit remediation or
new product scope.

---

## Task 3 — Cross-build and approval-gated live validation

**Tier:** main
**Depends on:** Task 2 green
**Commit:** none

### Required before the live approval boundary

```bash
./cross-build.sh
```

### Live gate

Stop and request Matthew's approval before Pi writes, live configuration
changes, binary deployment, or service operations. After approval:

- Record preflight application, hostapd, and Bluetooth PIDs and responsive IPC.
- Create a new rollback snapshot without replacing the retained operations
  snapshot.
- Preserve the Pi checkout and its unrelated dirty QML/submodule state.
- Deploy only the final reviewed aarch64 binary and the minimum temporary
  capture/config changes needed for validation.
- In an unambiguous day configuration, reconnect AA and decode the first
  outgoing `SensorEventIndicationMessage` as `is_night: false`; confirm the
  phone begins in day presentation without a transition.
- In an unambiguous night configuration, reconnect AA and decode the first
  outgoing indication as `is_night: true`; confirm immediate night
  presentation without a transition.
- Restore the exact original config, disable temporary capture, restart the
  application, and verify one process, responsive IPC, and successful wireless
  AA projection.
- Confirm hostapd and Bluetooth retained their PIDs. Do not restart either
  service.

Optional: repeat an unchanged-state phone disconnect/reconnect and verify the
latest first indication. Physical GPIO validation is not required.

**Out of scope:** Pi pull/reset/clean, unrelated file overwrite, Bluetooth or
hostapd restart, release packaging, tagging, or publication.

---

## Task 4 — Closure

**Tier:** main
**Depends on:** Task 3 required live rows green
**Commit:** `docs: complete AA night initial-state remediation`

### Files

- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md` by prepending exactly one entry
- Move this design and plan to `docs/archive/plans/` after marking both
  `COMPLETED 2026-07-22`

### Acceptance

- The handoff records changes, rationale, status, next steps, verification,
  Pi disposition, and review adjudication without exact suite counts.
- Both plans are archived together and no ACTIVE reference remains.
- Private remediation state is updated only outside tracked content, and all
  counts are recomputed rather than assumed.
- Link checks, targeted searches, `git diff --check`, and branch-history checks
  pass.
- Nothing has been pushed. Publication occurs only after Matthew's separate
  go-ahead, using this branch for a draft PR targeting `main`.

**Out of scope:** closing, renaming, or otherwise changing unrelated findings
or remediation state.
