# Android Auto AUXILIARY Role-Swap Experiment Plan

Status: ACTIVE

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

- [ ] Change the paired-topology test to expect
  `oaa::proto::enums::DisplayType::AUXILIARY` for descriptor channel 12 while
  retaining display ID 1 and input channel 13.
- [ ] Run `test_service_discovery_builder` and confirm it fails because the
  implementation still emits `CLUSTER`.
- [ ] Change `buildClusterVideoDescriptor()` to serialize
  `DisplayType::AUXILIARY` and change the session diagnostic role label to
  `AUXILIARY`. Do not rename the local session, APIs, QML context, or widget.
- [ ] Re-run `test_service_discovery_builder`, build the explicit
  `openauto-prodigy` target, and run `QT_QPA_PLATFORM=offscreen ctest
  --output-on-failure`.
- [ ] Run one independent Opus review of the bounded experimental diff. Fable
  is not used because its invocation previously failed to produce an observable
  result; the existing tooling follow-up remains open.
- [ ] Run `./cross-build.sh`, deploy the aarch64 binary, restart Prodigy once,
  and confirm the service returns `READY=1` and the descriptor log says
  `role=AUXILIARY display=1 video_ch=12 input_ch=13`.

## Task 2 — Capture the result and restore CLUSTER

- [ ] With baseline 480p/140-DPI/300x300 geometry, capture AUXILIARY with no
  active navigation and no media.
- [ ] Capture AUXILIARY with no navigation and YouTube Music actively playing;
  verify AA media channel 4 starts before the screenshot.
- [ ] Capture AUXILIARY with an active Maps route and turn-data bit 16 off.
- [ ] Capture the same active route with turn-data bit 16 on.
- [ ] Retain screenshots, manifests, negotiated descriptor/frame logs, and the
  user's visual assessment in the Windows capture directory.
- [ ] Reverse the temporary source/test/log-label patch, rebuild and redeploy
  the normal CLUSTER artifact, and confirm the Pi returns to the compiled
  480p/140-DPI/300x300 CLUSTER baseline.
- [ ] Record the confirmed result in live docs, mark this plan completed, move
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
