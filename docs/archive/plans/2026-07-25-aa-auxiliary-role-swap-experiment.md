# Android Auto AUXILIARY Role-Swap Experiment Plan

Status: COMPLETED 2026-07-25

Grounded on Prodigy `67371be` and open-android-auto's deleted
`dev/android-auto-17.3-analysis` branch recovered at
`231f932b3c57ed1cdb423c343dd0fd2a1aa09239`.

## Goal

Temporarily advertise the existing projected secondary display as
`DISPLAY_TYPE_AUXILIARY` instead of `DISPLAY_TYPE_CLUSTER`, capture what Android
Auto 17.3 projects under bounded phone states, then restore the normal CLUSTER
source and Pi binary.

## Architecture

Reuse the existing display ID 1, wire channels 12/13, paired input descriptor,
video handler, decoder, sink, crop, and runtime profile controls. Only the
phone-facing `AVChannel.display_type` and its diagnostic/test expectation change
for the experimental binary. Local CLUSTER names intentionally remain because
this is a disposable role swap, not a supported secondary-display abstraction.

## Constraints

- MAIN remains display ID 0 and `DISPLAY_TYPE_MAIN`.
- The secondary display remains ID 1 with exactly one matching input descriptor.
- No protocol-submodule or frozen External API protobuf edits.
- No runtime display-type selector and no simultaneous CLUSTER+AUXILIARY.
- The turn-data bit remains off except for the explicit active-navigation A/B.
- Preserve the installed pre-CLUSTER binary and restore the normal CLUSTER
  artifact after the experiment unless a result requires immediate follow-up.

## Task 1 — Produce and verify the temporary AUXILIARY artifact

**Files:**

- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`

- [x] Change the paired-topology test to expect
  `oaa::proto::enums::DisplayType::AUXILIARY` for descriptor channel 12 while
  retaining display ID 1 and input channel 13.
- [x] Run `test_service_discovery_builder` and confirm it fails because the
  implementation still emits `CLUSTER`.
- [x] Change `buildClusterVideoDescriptor()` to serialize
  `DisplayType::AUXILIARY` and change the session diagnostic role label to
  `AUXILIARY`. Do not rename the local session, APIs, QML context, or widget.
- [x] Re-run `test_service_discovery_builder`, build the explicit
  `openauto-prodigy` target, and run `QT_QPA_PLATFORM=offscreen ctest
  --output-on-failure`.
- [x] Run one independent Opus review of the bounded experimental diff. Fable
  is not used because its invocation previously failed to produce an observable
  result; the existing tooling follow-up remains open.
- [x] Run `./cross-build.sh`, deploy the aarch64 binary, restart Prodigy once,
  and confirm the service returns `READY=1` and the descriptor log says
  `role=AUXILIARY display=1 video_ch=12 input_ch=13`.

## Task 2 — Capture the result and restore CLUSTER

- [x] With baseline 480p/140-DPI/300x300 geometry, capture AUXILIARY with no
  active navigation and no media.
- [x] Capture AUXILIARY with no navigation and YouTube Music actively playing;
  verify AA media channel 4 starts before the screenshot.
- [x] Capture AUXILIARY with an active Maps route and turn-data bit 16 off.
- [x] Capture the same active route with turn-data bit 16 on.
- [x] Retain screenshots, manifests, negotiated descriptor/frame logs, and the
  user's visual assessment in the Windows capture directory.
- [x] Reverse the temporary source/test/log-label patch, rebuild and redeploy
  the normal CLUSTER artifact, and confirm the Pi returns to the compiled
  480p/140-DPI/300x300 CLUSTER baseline.
- [x] Record the confirmed result in live docs, mark this plan completed, move
  it to `docs/archive/plans/`, update the session handoff, run the tracked-live
  link check and `git diff --check`, and commit the documentation record.

## Acceptance criteria

- The experimental SDP contains MAIN ID 0 plus AUXILIARY ID 1 and one input
  descriptor matching each display.
- The phone opens channels 12/13, starts a secondary video stream, and Prodigy
  decodes the advertised carrier—or the exact rejection/failure is captured.
- The four phone-state screenshots distinguish projected content policy from
  local geometry.
- No experimental AUXILIARY source change remains after restoration.
- The Pi and repository end on the normal CLUSTER baseline with the experiment
  recorded as evidence.

## Execution result

The initial role-only AUXILIARY/UNKNOWN run opened and started channels 12/13
but emitted only the codec header and never produced a decoded frame. During
execution, the Maps 26.30.05 trace identified AV field 8 as an initial AUXILIARY
content selector. The bounded artifact was therefore extended to advertise the
already-defined `KEYCODE_TURN_CARD` value 65544. With that selector, no-route
and no-route-plus-media states remained frame-idle; an active Maps route
immediately produced an 800×480 compact maneuver card. Toggling the experimental
session bit 16 caused no content change, matching open-android-auto issue #10's
corrected 17.3 trace.

`KEYCODE_NAVIGATION` value 65538 was not tested because the hands-off protocol
enum omits it and the available consumer trace stops at AA 16.4. Current AA 17.3
confirmation and the upstream enum/provenance update are tracked in
open-android-auto issue #14. The experimental source and Pi binary were restored
to CLUSTER, and the restored active-route dashboard rendered Google Maps.
