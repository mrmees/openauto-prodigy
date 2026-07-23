# Bluetooth, HFP, and AVRCP State Remediation — Implementation Plan

Date: 2026-07-23
Status: COMPLETED 2026-07-23
Design: `docs/archive/plans/2026-07-23-bt-hfp-avrcp-state-remediation-design.md`
Base: `origin/main` at `caf368696c39e3ef2954e2492ecfb09944d2b187`

## Execution Rules

- One bounded task and commit at a time; nobody pushes mid-wave.
- Judgment-heavy D-Bus state and HFP ownership stay in the main tier. The
  bounded AVRCP task uses the approved opus tier dispatched as model
  `5.6-terra`.
- Preserve HFP HF-role, no-ofono, PipeWire Telephony ownership, frozen API and
  numerics, A2DP EQ/focus behavior, and wishlist-then-promote.
- Public content contains no private finding IDs, evidence, ledger path, or
  backlink.

## Task 1: Activate the consolidated wave

**Tier:** main

**Files:**

- Add: `docs/plans/2026-07-23-bt-hfp-avrcp-state-remediation-design.md`
- Add: `docs/plans/2026-07-23-bt-hfp-avrcp-state-remediation-plan.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`

**Acceptance criteria:** ACTIVE documents pin merged PR #32, record attempted
refutation without private identifiers, define exact bounded tasks, and include
the required/optional/approval-gated live matrix.

**Test command:** `git diff --check && python3 scripts/check-doc-links.py`

**Out of scope:** Runtime code, tests, deployment, or ledger closure.

## Task 2: Make Bluetooth state and Agent1 delivery coherent

**Tier:** main

**Files:**

- Modify: `src/core/services/BluetoothManager.hpp`
- Modify: `src/core/services/BluetoothManager.cpp`
- Modify: `qml/components/PairingDialog.qml`
- Modify: `tests/test_bluetooth_manager.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_api_serializers.cpp`
- Modify: `docs/architecture.md`
- Modify: `docs/design-decisions.md`

**Acceptance criteria:** one asynchronous/coalesced managed-object owner keeps
the Qt loop responsive and atomically derives adapter, paired, connected,
first-run, and auto-connect state; startup with an already-connected phone is
correct; later object add/removal cannot stale exported state; subscription
results are checked; bounded adapter calls remain; the DisplayYesNo Agent1
method surface has correct delayed/display-only/explicit-rejection semantics;
QML shows controls appropriate to the prompt mode; External API serialization
sees the derived connected state; observable signals are edge-only.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(bluetooth_manager|paired_devices_model|api_serializers)' --output-on-failure`

**Out of scope:** Adapter discovery policy, new auto-connect backoff, Bluetooth
daemon changes, re-pairing, plugin ABI changes, or new pairing-device support.

## Task 3: Remove blocking AVRCP discovery and close player loss

**Tier:** opus (dispatch model `5.6-terra`)

**Files:**

- Modify: `src/plugins/bt_audio/BtAudioPlugin.hpp`
- Modify: `src/plugins/bt_audio/BtAudioPlugin.cpp`
- Modify: `tests/test_bt_audio_plugin.cpp`
- Modify: `docs/architecture.md`
- Modify: `docs/design-decisions.md`

**Acceptance criteria:** startup enumeration is asynchronous; hot-plug handlers
perform no synchronous interface introspection/property reads; carried
transport/player/Device1 properties share one adoption contract; missing
properties remain safely unknown until later delivery; tracked player removal
resets playback, metadata, duration, position, and validity with edge-only
signals; sender filtering, multi-transport activity, controls, A2DP EQ tap,
focus behavior, and millisecond propagation remain unchanged.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(bt_audio_plugin|bt_tap_controller|media_status_service|api_serializers)' --output-on-failure`

**Out of scope:** AVRCP unit conversion, QML redesign, player arbitration
policy, PipeWire routing, EQ internals, media controls, or new codecs.

## Task 4: Enforce single-AG and evidence-based call state

**Tier:** main

**Files:**

- Modify: `src/core/services/TelephonyClient.hpp`
- Modify: `src/core/services/TelephonyClient.cpp`
- Modify: `src/core/services/PhoneStateService.hpp`
- Modify: `src/core/services/PhoneStateService.cpp`
- Modify: `src/plugins/phone/PhonePlugin.cpp`
- Modify: `tests/test_telephony_client.cpp`
- Modify: `tests/test_phone_state_service.cpp`
- Add: `tests/test_phone_plugin.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/architecture.md`
- Modify: `docs/design-decisions.md`

**Acceptance criteria:** the selected AG accepts only its same-object transport;
second-phone transport state, properties, and removal are ignored; Idle SCO
does not synthesize a call; setup/settle plus SCO, active-call loss debounce,
transport end, controls, and frozen call-state values remain; PhonePlugin
disconnects before clearing its provider and post-shutdown signals are safe.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(telephony_client|phone_state_service|phone_plugin|call_audio_policy|sco_node_monitor|api_serializers)' --output-on-failure`

**Out of scope:** AG/HF role changes, ofono, BVRA feature support, hold/swap,
multiparty, new API fields, call-audio routing, or live phone calls.

## Task 5: Gate, deploy, validate, close, and publish

**Tier:** main

**Files:**

- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move both ACTIVE documents to `docs/archive/plans/` with status
  `COMPLETED 2026-07-23`
- Private only: update the ignored remediation ledger overlay

**Acceptance criteria:** focused and full local gates pass; every repository
review finding is adjudicated and substantial fixes receive the required rerun;
aarch64 cross-build passes; rollback snapshot precedes deployment; one
responsive Pi process, immediate already-connected state, normal A2DP/AVRCP,
selected-AG stability, and unchanged hostapd/Bluetooth lifetimes pass; private
overlays close only after evidence; counts are recomputed; exactly one handoff
is appended; clean branch is pushed and a draft PR opened under standing
approval.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && cmake --build . --target openauto-prodigy -j$(nproc) && ctest --output-on-failure && cd /mnt/e/claude/personal/openautopro/openauto-prodigy && python3 scripts/check-doc-links.py && git diff --check origin/main`

**Out of scope:** Unplanned remediation, companion changes, daemon restart,
re-pairing, release tags, or ready-for-review promotion.
