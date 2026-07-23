# Android Auto Input, Video, and Night Remediation — Implementation Plan

Date: 2026-07-23
Status: COMPLETED 2026-07-23
Design: `docs/plans/2026-07-23-aa-input-video-night-remediation-design.md`
Base: `origin/main` at `8ec8aeab9b19a03fedce36761bd3be2a91bd1475`

## Execution Rules

- One bounded task and commit at a time; nobody pushes mid-wave.
- Protocol/threading work stays in the main session. The bounded night-service
  task uses the approved opus tier dispatched as model `5.6-terra`.
- Never edit protocol protobufs. Preserve wireless-only AA, HFP HF-role,
  no-ofono, frozen API/numerics, and wishlist-then-promote.
- Tests land with behavior. Public content contains no private finding IDs,
  evidence, ledger path, or backlink.

## Task 1: Activate the consolidated wave

**Tier:** main

**Files:**

- Add: `docs/plans/2026-07-23-aa-input-video-night-remediation-design.md`
- Add: `docs/plans/2026-07-23-aa-input-video-night-remediation-plan.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`

**Acceptance criteria:** ACTIVE documents pin merged PR #31, record attempted
refutation without private identifiers, define exact bounded tasks, and include
the required/optional live matrix.

**Test command:** `git diff --check && python3 scripts/check-doc-links.py`

**Out of scope:** Runtime code, tests, deployment, or ledger closure.

## Task 2: Reset video decode state at each stream boundary

**Tier:** main

**Files:**

- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Delete: `tests/test_video_decode_queue.cpp`
- Add: `tests/test_video_decoder.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/aa-protocol/aa-display-rendering.md`
- Modify: `docs/aa-protocol/aa-troubleshooting-runbook.md`
- Modify: `docs/design-decisions.md`

**Acceptance criteria:** a worker-ordered new-stream command clears prior queued
packets, parser/codec/frame/detection/fallback state before later packets;
H.264/H.265 re-detection works across repeated stream boundaries; tests drive
the real worker rather than copied deleted logic; existing Annex B, no-timestamp
configuration delivery, single-thread FFmpeg, fallback, frame pool, and sink
behavior remain unchanged.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(video_decoder|video_channel_handler|video_frame_pool|aa_orchestrator)' --output-on-failure`

**Out of scope:** New codecs, decoder selection policy, rendering/QML, frame
pool design, service discovery, touch, or night state.

## Task 3: Make evdev input state reader-thread-owned

**Tier:** main

**Files:**

- Modify: `src/core/aa/EvdevTouchReader.hpp`
- Modify: `src/core/aa/EvdevTouchReader.cpp`
- Modify: `src/core/aa/AndroidAutoRuntimeBridge.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Delete: `tests/test_sidebar_zones.cpp`
- Add: `tests/test_evdev_touch_reader.cpp`
- Modify: `tests/test_android_auto_runtime_bridge.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/aa-protocol/aa-video-resolution.md`
- Modify: `docs/aa-protocol/aa-display-rendering.md`
- Modify: `docs/design-decisions.md`

**Acceptance criteria:** initial configuration precedes thread start; later
mapping/grab/stop mutations are coherently consumed by the reader; phone-visible
membership excludes zone claims; same-report double-down/double-up produces
correct full arrays/actions/indices; DPI-scaled Navbar thickness is retained on
resolution change; poll/read device loss enters paced reopen and stop remains
responsive; registered tests drive product mapping and sync processing.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(evdev_touch_reader|touch_router|evdev_coord_bridge|android_auto_runtime_bridge|input_channel_handler)' --output-on-failure`

**Out of scope:** QML pointer handlers, new zones/gestures, input-device
selection, AA descriptor format, display layout, or synthetic live input.

## Task 4: Establish shared application-lifetime night state

**Tier:** opus (dispatch model `5.6-terra`)

**Files:**

- Add: `src/core/services/NightModeService.hpp`
- Add: `src/core/services/NightModeService.cpp`
- Modify: `src/core/aa/TimedNightMode.hpp`
- Modify: `src/core/aa/TimedNightMode.cpp`
- Modify: `src/core/aa/GpioNightMode.hpp`
- Modify: `src/core/aa/GpioNightMode.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/core/plugin/IHostContext.hpp`
- Modify: `src/core/plugin/HostContext.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/test_night_mode.cpp`
- Modify: `tests/test_aa_orchestrator.cpp`
- Modify: `docs/reference/config-schema.md`
- Modify: `docs/design-decisions.md`

**Acceptance criteria:** one application-lifetime service drives the real shell
theme and AA sensor cache; no session creates/destroys its own provider;
injected time tests pin every normal/inverted boundary and change-only signal;
GPIO direction/export/value failure is invalid and retryable through a
temporary sysfs seam; first valid recovery publishes once; unchanged values do
not; existing initial AA subscription delivery and force-dark behavior remain.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && cmake --build . --target openauto-prodigy -j$(nproc) && ctest -R 'test_(night_mode|aa_orchestrator|sensor_channel_handler|theme_service)' --output-on-failure`

**Out of scope:** New source semantics, QML settings, ambient sensors, GPIO
character-device migration, manual theme UX, External API schema, or AA proto.

## Task 5: Harden wireless admission and negotiated configuration

**Tier:** main

**Files:**

- Add: `src/core/aa/WirelessAaConfig.hpp`
- Add: `src/core/aa/WirelessAaConfig.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.hpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.cpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.hpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/test_aa_orchestrator.cpp`
- Modify: `tests/test_bt_discovery_service.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `tests/test_video_channel_handler.cpp`
- Modify: `docs/wireless-setup.md`
- Modify: `docs/aa-protocol/aa-video-resolution.md`
- Modify: `docs/design-decisions.md`

**Acceptance criteria:** active/backgrounded projection rejects a new TCP
socket without teardown or state change; pre-active replacement remains;
redundant RFCOMM status cannot change active state/watchdog; shutdown closes
admission and observes disconnect before calling stop, with no forced two-second
wait or reentry; one validated bind value and the effective listener port feed
Bluetooth advertisement; invalid/out-of-range text falls back; the setup
response count exactly matches the recognized advertised codec configs.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(aa_orchestrator|bt_discovery_service|service_discovery_builder|video_channel_handler|session_fsm)' --output-on-failure`

**Out of scope:** TLS identity/policy, AP peer allowlists, firewall, pairing,
credentials, RFCOMM protocol changes, new codecs, or USB transport.

## Task 6: Gate, deploy, validate, close, and publish

**Tier:** main

**Files:**

- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move both ACTIVE documents to `docs/archive/plans/` with status
  `COMPLETED 2026-07-23`
- Private only: update the ignored remediation ledger overlay

**Acceptance criteria:** focused and full local gates pass; every repository
review finding is adjudicated and substantial fixes receive the required
rerun; aarch64 cross-build passes; rollback snapshot precedes deployment; one
responsive Pi process, unchanged hostapd/Bluetooth lifetimes, both forced codec
sessions, active-client rejection, restored day/night config, and normal touch
mapping pass; all scoped private overlays close only after evidence; counts are
recomputed; exactly one handoff is appended; clean branch is pushed and a draft
PR opened under standing approval.

**Test command:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && cmake --build . --target openauto-prodigy -j$(nproc) && ctest --output-on-failure && cd /mnt/e/claude/personal/openautopro/openauto-prodigy && python3 scripts/check-doc-links.py && git diff --check origin/main`

**Out of scope:** Unplanned remediation, companion changes, daemon restart,
re-pairing, release tags, or ready-for-review promotion.
