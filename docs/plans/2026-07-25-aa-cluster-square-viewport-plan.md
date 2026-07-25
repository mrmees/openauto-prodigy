# Android Auto CLUSTER Square Viewport Implementation Plan

Date: 2026-07-25
Status: ACTIVE
Design: `docs/plans/2026-07-25-aa-cluster-square-viewport-design.md`
Implementation/review base: `b7f6451` (initial reviewed-plan commit; the
plan-review amendment itself is intentionally included in the final range)

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Have Android Auto render a centered 300x300 CLUSTER content viewport
inside its required 800x480 encoded carrier and display only that viewport,
uniformly upsized, in one fixed 3x3 dashboard widget.

**Architecture:** One immutable application-owned geometry value drives AA
service-discovery margins, CLUSTER session properties, decoded-frame
validation, QML crop geometry, and focused tests. The phone continues encoding
one 800x480 H.264 stream; QML uses one oversized and offset `VideoOutput`
inside a clipped square item, so no second decode, frame rewrite, shader, or
nonstandard protocol resolution is introduced.

**Tech Stack:** C++17, Qt 6.8 Core/Multimedia/QML/Quick, protobuf-generated AA
types, Qt Test, CMake, Docker aarch64 cross-build, Raspberry Pi 4 wireless
Android Auto bench with Pixel 8 / Android Auto 17.3.

## Global Constraints

- `libs/prodigy-oaa-protocol/proto/` is hands-off; `proto/api/` is frozen
  additive-only. This plan changes neither.
- Wireless Android Auto only. No USB/libusb transport.
- Qt 6.8 system packages; local builds stay in
  `~/builds/openauto-prodigy`, never the repository's `build/` directory.
- The experimental feature remains default-off and the disabled MAIN-only
  serialized descriptors remain byte-identical.
- CLUSTER remains display ID 1 on video/input channels 12/13 with exactly one
  H.264 baseline, `VIDEO_800x480`, 30 FPS, 140 DPI video configuration.
- The fixed content rectangle is `(250, 90, 300, 300)` and is expressed to the
  phone as total margins `500x180`; every consumer derives these values from
  one shared application-owned contract.
- CLUSTER input remains capability-empty. No touch, key, touchpad, haptic, or
  coordinate remapping is added.
- The widget is fixed at 3x3 and retains one `VideoOutput`, one sink claim, and
  current-page ownership. No `sourceRect`, transform, `ShaderEffect`, CPU crop,
  frame copy, enhancement, stretch, alternate resolution, or second decoder.
- A valid CLUSTER frame whose decoded dimensions are not 800x480 is rejected
  before sink delivery and terminates only that CLUSTER display generation;
  MAIN does not apply this geometry check.
- QML is compiled into the application binary, so the app target, aarch64
  cross-build, and binary deployment are mandatory.
- Per-task commits only. Nobody pushes before the repository review gate is
  adjudicated and Matthew gives explicit approval.

---

### Task 1: Shared Viewport Contract and Discovery Margins

**Tier:** `main`

**Definition of Ready:** The encoded carrier, phone-rendered content size,
center offsets, total margins, codec, FPS, and DPI are fixed by the reviewed
design. The shared value is application policy and requires no proto edit.

**Acceptance Criteria:**

- One constexpr geometry value stores only carrier and content dimensions and
  derives margins and center offsets.
- Compile-time checks reject nonpositive dimensions, nonsquare content,
  content larger than the carrier, or odd total margins.
- The geometry exists and resolves identically whether the experimental flag
  is enabled or disabled.
- Enabled CLUSTER discovery still advertises `VIDEO_800x480`, H.264 baseline,
  30 FPS, and 140 DPI, with margins 500/180 obtained from the shared value.
- Disabled MAIN-only descriptor bytes remain unchanged.

**Out of Scope:** Session/QML properties, decoded-frame validation, widget
sizing, QML rendering, config migration, and Pi deployment.

**Files:**

- Modify: `src/core/aa/ProjectedDisplayConfig.hpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `tests/test_projected_display_config.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`

**Interfaces:**

- Produces:

```cpp
namespace oap::aa {
struct ProjectedViewportGeometry {
    int encodedWidth;
    int encodedHeight;
    int contentWidth;
    int contentHeight;

    constexpr int marginWidth() const { return encodedWidth - contentWidth; }
    constexpr int marginHeight() const { return encodedHeight - contentHeight; }
    constexpr int contentX() const { return marginWidth() / 2; }
    constexpr int contentY() const { return marginHeight() / 2; }
    constexpr bool isValid() const
    {
        return encodedWidth > 0 && encodedHeight > 0
            && contentWidth > 0 && contentHeight > 0
            && contentWidth == contentHeight
            && contentWidth <= encodedWidth
            && contentHeight <= encodedHeight
            && marginWidth() % 2 == 0 && marginHeight() % 2 == 0;
    }
};

inline constexpr ProjectedViewportGeometry kClusterViewportGeometry{
    800, 480, 300, 300
};
}
```

- Consumes: the existing CLUSTER descriptor path in
  `ServiceDiscoveryBuilder::buildClusterVideoDescriptor()`.

- [ ] **Step 1: Write failing shared-geometry tests**

Extend `tests/test_projected_display_config.cpp` with a focused test using the
public constexpr value:

```cpp
void clusterViewportGeometryIsCenteredAndInternallyValid()
{
    constexpr auto geometry = oap::aa::kClusterViewportGeometry;
    QCOMPARE(geometry.encodedWidth, 800);
    QCOMPARE(geometry.encodedHeight, 480);
    QCOMPARE(geometry.contentWidth, 300);
    QCOMPARE(geometry.contentHeight, 300);
    QCOMPARE(geometry.marginWidth(), 500);
    QCOMPARE(geometry.marginHeight(), 180);
    QCOMPARE(geometry.contentX(), 250);
    QCOMPARE(geometry.contentY(), 90);

    oap::YamlConfig yaml;
    QVERIFY(!oap::aa::resolveProjectedClusterConfig(yaml).enabled);
    QCOMPARE(oap::aa::kClusterViewportGeometry.contentWidth, 300);

    yaml.setPluginValue("org.openauto.android-auto",
                        "experimental_cluster_display", true);
    QVERIFY(oap::aa::resolveProjectedClusterConfig(yaml).enabled);
    QCOMPARE(oap::aa::kClusterViewportGeometry.encodedWidth, 800);
    QCOMPARE(oap::aa::kClusterViewportGeometry.encodedHeight, 480);
    QCOMPARE(oap::aa::kClusterViewportGeometry.contentWidth, 300);
    QCOMPARE(oap::aa::kClusterViewportGeometry.contentHeight, 300);
    QCOMPARE(oap::aa::kClusterViewportGeometry.contentX(), 250);
    QCOMPARE(oap::aa::kClusterViewportGeometry.contentY(), 90);
}
```

Update the enabled CLUSTER assertions in
`tests/test_service_discovery_builder.cpp` to compare the serialized margins
to `static_cast<uint32_t>(kClusterViewportGeometry.marginWidth())` and the
equivalent height expression rather than independent literals. Retain the
existing implicit-default and explicit-false golden-byte checks unchanged.
Add these test functions as `private slots:` members of the existing Qt Test
classes, not as free functions.

- [ ] **Step 2: Run the focused tests to prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_projected_display_config test_service_discovery_builder -j$(nproc)
ctest --output-on-failure -R '^(test_projected_display_config|test_service_discovery_builder)$'
```

Expected: compilation fails because `ProjectedViewportGeometry` and
`kClusterViewportGeometry` do not exist.

- [ ] **Step 3: Implement the constexpr geometry with compile-time invariants**

Add the interface above beside `ProjectedClusterConfig`, then add these exact
class-of-invariant checks:

```cpp
static_assert(kClusterViewportGeometry.isValid());
```

The constexpr `isValid()` function makes the constraint reusable for any
future value of this application-owned type; the `static_assert` applies it to
the only value in this plan. Do not add setters, YAML keys, duplicated margin
members, or a protocol-library type.

- [ ] **Step 4: Re-run focused tests to expose the behavioral RED**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_projected_display_config test_service_discovery_builder -j$(nproc)
ctest --output-on-failure -R '^(test_projected_display_config|test_service_discovery_builder)$'
```

Expected: the config test compiles and passes, while discovery fails because
the implementation still serializes zero margins instead of 500/180.

- [ ] **Step 5: Consume the shared margins in CLUSTER discovery**

Keep the existing resolution/FPS/DPI/codec assignments and replace only the
two zero-margin assignments:

```cpp
config->set_margin_width(kClusterViewportGeometry.marginWidth());
config->set_margin_height(kClusterViewportGeometry.marginHeight());
```

- [ ] **Step 6: Run focused tests and commit Task 1**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_projected_display_config test_service_discovery_builder -j$(nproc)
ctest --output-on-failure -R '^(test_projected_display_config|test_service_discovery_builder)$'
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/core/aa/ProjectedDisplayConfig.hpp \
        src/core/aa/ServiceDiscoveryBuilder.cpp \
        tests/test_projected_display_config.cpp \
        tests/test_service_discovery_builder.cpp
git commit -m "feat: define AA cluster square viewport"
```

Expected: both focused tests pass; the commit contains no proto changes.

---

### Task 2: Role-Safe Geometry and CLUSTER Frame Guard

**Tier:** `main`

**Definition of Ready:** Task 1 provides
`oap::aa::kClusterViewportGeometry`; the reviewed design assigns geometry
validation to `ProjectedDisplaySession` at its existing
`frameReadyForGeneration` boundary.

**Acceptance Criteria:**

- A CLUSTER session exposes six immutable integer properties for the carrier
  and content rectangle; a MAIN session reports zero for all six.
- The test frame helper defaults to 800x480 so ordinary CLUSTER tests remain
  valid; MAIN tests pass an explicit small size, and only the rejection case
  publishes a mismatched CLUSTER size.
- A valid 800x480 CLUSTER frame reaches the sink and transitions to Rendering.
- A valid but differently sized CLUSTER frame logs once for that generation,
  does not reach the sink, and enters terminal Error with explicit geometry
  mismatch status.
- MAIN continues to accept a valid non-800x480 test frame.
- Existing generation, lifecycle, terminal-state, and exclusive-sink tests
  remain green.

**Out of Scope:** Decoder allocation, codec negotiation, discovery fields,
QML crop math, widget registration, and MAIN frame-size policy.

**Files:**

- Modify: `src/core/aa/ProjectedDisplaySession.hpp`
- Modify: `src/core/aa/ProjectedDisplaySession.cpp`
- Modify: `tests/test_projected_display_session.cpp`

**Interfaces:**

- Consumes: `oap::aa::kClusterViewportGeometry` from Task 1.
- Produces these QML-visible constant properties and C++ getters:

```cpp
Q_PROPERTY(int viewportEncodedWidth READ viewportEncodedWidth CONSTANT)
Q_PROPERTY(int viewportEncodedHeight READ viewportEncodedHeight CONSTANT)
Q_PROPERTY(int viewportContentX READ viewportContentX CONSTANT)
Q_PROPERTY(int viewportContentY READ viewportContentY CONSTANT)
Q_PROPERTY(int viewportContentWidth READ viewportContentWidth CONSTANT)
Q_PROPERTY(int viewportContentHeight READ viewportContentHeight CONSTANT)

int viewportEncodedWidth() const;
int viewportEncodedHeight() const;
int viewportContentX() const;
int viewportContentY() const;
int viewportContentWidth() const;
int viewportContentHeight() const;
```

- [ ] **Step 1: Make test-frame dimensions explicit and add failing property tests**

Change the friend helper in `tests/test_projected_display_session.cpp` to:

```cpp
static void publishFrame(
    VideoDecoder& decoder,
    quint64 generation,
    QSize size = QSize(kClusterViewportGeometry.encodedWidth,
                       kClusterViewportGeometry.encodedHeight))
{
    const QVideoFrameFormat format(size,
                                   QVideoFrameFormat::Format_YUV420P);
    {
        std::scoped_lock lock(decoder.latestFrameMutex_);
        decoder.latestFrame_ = QVideoFrame(format);
        decoder.hasLatestFrame_.store(true);
    }
    emit decoder.frameReadyForGeneration(generation);
}
```

Keep default-sized calls for CLUSTER. Change the existing MAIN sink-delivery
call to pass `QSize(2, 2)` explicitly. Add a test asserting all six properties
equal the shared geometry on `enabledClusterDisplay()` and all six equal zero
on `enabledMainDisplay()`:

```cpp
void geometryPropertiesAreRoleSafe()
{
    auto cluster = enabledClusterDisplay();
    QCOMPARE(cluster.viewportEncodedWidth(),
             oap::aa::kClusterViewportGeometry.encodedWidth);
    QCOMPARE(cluster.viewportEncodedHeight(),
             oap::aa::kClusterViewportGeometry.encodedHeight);
    QCOMPARE(cluster.viewportContentX(),
             oap::aa::kClusterViewportGeometry.contentX());
    QCOMPARE(cluster.viewportContentY(),
             oap::aa::kClusterViewportGeometry.contentY());
    QCOMPARE(cluster.viewportContentWidth(),
             oap::aa::kClusterViewportGeometry.contentWidth);
    QCOMPARE(cluster.viewportContentHeight(),
             oap::aa::kClusterViewportGeometry.contentHeight);

    auto main = enabledMainDisplay();
    QCOMPARE(main.viewportEncodedWidth(), 0);
    QCOMPARE(main.viewportEncodedHeight(), 0);
    QCOMPARE(main.viewportContentX(), 0);
    QCOMPARE(main.viewportContentY(), 0);
    QCOMPARE(main.viewportContentWidth(), 0);
    QCOMPARE(main.viewportContentHeight(), 0);
}
```

- [ ] **Step 2: Add failing CLUSTER mismatch-isolation tests**

Update `legacyDecoderSinkReceivesSessionFrames()` to attach through the public
session API, publish the default 800x480 frame, and expect one sink delivery
plus Rendering. Keep
`malformedSideMessageDoesNotTerminateLegacyMainRendering()` as the MAIN proof,
but make its existing publish call explicitly pass `QSize(2, 2)`; do not call
the CLUSTER-specific `openAndStart()` helper for MAIN.

Add `QMutex`, `QStringList`, and a scoped `qInstallMessageHandler` capture using
the same bounded ownership pattern as `tests/test_video_decoder.cpp`. Its
`joined()` accessor returns the captured messages under the mutex. Add the
mismatch case as a `private slots:` member and publish the same bad frame twice
to prove the terminal latch emits one warning for the generation:

```cpp
void clusterRejectsMismatchedFrameBeforeSinkDelivery()
{
    auto display = enabledClusterDisplay();
    display.beginProtocolSession();
    QSignalSpy resetSpy(display.decoder(),
                        &oap::aa::VideoDecoder::streamResetCompleted);
    openAndStart(display, resetSpy);

    QVideoSink sink;
    QSignalSpy frameSpy(&sink, &QVideoSink::videoFrameChanged);
    QVERIFY(display.attachVideoSink(&sink));
    ScopedMessageCapture capture;
    const quint64 generation = resetSpy[0][0].toULongLong();
    oap::aa::VideoDecoderTestAccess::publishFrame(
        *display.decoder(), generation, QSize(640, 480));
    oap::aa::VideoDecoderTestAccess::publishFrame(
        *display.decoder(), generation, QSize(640, 480));

    QCoreApplication::processEvents();
    QCOMPARE(frameSpy.count(), 0);
    QCOMPARE(display.state(),
             static_cast<int>(oap::aa::ProjectedDisplaySession::Error));
    QVERIFY(display.statusText().contains(QStringLiteral("geometry mismatch"),
                                          Qt::CaseInsensitive));
    QCOMPARE(capture.joined().count(
                 QStringLiteral("decoded frame geometry mismatch")), 1);
}
```

- [ ] **Step 3: Run the session test to prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_projected_display_session -j$(nproc)
ctest --output-on-failure -R '^test_projected_display_session$'
```

Expected: compilation fails because the six properties/getters do not exist;
after test compilation is possible, the mismatched CLUSTER frame is still
delivered and does not enter Error.

- [ ] **Step 4: Implement role-safe constant getters**

Implement each getter as a role check against the shared value, for example:

```cpp
int ProjectedDisplaySession::viewportEncodedWidth() const
{
    return role_ == ProjectedDisplayRole::Cluster
        ? kClusterViewportGeometry.encodedWidth : 0;
}
```

Apply the same pattern to encoded height, content X/Y, and content width/height.
Do not make the values mutable and do not emit a change signal.

- [ ] **Step 5: Reject mismatched CLUSTER frames at the delivery boundary**

Inside the valid-frame branch of the existing
`frameReadyForGeneration` connection, before first-frame success logging or
sink delivery, add the role-scoped guard:

```cpp
if (role_ == ProjectedDisplayRole::Cluster
    && (frame.width() != kClusterViewportGeometry.encodedWidth
        || frame.height() != kClusterViewportGeometry.encodedHeight)) {
    qCWarning(lcAA).noquote()
        << diagnosticPrefix_ << "decoded frame geometry mismatch expected="
        << kClusterViewportGeometry.encodedWidth << "x"
        << kClusterViewportGeometry.encodedHeight << "actual="
        << frame.width() << "x" << frame.height();
    enterTerminalState(
        Error, QStringLiteral("Projected frame geometry mismatch"));
    return;
}
```

The existing terminal latch bounds the warning to the active decoder
generation. Do not deliver the rejected frame and do not apply the guard to
MAIN.

- [ ] **Step 6: Run focused tests and commit Task 2**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_projected_display_session -j$(nproc)
ctest --output-on-failure -R '^test_projected_display_session$'
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/core/aa/ProjectedDisplaySession.hpp \
        src/core/aa/ProjectedDisplaySession.cpp \
        tests/test_projected_display_session.cpp
git commit -m "feat: guard AA cluster viewport geometry"
```

Expected: the focused session suite passes, including matching CLUSTER,
mismatched CLUSTER, and non-800x480 MAIN delivery.

---

### Task 3: Fixed 3x3 Widget and QML Crop/Upsize Surface

**Tier:** `opus`

**Definition of Ready:** Task 2 exposes the six immutable geometry properties.
The QML implementation is fixed to one clipped square, one oversized and
offset `VideoOutput`, and one current-page sink claim.

**Acceptance Criteria:**

- The descriptor's min/max/default rows and columns are all 3.
- The picker omits the widget for 2x2 space and includes it for 3x3 space.
- The QML fake reads its geometry from the same C++ shared value used by
  discovery and session code.
- At square root sizes 300x300 and 450x450, the crop item fills the root and
  the source rectangle `(250,90,300,300)` maps exactly onto it at uniform
  scales 1.0 and 1.5. At 600x400 it selects the 400-pixel minimum dimension,
  centers at x=100/y=0, and maps the same source rectangle at scale 4/3.
- QML retains exactly one `VideoOutput`, `PreserveAspectFit`, current-page
  ownership, retry behavior, and visible loading/error/duplicate-sink status.
- QML contains no `MouseArea`, `TapHandler`, `ShaderEffect`, `sourceRect`, or
  transform.
- The rendering guide distinguishes MAIN's existing aspect crop from the
  experimental CLUSTER fixed-margin square crop.

**Out of Scope:** General widget resizing, configurable geometry, production
placement migration, additional displays, touch input, shaders, CPU frame
processing, and Pi configuration changes.

**Files:**

- Modify: `src/plugins/android_auto/AAClusterWidgetRegistration.cpp`
- Modify: `qml/widgets/AAClusterWidget.qml`
- Modify: `tests/test_aa_cluster_widget.cpp`
- Modify: `docs/aa-protocol/aa-display-rendering.md`

**Interfaces:**

- Consumes from Task 2:

```qml
AAClusterDisplay.viewportEncodedWidth
AAClusterDisplay.viewportEncodedHeight
AAClusterDisplay.viewportContentX
AAClusterDisplay.viewportContentY
AAClusterDisplay.viewportContentWidth
AAClusterDisplay.viewportContentHeight
```

- Produces two test-visible QML object names:
  `clusterCropViewport` and `clusterVideoOutput`.

- [ ] **Step 1: Write failing fixed-size and runtime-geometry tests**

Update `enabledConfigRegistersFixedSquare()` to expect all six descriptor size
values to equal 3, `widgetsFittingSpace(2, 2)` to be empty, and
`widgetsFittingSpace(3, 3)` to contain the CLUSTER descriptor.

Extend `FakeClusterDisplay` with six `CONSTANT` integer properties whose
getters return `oap::aa::kClusterViewportGeometry` values. Include
`core/aa/ProjectedDisplayConfig.hpp` and `QQuickItem` in the test.

```cpp
Q_PROPERTY(int viewportEncodedWidth READ viewportEncodedWidth CONSTANT)
Q_PROPERTY(int viewportEncodedHeight READ viewportEncodedHeight CONSTANT)
Q_PROPERTY(int viewportContentX READ viewportContentX CONSTANT)
Q_PROPERTY(int viewportContentY READ viewportContentY CONSTANT)
Q_PROPERTY(int viewportContentWidth READ viewportContentWidth CONSTANT)
Q_PROPERTY(int viewportContentHeight READ viewportContentHeight CONSTANT)

int viewportEncodedWidth() const
{ return oap::aa::kClusterViewportGeometry.encodedWidth; }
int viewportEncodedHeight() const
{ return oap::aa::kClusterViewportGeometry.encodedHeight; }
int viewportContentX() const
{ return oap::aa::kClusterViewportGeometry.contentX(); }
int viewportContentY() const
{ return oap::aa::kClusterViewportGeometry.contentY(); }
int viewportContentWidth() const
{ return oap::aa::kClusterViewportGeometry.contentWidth; }
int viewportContentHeight() const
{ return oap::aa::kClusterViewportGeometry.contentHeight; }
```

After creating a current widget instance, locate the two named objects, assert
the crop object itself has `clip == true`, and check the two square scales:

```cpp
auto* crop = current->findChild<QQuickItem*>("clusterCropViewport");
auto* video = current->findChild<QQuickItem*>("clusterVideoOutput");
QVERIFY(crop);
QVERIFY(video);
QVERIFY(crop->clip());

for (const qreal side : {300.0, 450.0}) {
    current->setProperty("width", side);
    current->setProperty("height", side);
    QCoreApplication::processEvents();
    const qreal scale = side / 300.0;
    QCOMPARE(crop->width(), side);
    QCOMPARE(crop->height(), side);
    QCOMPARE(video->width(), 800.0 * scale);
    QCOMPARE(video->height(), 480.0 * scale);
    QCOMPARE(video->x(), -250.0 * scale);
    QCOMPARE(video->y(), -90.0 * scale);
}
```

Then exercise the production-shaped nonsquare path:

```cpp
current->setProperty("width", 600.0);
current->setProperty("height", 400.0);
QCoreApplication::processEvents();
const qreal scale = 400.0 / 300.0;
QCOMPARE(crop->width(), 400.0);
QCOMPARE(crop->height(), 400.0);
QCOMPARE(crop->x(), 100.0);
QCOMPARE(crop->y(), 0.0);
QCOMPARE(video->width(), 800.0 * scale);
QCOMPARE(video->height(), 480.0 * scale);
QCOMPARE(video->x(), -250.0 * scale);
QCOMPARE(video->y(), -90.0 * scale);
```

Retain the source-token checks and add checks for both object names,
the crop subtree, and all six `AAClusterDisplay.viewport...` properties. Do
not use a bare source-wide `clip: true` assertion because the root already has
that token; the runtime `crop->clip()` assertion owns the new requirement.

- [ ] **Step 2: Run the widget test to prove RED**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_aa_cluster_widget -j$(nproc)
ctest --output-on-failure -R '^test_aa_cluster_widget$'
```

Expected: descriptor assertions still see 2x2 and the crop/video named objects
and geometry properties do not exist.

- [ ] **Step 3: Change registration to a fixed 3x3 contract**

Set `minCols`, `maxCols`, `defaultCols`, `minRows`, `maxRows`, and
`defaultRows` to 3. Keep the widget ID, display name, category, description,
icon, and QML URL unchanged.

- [ ] **Step 4: Implement the single-output QML crop geometry**

Replace the full-parent `VideoOutput` with this rendering subtree while
retaining the surrounding sink-claim functions, retry timer, and destruction
cleanup:

```qml
Item {
    id: cropViewport
    objectName: "clusterCropViewport"
    width: Math.min(root.width, root.height)
    height: width
    anchors.centerIn: parent
    clip: true
    visible: root.ownsSink && AAClusterDisplay.rendering

    VideoOutput {
        id: videoOutput
        objectName: "clusterVideoOutput"
        readonly property real viewportScale:
            AAClusterDisplay.viewportContentWidth > 0
            ? cropViewport.width / AAClusterDisplay.viewportContentWidth
            : 0
        width: AAClusterDisplay.viewportEncodedWidth * viewportScale
        height: AAClusterDisplay.viewportEncodedHeight * viewportScale
        x: -AAClusterDisplay.viewportContentX * viewportScale
        y: -AAClusterDisplay.viewportContentY * viewportScale
        fillMode: VideoOutput.PreserveAspectFit
    }
}
```

Change only the status container's visibility predicate to
`!cropViewport.visible`. Keep `root.clip`, exactly one `VideoOutput`, and all
current-page/single-sink behavior. Do not add anchors to the `VideoOutput`, a
second output, writable `sourceRect`, transforms, or effects.

- [ ] **Step 5: Document the distinct CLUSTER rendering path**

In `docs/aa-protocol/aa-display-rendering.md`, add a bounded experimental
CLUSTER subsection stating:

- the encoded mode stays 800x480 H.264/30 at 140 DPI;
- total margins 500x180 request centered 300x300 phone content;
- CLUSTER input has no touch capability;
- one clipped QML square maps the known source rectangle and uniformly upsizes
  it; and
- this does not change MAIN's `PreserveAspectCrop` path or introduce frame
  processing, generalized multi-display, or a public setting.

Add `ProjectedDisplaySession.cpp`, `AAClusterWidgetRegistration.cpp`, and
`qml/widgets/AAClusterWidget.qml` to the implementation-source table with
their CLUSTER roles.

- [ ] **Step 6: Run focused tests and commit Task 3**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_aa_cluster_widget test_service_discovery_builder test_projected_display_session -j$(nproc)
ctest --output-on-failure -R '^(test_aa_cluster_widget|test_service_discovery_builder|test_projected_display_session)$'
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/plugins/android_auto/AAClusterWidgetRegistration.cpp \
        qml/widgets/AAClusterWidget.qml \
        tests/test_aa_cluster_widget.cpp \
        docs/aa-protocol/aa-display-rendering.md
git commit -m "feat: render AA cluster in square widget"
```

Expected: all three focused suites pass; the QML test proves two square scales,
the nonsquare min-and-center path, and the static forbidden-token checks.

---

### Task 4: Full Local Gate and Repository Review

**Tier:** `main`

**Definition of Ready:** Tasks 1-3 are committed; focused tests are green; no
unrelated or user-owned files have been staged.

**Acceptance Criteria:**

- The complete local build succeeds from the Linux-filesystem build tree.
- The explicit `openauto-prodigy` target succeeds, preventing a stale
  `main.cpp` object from masking an app break.
- Full CTest passes.
- `git diff --check` is clean and the hands-off/frozen protocol directories
  have no changes.
- `scripts/codex-review.sh` completes, and every P1/P2/P3 finding is either
  fixed or dismissed with a concrete technical reason.
- Any substantial confirmed fix is retested and triggers one review rerun.

**Out of Scope:** Cross-build, Pi mutation, live phone acceptance, push, PR,
tagging, and release packaging.

**Files:**

- Inspect: `b7f6451..HEAD`
- Do not modify unless a confirmed review finding requires a bounded fix.

- [ ] **Step 1: Run the mandatory local verification gate**

```bash
cd ~/builds/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git diff --check
git diff --name-only b7f6451..HEAD -- libs/prodigy-oaa-protocol/proto proto/api
```

Expected: all builds/tests pass, `git diff --check` prints nothing, and the
protocol-boundary diff prints nothing.

- [ ] **Step 2: Run and adjudicate the repository review gate**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
bash scripts/codex-review.sh b7f6451
```

Read the resulting file under `reviews/`. For each P1/P2/P3 finding, record
`confirmed` or `dismissed` plus evidence. Fix confirmed findings with a focused
test first, rerun the affected test and mandatory local gate, commit the fix,
and rerun the review once if the fix was substantial. If the script exits 2 or
4, record the required degraded Fable-only gate rather than silently passing.

- [ ] **Step 3: Confirm the reviewed tree is deployment-ready**

```bash
git status --short --branch
git log --oneline --decorate b7f6451..HEAD
```

Expected: only the user's pre-existing untracked wishlist baseline remains
outside commits; implementation commits are review-adjudicated and nothing has
been pushed.

---

### Task 5: Cross-Build, Guarded Pi Deployment, and Live Acceptance

**Tier:** `main`

**Definition of Ready:** Task 4 is green and adjudicated. SSH to
`matt@192.168.1.149` works. The live placement is re-read before mutation; the
currently observed expected state is instance `org.openauto.aa-cluster-16` at
dashboard `home`, page 1, col 0, row 0, span 2x2 in an 8x4 grid, with no
placement intersecting the proposed 3x3 rectangle.

**Acceptance Criteria:**

- Recoverable byte-for-byte backups of the deployed binary and config exist
  before replacement, with their SHA-256 values recorded.
- The Docker aarch64 cross-build succeeds and the staged binary is aarch64.
- The service is stopped before binary/config changes, preventing shutdown
  config flush from overwriting the guarded edit.
- Only the exact known bench placement's `col_span` and `row_span` change from
  2 to 3; if the instance is absent, the picker creates it normally; any other
  placement shape or collision stops deployment and restores the backups.
- The service returns active with one process, no restart loop, the flag still
  enabled, and the placement persisted at 3x3.
- Pixel 8 opens CLUSTER channels 12/13 and sends decoded 800x480 frames; the
  dashboard shows only the square phone-rendered content without borders,
  stretch, or overlap; MAIN exit and return remain healthy.
- Matthew explicitly accepts the live presentation before the work is marked
  complete. A rejected live result restores both known-good files and leaves
  the design/plan ACTIVE.

**Out of Scope:** Push/PR, milestone tag, release package, arbitrary placement
migration, default-on behavior, settings UI, and generalized display support.

**Files:**

- Build: `build-pi/src/openauto-prodigy`
- Deploy: `matt@192.168.1.149:~/openauto-prodigy/build/src/openauto-prodigy`
- Guarded runtime edit: `matt@192.168.1.149:~/.openauto/config.yaml`
- Modify after accepted live result:
  `docs/roadmap-current.md`, `docs/INDEX.md`, `docs/session-handoffs.md`,
  `docs/archive/session-handoffs/2026-07-24-handoffs.md`, this plan, and its
  design.

- [ ] **Step 1: Record and preserve the known-good Pi state**

Run one remote shell so the unique backup stamp is retained in persistent Pi
storage rather than `/tmp`:

```bash
ssh matt@192.168.1.149 '
set -eu
stamp=$(date +%Y%m%d-%H%M%S)
bin="$HOME/openauto-prodigy/build/src/openauto-prodigy"
cfg="$HOME/.openauto/config.yaml"
stamp_file="$HOME/.openauto/oap-square-viewport-backup-stamp"
printf "%s\n" "$stamp" > "$stamp_file"
cp -a "$bin" "$bin.pre-square-viewport-$stamp"
cp -a "$cfg" "$cfg.pre-square-viewport-$stamp"
sha256sum "$bin" "$bin.pre-square-viewport-$stamp" \
          "$cfg" "$cfg.pre-square-viewport-$stamp"
systemctl is-active openauto-prodigy.service
systemctl show openauto-prodigy.service -p MainPID -p NRestarts --no-pager
'
```

Expected: each original/backup pair has matching hashes; service state is
recorded before deployment.

- [ ] **Step 2: Cross-build and stage without touching the active binary**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
./cross-build.sh
file build-pi/src/openauto-prodigy
sha256sum build-pi/src/openauto-prodigy
rsync -av build-pi/src/openauto-prodigy \
  matt@192.168.1.149:~/openauto-prodigy/build/src/openauto-prodigy.square-viewport-stage
ssh matt@192.168.1.149 \
  'sha256sum "$HOME/openauto-prodigy/build/src/openauto-prodigy.square-viewport-stage"'
```

Expected: the cross-build passes, `file` reports an aarch64 ELF executable,
and the reboot-durable staged hash matches locally and remotely.

- [ ] **Step 3: Stop Prodigy and revalidate the exact placement branch**

```bash
ssh matt@192.168.1.149 '
set -eu
sudo systemctl stop openauto-prodigy.service
if systemctl is-active --quiet openauto-prodigy.service; then
  echo "openauto-prodigy.service is still running" >&2
  exit 1
fi
sed -n "/org.openauto.aa-cluster-16/,+7p" "$HOME/.openauto/config.yaml"
'
```

If the instance is absent, install/start the new binary and use the normal
picker to create a 3x3 instance; do not synthesize an ID or change
`next_instance_id`. If present, proceed only when its exact identity/origin/
page/span match the Definition of Ready and a fresh placement scan confirms
cols 0-2 and rows 0-2 on page 1 remain collision-free. Any mismatch restores
the backup and stops this task. The eight printed lines must match the exact
`old` block in Step 4 byte-for-byte; a visually similar block is not approval
to continue.

- [ ] **Step 4: Apply the exact guarded 2x2-to-3x3 edit and install**

Because the Pi has Python 3 plus PyYAML but no `yq`, use an exact text-block
replacement that preserves every unrelated byte. Run while the service is
stopped:

```bash
ssh matt@192.168.1.149 '
set -eu
if systemctl is-active --quiet openauto-prodigy.service; then
  echo "openauto-prodigy.service is still running" >&2
  exit 1
fi
bin="$HOME/openauto-prodigy/build/src/openauto-prodigy"
cfg="$HOME/.openauto/config.yaml"
stage="$HOME/openauto-prodigy/build/src/openauto-prodigy.square-viewport-stage"
stamp_file="$HOME/.openauto/oap-square-viewport-backup-stamp"
stamp=$(cat "$stamp_file")
deployment_succeeded=false
restore_on_exit() {
  status=$?
  trap - EXIT HUP INT TERM
  if [ "$deployment_succeeded" != true ]; then
    set +e
    sudo systemctl stop openauto-prodigy.service
    cp -a "$bin.pre-square-viewport-$stamp" "$bin"
    cp -a "$cfg.pre-square-viewport-$stamp" "$cfg"
    sudo systemctl start openauto-prodigy.service
  fi
  exit "$status"
}
trap restore_on_exit EXIT
trap "exit 129" HUP
trap "exit 130" INT
trap "exit 143" TERM
python3 - <<"PY"
from pathlib import Path

path = Path.home() / ".openauto/config.yaml"
text = path.read_text()
old = """        - instance_id: org.openauto.aa-cluster-16
          widget_id: org.openauto.aa-cluster
          col: 0
          row: 0
          col_span: 2
          row_span: 2
          opacity: 0.25
          page: 1"""
new = old.replace("col_span: 2", "col_span: 3").replace(
    "row_span: 2", "row_span: 3")
if text.count(old) != 1:
    raise SystemExit("expected exact cluster placement block not found once")
path.write_text(text.replace(old, new))
PY
install -m 0755 "$stage" "$bin"
diff -u "$cfg.pre-square-viewport-$stamp" "$cfg" || true
sha256sum "$stage" "$bin"
sudo systemctl start openauto-prodigy.service
deployment_succeeded=true
trap - EXIT HUP INT TERM
'
```

Expected: the config diff contains only the two span changes, binary hashes
match, and the service starts.

- [ ] **Step 5: Verify stable service and persisted configuration**

```bash
ssh matt@192.168.1.149 '
systemctl is-active openauto-prodigy.service
systemctl show openauto-prodigy.service -p MainPID -p NRestarts --no-pager
pgrep -a -x openauto-prodigy
sed -n "/org.openauto.aa-cluster-16/,+7p" "$HOME/.openauto/config.yaml"
journalctl -u openauto-prodigy.service -b --no-pager | \
  grep -E "CLUSTER|geometry mismatch|first decoded frame" | tail -n 80
'
```

Expected: active service, one process, no restart loop, persisted 3x3 span,
and no geometry-mismatch terminal error.

- [ ] **Step 6: Run the Pixel/dashboard acceptance sequence**

Reconnect the Pixel and capture the journal while checking, in order:

1. CLUSTER video/input channels 12/13 open.
2. First CLUSTER decoded frame logs exactly 800x480.
3. Exit MAIN to dashboard page 1.
4. The 3x3 widget shows only the square phone-rendered CLUSTER content with no
   encoded-frame borders, stretching, or overlap.
5. Reopen MAIN; projection, focus, touch, and return-to-dashboard stay healthy.
6. Matthew accepts or rejects the visible result.

If any item fails or Matthew rejects the presentation, restore both files:

```bash
ssh matt@192.168.1.149 '
set -eu
bin="$HOME/openauto-prodigy/build/src/openauto-prodigy"
cfg="$HOME/.openauto/config.yaml"
stamp_file="$HOME/.openauto/oap-square-viewport-backup-stamp"
if [ -r "$stamp_file" ]; then
  stamp=$(cat "$stamp_file")
else
  newest=$(ls -1t "$bin".pre-square-viewport-* 2>/dev/null | head -n 1)
  [ -n "$newest" ]
  stamp=${newest##*.pre-square-viewport-}
fi
[ -f "$bin.pre-square-viewport-$stamp" ]
[ -f "$cfg.pre-square-viewport-$stamp" ]
sudo systemctl stop openauto-prodigy.service
cp -a "$bin.pre-square-viewport-$stamp" "$bin"
cp -a "$cfg.pre-square-viewport-$stamp" "$cfg"
sudo systemctl start openauto-prodigy.service
sha256sum "$bin" "$bin.pre-square-viewport-$stamp" \
  "$cfg" "$cfg.pre-square-viewport-$stamp"
'
```

- [ ] **Step 7: Complete and archive the accepted work**

Only after Matthew accepts the live result:

- change both design and plan status to `COMPLETED 2026-07-25` and move them
  together to `docs/archive/plans/`;
- move the roadmap item from Now to Done with the actual Pixel/Pi outcome;
- remove the design and plan entries from both `## Active Plans` and
  `## Plans (plans/)` in `docs/INDEX.md`, remove the empty active list/section
  if appropriate, and add one paired design/plan entry under
  `## Archive (archive/)` following the existing projected-CLUSTER entry;
- append `docs/session-handoffs.md` with what/why/status, next 1-3 steps, local
  build/app/CTest/cross-build/live results, review confirmed/dismissed counts,
  backup paths, and rollback disposition; and
- because `docs/session-handoffs.md` already exceeds 300 lines and contains
  only 2026-07-24 entries, rotate those entries into
  `docs/archive/session-handoffs/2026-07-24-handoffs.md` before adding the new
  2026-07-25 entry, retaining the title/preamble in the current file; and
- leave `docs/aa-protocol/wishlist-baselines/aa-display-rendering.md` untouched.

Then commit the completion record:

```bash
git mv docs/plans/2026-07-25-aa-cluster-square-viewport-design.md \
       docs/archive/plans/2026-07-25-aa-cluster-square-viewport-design.md
git mv docs/plans/2026-07-25-aa-cluster-square-viewport-plan.md \
       docs/archive/plans/2026-07-25-aa-cluster-square-viewport-plan.md
git add docs/archive/plans/2026-07-25-aa-cluster-square-viewport-design.md \
        docs/archive/plans/2026-07-25-aa-cluster-square-viewport-plan.md \
        docs/archive/session-handoffs/2026-07-24-handoffs.md \
        docs/roadmap-current.md docs/INDEX.md docs/session-handoffs.md
git commit -m "docs: complete AA cluster square viewport"
git status --short --branch
```

Expected: the active plan directory and index contain no stale link, the
roadmap/handoff report only observed results, the user-owned baseline remains
untracked, and the branch remains unpushed pending Matthew's instruction.

## Plan Self-Review

- **Spec coverage:** Tasks 1-3 cover the shared geometry, discovery margins,
  role-safe session properties, mismatch semantics, fixed 3x3 registration,
  one-output QML crop, and rendering documentation. Tasks 4-5 cover mandatory
  local gates, repository review, recoverable cross-deployment, live Pixel
  proof, acceptance, and all completion-document/archive links.
- **Placeholder scan:** The plan contains no TBD/TODO steps, generic error
  handling instruction, unspecified test request, or unresolved implementation
  decision. The only deployment branch is the reviewed present/absent
  placement behavior, with a stop condition for any third state.
- **Type consistency:** `ProjectedViewportGeometry`,
  `kClusterViewportGeometry`, the six `viewport...` getters/properties, the
  QML property reads, fake-object properties, and test names are identical
  across producer and consumer tasks.

## Opus Plan Review Adjudication

Opus reviewed commit `b7f6451` and returned `NEEDS_CHANGES` with two blockers,
five major findings, and a set of minor findings. All were confirmed and
incorporated:

- the Pi stamp and staged binary are now reboot-durable, and the guarded
  install uses `set -eu` plus an error trap that restores both files and
  restarts the known-good service before exiting;
- widget tests now exercise a production-shaped nonsquare root as well as two
  square scales;
- MAIN validation reuses its existing correct channel-open sequence rather
  than the CLUSTER-only helper;
- both active-link sections and the archive destination in `docs/INDEX.md` are
  explicit, and the oversized handoff file receives a concrete rotation;
- the feature review base is pinned consistently to `b7f6451`;
- flag independence, reusable constexpr validity, signed protobuf comparisons,
  behavioral RED, one-warning latching, zero-safe QML division, crop-scoped
  clipping, exact placement matching, Qt `private slots:`, and existing CTest
  environment ownership are now explicit.

The binding `VideoDecoderTestAccess` sizing note was already fully covered.
The binding INDEX/roadmap archival note was partially covered initially and is
fully covered after the amendments above.

The final Opus pass reviewed commit `f28cf9a` and returned `PASS` with no
blocker or major findings, explicitly confirming both binding notes. Its three
nonblocking minor observations were incorporated mechanically: archive steps
now use `git mv`, the placement print range is exactly eight lines, and the
guarded edit reasserts that the service is stopped immediately before mutation.
