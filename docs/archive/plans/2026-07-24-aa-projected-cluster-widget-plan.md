# Android Auto Projected CLUSTER Dashboard Widget Implementation Plan

Date: 2026-07-24
Status: COMPLETED 2026-07-24
Design: `docs/archive/plans/2026-07-24-aa-projected-cluster-widget-design.md`
Implementation base: `9062d66` (the design was first grounded at `3766d4f`,
then amended and re-reviewed at `9062d66`; this plan was first reviewed at
`d5a8282`). All implementation and final review ranges intentionally use
`9062d66..HEAD`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Advertise one experimental Android Auto CLUSTER display beside the
existing MAIN display and render its independent phone-produced video stream in
one fixed 2×2 dashboard widget without changing flag-off MAIN behavior.

**Architecture:** A startup-resolved `ProjectedClusterConfig` controls service
discovery, orchestration, and widget registration. Two fixed
`ProjectedDisplaySession` objects own independent video/input handlers,
decoders, focus, stream generations, sinks, and presentation state; MAIN alone
retains global projection/touch effects. CLUSTER uses display ID 1, video wire
channel 12, input wire channel 13, and one H.264 800×480/30 configuration.

**Tech Stack:** C++17, Qt 6.8 Core/Multimedia/QML, protobuf-generated AA types,
Qt Test, CMake, Docker aarch64 cross-build, Raspberry Pi 4 wireless Android
Auto bench.

## Completion Result

Implemented and live-validated as a positive experimental result on the Pi 4
and Pixel 8. The phone activated both fixed displays, streamed CLUSTER over
channels 12/13, and Prodigy rendered the independent 800×480 Maps surface in
the 2×2 widget while preserving legacy MAIN behavior when disabled. Exit-to-car
and MAIN focus return passed with CLUSTER continuing in the dashboard. An
active route was not available during capture, so route-specific content stays
unvalidated. The feature remains default-off and generalized multi-display is
still outside this completed plan.

## Global Constraints

- `libs/prodigy-oaa-protocol/proto/` is hands-off; `proto/api/` is frozen
  additive-only. This plan changes neither.
- Wireless Android Auto only. No USB/libusb path.
- Qt 6.8 system packages; local builds remain in `~/builds/openauto-prodigy`.
- The experimental feature is absent/false by default and produces the legacy
  MAIN-only serialized descriptors when disabled.
- Enabled topology is fixed: MAIN display ID 0 on video/input channels 3/1;
  CLUSTER display ID 1 on video/input channels 12/13.
- CLUSTER advertises H.264, 800×480, 30 FPS, 140 DPI, zero margins, exactly one
  config, and a matching input descriptor with no touch/key/touchpad/haptic
  capabilities.
- The first widget is fixed at 2×2 and uses ordinary
  `VideoOutput.PreserveAspectFit` upsizing/downsizing only. No crop, stretch,
  rerender, enhancement, or protocol renegotiation.
- MAIN alone controls plugin activation, `Connected`/`Backgrounded`, evdev
  touch ownership, `aa.requestFocus`, and exit-to-car.
- App-layer CLUSTER failures are isolated after valid transport assembly.
  Malformed-fragment and phone/session termination remain explicitly
  session-global experimental outcomes.
- QML ships inside `openauto-prodigy`; QML changes require app build,
  cross-build, and binary deployment.
- No AUXILIARY display, arbitrary registry, CLUSTER input, alternate
  resolution, settings UI, automatic fallback, or production enablement.
- Per-task commits only. Nobody pushes until the final review is adjudicated
  and the user gives the go-ahead.

---

### Task 1: Startup Config and Discovery Topology

**Tier:** `main`

**Definition of Ready:** The exact display IDs, channel IDs, video format,
flag path, focus diagnostic values, flag-off compatibility rule, and no-proto
boundary are fixed by the reviewed design. No open decision remains.

**Acceptance Criteria:**

- Missing/false config resolves to disabled plus
  `PROJECTED_NO_INPUT_FOCUS`; invalid focus text falls back to that mode.
- One immutable resolved value can be passed to the plugin, orchestrator,
  builder, and widget registration without rereading dotted config paths.
- Disabled discovery contains the legacy channel list and leaves MAIN AV field
  6/display type/input display ID/touch display type absent.
- Disabled discovery serializes to the checked-in pre-feature golden bytes for
  the MAIN video/input descriptors, not merely the same parsed field set.
- Enabled discovery contains exactly one explicit MAIN and one CLUSTER plus one
  matching input descriptor each; MAIN input and touch identities are both
  explicit; CLUSTER has exactly the fixed video config and no input
  capabilities.
- No protobuf source changes.

**Out of Scope:** Handler construction, decoders, session registration, QML,
runtime focus behavior, and Pi deployment.

**Files:**

- Create: `src/core/aa/ProjectedDisplayConfig.hpp`
- Create: `src/core/aa/ProjectedDisplayConfig.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Channel/ChannelId.hpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.hpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/test_projected_display_config.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

- Produces:

```cpp
namespace oap::aa {
enum class ProjectedDisplayRole { Main, Cluster };
enum class ProjectedSetupFocus { ProjectedNoInput, Projected };

struct ProjectedClusterConfig {
    bool enabled = false;
    ProjectedSetupFocus setupFocus = ProjectedSetupFocus::ProjectedNoInput;
};

ProjectedClusterConfig resolveProjectedClusterConfig(const oap::YamlConfig& config);

inline constexpr uint8_t kMainDisplayId = 0;
inline constexpr uint8_t kClusterDisplayId = 1;
}
```

- Extends:

```cpp
void ServiceDiscoveryBuilder::setProjectedClusterConfig(
    const ProjectedClusterConfig& config);
uint32_t ServiceDiscoveryBuilder::videoConfigCount(
    ProjectedDisplayRole role) const;
uint32_t ServiceDiscoveryBuilder::videoConfigCount() const; // retained MAIN default
```

- Consumes: `YamlConfig::pluginValue(pluginId, key)` and existing generated
  `ChannelDescriptor`, `AVChannel`, `InputChannelConfig`, `VideoConfig`, and
  `DisplayType` APIs.

- [ ] **Step 1: Add failing config-resolution tests**

Create `tests/test_projected_display_config.cpp` with cases equivalent to:

```cpp
void missingValuesUseSafeDefaults()
{
    oap::YamlConfig yaml;
    const auto config = oap::aa::resolveProjectedClusterConfig(yaml);
    QVERIFY(!config.enabled);
    QCOMPARE(config.setupFocus,
             oap::aa::ProjectedSetupFocus::ProjectedNoInput);
}

void readsDottedPluginIdThroughPluginValue()
{
    oap::YamlConfig yaml;
    yaml.setPluginValue("org.openauto.android-auto",
                        "experimental_cluster_display", true);
    yaml.setPluginValue("org.openauto.android-auto",
                        "experimental_cluster_setup_focus", "projected");
    const auto config = oap::aa::resolveProjectedClusterConfig(yaml);
    QVERIFY(config.enabled);
    QCOMPARE(config.setupFocus, oap::aa::ProjectedSetupFocus::Projected);
}

void invalidFocusFallsBackToNoInput()
{
    oap::YamlConfig yaml;
    yaml.setPluginValue("org.openauto.android-auto",
                        "experimental_cluster_setup_focus", "invalid");
    QCOMPARE(oap::aa::resolveProjectedClusterConfig(yaml).setupFocus,
             oap::aa::ProjectedSetupFocus::ProjectedNoInput);
}
```

- [ ] **Step 2: Register and run the new focused test to prove RED**

Add `core/aa/ProjectedDisplayConfig.cpp` to `openauto-core` and
`test_projected_display_config` to `tests/CMakeLists.txt`, then run:

```bash
cd ~/builds/openauto-prodigy
cmake /mnt/e/claude/personal/openautopro/openauto-prodigy
cmake --build . --target test_projected_display_config -j$(nproc)
ctest --output-on-failure -R '^test_projected_display_config$'
```

Expected: compile/link failure because the resolver and types do not exist.

- [ ] **Step 3: Implement the immutable startup resolver**

Implement only the exact two `pluginValue()` reads. Treat missing, false, and
unrecognized focus values deterministically; do not call `valueByPath()` and do
not persist defaults.

- [ ] **Step 4: Add failing disabled/enabled descriptor tests**

Extend `tests/test_service_discovery_builder.cpp` with descriptor lookup helpers
and assertions equivalent to:

```cpp
void disabledClusterPreservesLegacyMainDescriptorShape()
{
    oap::YamlConfig yaml;
    oap::aa::ServiceDiscoveryBuilder builder(&yaml);
    builder.setProjectedClusterConfig({false, {}});
    const auto config = builder.build();

    QCOMPARE(channelIds(config),
             QList<uint8_t>({3, 4, 5, 6, 1, 2, 8, 14, 7, 9, 10, 11}));
    const auto video = descriptor(config, 3).av_channel();
    QVERIFY(!video.has_channel_id());
    QVERIFY(!video.has_display_type());
    const auto input = descriptor(config, 1).input_channel();
    QVERIFY(!input.has_display_id());
    QVERIFY(!input.touch_screen_configs(0).has_display_type());
}

void enabledClusterAdvertisesPairedFixedTopology()
{
    oap::YamlConfig yaml;
    oap::aa::ServiceDiscoveryBuilder builder(&yaml);
    builder.setProjectedClusterConfig({true, {}});
    const auto config = builder.build();

    const auto mainVideo = descriptor(config, 3).av_channel();
    QCOMPARE(mainVideo.channel_id(), 0u);
    QCOMPARE(mainVideo.display_type(),
             oaa::proto::enums::DisplayType::MAIN);
    QCOMPARE(descriptor(config, 1).input_channel().display_id(), 0u);
    QCOMPARE(descriptor(config, 1).input_channel()
                 .touch_screen_configs(0).display_type(),
             oaa::proto::enums::DisplayType::MAIN);

    const auto clusterVideo = descriptor(config, 12).av_channel();
    QCOMPARE(clusterVideo.channel_id(), 1u);
    QCOMPARE(clusterVideo.display_type(),
             oaa::proto::enums::DisplayType::CLUSTER);
    QCOMPARE(clusterVideo.video_configs_size(), 1);
    QCOMPARE(clusterVideo.video_configs(0).video_resolution(),
             oaa::proto::enums::VideoResolution::VIDEO_800x480);
    QCOMPARE(clusterVideo.video_configs(0).video_fps(),
             oaa::proto::enums::VideoFPS::_30);
    QCOMPARE(clusterVideo.video_configs(0).codec(),
             oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP);
    QCOMPARE(clusterVideo.video_configs(0).dpi(), 140u);
    QCOMPARE(clusterVideo.video_configs(0).margin_width(), 0u);
    QCOMPARE(clusterVideo.video_configs(0).margin_height(), 0u);

    const auto clusterInput = descriptor(config, 13).input_channel();
    QCOMPARE(clusterInput.display_id(), 1u);
    QCOMPARE(clusterInput.touch_screen_configs_size(), 0);
    QCOMPARE(clusterInput.supported_keycodes_size(), 0);
    QCOMPARE(clusterInput.touchpad_configs_size(), 0);
    QCOMPARE(clusterInput.supported_haptic_types_size(), 0);
    QCOMPARE(builder.videoConfigCount(
                 oap::aa::ProjectedDisplayRole::Cluster), 1u);
}
```

Also compare the serialized bytes for descriptors 3 and 1 against golden byte
arrays captured from commit `9062d66`. The golden is a literal checked into the
test, so both the implicit default and explicit disabled config are compared
against pre-feature output rather than against two paths in the new code.

- [ ] **Step 5: Run discovery tests to prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_service_discovery_builder -j$(nproc)
ctest --output-on-failure -R '^test_service_discovery_builder$'
```

Expected: enabled topology assertions fail because channels 12/13 and explicit
display identities do not exist.

- [ ] **Step 6: Implement the two builder paths**

Keep the current MAIN builder functions untouched for the disabled path. In the
enabled path, set MAIN identity explicitly, append fixed CLUSTER video/input
descriptors, and return role-specific config counts. Use generated enum values;
do not replace or rename any proto field. Add `ClusterVideo = 12` and
`ClusterInput = 13` to `ChannelId.hpp` in this task and use those names in the
builder; retain the existing no-argument `videoConfigCount()` as a MAIN-default
delegating overload so the intermediate commit remains source-compatible.

- [ ] **Step 7: Run focused tests and the full local gate**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_projected_display_config test_service_discovery_builder -j$(nproc)
ctest --output-on-failure -R 'test_projected_display_config|test_service_discovery_builder'
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

Expected: all commands pass.

- [ ] **Step 8: Commit Task 1**

```bash
git add src/core/aa/ProjectedDisplayConfig.hpp \
        src/core/aa/ProjectedDisplayConfig.cpp \
        libs/prodigy-oaa-protocol/include/oaa/Channel/ChannelId.hpp \
        src/core/aa/ServiceDiscoveryBuilder.hpp \
        src/core/aa/ServiceDiscoveryBuilder.cpp \
        src/CMakeLists.txt \
        tests/test_projected_display_config.cpp \
        tests/test_service_discovery_builder.cpp \
        tests/CMakeLists.txt
git commit -m "feat: add projected cluster discovery topology"
```

---

### Task 2: Configurable Display Handlers and Capture Classification

**Tier:** `main`

**Definition of Ready:** Channels 12/13 are fixed by the design; handler default
construction must remain source-compatible; setup focus is selected from the
resolved startup enum; protocol capture must suppress channel 12 media when
`include_media=false`.

**Acceptance Criteria:**

- Default video/input handlers still use channels 3/1 and `PROJECTED` setup
  focus.
- Injected handlers send setup, focus, ACK, touch/button, and binding replies
  only on their injected channels.
- CLUSTER setup can emit either `PROJECTED_NO_INPUT_FOCUS` or `PROJECTED`.
- Channels 12/13 have fixed names for this experiment, and channel 12 is
  classified as AV video by message naming and media suppression.
- Handler frame/ACK counters reset on channel open and increase together.
- Parsed setup requests and local parse failures emit bounded semantic signals
  so the application can add role/display context without parsing log text.
- Existing malformed-fragment tests remain green and explicitly document that
  `FrameAssembler::assemblyFailed` is session-global, outside display isolation.

**Out of Scope:** Service-discovery fields, decoder changes, display-session
state, app orchestration, and malformed-fragment recovery.

**Files:**

- Modify: `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/VideoChannelHandler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/VideoChannelHandler.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/InputChannelHandler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/InputChannelHandler.cpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/ProtocolLogger.cpp`
- Modify: `tests/test_video_channel_handler.cpp`
- Modify: `tests/test_input_channel_handler.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_protocol_logger.cpp`

**Interfaces:**

- Produces source-compatible overloads:

```cpp
VideoChannelHandler(QObject* parent = nullptr);
VideoChannelHandler(
    uint8_t channelId,
    oaa::proto::enums::VideoFocusMode::Enum setupFocusMode,
    QObject* parent = nullptr);

InputChannelHandler(QObject* parent = nullptr);
InputChannelHandler(uint8_t channelId, QObject* parent = nullptr);

uint64_t VideoChannelHandler::receivedFrameCount() const;
uint64_t VideoChannelHandler::ackCount() const;

signals:
    void setupRequested(int codec);
    void handlerError(const QString& message);
```

`InputChannelHandler` adds the same `handlerError(const QString&)` signal for
malformed binding messages.

`VideoChannelHandler.hpp` explicitly includes the generated
`oaa/video/VideoFocusModeEnum.pb.h`; its nested proto enum cannot be
forward-declared.

- Consumes the fixed application channel constants added in Task 1:

```cpp
constexpr uint8_t ClusterVideo = 12;
constexpr uint8_t ClusterInput = 13;
```

- Consumes: generated `VideoFocusMode::Enum` and existing handler send signals.

- [ ] **Step 1: Add failing handler-routing and focus tests**

Extend the handler tests with cases equivalent to:

```cpp
void clusterVideoUsesInjectedChannelAndNoInputFocus()
{
    oaa::hu::VideoChannelHandler handler(
        oaa::ChannelId::ClusterVideo,
        oaa::proto::enums::VideoFocusMode::PROJECTED_NO_INPUT_FOCUS);
    handler.setNumVideoConfigs(1);
    handler.onChannelOpened();
    QSignalSpy sent(&handler, &oaa::IChannelHandler::sendRequested);
    handler.onMessage(oaa::AVMessageId::SETUP_REQUEST, setupRequestBytes());
    QCOMPARE(sent[0][0].value<uint8_t>(), oaa::ChannelId::ClusterVideo);
    QCOMPARE(sent[1][0].value<uint8_t>(), oaa::ChannelId::ClusterVideo);
    QCOMPARE(parseFocus(sent[1][2].toByteArray()).focus_mode(),
             oaa::proto::enums::VideoFocusMode::PROJECTED_NO_INPUT_FOCUS);
}

void clusterInputUsesInjectedChannel()
{
    oaa::hu::InputChannelHandler handler(oaa::ChannelId::ClusterInput);
    handler.onChannelOpened();
    QSignalSpy sent(&handler, &oaa::IChannelHandler::sendRequested);
    handler.onMessage(oaa::InputMessageId::BINDING_REQUEST,
                      bindingRequestBytes());
    QCOMPARE(sent[0][0].value<uint8_t>(), oaa::ChannelId::ClusterInput);
}
```

Also send a valid `START_INDICATION` before media, then assert two media frames
yield received/ACK counters of two; reopening the channel resets both to zero.
Feed malformed setup/binding payloads and
assert one bounded `handlerError`; feed a valid setup request and assert
`setupRequested` carries its codec.

- [ ] **Step 2: Add failing protocol-logger tests**

In `test_protocol_logger.cpp`, assert:

```cpp
QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::ClusterVideo),
         std::string("CLUSTER_VIDEO"));
QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::ClusterInput),
         std::string("CLUSTER_INPUT"));
QCOMPARE(oaa::ProtocolLogger::messageName(
             oaa::ChannelId::ClusterVideo, oaa::AVMessageId::SETUP_REQUEST),
         std::string("AV_SETUP_REQUEST"));
```

Write a JSONL capture with `include_media=false`, log one channel 12 media
frame and one setup request, and assert the file contains only the setup line.

- [ ] **Step 3: Run focused tests to prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_video_channel_handler test_input_channel_handler test_oaa_protocol_logger test_frame_assembler -j$(nproc)
ctest --output-on-failure -R 'test_video_channel_handler|test_input_channel_handler|test_oaa_protocol_logger|test_frame_assembler'
```

Expected: compile failures for missing constructors/constants/counters.

- [ ] **Step 4: Implement injected identity and setup focus**

Store `channelId_` and `setupFocusMode_` per video handler and `channelId_` per
input handler. Keep existing constructors as delegating overloads. Replace only
hard-coded `channelId()` and setup-focus uses; do not change request/response
semantics or `requestVideoFocus(bool)`. Emit `setupRequested` only after a
successful parse and `handlerError` only for local parse/setup failures; keep
unknown-message reporting on its existing nonterminal signal.

- [ ] **Step 5: Implement bounded counters and capture classification**

Reset counters in `onChannelOpened()`, increment received before emitting the
frame, and increment acknowledged after emitting its ACK. Add channels 12/13
to `channelName()` and the appropriate AV/input branches in `messageName()`;
include channel 12 in `isAVMediaFrame()`.

- [ ] **Step 6: Run focused and full gates**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_video_channel_handler test_input_channel_handler test_oaa_protocol_logger test_frame_assembler -j$(nproc)
ctest --output-on-failure -R 'test_video_channel_handler|test_input_channel_handler|test_oaa_protocol_logger|test_frame_assembler'
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

- [ ] **Step 7: Commit Task 2**

```bash
git add libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/VideoChannelHandler.hpp \
        libs/prodigy-oaa-protocol/src/HU/Handlers/VideoChannelHandler.cpp \
        libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/InputChannelHandler.hpp \
        libs/prodigy-oaa-protocol/src/HU/Handlers/InputChannelHandler.cpp \
        libs/prodigy-oaa-protocol/src/Messenger/ProtocolLogger.cpp \
        tests/test_video_channel_handler.cpp \
        tests/test_input_channel_handler.cpp \
        libs/prodigy-oaa-protocol/tests/test_protocol_logger.cpp
git commit -m "feat: parameterize projected display channels"
```

---

### Task 3: Decoder End Boundary and Observable Failures

**Tier:** `main`

**Definition of Ready:** The reviewed design requires an explicit end boundary,
queued/latest-frame purge, late-frame suppression by the owning display
generation, and an observable local decoder error. It does not require decoder
reconnect policy or malformed-frame transport recovery.

**Acceptance Criteria:**

- `beginStream()` opens a new ordered generation and accepts frames.
- `endStream()` stops accepting new frames, clears queued work, orders an end
  barrier after any in-flight decode, purges the latest frame/pool, and reports
  completion for its generation.
- Frames submitted while ended do not produce codec detection or frame-ready
  output.
- Constructor/reset/codec-switch initialization failures expose operational
  state and one bounded error signal instead of logging only.
- Existing H.264/H.265 generation-order tests remain green.

**Out of Scope:** Decoder algorithm changes, multi-threaded FFmpeg, new codecs,
FrameAssembler behavior, and widget state.

**Files:**

- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp`
- Modify: `tests/test_video_decoder.cpp`

**Interfaces:**

```cpp
bool VideoDecoder::isOperational() const;
quint64 VideoDecoder::beginStream();
void VideoDecoder::endStream();
void VideoDecoder::setDiagnosticLabel(const QString& label);

signals:
    void frameReadyForGeneration(quint64 generation);
    void streamEnded(quint64 generation);
    void streamError(quint64 generation, const QString& message);
```

The worker gains `WorkKind::EndStream`; `beginStream()` returns the generation
assigned to its ordered begin item, and `beginStream()`/`endStream()` are the
only methods that change frame acceptance.

- [ ] **Step 1: Add failing end-boundary tests**

Extend `tests/test_video_decoder.cpp` with:

```cpp
void endBoundaryPurgesAndRejectsFramesUntilNextBegin()
{
    oap::aa::VideoDecoder decoder;
    QSignalSpy ended(&decoder, &oap::aa::VideoDecoder::streamEnded);
    QSignalSpy codec(&decoder, &oap::aa::VideoDecoder::streamCodecDetected);

    decoder.beginStream();
    decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x67')));
    QTRY_COMPARE(codec.count(), 1);

    decoder.endStream();
    QTRY_COMPARE(ended.count(), 1);
    QVERIFY(!decoder.takeLatestFrame().isValid());
    decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x42')));
    QTest::qWait(30);
    QCOMPARE(codec.count(), 1);

    decoder.beginStream();
    decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x42')));
    QTRY_COMPARE(codec.count(), 2);
}
```

Add a narrow test seam that forces `initCodec()` failure during reset, then
assert `isOperational()==false` and exactly one `streamError` emission for that
generation. Set diagnostic label `CLUSTER[id=1,ch=12]` and capture Qt messages
to assert the label prefixes reset/error/performance diagnostics.

The seam is a private `std::atomic<bool> failCodecInitForTest_{false}` read by
an internal `initializeCodec()` wrapper before falling back to `initCodec()`;
declare `oap::aa::VideoDecoderTestAccess` as a friend so production callers
cannot inject it. The test flips the atomic only after normal constructor
initialization and before enqueueing the reset under test, avoiding a
test-thread/worker-thread data race. The worker emits
`frameReadyForGeneration(generation)` alongside the legacy parameterless
`frameReady()` so display-session consumers can reject stale queued callbacks
without breaking existing callers.

- [ ] **Step 2: Run the decoder test to prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_video_decoder -j$(nproc)
ctest --output-on-failure -R '^test_video_decoder$'
```

- [ ] **Step 3: Implement ordered begin/end work and operational state**

Use the worker mutex to clear pending work and enqueue one generation-tagged
boundary. `endStream()` flips an atomic acceptance flag before taking that
mutex. The worker's end item clears packet/frame/latest/pool state without
initializing a codec. A later begin re-enables acceptance before enqueuing its
reset. Emit `streamError` on the QObject's established queued boundary; do not
call QML or sinks from the decode thread.

- [ ] **Step 4: Run focused and full gates**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_video_decoder -j$(nproc)
ctest --output-on-failure -R '^test_video_decoder$'
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

- [ ] **Step 5: Commit Task 3**

```bash
git add src/core/aa/VideoDecoder.hpp src/core/aa/VideoDecoder.cpp \
        tests/test_video_decoder.cpp
git commit -m "fix: add explicit projected stream end boundary"
```

---

### Task 4: One Projected Display Session Owner

**Tier:** `main`

**Definition of Ready:** Tasks 1–3 provide fixed role/config types,
parameterized handlers, counters, ordered decoder boundaries, and decoder
errors. State names, one-sink behavior, and lifecycle/focus isolation are fixed
by the design.

**Acceptance Criteria:**

- One object owns one role's handler, input handler, decoder, IDs, focus, state,
  counters, and sink.
- Begin/end protocol generations connect once, purge safely, and ignore stale
  queued callbacks.
- CLUSTER state deterministically covers Disabled, Disconnected,
  WaitingForChannel, Rejected, WaitingForFrames, Rendering, and Error.
- Only one video sink may attach; a competing sink is rejected and cannot steal
  the first; detach is pointer-owned.
- Periodic summaries distinguish role/display/wire IDs and include received vs
  ACK counts without per-frame logging.
- Setup, stream, focus, first media frame, first decoded frame/dimensions, sink,
  local error, and teardown diagnostics all carry the same role/display/wire
  prefix.

**Out of Scope:** `AASession` construction/registration, global projection
state, dashboard registration, and Pi behavior.

**Files:**

- Create: `src/core/aa/ProjectedDisplaySession.hpp`
- Create: `src/core/aa/ProjectedDisplaySession.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/test_projected_display_session.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

```cpp
class ProjectedDisplaySession : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool rendering READ isRendering NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
public:
    enum State {
        Disabled = 0,
        Disconnected,
        WaitingForChannel,
        Rejected,
        WaitingForFrames,
        Rendering,
        Error
    };
    Q_ENUM(State)

    ProjectedDisplaySession(ProjectedDisplayRole role,
                            uint8_t displayId,
                            uint8_t videoChannelId,
                            uint8_t inputChannelId,
                            bool enabled,
                            ProjectedSetupFocus setupFocus,
                            oap::YamlConfig* yamlConfig,
                            QObject* parent = nullptr);

    oaa::hu::VideoChannelHandler* videoHandler();
    oaa::hu::InputChannelHandler* inputHandler();
    VideoDecoder* decoder();
    bool isRendering() const;
    void setAdvertisedVideoConfigCount(uint32_t count);
    void beginProtocolSession();
    void endProtocolSession();
    void noteChannelOpened(uint8_t channelId);
    void noteChannelRejected(int32_t channelId);
    Q_INVOKABLE bool attachVideoSink(QVideoSink* sink);
    Q_INVOKABLE void detachVideoSink(QVideoSink* sink);
};
```

- Consumes: `ProjectedDisplayConfig`, configurable handlers, `VideoDecoder`
  begin/end/error, and `QVideoSink`.
- Produces: stable APIs used by `AndroidAutoOrchestrator` and QML.

- [ ] **Step 1: Add failing lifecycle/state tests**

Create tests equivalent to:

```cpp
void clusterStateFollowsOnlyItsLifecycle()
{
    oap::aa::ProjectedDisplaySession display(
        oap::aa::ProjectedDisplayRole::Cluster, 1, 12, 13, true,
        oap::aa::ProjectedSetupFocus::ProjectedNoInput, nullptr);
    QCOMPARE(display.state(), display.Disconnected);
    display.beginProtocolSession();
    QCOMPARE(display.state(), display.WaitingForChannel);
    display.noteChannelOpened(12);
    QCOMPARE(display.state(), display.WaitingForFrames);
    QSignalSpy reset(display.decoder(),
                     &oap::aa::VideoDecoder::streamResetCompleted);
    display.videoHandler()->onMessage(
        oaa::AVMessageId::START_INDICATION, startIndicationBytes());
    QTRY_COMPARE(reset.count(), 1);
    display.decoder()->frameReadyForGeneration(reset[0][0].toULongLong());
    QCoreApplication::processEvents();
    QCOMPARE(display.state(), display.Rendering);
    display.endProtocolSession();
    QCOMPARE(display.state(), display.Disconnected);
}

void rejectionAndDecoderErrorAreExplicit()
{
    auto display = enabledClusterDisplay();
    display.beginProtocolSession();
    display.noteChannelRejected(13);
    QCOMPARE(display.state(), display.Rejected);
    display.beginProtocolSession();
    display.decoder()->streamError(2, "codec init failed");
    QCoreApplication::processEvents();
    QCOMPARE(display.state(), display.Error);
}
```

Also assert a `frameReadyForGeneration()` or `streamError()` callback from an
ended generation cannot move the new generation to Rendering/Error. The
session records the generation returned by each begin boundary and
compares every generation-bearing decoder callback against it.

Add a no-sink flow test: begin/open the CLUSTER display, drive a video start and
media indication, assert received/ACK counters advance with no sink attached,
capture the generation from `streamResetCompleted`, emit
`frameReadyForGeneration(generation)`, and assert the display reaches
Rendering. Then attach a sink and verify the display remains in the same
protocol generation. Sink absence must never stop or renegotiate the handler.

- [ ] **Step 2: Add failing single-sink tests**

Instantiate two `QVideoSink`s. Assert first attach succeeds, second fails,
detaching the second is a no-op, detaching the first clears the decoder sink,
and the second can then attach.

- [ ] **Step 3: Register and run the focused test to prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake /mnt/e/claude/personal/openautopro/openauto-prodigy
cmake --build . --target test_projected_display_session -j$(nproc)
ctest --output-on-failure -R '^test_projected_display_session$'
```

Set `QT_QPA_PLATFORM=offscreen` for this test in `tests/CMakeLists.txt`, matching
the existing orchestrator test, because it constructs `QVideoSink`s.

- [ ] **Step 4: Implement the bounded session owner**

Connect handler/decoder signals once in the constructor. The display session
owns the AV decoder boundary: `beginProtocolSession()` activates/increments the
protocol generation and enters `WaitingForChannel` but does not begin decoder
acceptance; `streamStarted` calls `decoder.beginStream()` and overwrites the
recorded active decoder generation with its return value; `streamStopped` and
`endProtocolSession()` call `decoder.endStream()` and clear the recorded
decoder generation. Preserve the current
`Qt::DirectConnection` from `videoFrameData` to `VideoDecoder::decodeFrame` so
MAIN does not gain an event-loop hop. Maintain a monotonically increasing
protocol generation and an active flag checked by every queued slot. On end,
mark inactive before calling `decoder.endStream()`. In production,
`frameReadyForGeneration()` is emitted only after a valid frame is stored, so
the first such callback in the current active generation transitions to
Rendering; taking/pushing the frame remains separately guarded by
`QVideoFrame::isValid()`. This signal contract makes the synthetic lifecycle
test legal without weakening sink delivery. State changes and sink calls
remain on the QObject owner thread.

Use the video handler's counters for a rate-limited summary on state change and
at most once per five seconds while frames arrive; do not log each frame.

Set the decoder diagnostic label from role/display/video-channel identity.
Connect `setupRequested`, `handlerError`, `streamStarted`, `streamStopped`,
`videoFocusChanged`, `videoFrameData`, `frameReadyForGeneration`, `streamError`, and
`streamEnded` once. Log setup codec, focus mode, first media frame, first valid
decoded frame dimensions, sink claim/release, rate-limited received/ACK totals,
and begin/end boundaries with the shared prefix. A valid decoded frame is still
pushed to the current sink on the GUI thread; logging must not consume the
latest-frame slot without delivering it.

- [ ] **Step 5: Run focused and full gates**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_projected_display_session -j$(nproc)
ctest --output-on-failure -R '^test_projected_display_session$'
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

- [ ] **Step 6: Commit Task 4**

```bash
git add src/core/aa/ProjectedDisplaySession.hpp \
        src/core/aa/ProjectedDisplaySession.cpp src/CMakeLists.txt \
        tests/test_projected_display_session.cpp tests/CMakeLists.txt
git commit -m "feat: add isolated projected display session"
```

---

### Task 5: Orchestrator MAIN Refactor and Optional CLUSTER Registration

**Tier:** `main`

**Definition of Ready:** Tasks 1–4 are green. The exact fixed sessions,
startup config, session begin/end order, global MAIN-only effects, CLUSTER
rejection handling, and flag-off topology are specified with testable APIs.

**Acceptance Criteria:**

- Existing constructor call sites remain source-compatible through a
  delegating MAIN-only overload.
- MAIN's standalone handler/input/decoder fields are replaced by one
  `ProjectedDisplaySession`; public MAIN accessors retain their behavior.
- A config-aware overload constructs the optional CLUSTER session using the
  same immutable startup snapshot passed to the builder.
- Enabled AA sessions register channels 12/13; disabled sessions do not.
- MAIN focus alone changes global connection state; CLUSTER focus, close,
  rejection, decoder error, and sink state cannot do so.
- Teardown ends both enabled display generations before `AASession::finalize()`
  disconnects persistent handlers.
- Protocol capture names/suppresses channel 12 correctly.
- Descriptor construction plus AASession open/reject routing add the same
  role/display/wire context used by `ProjectedDisplaySession` diagnostics.

**Out of Scope:** Widget registration/QML, settings UI, FrameAssembler policy,
audio/AVInput changes, and live Pi claims.

**Files:**

- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `tests/test_aa_orchestrator.cpp`

**Interfaces:**

```cpp
AndroidAutoOrchestrator(IConfigService*, IAudioService*, YamlConfig*,
                        IEventBus* = nullptr, EqualizerService* = nullptr,
                        QObject* parent = nullptr); // legacy MAIN-only delegate

AndroidAutoOrchestrator(IConfigService*, IAudioService*, YamlConfig*,
                        const ProjectedClusterConfig&,
                        IEventBus* = nullptr, EqualizerService* = nullptr,
                        QObject* parent = nullptr);

VideoDecoder* videoDecoder();                 // MAIN compatibility accessor
TouchHandler* touchHandler();                 // MAIN compatibility accessor
InputChannelHandler* inputHandler();          // MAIN compatibility accessor
ProjectedDisplaySession* clusterDisplay();    // stable QML/controller object
```

`AndroidAutoPlugin` receives the same `ProjectedClusterConfig` through a
source-compatible overload and passes it to the orchestrator.

- [ ] **Step 1: Add failing MAIN compatibility tests**

Update `AndroidAutoOrchestratorTestAccess` to access the MAIN display session,
then preserve existing assertions through the public compatibility accessors.
Add a test that the legacy constructor yields disabled CLUSTER and that MAIN
handler IDs remain 3/1. Add a narrow `setConnectedForFocusTest()` helper on
the existing friend access class that calls the private `setState(Connected,
...)`; use it to establish the real precondition for MAIN's NATIVE-focus
transition without pretending the loopback socket completed AA negotiation.

- [ ] **Step 2: Add failing enabled registration/isolation tests**

Use the existing loopback/fake transport seams to build an orchestrator with
`ProjectedClusterConfig{true, ProjectedNoInput}`. Assert service discovery has
channels 12/13, both handlers accept channel-open routing, and CLUSTER reports
one video config.

Drive these cases independently:

```cpp
const int originalState = orch.connectionState();
cluster.videoHandler()->videoFocusChanged(
    oaa::proto::enums::VideoFocusMode::NATIVE, false);
QCOMPARE(orch.connectionState(), originalState);

session->channelOpenRejected(oaa::ChannelId::ClusterVideo);
QCOMPARE(cluster.state(), ProjectedDisplaySession::Rejected);
QCOMPARE(orch.connectionState(), originalState);

main.videoHandler()->videoFocusChanged(
    oaa::proto::enums::VideoFocusMode::NATIVE, false);
QCOMPARE(orch.connectionState(), AndroidAutoOrchestrator::Backgrounded);
```

Call `setConnectedForFocusTest()` immediately before the MAIN-focus assertion;
otherwise `Backgrounded` is not reachable from the test's default state.

Add reconnect/teardown assertions that both display sessions end, late old
frame callbacks do not render, and the next MAIN generation still begins.

- [ ] **Step 3: Run orchestrator tests to prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_aa_orchestrator -j$(nproc)
ctest --output-on-failure -R '^test_aa_orchestrator$'
```

- [ ] **Step 4: Replace standalone MAIN ownership with the session owner**

Initialize fixed MAIN and CLUSTER session members in constructor order. Point
`TouchHandler` only at MAIN input. Move frame wiring into each display object;
remove per-session `disconnect(this)` calls that the new generation boundaries
replace. Preserve audio, AVInput, theme, navigation, phone, sensor, Bluetooth,
WiFi, watchdog, and transport code unchanged.

- [ ] **Step 5: Register descriptors/handlers and scope global effects**

Pass the immutable config to `ServiceDiscoveryBuilder`. Register MAIN always
and CLUSTER only when enabled. Feed role-specific config counts before start.
Connect `AASession::channelOpened` and `channelOpenRejected` to the matching
display by wire ID. Connect only MAIN `videoFocusChanged` to global state.
After handler registration and `setAdvertisedVideoConfigCount()`, call
`beginProtocolSession()` once for each enabled display before
`session_->start()`; do not defer display activation until a channel-open event.

Emit one descriptor summary at service-discovery construction for each enabled
role (display ID/type, video/input wire IDs, config count) and one contextual
open/reject summary when `AASession` reports either display channel.

In teardown, mark display generations inactive and end decoders before
`session_->finalize()`. Keep shared transport/session errors global and log
whether the last activity was on CLUSTER without reclassifying the disconnect.

- [ ] **Step 6: Run focused and full gates**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_aa_orchestrator test_projected_display_session test_service_discovery_builder -j$(nproc)
ctest --output-on-failure -R 'test_aa_orchestrator|test_projected_display_session|test_service_discovery_builder'
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

- [ ] **Step 7: Commit Task 5**

```bash
git add src/core/aa/AndroidAutoOrchestrator.hpp \
        src/core/aa/AndroidAutoOrchestrator.cpp \
        src/plugins/android_auto/AndroidAutoPlugin.hpp \
        src/plugins/android_auto/AndroidAutoPlugin.cpp \
        tests/test_aa_orchestrator.cpp
git commit -m "feat: orchestrate optional projected cluster display"
```

---

### Task 6: Fixed Square Dashboard Widget

**Tier:** `opus`

**Definition of Ready:** The cluster display object exists for QML, exposes
state/status plus owned sink attach/detach, and is enabled by the immutable
startup snapshot. The widget is noninteractive, picker-visible only when
enabled, fixed 2×2, and supports one live sink.

**Acceptance Criteria:**

- Disabled startup registers no CLUSTER widget and exposes no new picker item.
- Enabled startup registers exactly one descriptor definition with min/max/
  default 2×2 and `navigation` category.
- The root QML context receives the stable CLUSTER display object.
- QML attaches its `VideoOutput.videoSink`, detaches by pointer equality, shows
  state-specific placeholder copy, and shows a local already-in-use state when
  sink claim fails.
- `VideoOutput.PreserveAspectFit` is explicit; no ShaderEffect, image provider,
  crop, transform, or touch forwarding exists.
- Normal `WidgetHost` long-press management remains available.

**Out of Scope:** Multiple render sinks, a widget-model max-instance feature,
tap-to-launch behavior, responsive sizes, settings UI, and MAIN QML redesign.

**Files:**

- Create: `src/plugins/android_auto/AAClusterWidgetRegistration.hpp`
- Create: `src/plugins/android_auto/AAClusterWidgetRegistration.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `src/main.cpp`
- Create: `qml/widgets/AAClusterWidget.qml`
- Create: `tests/test_aa_cluster_widget.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

```cpp
namespace oap::plugins {
bool registerAAClusterWidget(oap::WidgetRegistry& registry,
                             const oap::aa::ProjectedClusterConfig& config);
}
```

The enabled descriptor uses ID `org.openauto.aa-cluster`, QML URL
`qrc:/OpenAutoProdigy/AAClusterWidget.qml`, and exact fixed grid dimensions.

- [ ] **Step 1: Add failing registration tests**

Create `tests/test_aa_cluster_widget.cpp`:

```cpp
void disabledConfigRegistersNothing()
{
    oap::WidgetRegistry registry;
    QVERIFY(!oap::plugins::registerAAClusterWidget(registry, {false, {}}));
    QVERIFY(!registry.descriptor("org.openauto.aa-cluster").has_value());
}

void enabledConfigRegistersFixedSquare()
{
    oap::WidgetRegistry registry;
    QVERIFY(oap::plugins::registerAAClusterWidget(registry, {true, {}}));
    const auto d = registry.descriptor("org.openauto.aa-cluster");
    QVERIFY(d.has_value());
    QCOMPARE(d->minCols, 2); QCOMPARE(d->maxCols, 2);
    QCOMPARE(d->minRows, 2); QCOMPARE(d->maxRows, 2);
    QCOMPARE(d->defaultCols, 2); QCOMPARE(d->defaultRows, 2);
    QCOMPARE(d->category, QString("navigation"));
    QCOMPARE(d->qmlComponent,
             QUrl("qrc:/OpenAutoProdigy/AAClusterWidget.qml"));
}
```

- [ ] **Step 2: Add a failing QML contract test**

In the same test target, read `qml/widgets/AAClusterWidget.qml` using a
`TEST_SOURCE_DIR` definition and assert it contains `import QtMultimedia`, one
`VideoOutput`, `PreserveAspectFit`, `AAClusterDisplay.rendering`,
`attachVideoSink(videoOutput.videoSink)`, and
`detachVideoSink(videoOutput.videoSink)`, while containing no `MouseArea`,
`TapHandler`, `ShaderEffect`, `sourceRect`, or instance-enum lookup such as
`AAClusterDisplay.Rendering`.

- [ ] **Step 3: Register test/CMake entries and prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake /mnt/e/claude/personal/openautopro/openauto-prodigy
cmake --build . --target test_aa_cluster_widget -j$(nproc)
ctest --output-on-failure -R '^test_aa_cluster_widget$'
```

Set `QT_QPA_PLATFORM=offscreen` for this target in `tests/CMakeLists.txt`
because its transitive display/QML surface constructs multimedia objects.

- [ ] **Step 4: Implement conditional registration and startup sharing**

Resolve `ProjectedClusterConfig` once in `main.cpp` immediately after YAML
load. Pass the copy to `AndroidAutoPlugin`, call the registration helper after
creating `WidgetRegistry`, and expose
`aaPlugin->orchestrator()->clusterDisplay()` as `AAClusterDisplay` on the root
context. Preserve the existing `aaPlugin && aaPlugin->orchestrator()` guard;
only dereference and set the context property inside that guarded path. Do not
reread YAML in any of those paths.

- [ ] **Step 5: Implement the fixed QML widget**

Use this ownership pattern:

```qml
import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root
    property QtObject widgetContext: null
    property bool ownsSink: false

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: root.ownsSink && AAClusterDisplay.rendering
    }

    Component.onCompleted: {
        ownsSink = AAClusterDisplay.attachVideoSink(videoOutput.videoSink)
    }
    Component.onDestruction: {
        if (ownsSink)
            AAClusterDisplay.detachVideoSink(videoOutput.videoSink)
    }
}
```

Add a centered subdued icon/status layer for disabled/disconnected/waiting/
rejected/error and local duplicate-sink states. Keep `VideoOutput` and visual
items noninteractive so the host's behind-content long press remains the only
pointer behavior.

- [ ] **Step 6: Add QML to the compiled resource and run gates**

Add `set_source_files_properties()` plus the file under `qt_add_qml_module` in
`src/CMakeLists.txt`, then run:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_aa_cluster_widget -j$(nproc)
ctest --output-on-failure -R '^test_aa_cluster_widget$'
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

- [ ] **Step 7: Commit Task 6**

```bash
git add src/plugins/android_auto/AAClusterWidgetRegistration.hpp \
        src/plugins/android_auto/AAClusterWidgetRegistration.cpp \
        src/CMakeLists.txt src/main.cpp qml/widgets/AAClusterWidget.qml \
        tests/test_aa_cluster_widget.cpp tests/CMakeLists.txt
git commit -m "feat: render projected cluster in dashboard widget"
```

---

### Task 7: Review Gate, Pi Experiment, and Completion Record

**Tier:** `main`

**Definition of Ready:** Tasks 1–6 are committed; local build, explicit app
target, and full CTest pass; the original Pi config can be backed up and
restored; `python3` plus PyYAML are available on the Pi; the user has authorized
binary deployment, application restart, temporary flag changes, AA reconnect,
and config restoration.

**Acceptance Criteria:**

- Final local gates pass from the complete implementation range.
- `bash scripts/codex-review.sh 9062d66` completes and every finding is fixed or
  dismissed with a stated technical reason; substantial fixes receive the one
  required rerun.
- Cross-build succeeds after review.
- Flag-off deployment proves MAIN rendering, touch, background/reopen, and
  reconnect before CLUSTER is enabled.
- Flag-on capture classifies the exact outcome: activation/rendering,
  channel rejection, silence, topology rejection, malformed-fragment
  disconnect, or phone-initiated session termination.
- If default no-input focus does not activate CLUSTER, repeat with `projected`
  before concluding nonactivation.
- Original Pi config is restored byte-for-byte; one app process remains;
  hostapd and Bluetooth are not restarted.
- Docs record actual results; on full PASS the design and plan become
  `COMPLETED 2026-07-24` and move to `docs/archive/plans/` in the same commit.
  If phone activation fails, archive as `COMPLETED` experimental research with
  the exact negative result; do not claim a shipped production feature.

**Out of Scope:** Pushing, opening a PR, merging, milestone tagging, package/
release publication, changing hostapd/Bluetooth, or hiding a negative result.

**Files:**

- Modify: `docs/roadmap-current.md`
- Modify: `docs/wishlist.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move on completion: `docs/plans/2026-07-24-aa-projected-cluster-widget-design.md`
  to `docs/archive/plans/2026-07-24-aa-projected-cluster-widget-design.md`
- Move on completion: `docs/plans/2026-07-24-aa-projected-cluster-widget-plan.md`
  to `docs/archive/plans/2026-07-24-aa-projected-cluster-widget-plan.md`

**Interfaces:** No new code interface. Produces the capture, validation record,
review adjudication, and final docs state.

- [ ] **Step 1: Run the complete local verification gate**

```bash
cd ~/builds/openauto-prodigy
cmake /mnt/e/claude/personal/openautopro/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git diff --check 9062d66..HEAD
```

- [ ] **Step 2: Run and adjudicate the repository review gate**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
bash scripts/codex-review.sh 9062d66
```

Read the saved review in `reviews/`; record every P1/P2/P3 as confirmed/fixed
or dismissed/reasoned. Apply small fixes inline or a substantial follow-up task,
rerun focused/full tests, commit fixes, and rerun the review once if any fix was
substantial.

- [ ] **Step 3: Cross-build the reviewed application**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
./cross-build.sh
```

- [ ] **Step 4: Snapshot Pi state and deploy with the flag off**

Record app, hostapd, Bluetooth PIDs/restart counts and the config hash. Create a
recoverable backup, then deploy/restart only Prodigy:

```bash
ssh matt@192.168.1.149 'cp -a ~/.openauto/config.yaml ~/.openauto/config.yaml.cluster-spike-backup'
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/
ssh matt@192.168.1.149 'sudo systemctl restart openauto-prodigy.service'
```

Do not enable CLUSTER yet. Confirm wireless MAIN reconnects, renders, accepts
touch, exits to dashboard, reopens, disconnects, and reconnects with one app
process and unchanged hostapd/Bluetooth lifetimes.

- [ ] **Step 5: Enable capture plus CLUSTER atomically and restart Prodigy**

Use the Pi's installed PyYAML to load the backup-protected config, set only:

```yaml
connection:
  protocol_capture:
    enabled: true
    format: jsonl
    include_media: false
plugin_config:
  org.openauto.android-auto:
    experimental_cluster_display: true
    experimental_cluster_setup_focus: projected_no_input
```

Write a sibling temporary file, `fsync`, preserve the original mode, and
`os.replace()` it over `~/.openauto/config.yaml`; then restart only
`openauto-prodigy.service`. Before continuing, parse the saved file and assert
the four exact values above.

- [ ] **Step 6: Run the live MAIN-plus-CLUSTER matrix**

Reconnect wireless AA, start Google Maps navigation, capture the initial
service discovery and channel lifecycle, request exit-to-car, and observe the
dashboard widget. Record:

- serialized MAIN/CLUSTER display IDs/types and matching input IDs;
- channel 12/13 open/reject and setup/start/focus events;
- first CLUSTER media/decoded frame and dimensions;
- whether frames continue while MAIN focus is `NATIVE`;
- widget rendering and aspect fit;
- MAIN reopen/touch/focus health;
- received/ACKed counts and MAIN/CLUSTER decoder latency summaries;
- app CPU/RSS and process/service health.

If CLUSTER does not activate, atomically change only
`experimental_cluster_setup_focus` to `projected`, restart Prodigy, reconnect,
and repeat before classifying the outcome.

- [ ] **Step 7: Restore config and verify recovery**

Stop only Prodigy, restore the backup with `cp --preserve=all`, restart
Prodigy, compare the restored SHA-256 to the original, and confirm MAIN wireless
projection plus one responsive app process. Remove the temporary backup only
after the hash matches; do not restart hostapd or Bluetooth.

- [ ] **Step 8: Update docs with actual evidence**

Update roadmap/wishlist/INDEX according to the observed result. Append a
session handoff containing what changed, why, status, next 1–3 steps, all local
verification commands/results, Opus design/plan review adjudication, repository
review adjudication, cross-build result, Pi rollback path, capture path, exact
live outcome, service health, and config restoration hash.

On a completed experiment, change both plan headers to
`Status: COMPLETED 2026-07-24` and use `git mv` to move them to
`docs/archive/plans/` in the same commit. Preserve a negative activation result
as completed research and leave generalized multi-display in the wishlist.

- [ ] **Step 9: Verify docs and commit Task 7**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git diff --check
python3 scripts/check-doc-links.py
git mv docs/plans/2026-07-24-aa-projected-cluster-widget-design.md \
       docs/archive/plans/2026-07-24-aa-projected-cluster-widget-design.md
git mv docs/plans/2026-07-24-aa-projected-cluster-widget-plan.md \
       docs/archive/plans/2026-07-24-aa-projected-cluster-widget-plan.md
git add docs/roadmap-current.md docs/wishlist.md docs/INDEX.md \
        docs/session-handoffs.md docs/archive/plans/2026-07-24-aa-projected-cluster-widget-design.md \
        docs/archive/plans/2026-07-24-aa-projected-cluster-widget-plan.md
git commit -m "docs: complete projected cluster widget experiment"
```

If the known unrelated untracked
`docs/aa-protocol/wishlist-baselines/aa-display-rendering.md` still causes the
repository-wide link checker to report its 14 external-tree links, run and
record a scoped check over every changed tracked doc and do not edit, delete,
or commit that user-owned reference file.

---

## Execution Decision

Execute `main`-tier Tasks 1–5 and 7 inline with
`superpowers:executing-plans`; Task 5 stays in the main session because its
teardown-before-finalize ordering and focus ownership are protocol/threading
judgments under the repo tier rules. Dispatch Opus-tier Task 6 as a prepared,
write-capable Opus job only after all consumed interfaces are committed and
green; its prompt pins files, acceptance criteria, out-of-scope boundary, and
focused test command from this plan. The main session reads the returned
verdict and `git diff`, reruns the task's focused/full gates, and commits only
verified work. If the write job cannot preserve shared-worktree state or
exhausts two focused attempts, the main session takes over rather than
broadening scope.
