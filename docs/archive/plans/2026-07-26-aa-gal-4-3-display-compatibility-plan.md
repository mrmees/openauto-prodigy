# Android Auto GAL 4.3 Display Compatibility Implementation Plan

Status: COMPLETED 2026-07-26

Date: 2026-07-26

Design:
[2026-07-26-aa-gal-4-3-display-compatibility-design.md](2026-07-26-aa-gal-4-3-display-compatibility-design.md)

Grounded on Prodigy `27a81a3`, local proto pin `c8340eb`, and audited upstream
proto commit `cabe46ec9c5e1628264427aa77d910b1f574bb34`.

## Goal

Implement and live-validate a default-off GAL 4.3 compatibility mode for the
existing projected-display lab while retaining GAL 1.7 as the exact default.
The implementation stops at the first display-relevant boundary: complete
version diagnostics plus version-gated MAIN clock and lab-only CLUSTER native
turn-card declarations.

## Global Constraints

- Read the root, `src/`, `src/core/aa/`, and
  `libs/prodigy-oaa-protocol/` `AGENTS.md` files before implementation.
- Never edit tracked content inside `libs/prodigy-oaa-protocol/proto/`; update
  only the gitlink to the audited upstream commit.
- Never edit frozen `proto/api/` for this work.
- GAL 1.7 remains the default and GAL 4.3 remains experimental and
  process-lifetime only.
- Do not advertise 5.x/6.x or alter media ACK behavior.
- Do not add AUXILIARY, a third display, a display registry, overlay behavior,
  blended UI, or native semantic turn-card rendering.
- Preserve MAIN focus/touch ownership, CLUSTER channels 12/13, CLUSTER omission
  of AV field 8, decoder/sink/crop ownership, and wireless-only transport.
- Implement each task test-first where behavior is testable. Run focused tests
  while iterating and the full repository gate only on the final tree.
- Keep the request-only 4.3 checkpoint as an intermediate local artifact; do
  not push or tag it.
- After two failed remediation attempts in one area, stop and re-evaluate the
  premise rather than widening the change.

## Task 0 — Capture and lock the actual GAL 1.7 baseline

**Files:**

- Read: deployed Pi binary/service and live AA logs/capture
- Update after capture: `docs/validation-current.md` only if the observation is
  incomplete or contradicts the design
- Update after confirmed capture: this design/plan and current AA docs in Task
  5, not archived history

**Test command:** live Pi/Pixel session plus raw control-frame capture; no code
test applies before the baseline is known.

1. Record the current local SHA, Pi binary SHA-256, service MainPID/executable,
   and recoverable binary/config copies using the existing guarded deployment
   procedure.
2. With the current deployed tree, capture raw control channel message `0x0001`
   and its complete `0x0002` response before TLS.
3. Confirm whether the request bytes are `00 01 00 07`. Record all response
   bytes, not only the six-byte fixed prefix.
4. Confirm the existing MAIN+CLUSTER session still opens channels 3/1 and
   12/13, renders both displays, and keeps media ACK/audio behavior healthy.
5. If the deployed request is not 1.7, stop. Update the active design to the
   observed baseline and get the changed compatibility premise approved before
   Task 1.

**Acceptance:** the deployed request version and full response are captured,
the current MAIN+CLUSTER baseline is healthy, and rollback artifacts are
identified.

## Task 1 — Update the community schema pin and reconcile wire consumers

**Files:**

- Modify gitlink: `libs/prodigy-oaa-protocol/proto`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Channel/MessageIds.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/VideoChannelHandler.cpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/AudioChannelHandler.cpp`
- Modify as compilation requires:
  `libs/prodigy-oaa-protocol/src/HU/Handlers/AVInputChannelHandler.cpp`
- Create: `libs/prodigy-oaa-protocol/tests/test_protocol_constants.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/CMakeLists.txt`

**Focused tests:**

```bash
cmake --build ~/builds/openauto-prodigy --target \
  test_protocol_constants test_service_discovery_builder \
  test_video_channel_handler test_audio_channel_handler \
  test_avinput_channel_handler -j$(nproc)
ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
  -R 'test_protocol_constants|test_service_discovery_builder|test_video_channel_handler|test_audio_channel_handler|test_avinput_channel_handler'
```

1. Add failing/characterization assertions before advancing the pin:
   - stable wire values for AV field 1 audio/video meanings;
   - logical display field 6 and input display field 5 round trips;
   - `KEYCODE_NAVIGATION == 65538` and `KEYCODE_TURN_CARD == 65544` after the
     pin;
   - audited AV message IDs, with separate names where direction differs;
   - the established MAIN-only golden descriptors and CLUSTER descriptors.
2. Advance only the submodule gitlink to
   `cabe46ec9c5e1628264427aa77d910b1f574bb34`. Verify
   `git -C libs/prodigy-oaa-protocol/proto status --short` is empty.
3. Replace generated AV field-6 `channel_id` accessors with `display_id` in
   application code and tests. Do not change the values 0 and 1.
4. Set AV field 1 with the corrected `MediaCodecType` enum. Preserve the
   existing wire values: PCM 1 for audio and H.264 BP 3 for video. Keep the
   per-`VideoConfig` codec list authoritative for H.264/H.265 selection.
5. Reconcile `MessageIds.hpp` with the audited 17.3 map before changing handler
   switches. Keep proven setup/start/stop/focus behavior and make unimplemented
   modern messages explicit no-ops/unknowns; do not invent overlay or UI-config
   handling. Add handler tests that feed the corrected raw IDs so shifted stale
   constants cannot pass by self-reference.
6. Re-run the descriptor golden tests and inspect byte diffs. Any legacy byte
   change must be explained by an audited field-name/type correction with an
   unchanged numeric value, or the task stops.

**Acceptance:** the audited pin generates and compiles cleanly, the submodule
worktree is untouched, manual IDs match audited numerics, and established
descriptors remain wire-compatible.

## Task 2 — Add typed GAL selection and complete version-response diagnostics

**Files:**

- Modify: `src/core/aa/ProjectedDisplayConfig.hpp`
- Modify: `src/core/aa/ProjectedDisplayConfig.cpp`
- Modify: `src/core/aa/ProjectedDisplaySession.hpp`
- Modify: `src/core/aa/ProjectedDisplaySession.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.hpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/SessionConfig.hpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Channel/ControlChannel.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Channel/ControlChannel.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/AASession.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Session/AASession.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_control_channel.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_config.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_fsm.cpp`
- Modify: `tests/test_projected_display_config.cpp`
- Modify: `tests/test_projected_display_session.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `tests/test_aa_orchestrator.cpp`

**Focused tests:**

```bash
cmake --build ~/builds/openauto-prodigy --target \
  test_control_channel test_session_config test_session_fsm \
  test_projected_display_config test_projected_display_session \
  test_service_discovery_builder test_aa_orchestrator -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure \
  -R 'test_control_channel|test_session_config|test_session_fsm|test_projected_display_config|test_projected_display_session|test_service_discovery_builder|test_aa_orchestrator'
```

1. Add failing tests for a typed `(major, minor)` GAL value with ordered numeric
   comparison and formatting. Only 1.7 and 4.3 may enter the runtime profile.
2. Extend `ProjectedClusterProfile` with `galVersion`, defaulting to 1.7.
   Extend atomic profile application with `gal_version`. Reject arbitrary
   versions and reject native-turn-card true with 1.7. Update equality, reset,
   generation, and no-op tests.
3. Set `SessionConfig.protocolMajor/minor` from the staged profile in
   `ServiceDiscoveryBuilder`. Add an explicit minimum-compatible-response
   policy that is false for 1.7 and true for 4.3.
4. Change `ControlChannel::versionReceived` to retain raw status and every
   byte after the six-byte prefix. Tests cover exactly six bytes, valid trailing
   protobuf bytes, opaque trailing bytes, and each truncated prefix length.
5. In `AASession`, record/log requested and phone-reported versions, raw status,
   trailing length, and parsed `ControlChannelConfigWrapper` summary when valid.
   Keep malformed optional data bounded and non-fatal.
6. Preserve current status-based 1.7 acceptance. For requested 4.3, require
   status MATCH and a phone-reported tuple numerically greater than or equal to
   4.3. Add FSM coverage that reported 4.3 and 6.0 advance to TLS while reported
   4.2 and every non-MATCH status terminate with `VersionMismatch` first. Keep
   requested 4.3, not the separately reported tuple, authoritative for all
   local feature policy.
7. Update orchestrator diagnostics to print the staged requested GAL beside
   the descriptor generation. Do not expose arbitrary mutation on
   `AASession` after construction.
8. Do not serialize `AdditionalVideoConfig` in this task. Preserve legacy
   session clock policy so this artifact isolates only schema compatibility,
   request version, and response handling.

**Acceptance:** automated tests prove 1.7 default behavior, exact 4.3 request
bytes, retained full responses, compatible higher-response acceptance, and
lower/non-MATCH failure, while all service-discovery descriptors remain
unchanged from Task 1 and remain keyed to requested 4.3.

## Hardware checkpoint — Prove request-only GAL 4.3

**Files:** no new source changes; retain capture artifacts outside tracked
source unless a confirmed defect belongs in `docs/validation-current.md`.

**Commands:**

```bash
./cross-build.sh
```

Then deploy through the repository's guarded binary replacement procedure;
record local/staged/installed hashes and service identity.

1. Run case A on the Task 2 binary at GAL 1.7 and confirm parity with Task 0.
2. Through the process-lifetime lab control, stage GAL 4.3 with native turn card
   false and reconnect AA.
3. Capture case B: exact 4.3 request, compatible MATCH response, TLS, service
   discovery, every expected channel open, MAIN+CLUSTER setup/start, first NAL/decoded frame,
   sustained audio/video, focus/touch, and media ACK cadence.
4. If version exchange, media, focus, touch, or audio regresses, restore 1.7 and
   stop the plan. Do not implement Task 3 on an unproven handshake.
5. Preserve the request-only binary SHA and capture identity, but do not push or
   tag the intermediate artifact.

**Acceptance:** both 1.7 and request-only 4.3 sustain the established
MAIN+CLUSTER session, and the logs/capture unambiguously identify the requested
and phone-reported versions.

## Task 3 — Add version-gated per-video UI feature serialization

**Files:**

- Modify: `src/core/aa/ProjectedDisplayConfig.hpp`
- Modify: `src/core/aa/ProjectedDisplayConfig.cpp`
- Modify: `src/core/aa/ProjectedDisplaySession.hpp`
- Modify: `src/core/aa/ProjectedDisplaySession.cpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `tests/test_projected_display_config.cpp`
- Modify: `tests/test_projected_display_session.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `tests/test_aa_orchestrator.cpp`

**Focused tests:**

```bash
cmake --build ~/builds/openauto-prodigy --target \
  test_projected_display_config test_projected_display_session \
  test_service_discovery_builder test_aa_orchestrator -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure \
  -R 'test_projected_display_config|test_projected_display_session|test_service_discovery_builder|test_aa_orchestrator'
```

1. Replace the current `turnDataAvailable`/`turn_data_available` session-bit
   concept with `nativeTurnCardAvailable`/
   `native_turn_card_available`. Remove every `sessionConfiguration |= 16`
   path and add a regression assertion that value 16 is never emitted.
2. Add failing service-discovery matrix tests:
   - 1.7 + navbar on: legacy session bit 1, no additional config on any MAIN or
     CLUSTER video config;
   - 1.7 + navbar off: session mask zero, no additional config;
   - invalid 1.7 + native turn card true: profile rejected before build;
   - 4.3 + navbar on: session mask zero and `UI_ELEMENT_CLOCK` exactly once on
     every MAIN video config, with field-1 per-edge insets mirroring that
     config's unchanged legacy total margins;
   - 4.3 + navbar off: no MAIN clock declaration;
   - 4.3 + native turn card false: CLUSTER field 11 absent unless another
     separately supported CLUSTER feature requires it;
   - 4.3 + native turn card true: CLUSTER field 11 contains exactly
     `UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE`, with field-1 insets mirroring
     its unchanged legacy totals;
   - fields 2-4 and 6-8 remain absent in every case.
3. Add a small builder helper that appends hidden elements to an individual
   `VideoConfig`. It must create `additional_config` lazily, copy the legacy
   totals into explicit per-edge field-1 values (`left=floor(width/2)`, right
   gets the remainder, and likewise for top/bottom), and deduplicate enum
   values. Do not create an empty or inset-only field-11 submessage.
4. Apply the MAIN clock element inside the codec loop so every selectable MAIN
   configuration has identical UI policy. Leave existing margin, DPI,
   resolution, FPS, and codec fields untouched.
5. Apply the native-turn-card element only to the CLUSTER video config and only
   at requested GAL 4.3 with the explicit lab flag true. Do not set AV field 8.
6. Assert the legacy MAIN-only golden bytes and the 1.7 CLUSTER descriptors
   remain unchanged. For 4.3, assert that clearing `additional_config` from the
   serialized message yields the same base `VideoConfig` fields and margins.
   Lock the live MAIN totals `0x58` to top/bottom 29 and left/right 0, and the
   native-true CLUSTER totals 500x180 to left/right 250 and top/bottom 90.
7. Update profile diagnostics to show requested GAL and the honest native
   turn-card declaration state.

**Acceptance:** field 11 is absent on every 1.7 path, correctly scoped on 4.3,
every created field 11 has the margin-preserving field-1 companion insets,
legacy bit 16 is gone, and descriptor geometry/roles remain unchanged.

## Task 4 — Update Debug Settings and action adapters

**Files:**

- Modify: `qml/applications/settings/DebugSettings.qml`
- Modify: `src/main.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`
- Modify as needed for diagnostics:
  `src/core/aa/ProjectedDisplaySession.hpp`

**Focused tests:**

```bash
cmake --build ~/builds/openauto-prodigy --target \
  test_settings_menu_structure test_projected_display_session -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure \
  -R 'test_settings_menu_structure|test_projected_display_session'
```

1. Add a two-value GAL selector (`1.7`, `4.3`) to the existing Projected
   CLUSTER Lab. Do not expose free-form numeric input.
2. Rename the turn toggle and payload to `native_turn_card_available`. Label it
   `Advertise native HU turn card (lab)` and show that it is available only at
   4.3. Switching to 1.7 must also stage false in the same action payload.
3. Keep `aa.cluster.applyProfile` and `aa.cluster.resetProfile` as adapters to
   the controller. Do not add an External API protobuf field or let QML mutate
   `SessionConfig` directly.
4. Extend the diagnostics row with requested GAL, generation, active crop, and
   result text. The UI must not call the orchestrator global directly.
5. Test source structure for the two allowed version labels, new payload key,
   removed old key/bit-16 wording, ActionRegistry routing, and provider-owned
   diagnostics.

**Acceptance:** operators can stage only 1.7 or 4.3, invalid flag/version
combinations cannot be emitted by the UI, and one accepted change causes at
most one AA-only reconnect.

## Task 5 — Update current guidance and run the pre-bench repository gate

**Files:**

- Modify: `docs/aa-protocol/aa-display-rendering.md`
- Modify: `docs/aa-protocol/aa-phone-side-debug.md`
- Modify: `docs/reference/settings-tree.md`
- Modify: `docs/reference/plugin-api.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Modify if observations remain unconfirmed: `docs/validation-current.md`

1. Correct current guidance that still says Prodigy requests GAL 1.1. Preserve
   historical archived claims as history; do not rewrite archive files.
2. Document the exact 1.7/4.3 policy matrix, complete response diagnostics,
   field-11 behavior, removed bit-16 no-op, reconnect semantics, and honest
   meaning of the native-turn-card lab flag.
3. Record Task 0 and Task 2 hardware checkpoint evidence without claiming the
   final hidden-UI matrix has passed yet.
4. Run the repository and ARM gates on the candidate bench tree:

```bash
cmake --build ~/builds/openauto-prodigy -j$(nproc)
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure
./cross-build.sh
python3 scripts/check-doc-links.py --scope tracked-live
git diff --check
git -C libs/prodigy-oaa-protocol/proto status --short
```

**Acceptance:** current docs agree with implemented behavior, all repository
and ARM gates pass, and the proto submodule is clean at the pinned commit. The
candidate is ready for the required hardware verification; completion and
review have not yet been claimed.

## Task 6 — Run the final Pi/Pixel display matrix and select the accepted state

**Files:**

- Update: `docs/session-handoffs.md`
- Update or clear applicable entries: `docs/validation-current.md`

1. Preserve and hash the currently deployed known-good binary and configuration
   before replacing either one. Cross-check the stopped/started service's
   MainPID and exact executable path.
2. Deploy the reviewed cross-build and verify local, staged, and installed
   SHA-256 values match.
3. Run final cases A, C, D, and E from the design. Case B remains tied to the
   recorded request-only Task 2 artifact.
4. For every case, collect the version exchange, service discovery, channel and
   media lifecycle, focus/input/audio evidence, phone logcat, screenshots,
   exact CLUSTER carrier/crop geometry, and Pi resource observations specified
   by the design.
5. Specifically compare MAIN navbar margins/touch at 1.7 and 4.3, and compare
   CLUSTER route-active/route-inactive output with native-turn-card false and
   true.
6. Return `native_turn_card_available` to false after case E. Choose the
   accepted deployed version only from proven states; if 4.3 has any material
   regression, restore the preserved 1.7 binary/configuration.
7. Record the exact SHAs/hashes, commands, observations, accepted state, and
   rollback location for final closeout. Do not promote AUXILIARY or GAL 5.x
   follow-ups into this plan.

**Acceptance:** the final matrix distinguishes handshake effects from modern
descriptor effects, 1.7 rollback is proven, no unsupported native-turn-card
claim remains enabled, and the accepted Pi state is explicitly recorded.

**Executed evidence (2026-07-26):** Task 0 at repository HEAD `ba63f9fa` proved
the 1.7 baseline on deployed executable SHA-256 `6a8a6573…`. The request-only
`9501bbef` artifact (`ALPHA-26-07-24-01-91-g9501bbe`, SHA-256 `df2b6b47…`)
proved 4.3 → 6.0/MATCH before modern metadata. Candidate `46c5be9`
(`ALPHA-26-07-24-01-95-g46c5be9`, SHA-256 `9b6ed4e3…`) exposed a hidden-only
field-11 MAIN regression with bottom chrome at y=489..599. Remediation
`d06fa40a2d9141f4a62155ce75e3bb3d2d2550f3` added only the required field-1
companion insets and passed the full A/C/D/E rerun. MAIN 4.3/native false and
true restored the 1.7 Navbar boundary at y=541..599 with side phone chrome.
CLUSTER native false, true, and restored false had identical 364x364 dashboard
crop geometry; the false banner was present, the true enum-5 banner was absent,
and the restored-false banner returned with field 11 absent. The Pi was left
healthy on `ALPHA-26-07-24-01-97-gd06fa40`, SHA-256
`4b73d69a40f7e5be4701f1775af53a11fa1d3a7863cb0ddb3d379afad0fded48`,
GAL 4.3/native false, unchanged config SHA-256
`afd7f1a8cdb1bd2e067563bf16361965a59b841ad767edd05fea9b5f024985a7`.
Rollback is
`/var/backups/openauto-prodigy/20260726T185152Z-pre-companion-inset`.
Capture roots are `gal-4-3-captures-2026-07-26`,
`gal-4-3-remediation-captures-2026-07-26`,
`gal-final-matrix-captures-2026-07-26`, and
`gal-companion-inset-remediation-captures-2026-07-26` beside the repository.
ADB/logcat was unavailable for the final run; this is an evidence-source
limitation, not an unresolved defect.

## Task 7 — Final record, verification, bounded review, and archival

**Files:**

- Modify: `docs/session-handoffs.md`
- Modify or clear applicable entries: `docs/validation-current.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Complete and move this design and plan to `docs/archive/plans/` in the final
  Task 7C commit

1. **Task 7A — final evidence and exact post-bench gate.** Correct every live
   document to the field-1 companion-inset contract, append the final handoff
   with the full hardware chain, clear resolved validation observations, and
   rotate the 2026-07-25 handoffs. Commit that documentation coherently, then
   run the complete Task 5 repository/ARM gate on that exact committed HEAD.
   Keep this design/plan ACTIVE and the roadmap review-pending.
2. **Task 7B — bounded review.** Run one Codex-authored major review through
   the repository review gate using
   Fable. Adjudicate every finding under the two-pass limit and record
   confirmed/dismissed/deferred counts. Do not start another independent
   reviewer.
3. For confirmed review fixes, re-run the affected automated gates and repeat
   any live matrix case whose production behavior or deployed artifact changed.
   A documentation-only review fix does not require another application build
   or deployment.
4. **Task 7C — completion and archival.** After the exact final tree and any
   required live reruns are green, mark the
   design and plan `COMPLETED <date>`, move both to `docs/archive/plans/`, move
   the roadmap item from Now to Done, and update the index. Do not archive while
   a required hardware case or supported-production blocker remains open.

**Completed status:** Task 7A committed the final evidence and passed the exact
post-bench repository/ARM gate. The one bounded Fable major review then found
no blocker or major and three nonblocking minor research items, all confirmed
and deferred to the engineering backlog without source churn. Task 7C recorded
the dispositions and archived both artifacts.

**Acceptance:** fresh evidence covers the exact final tree, the bounded review
has no supported-production blocker, all findings are recorded, and Task 7C
completes and archives the plan artifacts consistently.

## Final Definition of Done

- Raw deployed baseline request/response captured.
- Audited proto gitlink advanced with a clean submodule worktree.
- Generated accessors/types and manual modern AV IDs reconciled with numeric
  regression tests.
- Complete bounded version responses retained and diagnosed.
- Default GAL 1.7 path and descriptors remain compatible.
- Request-only GAL 4.3 hardware checkpoint passes before modern metadata.
- Version-gated MAIN clock and CLUSTER lab metadata serialize exactly as
  designed.
- Legacy session bit 16 is absent.
- Native build, explicit app target, CTest, cross-build, doc checks, and bounded
  major review pass.
- Final Pi/Pixel matrix and accepted/rollback state are recorded.
- Active plan/design are completed and archived only after all required work,
  including hardware validation, is actually finished.
