# Bluetooth AVRCP Time-Unit Remediation — Implementation Plan

Status: ACTIVE

**Design (read first):**
`docs/plans/2026-07-22-bt-avrcp-time-units-remediation-design.md`
**Approved by Matthew:** 2026-07-22
**Grounded against:** `origin/main` at `19ec68f6`
**Branch:** `agent/bt-avrcp-time-units-remediation`
**Publication:** one bounded commit at a time; no push until all gates,
adjudication, approved live validation, and closure are complete

## Goal

Preserve BlueZ AVRCP Duration and Position milliseconds through initial player
adoption, later property updates, shared media state, QML consumers, and
External API serialization without changing unrelated Bluetooth behavior.

## Global Constraints

- Read root `AGENTS.md` and `src/AGENTS.md` before implementation.
- Read `qml/AGENTS.md` only if an approved implementation change reaches QML;
  the approved design expects no QML edit.
- Preserve wireless-only AA, HFP Hands-Free role, no-ofono, frozen API schema,
  External API rails, and frozen numerics.
- Do not modify `libs/prodigy-oaa-protocol/proto/` or `proto/api/`.
- Fix units once at the BlueZ ingestion boundary; do not compensate in
  individual consumers.
- Preserve sender filtering, multi-transport activity, metadata/playback
  state, controls, EQ tap, focus, and routing behavior.
- Update current behavior documentation in the implementation commit.
- One bounded task and commit at a time. Nobody pushes mid-execution.
- Do not deploy, restart the application, operate Bluetooth playback, or
  modify live state without Matthew's explicit approval at the live gate.
- Preserve the Pi checkout's unrelated dirty state; never pull, reset, clean,
  or overwrite unrelated Pi files.
- New discoveries remain out of scope and follow wishlist-then-promote.
- Private audit state stays only in the ignored ledger overlay. Tracked files
  contain no audit identifiers, evidence, or backlinks.
- Do not edit archived plans or old handoff entries. Closure prepends exactly
  one handoff and archives this design and plan together.

---

## Task 1 — Preserve BlueZ milliseconds and coherent progress

**Tier:** main
**Commit:** `fix(bt-audio): preserve BlueZ playback times in milliseconds`

### Files

- Modify: `src/plugins/bt_audio/BtAudioPlugin.hpp`
- Modify: `src/plugins/bt_audio/BtAudioPlugin.cpp`
- Modify: `src/core/services/MediaStatusService.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_bt_audio_plugin.cpp`
- Modify: `tests/test_media_status_service.cpp`
- Modify: `tests/test_api_serializers.cpp`
- Modify: `docs/architecture.md`

**Out of scope:** logging configuration, general D-Bus async refactoring,
player-removal playback-state changes, transport/focus/EQ/routing changes,
QML edits, protobuf/API changes, AA, HFP, ofono, pairing, and unrelated audit
findings.

### Acceptance

- BlueZ Duration 215000 remains 215000 milliseconds.
- BlueZ Position 61000 remains 61000 milliseconds.
- Startup `GetManagedObjects`, hot player adoption, and later
  `PropertiesChanged` use one time/type contract.
- `Track` is correctly read from `QVariantMap` and manually demarshaled
  `QDBusArgument` shapes.
- Valid `uint32` values above `INT_MAX` widen to `qint64` without wrap or
  scaling.
- A duration-only Track update changes duration without requiring title,
  artist, or album changes.
- Metadata, duration, position, and coherent-progress signals emit only when
  their observable state changes; a batch changing both time fields publishes
  one consistent downstream snapshot.
- Missing and invalid time fields retain documented unknown/zero behavior.
- Initial and reconnection catch-up publishes current valid progress to
  `MediaStatusService`.
- `MediaStatusService` receives exact millisecond values and suppresses
  duplicate Bluetooth progress notifications.
- The Bluetooth time labels and progress ratio, shared now-playing widget, and
  External API continue consuming milliseconds with no compensating
  conversion.
- Existing sender filtering, multi-transport activity, playback state,
  metadata, controls, EQ tap, and focus tests remain green.
- No forbidden protocol, API, transport, telephony, routing, logging, or QML
  change occurs.

### Focused verification

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target \
  test_bt_audio_plugin test_media_status_service test_api_serializers \
  -j$(nproc)
ctest -R '^(test_bt_audio_plugin|test_media_status_service|test_api_serializers)$' \
  --output-on-failure
```

---

## Task 2 — Full local verification and repository review

**Tier:** main
**Depends on:** Task 1 committed
**Commit:** none unless an adjudicated fix is required

```bash
cd ~/builds/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure

cd /mnt/e/claude/personal/openautopro/openauto-prodigy
python3 scripts/check-doc-links.py
git diff --check
git diff --name-only origin/main...HEAD
bash scripts/codex-review.sh origin/main
```

Adjudicate every review finding. Confirmed findings are fixed in their own
bounded commit; dismissals carry a reason. A substantial confirmed fix
triggers the permitted single review-gate rerun.

**Out of scope:** expanding review findings into unrelated remediation or new
product scope.

---

## Task 3 — Cross-build and approval-gated live validation

**Tier:** main
**Depends on:** Task 2 green
**Commit:** none

```bash
./cross-build.sh
```

Stop and request Matthew's approval before Pi writes, binary deployment,
application restart, Bluetooth playback operation, or other live changes.

### Required after approval

- Record preflight application, hostapd, and Bluetooth PIDs and responsive
  IPC without changing service state.
- With an already-paired phone playing a known multi-minute track, capture the
  live BlueZ `MediaPlayer1.Track.Duration` and `Position` values.
- Compare those values with the Bluetooth view, shared now-playing state, and
  External API media status.
- Deploy only the final reviewed aarch64 binary after separate deployment
  approval and create a new rollback snapshot without replacing retained
  snapshots.
- Verify one application process, responsive IPC, normal A2DP playback,
  correct progress/time display, and unchanged hostapd/Bluetooth PIDs.
- Preserve the Pi checkout and its unrelated dirty QML/submodule state.

Optional: pause/resume, track change, and a second phone/player.

Not required: Bluetooth daemon restart, re-pairing, HFP call testing, AA
protocol capture, unrelated QML inspection, tagging, or release publication.

---

## Task 4 — Closure

**Tier:** sonnet
**Depends on:** Task 3 required live rows green
**Commit:** `docs: complete Bluetooth AVRCP time-unit remediation`

### Files

- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md` by prepending exactly one entry
- Move this design and plan to `docs/archive/plans/` after marking both
  `COMPLETED 2026-07-22`
- Update only the ignored private ledger overlay for remediation lifecycle and
  publication state

### Acceptance

- The handoff records changes, rationale, status, next steps, verification,
  live disposition, and review adjudication without exact suite counts.
- Both plans are archived together and no ACTIVE reference remains.
- Private counts are recomputed from immutable array positions plus overlays.
- Documentation links, targeted searches, `git diff --check`, and branch
  history checks pass.
- Nothing has been pushed. Publication occurs only after Matthew's separate
  go-ahead, using this branch for a standalone draft PR targeting `main`.

**Out of scope:** changing unrelated findings or remediation state.
