# Android Auto GAL 5.0–6.0 Production Compatibility Implementation Plan

Status: COMPLETED 2026-07-27

Date: 2026-07-26

Design:
[2026-07-26-aa-gal-5-0-to-6-0-production-compatibility-design.md](2026-07-26-aa-gal-5-0-to-6-0-production-compatibility-design.md)

Grounded on Prodigy `deeecca8e66455f25cc384d1fb5e99cfa38be4d6`,
hardware-accepted GAL 4.3 code `d06fa40a2d9141f4a62155ce75e3bb3d2d2550f3`,
open-android-auto `v1.5` main
`61eab61c5f9968154ff1a80faa8c0a427b208479`, and consumer `dist`
`5ff4aa218dd33913237993f2968bf70e16dc464e`.

## Goal

Promote GAL selection from the CLUSTER laboratory into session-wide production
configuration, implement every evidenced HU obligation through GAL 6.0, and
make the highest hardware-accepted version the shipped default while
preserving exact lower-version choices.

## Execution contract

- Read root, `src/`, `src/core/aa/`, and
  `libs/prodigy-oaa-protocol/` `AGENTS.md` before implementation.
- Execute tasks in order. A later GAL phase may not start until the previous
  phase has a recorded hardware-accepted SHA.
- Use the requested bounded subagent execution method: one sequential
  implementation owner per code task, with exact file ownership and test
  command. The primary session verifies every diff and integrates it. Do not
  create per-task reviewer agents; run the repository's single Fable gate on
  the final major range.
- Never edit tracked content inside
  `libs/prodigy-oaa-protocol/proto/`; Task 0 changes only its gitlink.
- Never edit frozen `proto/api/`.
- Requested GAL is the sole local policy input. Phone-reported GAL is
  admission/diagnostic data only.
- Supported final requests are exactly 1.7, 4.3, 5.0, 5.1, and 6.0. Do not
  add 6.1 or arbitrary version entry.
- The default advances only in a candidate that passes its phase's hardware
  gate. An unaccepted candidate is not pushed, tagged, or described as
  production-ready.
- Retain video ACKs through 6.0. Only audio channels become ackless at 5.0+.
- Do not assign application meaning to unresolved MediaOptions fields, change
  keepalive from their nested messages, send `AVChannelMediaStats`, add EV UI,
  add overlays, or change AUXILIARY/third-display behavior.
- Implement deterministic behavior test-first. Run focused tests while
  iterating and the complete repository/ARM gate once on the final tree.
- After two failed remediation attempts in one phase, restore the previous
  hardware-accepted checkpoint and re-evaluate the evidence.

## Target file structure and interfaces

### New protocol policy

Create
`libs/prodigy-oaa-protocol/include/oaa/Session/SessionProtocolPolicy.hpp` as a
header-only value type and list it in
`libs/prodigy-oaa-protocol/CMakeLists.txt`.

Its public contract is:

```cpp
namespace oaa {

struct ProtocolVersion {
    uint16_t major = 1;
    uint16_t minor = 7;

    constexpr bool operator==(const ProtocolVersion&) const;
    constexpr bool operator!=(const ProtocolVersion&) const;
    constexpr bool operator<(const ProtocolVersion&) const;
    constexpr bool operator>=(const ProtocolVersion&) const;
};

inline constexpr ProtocolVersion kGalVersion1_7{1, 7};
inline constexpr ProtocolVersion kGalVersion4_3{4, 3};
inline constexpr ProtocolVersion kGalVersion5_0{5, 0};
inline constexpr ProtocolVersion kGalVersion5_1{5, 1};
inline constexpr ProtocolVersion kGalVersion6_0{6, 0};

class SessionProtocolPolicy {
public:
    explicit constexpr SessionProtocolPolicy(
        ProtocolVersion requested = kGalVersion1_7);
    constexpr ProtocolVersion requestedVersion() const;
    constexpr bool atLeast(ProtocolVersion threshold) const;
    constexpr bool requiresMinimumCompatibleResponse() const;
    constexpr bool usesModernDisplayPolicy() const;
    constexpr bool usesAcklessAudio() const;
    constexpr bool requiresSingleVideoCodecPerDisplay() const;
    constexpr bool acceptsAudioMediaOptions() const;
    constexpr bool acceptsVehicleEnergyForecast() const;
    constexpr bool acceptsVideoMediaOptions() const;
};

} // namespace oaa
```

Thresholds are fixed at 4.3, 5.0, 5.0, 5.1, 5.1, and 6.0 respectively.
`requiresMinimumCompatibleResponse()` is false only for the legacy 1.7 policy.

`oaa::SessionConfig` retains `protocolMajor` and `protocolMinor`, removes the
separately mutable `requireMinimumCompatibleProtocolVersion`, and provides:

```cpp
SessionProtocolPolicy protocolPolicy() const
{
    return SessionProtocolPolicy{{protocolMajor, protocolMinor}};
}
```

`oaa::IChannelHandler` provides a default no-op:

```cpp
virtual void configureSession(const SessionProtocolPolicy& policy);
```

`AASession::registerChannel()` invokes it before storing or opening the
handler. Audio, video, and navigation override it and store a policy value.

### New application selection policy

Create `src/core/aa/GalVersionPolicy.hpp` and `.cpp`, list them in
`src/CMakeLists.txt`, and provide:

```cpp
QString galVersionToString(oaa::ProtocolVersion version);
bool parseSupportedGalVersion(const QString& text,
                              oaa::ProtocolVersion* result);
oaa::ProtocolVersion resolveConfiguredGalVersion(
    const oap::YamlConfig& config);
QStringList supportedGalVersionStrings();
```

`kHighestAcceptedGalVersion` begins at 4.3 in Task 1, advances to 5.0 in Task
2, to 5.1 in Task 4, and to 6.0 in Task 6. The supported string list grows at
the same checkpoints; it never contains a future or unimplemented tuple.

### Handler diagnostics

Extend the existing handlers without changing their established stream
signals:

```cpp
// AudioChannelHandler
void streamStartDetailsReceived(int32_t session, uint32_t config,
                                int sessionType, bool hasMediaConfig,
                                const QString& boundedSummary);
void mediaOptionsReceived(const QString& boundedSummary);

// VideoChannelHandler
void streamStartDetailsReceived(int32_t session, uint32_t config,
                                int sessionType, bool hasMediaConfig,
                                const QString& boundedSummary);
void mediaOptionsReceived(const QString& boundedSummary);

// NavigationChannelHandler
void vehicleEnergyForecastReceived(bool innerParsed,
                                   const QString& boundedSummary);
```

The summaries are derived from typed parsed messages and capped at 512
characters. They are reusable protocol diagnostics; the Prodigy UI does not
connect them in this plan. An absent optional `session_type` is reported as
`-1`; an absent `media_config` reports `hasMediaConfig=false` and an empty
summary.

---

## Task 0 — Restore the released open-android-auto consumer boundary

**Files:**

- Modify gitlink only: `libs/prodigy-oaa-protocol/proto`

**Out of scope:** any edit inside the submodule, generated-source commit, or
Prodigy behavior change.

**Test command:**

```bash
git -C libs/prodigy-oaa-protocol/proto fetch origin \
  refs/heads/dist:refs/remotes/origin/dist
git -C libs/prodigy-oaa-protocol/proto checkout --detach \
  5ff4aa218dd33913237993f2968bf70e16dc464e
git -C libs/prodigy-oaa-protocol/proto status --short
cmake --build ~/builds/openauto-prodigy --target \
  prodigy-oaa-protocol test_protocol_constants \
  test_service_discovery_builder openauto-prodigy -j$(nproc)
ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
  -R 'test_protocol_constants|test_service_discovery_builder'
./cross-build.sh
```

1. Record the root and submodule starting SHAs and confirm the submodule
   worktree is clean.
2. Fetch the explicit `dist` ref. Do not rely on the shallow submodule's stale
   default fetchspec.
3. Detach at exact commit `5ff4aa2` and verify it is contained by
   `origin/dist`.
4. Verify its tree contains 248 `oaa/**/*.proto` files plus README and LICENSE,
   with no other files.
5. Run the focused native/app and ARM commands above.
6. Commit only the gitlink:

```bash
git add libs/prodigy-oaa-protocol/proto
git commit -m "chore(protocol): pin open-android-auto v1.5 dist"
```

**Acceptance:** root records gitlink `5ff4aa2`, the submodule has no local
changes, generated bindings compile, focused tests pass, and the ARM app
builds.

## Task 1 — Establish production GAL policy and promote accepted 4.3

**Files:**

- Create:
  `libs/prodigy-oaa-protocol/include/oaa/Session/SessionProtocolPolicy.hpp`
- Create: `libs/prodigy-oaa-protocol/tests/test_session_protocol_policy.cpp`
- Modify: `libs/prodigy-oaa-protocol/CMakeLists.txt`
- Modify: `libs/prodigy-oaa-protocol/tests/CMakeLists.txt`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/SessionConfig.hpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Channel/IChannelHandler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Channel/IChannelHandler.cpp`
- Modify: `libs/prodigy-oaa-protocol/src/Session/AASession.cpp`
- Create: `src/core/aa/GalVersionPolicy.hpp`
- Create: `src/core/aa/GalVersionPolicy.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `src/core/aa/ProjectedDisplayConfig.hpp`
- Modify: `src/core/aa/ProjectedDisplayConfig.cpp`
- Modify: `src/core/aa/ProjectedDisplaySession.hpp`
- Modify: `src/core/aa/ProjectedDisplaySession.cpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.hpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `qml/applications/settings/AASettings.qml`
- Modify: `qml/applications/settings/DebugSettings.qml`
- Modify: `tests/test_yaml_config.cpp`
- Modify: `tests/test_config_service.cpp`
- Modify: `tests/test_config_key_coverage.cpp`
- Modify: `tests/test_projected_display_config.cpp`
- Create: `tests/test_gal_version_policy.cpp`
- Modify: `tests/test_projected_display_session.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `tests/test_aa_orchestrator.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_config.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_fsm.cpp`

**Out of scope:** 5.x behavior, ACK changes, codec cardinality changes, or
CLUSTER geometry/native-turn redesign.

**Focused test command:**

```bash
cmake --build ~/builds/openauto-prodigy --target \
  test_session_protocol_policy test_session_config test_session_fsm \
  test_yaml_config test_config_service test_config_key_coverage \
  test_projected_display_config test_gal_version_policy \
  test_projected_display_session \
  test_service_discovery_builder test_aa_orchestrator \
  test_settings_menu_structure openauto-prodigy -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure \
  -R 'test_session_protocol_policy|test_session_config|test_session_fsm|test_yaml_config|test_config_service|test_config_key_coverage|test_projected_display_config|test_gal_version_policy|test_projected_display_session|test_service_discovery_builder|test_aa_orchestrator|test_settings_menu_structure'
```

1. Write policy tests covering numeric pair order and every threshold for all
   five allowed versions. Verify a reported version is not part of the policy
   API.
2. Implement `SessionProtocolPolicy`, derive AASession modern admission from
   it, and add requested 5.0/5.1/6.0 lower/equal/higher FSM table rows without
   enabling application selection yet.
3. Add the default no-op handler configuration hook and test that registration
   receives the requested policy before channel open, including a reused
   handler on a replacement session.
4. Implement `GalVersionPolicy` with only 1.7 and 4.3 accepted in this task and
   `kHighestAcceptedGalVersion = kGalVersion4_3`. Its dedicated test covers
   valid exact strings, invalid/future strings, missing config, explicit
   downgrade, and numeric formatting.
5. Add `connection.gal_version: "4.3"` to YAML defaults. Invalid/missing values
   resolve to 4.3; explicit 1.7 remains 1.7.
6. Remove `GalVersion`, `ProjectedClusterProfile::galVersion`,
   `requestedGalVersion`, and `gal_version` from the CLUSTER update/reset/action
   path. Update structural tests to prove the old key is rejected atomically.
7. Give `ServiceDiscoveryBuilder` an explicit
   `setProtocolVersion(oaa::ProtocolVersion)` input. Apply modern display
   metadata at `policy.usesModernDisplayPolicy()` rather than exact equality.
8. In the orchestrator, resolve one version immediately before each build,
   pass it to the builder, and log the session snapshot independently of
   CLUSTER state.
9. Add a normal Android Auto Settings picker for 1.7/4.3 backed by
   `connection.gal_version`. Remove the GAL picker and diagnostics from Debug
   Settings. Add the new config path to `AndroidAutoPlugin::onConfigChanged`
   so an active/backgrounded session renegotiates.
10. Verify 1.7 descriptor bytes remain golden-compatible, 4.3 retains accepted
    clock/inset/native-turn behavior, and CLUSTER-disabled sessions can request
    4.3.
11. Run the focused command and commit:

```bash
git add libs/prodigy-oaa-protocol src qml/applications/settings \
  tests
git commit -m "feat(aa): promote GAL policy to production settings"
```

**Acceptance:** GAL is session-wide, durable, independent of CLUSTER, defaults
to accepted 4.3, preserves explicit 1.7 behavior, and supplies one requested
policy to AASession and registered handlers.

## Task 2 — Implement GAL 5.0 audio and codec obligations

**Files:**

- Modify:
  `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/AudioChannelHandler.hpp`
- Modify:
  `libs/prodigy-oaa-protocol/src/HU/Handlers/AudioChannelHandler.cpp`
- Modify: `src/core/aa/GalVersionPolicy.cpp`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.hpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `qml/applications/settings/AASettings.qml`
- Modify: `tests/test_audio_channel_handler.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `tests/test_gal_version_policy.cpp`
- Modify: `tests/test_yaml_config.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_protocol_policy.cpp`

**Out of scope:** audio MediaOptions, energy forecast, video MediaOptions,
video ACK removal, media stats, or choosing H.265 as default.

**Focused test command:**

```bash
cmake --build ~/builds/openauto-prodigy --target \
  test_session_protocol_policy test_audio_channel_handler \
  test_service_discovery_builder test_gal_version_policy test_yaml_config \
  test_settings_menu_structure openauto-prodigy -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure \
  -R 'test_session_protocol_policy|test_audio_channel_handler|test_service_discovery_builder|test_gal_version_policy|test_yaml_config|test_settings_menu_structure'
```

1. Add failing audio tests proving 1.7/4.3 send one ACK per accepted packet and
   5.0+ deliver the packet without any `ACK_INDICATION`.
2. Add start-indication rows with fields 1/2 only and with fields 1–4. Verify
   `streamStarted` remains compatible and `streamStartDetailsReceived` reports
   bounded typed presence for the extended form.
3. Store the injected policy in `AudioChannelHandler`; suppress only audio
   `sendAck()` at `usesAcklessAudio()`. Leave setup `max_unacked=10` and
   closed/not-streaming behavior unchanged.
4. Add builder tests proving every display has exactly one codec type at 5.0+
   while 1.7/4.3 retain their configured codec list. MAIN and CLUSTER use the
   same first recognized configured codec. The shipped order is H.265 then
   H.264; explicit H.264-first configuration remains order-authoritative.
5. Add 5.0 to the production picker/allowlist and advance the candidate
   default to 5.0.
6. Run the focused command and commit:

```bash
git add libs/prodigy-oaa-protocol src/core src/CMakeLists.txt \
  qml/applications/settings/AASettings.qml tests
git commit -m "feat(aa): add GAL 5.0 audio compatibility"
```

**Acceptance:** deterministic tests prove ackless audio and single-codec
descriptors at 5.0 without altering video ACKs or lower GAL behavior.

## Task 3 — Hardware-accept GAL 5.0

**Files:**

- Update: `docs/session-handoffs.md`
- Update only if evidence is incomplete: `docs/validation-current.md`

**Test command:** full native/app/CTest gate, `./cross-build.sh`, guarded Pi
deployment, framed protocol capture, and the live matrix below.

1. Record root SHA, Pi executable/version/hash, configuration hash, service
   PID/restart count, and a recoverable binary/config rollback snapshot.
2. Run the full repository and ARM gate, deploy the exact ARM artifact, and
   verify its hash on the Pi before restart.
3. Capture request 5.0 and the complete response. Require MATCH with reported
   tuple at least 5.0.
4. Decode service discovery and prove exactly one codec type on MAIN and the
   enabled CLUSTER display.
5. Run ten sustained minutes covering music, active navigation prompts,
   Assistant speech/microphone, and system sounds. Require continued audio on
   channels 4/5/6 with zero outgoing audio `0x8004` ACK messages; require
   continuing video receive ACKs.
6. Exercise MAIN touch, Back/Home, and `KEYCODE_TEL`; confirm the 4.5+ direct
   dialer path remains usable.
7. Exercise MAIN+CLUSTER video, exit/return focus, and three clean AA-only
   reconnects without an application restart.
8. Select explicit 4.3 and run a connection/media/reconnect regression, then
   restore 5.0.
9. Record the accepted SHA and evidence locations. If any required item fails,
   restore the previous accepted 4.3 artifact and stop before Task 4.

**Acceptance:** GAL 5.0 is healthy on the Pixel with captured ackless audio,
single-codec descriptors, video ACKs, all audio roles, input, displays, and
reconnect. The accepted SHA makes 5.0 the production default.

## Task 4 — Implement GAL 5.1 typed message tolerance

**Files:**

- Modify:
  `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/AudioChannelHandler.hpp`
- Modify:
  `libs/prodigy-oaa-protocol/src/HU/Handlers/AudioChannelHandler.cpp`
- Modify:
  `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp`
- Modify:
  `libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Channel/MessageIds.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/ProtocolLogger.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_protocol_constants.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_protocol_logger.cpp`
- Modify: `src/core/aa/GalVersionPolicy.cpp`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `qml/applications/settings/AASettings.qml`
- Modify: `tests/test_audio_channel_handler.cpp`
- Modify: `tests/test_navigation_channel_handler.cpp`
- Modify: `tests/test_gal_version_policy.cpp`
- Modify: `tests/test_yaml_config.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`

**Out of scope:** semantic MediaOptions application, EV UI, forecast
persistence/calculation, or any HU response to these messages.

**Focused test command:**

```bash
cmake --build ~/builds/openauto-prodigy --target \
  test_audio_channel_handler test_navigation_channel_handler \
  test_protocol_constants test_oaa_protocol_logger \
  test_gal_version_policy test_yaml_config test_settings_menu_structure \
  openauto-prodigy -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure \
  -R 'test_audio_channel_handler|test_navigation_channel_handler|test_protocol_constants|test_oaa_protocol_logger|test_gal_version_policy|test_yaml_config|test_settings_menu_structure'
```

1. Add `NavigationMessageId::VEHICLE_ENERGY_FORECAST = 0x8008` and lock its
   numeric value in protocol-constant coverage. Add the corresponding bounded
   ProtocolLogger name and test it.
2. Add serialized audio MediaOptions tests with all wire types represented.
   Require typed parse, one bounded diagnostic signal, no unknown-message
   signal, and safe rejection of malformed payload.
3. Add serialized outer/inner energy forecast tests, including empty outer,
   valid inner, and malformed inner bytes. Require outer parsing to remain
   independent from optional inner success.
4. Implement the typed handlers and 512-character summaries. Do not use
   MediaOptions `PingConfiguration` fields to change liveness or media policy.
5. Add 5.1 to the production picker/allowlist and advance the candidate
   default to 5.1.
6. Run the focused command and commit:

```bash
git add libs/prodigy-oaa-protocol src/core \
  qml/applications/settings/AASettings.qml tests
git commit -m "feat(aa): add GAL 5.1 message compatibility"
```

**Acceptance:** valid 5.1 messages are typed and observable, malformed forms
are bounded, no unresolved meaning affects runtime behavior, and all lower
version tests remain green.

## Task 5 — Hardware-accept GAL 5.1

**Files:**

- Update: `docs/session-handoffs.md`
- Update only if evidence is incomplete: `docs/validation-current.md`

1. Repeat the guarded candidate build/deploy identity and rollback procedure.
2. Capture request 5.1 and require MATCH with reported tuple at least 5.1.
3. Run music, navigation route start/change/stop, Assistant, system audio,
   MAIN+CLUSTER, input, focus exit/return, and three AA-only reconnects.
4. Capture whether audio `0x8014` or navigation `0x8008` occurs. When present,
   match the live payload to the typed handler diagnostic. When absent, record
   conditional non-delivery without weakening the synthetic/replay tests.
5. Select explicit 5.0 for a connection/media/reconnect regression, then
   restore 5.1.
6. Record the accepted SHA. On a required failure, restore the accepted 5.0
   artifact and stop before Task 6.

**Acceptance:** GAL 5.1 sustains the complete Pi/Pixel workflow and its newly
permitted messages cannot destabilize their channels. The accepted SHA makes
5.1 the production default.

## Task 6 — Implement GAL 6.0 video tolerance

**Files:**

- Modify:
  `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/VideoChannelHandler.hpp`
- Modify:
  `libs/prodigy-oaa-protocol/src/HU/Handlers/VideoChannelHandler.cpp`
- Modify: `src/core/aa/GalVersionPolicy.cpp`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `qml/applications/settings/AASettings.qml`
- Modify: `tests/test_video_channel_handler.cpp`
- Modify: `tests/test_gal_version_policy.cpp`
- Modify: `tests/test_yaml_config.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`

**Out of scope:** removing video ACKs, emitting `AVChannelMediaStats`,
integrated overlay behavior, or interpreting unresolved media timing fields.

**Focused test command:**

```bash
cmake --build ~/builds/openauto-prodigy --target \
  test_video_channel_handler test_service_discovery_builder \
  test_gal_version_policy test_yaml_config test_settings_menu_structure \
  openauto-prodigy -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure \
  -R 'test_video_channel_handler|test_service_discovery_builder|test_gal_version_policy|test_yaml_config|test_settings_menu_structure'
```

1. Add video start tests with optional `session_type` and `media_config`; keep
   the existing `streamStarted(session, config)` contract and emit the new
   bounded details signal.
2. Add standalone video MediaOptions valid/malformed tests. Require typed
   handling with no policy mutation.
3. Keep one video ACK per accepted packet at every supported GAL. Add an
   explicit 6.0 test so later audio-ackless refactors cannot suppress it.
4. Add 6.0 to the production picker/allowlist and advance the candidate
   default to 6.0.
5. Verify 6.0 descriptors still contain one codec type per display and that
   explicitly configured H.264-only and H.265-only selections each serialize
   one matching config.
6. Run the focused command and commit:

```bash
git add libs/prodigy-oaa-protocol src/core \
  qml/applications/settings/AASettings.qml tests
git commit -m "feat(aa): add GAL 6.0 video compatibility"
```

**Acceptance:** GAL 6.0 extended video messages are typed and bounded, video
ACK behavior remains explicit, and both single-codec candidates are testable.

## Task 7 — Hardware-accept GAL 6.0 with the accepted H.265 default

**Files:**

- Update: `docs/session-handoffs.md`
- Update only if evidence is incomplete: `docs/validation-current.md`

H.265 is already the accepted product codec from repeated live Pi/phone
validation. GAL 5.0 accidentally made the legacy H.264-first list decisive
when it introduced one codec per display; GAL 6.0 did not change compressed
video framing, decoding, rendering, or video ACKs. The corrected default is
H.265 then H.264, while explicit order remains authoritative and the H.264 GAL
6.0 fallback is proven on `2bc574e`.

1. Run the full native/app/CTest and ARM gate, create rollback artifacts,
   deploy the exact H.265-default candidate, and record executable,
   configuration, and service identity.
2. Capture request 6.0, require MATCH with a reported tuple at least 6.0, and
   prove MAIN and CLUSTER each advertise exactly one H.265 config/stream type.
3. Run independent operator checkpoints for MAIN+CLUSTER projection and visual
   health, media/speech/system audio, Assistant microphone/response,
   navigation start/change/stop, MAIN input and phone keys, focus exit/return,
   and service/process stability. Do not replace these checkpoints with a
   timer-based functional choreography.
4. Record protocol errors, restarts, video receive/ACK counts, FPS, decoder
   queue, and decode/copy/total timing as regression diagnostics. No
   comparative resource threshold decides the already accepted codec default.
5. Complete three AA-only disconnect/reconnect cycles without restarting the
   application.
6. On another available project test phone, run request/response, MAIN
   video/audio/input, and one reconnect with the H.265 default.
7. Run explicit 1.7, 4.3, 5.0, and 5.1 connection/media smokes, then restore
   6.0.
8. Record the hardware-accepted GAL 6.0 SHA and H.265 result. On any required
   failure, restore accepted 5.1 and stop.

**Acceptance:** GAL 6.0 with the accepted H.265 default passes every
independent Pixel checkpoint and three reconnects, a second phone passes the
production smoke, and every lower supported request remains usable. H.264
remains the proven explicit fallback.

## Task 8 — Reconcile documentation and run the final gate

**Files:**

- Modify: `src/core/aa/AGENTS.md`
- Modify: `docs/aa-protocol/aa-display-rendering.md`
- Modify: `docs/aa-protocol/android-auto-protocol-cross-reference.md`
- Modify: `docs/reference/config-schema.md`
- Modify: `docs/reference/settings-tree.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Complete/archive after review: this design and implementation plan

1. Document the production setting, exact supported list, highest-accepted
   default rule, requested-versus-reported authority, version thresholds,
   single-codec behavior, ackless-audio/video-ACK split, typed 5.1/6.0
   tolerance, and the H.265 regression-acceptance result.
2. Remove live guidance that calls GAL 4.3 a default-off lab or says GAL is
   owned by the CLUSTER profile. Preserve archived history unchanged.
3. Record each accepted SHA, hardware evidence location, and OAA `v1.5`
   main/dist anchors in the handoff.
4. Run the complete final verification:

```bash
cmake --build ~/builds/openauto-prodigy -j$(nproc)
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure
./cross-build.sh
python3 scripts/check-doc-links.py --scope tracked-live
git diff --check
git -C libs/prodigy-oaa-protocol/proto status --short
git -C libs/prodigy-oaa-protocol/proto rev-parse HEAD
```

5. Run the single bounded major review against the immutable planning base:

```bash
bash scripts/review-gate.sh --author codex --major \
  --base "$GAL_PLANNING_BASE" --accepted "$GAL_6_ACCEPTED_SHA"
```

   `GAL_PLANNING_BASE` is the plan/documentation commit recorded before Task 0;
   `GAL_6_ACCEPTED_SHA` is the exact Task 7 hardware-accepted code SHA. Follow
   the repository's one initial/one remediation limit and adjudicate every
   finding against supported production reachability.
6. After the gate has no supported-production blocker, mark both active files
   `COMPLETED <date>`, move them to `docs/archive/plans/`, update links in the
   same commit, and rerun the docs-only link/diff checks.

**Acceptance:** all repository, ARM, hardware, documentation, and review gates
are green; the submodule is clean at `5ff4aa2`; GAL 6.0 is the production
default; selected lower versions remain supported; and no ACTIVE plan remains
for this completed scope.
