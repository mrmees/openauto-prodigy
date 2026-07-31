# Native Dashboard Turn Card Stage 1 Implementation Plan

**Status:** COMPLETED 2026-07-31

**Design:**
[`2026-07-28-native-dashboard-turn-card-design.md`](2026-07-28-native-dashboard-turn-card-design.md)

**Grounded on:** `4439c29`

> **For agentic workers:** REQUIRED SUB-SKILL: use
> `superpowers:executing-plans`. Repository policy keeps one inline
> implementation owner; use subagents only if the user explicitly authorizes
> bounded, non-overlapping delegation.

**Goal:** Replace the selected phone-rendered dashboard turn card with an
immediate, theme-aware Prodigy card that covers every defined maneuver and
renders complete current-step lane guidance, while preserving the accepted
projected map path.

**Architecture:** The phone always receives the accepted AUXILIARY navigation
map descriptor. `AAClusterWidget.qml` treats `map`/`turn_card` as a local
presentation choice, owning the video sink only in Map mode. The protocol
handler publishes current-step lane value snapshots, `NavigationDataBridge`
normalizes them into a provider-owned model, and focused QML components render
the maneuver card and continuous lane band.

**Tech Stack:** C++17, Qt 6.8 Core/Gui/Quick/QML/Multimedia, QAbstractListModel,
protobuf-generated OAA v1.5 bindings, clean-room QML Canvas navigation
geometry, CMake/CTest, Pi 4 cross-build and live validation.

## Global Constraints

- Do not edit `libs/prodigy-oaa-protocol/proto/`; OAA v1.5 already defines all
  Stage 1 fields.
- `proto/api/` remains frozen additive-only; Stage 1 does not add lane data to
  External API v1.
- Preserve wireless-only AA, GAL 6.0 production policy, H.265-first video, the
  accepted channels 12/13 AUXILIARY topology, and existing map geometry.
- The phone descriptor always uses `KEYCODE_NAVIGATION`; local Turn card mode
  does not send `KEYCODE_TURN_CARD` or reconnect AA.
- QML consumes `INavigationProvider` state only, never protocol handlers,
  protobufs, EventBus topics, or transport objects.
- At 1024x600, target distance text is 92-100 px, unit 38 px,
  road/instruction 40 px, and labels no smaller than 22 px.
- Lane guidance is one continuous noninteractive roadway band. No per-lane
  boxes, button styling, pointer handlers, or scrolling.
- Every defined `ManeuverType` and `LaneShape` has an intentional mapping;
  only undefined/future values use the fallback.
- Stage 2 fields are out of scope until Stage 1 hardware evidence is recorded.
- QML ships in the binary, so Pi acceptance uses a new ARM cross-build and
  binary deployment.
- Keep commits coherent and atomic. Do not push until the user authorizes it.

## File and interface map

### New files

- `src/core/aa/NavigationLaneModel.hpp` — app-owned stable lane presentation
  types and a one-row-per-physical-lane QAbstractListModel.
- `src/core/aa/NavigationLaneModel.cpp` — atomic snapshot replacement and QML
  `directions` role serialization.
- `qml/widgets/NavigationManeuverGlyph.qml` — exhaustive raw maneuver mapping,
  shared lane-primitive composition, and distinct clean-room Canvas semantics.
- `qml/widgets/NavigationLaneDirectionGlyph.qml` — exhaustive stable lane-shape
  token mapping.
- `qml/widgets/NavigationLaneGuidanceBand.qml` — continuous roadway band and
  lane/direction repeaters.
- `qml/widgets/NativeNavigationCard.qml` — approved card layout, typography,
  theme, and empty states.
- `tests/test_navigation_lane_model.cpp` — model replacement, role, ordering,
  and clear coverage.

### Modified runtime files

- `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp`
  — protocol-library lane snapshot value types and signal.
- `libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp` —
  extract only `steps[0].lanes`, preserve direction order, and emit empty
  snapshots to clear previous lanes.
- `src/core/services/INavigationProvider.hpp` — additive default lane-model,
  lane-presence, and distance-presence accessors.
- `src/core/aa/NavigationDataBridge.hpp/.cpp` — own the lane model, normalize
  lane shapes, track distance presence, and clear lane state.
- `src/core/aa/ProjectedDisplayConfig.hpp/.cpp` — remove phone-provider content
  selection from projected display configuration.
- `src/core/aa/ServiceDiscoveryBuilder.cpp` — always advertise
  `KEYCODE_NAVIGATION` for the auxiliary display.
- `src/core/aa/AndroidAutoOrchestrator.cpp` — remove selector reload/log
  branching; diagnostics always report the navigation provider.
- `src/plugins/android_auto/AndroidAutoPlugin.cpp` — remove
  `video.secondary_display_content` from session-renegotiation settings.
- `qml/widgets/AAClusterWidget.qml` — reactive local mode selection, sink
  release/claim, native-card selection, and neutral Map fallback copy.
- `src/CMakeLists.txt` — compile the model and embed the four new QML files.

### Modified tests

- `tests/test_projected_display_config.cpp`
- `tests/test_service_discovery_builder.cpp`
- `tests/test_settings_menu_structure.cpp`
- `tests/test_navigation_channel_handler.cpp`
- `tests/test_navigation_data_bridge.cpp`
- `tests/test_aa_cluster_widget.cpp`
- `tests/CMakeLists.txt`

### Modified current guidance on completion

- `src/core/aa/AGENTS.md`
- `docs/aa-protocol/aa-display-rendering.md`
- `docs/reference/config-schema.md`
- `docs/reference/settings-tree.md`
- `docs/engineering-backlog.md`
- `docs/roadmap-current.md`
- `docs/INDEX.md`
- `docs/session-handoffs.md`

---

### Task 1: Make Dashboard Navigation a local presentation setting

**Files:**

- Modify: `src/core/aa/ProjectedDisplayConfig.hpp`
- Modify: `src/core/aa/ProjectedDisplayConfig.cpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `tests/test_projected_display_config.cpp`
- Modify: `tests/test_service_discovery_builder.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`

**Interfaces:**

- Consumes: durable `video.secondary_display_content` values `map` and
  `turn_card`, retained in `YamlConfig` for QML.
- Produces: one invariant AUXILIARY descriptor using
  `KEYCODE_NAVIGATION`; the setting no longer enters AA session renegotiation.

- [ ] **Step 1: Replace the old selector tests with the local-mode contract**

In `tests/test_service_discovery_builder.cpp`, replace
`auxiliaryContentSelectionChoosesConnectionTimeKeycode()` with a test that
constructs an enabled `ProjectedClusterConfig` and requires:

```cpp
void auxiliaryAlwaysSelectsNavigationProvider()
{
    oap::aa::ProjectedClusterConfig cluster;
    cluster.enabled = true;
    oap::aa::ServiceDiscoveryBuilder builder;
    builder.setProjectedClusterConfig(cluster);

    const auto secondary = descriptorById(builder.build(), 12).av_channel();
    QCOMPARE(secondary.display_type(),
             oaa::proto::enums::DisplayType::AUXILIARY);
    QCOMPARE(secondary.keycode(),
             oaa::proto::enums::AndroidKeycode::KEYCODE_NAVIGATION);
}
```

In `tests/test_projected_display_config.cpp`, delete assertions that the YAML
value resolves into `ProjectedClusterConfig::content`. In
`tests/test_settings_menu_structure.cpp`, invert the plugin assertion:

```cpp
QVERIFY2(pluginSource.indexOf(QStringLiteral(
             "video.secondary_display_content")) < 0,
         "Dashboard presentation must not trigger AA renegotiation");
```

Retain the AA Settings assertions proving the picker and its durable values
still exist.

- [ ] **Step 2: Run the focused tests and confirm the old implementation fails**

```bash
cmake --build ~/builds/openauto-prodigy \
  --target test_projected_display_config test_service_discovery_builder \
           test_settings_menu_structure -j$(nproc)
ctest --test-dir ~/builds/openauto-prodigy \
  -R '^(test_projected_display_config|test_service_discovery_builder|test_settings_menu_structure)$' \
  --output-on-failure
```

Expected: failures show that `content` still affects the descriptor and the
plugin still treats the setting as session-negotiated.

- [ ] **Step 3: Remove the phone turn-card selection path**

Remove `ProjectedSecondaryContent` and `ProjectedClusterConfig::content` from
`ProjectedDisplayConfig.hpp`. Stop reading
`video.secondary_display_content` in `resolveProjectedClusterConfig()`.

In `ServiceDiscoveryBuilder::buildClusterVideoDescriptor()`, replace the
conditional with:

```cpp
avChannel->set_keycode(
    oaa::proto::enums::AndroidKeycode::KEYCODE_NAVIGATION);
```

In `AndroidAutoOrchestrator::beginSession()`, remove the pre-build content
reload and log the selector as `NAVIGATION` without branching. In
`AndroidAutoPlugin::onConfigChanged()`, retain only session-owned settings:

```cpp
static const QStringList sessionSettings = {
    QStringLiteral("video.resolution"),
    QStringLiteral("video.fps"),
    QStringLiteral("connection.gal_version"),
};
```

- [ ] **Step 4: Run focused configuration and descriptor tests**

Run the command from Step 2. Expected: all three focused tests pass, including
the byte-exact descriptor fixture with the existing navigation keycode.

- [ ] **Step 5: Commit the local presentation boundary**

```bash
git add src/core/aa/ProjectedDisplayConfig.hpp \
        src/core/aa/ProjectedDisplayConfig.cpp \
        src/core/aa/ServiceDiscoveryBuilder.cpp \
        src/core/aa/AndroidAutoOrchestrator.cpp \
        src/plugins/android_auto/AndroidAutoPlugin.cpp \
        tests/test_projected_display_config.cpp \
        tests/test_service_discovery_builder.cpp \
        tests/test_settings_menu_structure.cpp
git commit -m "refactor(aa): make dashboard navigation mode local"
```

---

### Task 2: Carry current-step lane guidance into the provider

**Files:**

- Modify: `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp`
- Create: `src/core/aa/NavigationLaneModel.hpp`
- Create: `src/core/aa/NavigationLaneModel.cpp`
- Modify: `src/core/services/INavigationProvider.hpp`
- Modify: `src/core/aa/NavigationDataBridge.hpp`
- Modify: `src/core/aa/NavigationDataBridge.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/test_navigation_channel_handler.cpp`
- Create: `tests/test_navigation_lane_model.cpp`
- Modify: `tests/test_navigation_data_bridge.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: `NavigationNotification.steps[0].lanes[*].directions[*]` with raw
  `shape` and `is_recommended`.
- Produces in the protocol library:

```cpp
struct NavigationLaneDirectionData {
    int shape = 0;
    bool recommended = false;
};
struct NavigationLaneData {
    QList<NavigationLaneDirectionData> directions;
};
using NavigationLaneGuidance = QList<NavigationLaneData>;
```

- Produces in `INavigationProvider`:

```cpp
Q_PROPERTY(QAbstractItemModel* laneModel READ laneModel CONSTANT)
Q_PROPERTY(bool hasLaneGuidance READ hasLaneGuidance
           NOTIFY laneGuidanceChanged)
Q_PROPERTY(bool hasDistance READ hasDistance NOTIFY distanceChanged)

virtual QAbstractItemModel* laneModel() const { return nullptr; }
virtual bool hasLaneGuidance() const { return false; }
virtual bool hasDistance() const { return false; }
```

- Produces QML lane rows with role name `directions`; each direction is a
  `QVariantMap` containing stable string `shape` and Boolean `recommended`.

- [ ] **Step 1: Write failing handler tests for first-step lane snapshots**

Extend `testNotificationMultiStepWithLanes()` with a
`navigationLaneGuidanceChanged` spy. Require exactly one emitted physical lane
from step 1, not the lookahead lane from step 2:

```cpp
QSignalSpy laneSpy(
    &handler,
    &oaa::hu::NavigationChannelHandler::navigationLaneGuidanceChanged);
// send existing two-step fixture
QCOMPARE(laneSpy.count(), 1);
const auto lanes = qvariant_cast<oaa::hu::NavigationLaneGuidance>(
    laneSpy[0][0]);
QCOMPARE(lanes.size(), 1);
QCOMPARE(lanes[0].directions.size(), 1);
QCOMPARE(lanes[0].directions[0].shape, 1);
QVERIFY(lanes[0].directions[0].recommended);
```

Add a second test whose current lane contains two ordered directions and whose
next notification contains no lanes. Require order preservation and an empty
second snapshot.

- [ ] **Step 2: Write failing model and bridge tests**

Create `tests/test_navigation_lane_model.cpp` with fixtures that require:

- one row per physical lane;
- `directions` contains ordered maps;
- `replaceLanes()` resets rather than appends;
- `clear()` leaves zero rows.

Extend `tests/test_navigation_data_bridge.cpp` to emit a protocol snapshot and
require stable tokens:

```cpp
oaa::hu::NavigationLaneGuidance lanes{
    {{{1, false}, {5, true}}},
    {{{8, true}}},
};
emit handler.navigationLaneGuidanceChanged(lanes);
QVERIFY(bridge.hasLaneGuidance());
QCOMPARE(bridge.laneModel()->rowCount(), 2);
```

Read the first row's `directions` role and require `straight`, `normal_right`,
and the original recommendation flags. Emit `navigationStateChanged(false)`
and require an empty model. Add distance-presence tests requiring false by
default, true after a valid legacy or modern distance, and false after
deactivation.

- [ ] **Step 3: Run the focused tests and confirm missing interfaces fail**

```bash
cmake --build ~/builds/openauto-prodigy \
  --target test_navigation_channel_handler test_navigation_data_bridge \
  -j$(nproc)
```

Expected: compilation fails because the snapshot signal, model, and provider
properties do not exist.

- [ ] **Step 4: Implement the protocol-library snapshot signal**

Add the value types above to `NavigationChannelHandler.hpp`, declare
`Q_DECLARE_METATYPE(oaa::hu::NavigationLaneGuidance)`, and add:

```cpp
void navigationLaneGuidanceChanged(
    const oaa::hu::NavigationLaneGuidance& lanes);
```

Register the type in the handler constructor. In `handleNavStep()`, build the
snapshot from `msg.steps(0)` only. Append every lane even when its direction
list is empty, preserve direction order, then emit once after the existing
step signal. Emit an empty list when there is no first step or no lanes.

- [ ] **Step 5: Implement the app-owned lane model**

Define presentation types in `NavigationLaneModel.hpp`:

```cpp
struct LaneDirectionPresentation {
    QString shape;
    bool recommended = false;
};
using LanePresentation = QList<LaneDirectionPresentation>;
using LanePresentationList = QList<LanePresentation>;
```

`NavigationLaneModel` derives from `QAbstractListModel`, exposes
`DirectionsRole = Qt::UserRole + 1`, and implements:

```cpp
int rowCount(const QModelIndex& parent = {}) const override;
QVariant data(const QModelIndex& index, int role) const override;
QHash<int, QByteArray> roleNames() const override;
void replaceLanes(const LanePresentationList& lanes);
void clear();
```

`data()` returns a `QVariantList` of maps with exactly `shape` and
`recommended`. `replaceLanes()` uses one `beginResetModel()` / `endResetModel()`
pair so QML never observes a mixed snapshot.

- [ ] **Step 6: Extend and implement the provider bridge**

Add the default provider properties and `laneGuidanceChanged()` signal to
`INavigationProvider.hpp`. `NavigationDataBridge` owns a
`std::unique_ptr<NavigationLaneModel>`, overrides the three new accessors, and
connects the handler lane signal to a new slot.

Normalize all raw lane shapes explicitly:

```cpp
switch (shape) {
case 0: return QStringLiteral("unknown");
case 1: return QStringLiteral("straight");
case 2: return QStringLiteral("slight_left");
case 3: return QStringLiteral("slight_right");
case 4: return QStringLiteral("normal_left");
case 5: return QStringLiteral("normal_right");
case 6: return QStringLiteral("sharp_left");
case 7: return QStringLiteral("sharp_right");
case 8: return QStringLiteral("u_turn_left");
case 9: return QStringLiteral("u_turn_right");
default: return QStringLiteral("unknown_future");
}
```

Track `hasDistance_`: set it only for a nonnegative legacy distance or
nonempty modern display text; clear it with the existing navigation data.
Clear lanes and emit `laneGuidanceChanged()` on deactivation and on an empty
replacement snapshot.

Add `NavigationLaneModel.cpp` to `openauto-core` and register
`test_navigation_lane_model` in `tests/CMakeLists.txt`.

- [ ] **Step 7: Run the lane pipeline tests**

```bash
cmake --build ~/builds/openauto-prodigy \
  --target test_navigation_channel_handler test_navigation_lane_model \
           test_navigation_data_bridge -j$(nproc)
ctest --test-dir ~/builds/openauto-prodigy \
  -R '^(test_navigation_channel_handler|test_navigation_lane_model|test_navigation_data_bridge)$' \
  --output-on-failure
```

Expected: all focused tests pass.

- [ ] **Step 8: Commit the semantic lane pipeline**

```bash
git add libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp \
        libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp \
        src/core/services/INavigationProvider.hpp \
        src/core/aa/NavigationLaneModel.hpp src/core/aa/NavigationLaneModel.cpp \
        src/core/aa/NavigationDataBridge.hpp src/core/aa/NavigationDataBridge.cpp \
        src/CMakeLists.txt tests/test_navigation_channel_handler.cpp \
        tests/test_navigation_lane_model.cpp tests/test_navigation_data_bridge.cpp \
        tests/CMakeLists.txt
git commit -m "feat(aa): publish semantic lane guidance"
```

---

### Task 3: Add exhaustive maneuver and lane-direction glyph components

**Hardware-correction amendment (2026-07-31):** Material Symbols and the
original primary/badge interface below were replaced during live visual
acceptance. The accepted contract is the shared clean-room renderer described
by design sections 7-8: hero direction semantics reuse
`NavigationLaneDirectionGlyph`, distinct semantics use matching 24x24 Canvas
geometry, and compound physical lanes use one centered 0.90 optical scale.
The historical steps below record the initial implementation sequence.

**Files:**

- Create: `qml/widgets/NavigationManeuverGlyph.qml`
- Create: `qml/widgets/NavigationLaneDirectionGlyph.qml`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/test_aa_cluster_widget.cpp`

**Interfaces:**

- `NavigationManeuverGlyph` consumes `property int maneuverType` and exposes
  readonly `primaryGlyph`, `badgeGlyph`, `mirrorPrimary`, and `isFallback`.
- `NavigationLaneDirectionGlyph` consumes `property string shapeToken` and
  `property bool recommended`, and exposes readonly `glyph`,
  `drawNeutralStem`, and `isFallback`.
- Both render with caller-provided `color` and size; neither reads protocol or
  provider objects.

- [ ] **Step 1: Add exhaustive failing QML mapping tests**

Extend the temporary QML fixture helper in `test_aa_cluster_widget.cpp` to copy
the new component sources and stub `MaterialIcon.qml`.

Instantiate `NavigationManeuverGlyph.qml`, set each defined value
`0-29, 32-50`, and require `isFallback == false`. Require fallback for 30, 31,
negative one, and 999. Also require left/right pairs to differ by glyph,
mirroring, or badge composition.

Instantiate `NavigationLaneDirectionGlyph.qml` for:

```cpp
const QStringList defined{
    "unknown", "straight", "slight_left", "slight_right",
    "normal_left", "normal_right", "sharp_left", "sharp_right",
    "u_turn_left", "u_turn_right",
};
```

Require no fallback for those tokens, neutral-stem rendering for `unknown`,
and fallback for `unknown_future` and arbitrary strings.

- [ ] **Step 2: Run the QML test and confirm components are missing**

```bash
cmake --build ~/builds/openauto-prodigy --target test_aa_cluster_widget -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  -R '^test_aa_cluster_widget$' --output-on-failure
```

Expected: the mapping-component fixtures cannot load.

- [ ] **Step 3: Implement exhaustive maneuver mapping**

Use these bundled Material Symbols codepoints as the base vocabulary:

```qml
readonly property string genericNavigation: "\ue55d"
readonly property string depart: "\ue569"
readonly property string straight: "\ueb95"
readonly property string rampRight: "\ueb96"
readonly property string merge: "\ueb98"
readonly property string roundaboutLeft: "\ueb99"
readonly property string slightRight: "\ueb9a"
readonly property string rampLeft: "\ueb9c"
readonly property string forkLeft: "\ueba0"
readonly property string uTurnLeft: "\ueba1"
readonly property string uTurnRight: "\ueba2"
readonly property string roundaboutRight: "\ueba3"
readonly property string slightLeft: "\ueba4"
readonly property string turnLeft: "\ueba6"
readonly property string sharpLeft: "\ueba7"
readonly property string sharpRight: "\uebaa"
readonly property string turnRight: "\uebab"
readonly property string forkRight: "\uebac"
readonly property string destination: "\ue153"
readonly property string ferryBoat: "\ue532"
readonly property string ferryTrain: "\ue570"
```

Implement an explicit switch matching design section 7. Use ramp glyphs for
the ramp families, U-turn glyphs for 19/20, a mirrored merge base for the
side-specific merge pair, direction glyph plus destination/ferry badge for
side-specific destination and ferry values, and clockwise/right versus
counterclockwise/left roundabout glyphs. Case 0 is an intentional generic
mapping with `isFallback == false`; only undefined/future values set fallback.

- [ ] **Step 4: Implement exhaustive lane-direction mapping**

Map all ten stable shape tokens to the corresponding straight/slight/normal/
sharp/U-turn glyphs. `unknown` draws a vertical neutral stem without an
arrowhead and is not a fallback. `unknown_future` or any unrecognized token
uses the same safe neutral visual with `isFallback == true`.

Use `ThemeService` nowhere in these components; color is supplied by the
containing card so tests and future consumers can reuse them.

- [ ] **Step 5: Embed the components and run exhaustive mapping tests**

Add both QML files to `set_source_files_properties()` and `QML_FILES` in
`src/CMakeLists.txt`, following `AAClusterWidget.qml`'s resource aliases and
cache-generation policy. Run the Step 2 command. Expected: all defined values
resolve intentionally and fallback cases remain safe.

- [ ] **Step 6: Commit the presentation vocabulary**

```bash
git add qml/widgets/NavigationManeuverGlyph.qml \
        qml/widgets/NavigationLaneDirectionGlyph.qml \
        src/CMakeLists.txt tests/test_aa_cluster_widget.cpp
git commit -m "feat(ui): map complete navigation glyph set"
```

---

### Task 4: Render the native card and continuous lane band

**Files:**

- Create: `qml/widgets/NavigationLaneGuidanceBand.qml`
- Create: `qml/widgets/NativeNavigationCard.qml`
- Modify: `qml/widgets/AAClusterWidget.qml`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/test_aa_cluster_widget.cpp`

**Interfaces:**

- `NavigationLaneGuidanceBand` consumes `property var laneModel` and renders
  each row's `directions` role.
- `NativeNavigationCard` consumes `property QtObject navigationProvider` and
  `property bool aaConnected`.
- `AAClusterWidget` reads `ConfigService.value("video.secondary_display_content")`,
  listens to `configChanged(path, value)`, and binds `ProjectionStatus` states
  3 (`Connected`) and 4 (`Backgrounded`) as connected.

- [ ] **Step 1: Write failing runtime tests for local mode and sink ownership**

Extend `test_aa_cluster_widget.cpp` with QObject fakes:

```cpp
class FakeConfigService : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE QVariant value(const QString&) const { return mode_; }
    void setMode(const QString& mode) {
        mode_ = mode;
        emit configChanged("video.secondary_display_content", mode_);
    }
signals:
    void configChanged(const QString& path, const QVariant& value);
private:
    QString mode_ = QStringLiteral("map");
};
```

Expose a real `NavigationDataBridge`, fake handler, `FakeConfigService`, and a
projection-status object to the QML engine. Require:

- current-page Map mode owns the singleton video sink;
- changing to `turn_card` releases the sink without changing the display
  object or requiring a reconnect callback;
- `nativeNavigationCard` becomes visible immediately;
- changing back to `map` reclaims the sink through the existing bounded retry;
- invalid values fall back to Map;
- inactive and disconnected copy differ;
- an active fixture with lanes makes `laneGuidanceBand` visible.

Add source assertions forbidding `MouseArea`, `TapHandler`, per-lane
`Button`, and a `Flickable` inside `NavigationLaneGuidanceBand.qml`.

- [ ] **Step 2: Run the widget test and confirm native components are missing**

Run Task 3 Step 2. Expected: new object/state assertions fail.

- [ ] **Step 3: Implement the continuous lane band**

`NavigationLaneGuidanceBand.qml` owns one full-width surface and one horizontal
Repeater over `laneModel`. Each lane item has no background or outline. A
subtle separator tick appears between neighboring lanes. A nested Repeater
creates one `NavigationLaneDirectionGlyph` per direction, preserving order.

Use:

```qml
color: direction.recommended
       ? ThemeService.primary
       : ThemeService.onSurfaceVariant
opacity: direction.recommended ? 1.0 : 0.48
```

Compute lane and glyph width from the available band width. Keep the band
fixed-height; never shrink the main card text to fit lanes.

- [ ] **Step 4: Implement the approved native card**

`NativeNavigationCard.qml` selects three states:

```qml
readonly property bool routeActive:
    navigationProvider && navigationProvider.navActive
readonly property bool showGuidance: aaConnected && routeActive
```

- disconnected: `Connect Android Auto`;
- connected/inactive: `Start a route in Android Auto`;
- active: maneuver tile, distance only when `hasDistance`, road/instruction
  only when nonempty, and the lane band only when `hasLaneGuidance`.

At 1024x600 use the design targets directly; derive responsive sizes from
`root.height` with clamps that preserve the specified minimum hierarchy. Use
`ThemeService` tokens for every color. Do not hardcode cyan. Give key objects
stable names: `nativeNavigationCard`, `maneuverGlyph`, `distanceText`,
`roadText`, and `laneGuidanceBand`.

- [ ] **Step 5: Switch `AAClusterWidget` locally and preserve map behavior**

Add a normalized mutable property initialized from ConfigService:

```qml
property string dashboardNavigationMode: "map"
function normalizeDashboardNavigationMode(value) {
    return value === "turn_card" ? "turn_card" : "map"
}
```

On component completion and matching `ConfigService.configChanged`, update the
mode and call `syncSinkClaim()`. That function releases the sink whenever the
page is not current **or** the mode is not Map. The existing VideoOutput/crop
is visible only in Map mode. Native card visibility is Turn card mode.

Keep the bounded retry timer Map-only. Replace the old mode-agnostic transient
copy with neutral Map wording such as `Waiting for map` while retaining actual
display error/status text.

`NativeNavigationCard` receives:

```qml
navigationProvider: NavigationProvider
aaConnected: ProjectionStatus
             && (ProjectionStatus.projectionState === 3
                 || ProjectionStatus.projectionState === 4)
```

- [ ] **Step 6: Embed both components and run widget/provider tests**

Add resource aliases and QML module entries in `src/CMakeLists.txt`. Then run:

```bash
cmake --build ~/builds/openauto-prodigy \
  --target test_aa_cluster_widget test_navigation_data_bridge -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  -R '^(test_aa_cluster_widget|test_navigation_data_bridge)$' \
  --output-on-failure
```

Expected: local Map/Turn card switching, sink ownership, native states,
typography structure, and lane-band fixtures pass.

- [ ] **Step 7: Commit the native card integration**

```bash
git add qml/widgets/NavigationLaneGuidanceBand.qml \
        qml/widgets/NativeNavigationCard.qml \
        qml/widgets/AAClusterWidget.qml src/CMakeLists.txt \
        tests/test_aa_cluster_widget.cpp
git commit -m "feat(aa): render native dashboard turn card"
```

---

### Task 5: Reconcile current guidance and run the repository gate

**Files:**

- Modify: `src/core/aa/AGENTS.md`
- Modify: `docs/aa-protocol/aa-display-rendering.md`
- Modify: `docs/reference/config-schema.md`
- Modify: `docs/reference/settings-tree.md`
- Modify: `docs/engineering-backlog.md`
- Modify: `docs/session-handoffs.md`

**Interfaces:**

- Consumes: completed Stage 1 runtime behavior and focused test evidence.
- Produces: current documentation describing local presentation selection and
  an implementation SHA ready for Pi deployment.

- [ ] **Step 1: Update behavior documentation**

Document that:

- the auxiliary descriptor always uses `KEYCODE_NAVIGATION`;
- `video.secondary_display_content` is a local immediate presentation setting;
- Turn card uses provider semantics and a continuous lane band;
- switching does not reconnect AA;
- the map decoder remains live in Stage 1;
- Stage 2 semantic fields remain evidence-gated.

Remove the completed projected-dashboard fallback-wording entry from
`docs/engineering-backlog.md`. Leave the settings-striping and internal naming
entries untouched. Add a handoff entry with focused commands and results; do
not state exact test counts.

- [ ] **Step 2: Run docs and diff checks**

```bash
python3 scripts/check-doc-links.py --scope tracked-live
git diff --check
```

Expected: zero broken links and no whitespace errors.

- [ ] **Step 3: Commit current guidance**

```bash
git add src/core/aa/AGENTS.md docs/aa-protocol/aa-display-rendering.md \
        docs/reference/config-schema.md docs/reference/settings-tree.md \
        docs/engineering-backlog.md docs/session-handoffs.md
git commit -m "docs(aa): document native dashboard guidance"
```

- [ ] **Step 4: Run the full native gate once on the final code tree**

```bash
cmake --build ~/builds/openauto-prodigy -j$(nproc)
cmake --build ~/builds/openauto-prodigy \
  --target openauto-prodigy -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure
```

Expected: the native build, explicit app target, and complete CTest suite pass.
Do not proceed to ARM deployment on a red native tree.

- [ ] **Step 5: Cross-build the embedded QML binary**

```bash
./cross-build.sh
sha256sum build-pi/src/openauto-prodigy
```

Expected: ARM cross-build passes and records the local binary hash.

---

### Task 6: Deploy, validate each hardware case independently, and review

**Files:**

- Modify after evidence: `docs/session-handoffs.md`
- Modify after acceptance: `docs/roadmap-current.md`
- Modify after acceptance: `docs/INDEX.md`
- Move after completion:
  `docs/plans/2026-07-28-native-dashboard-turn-card-design.md` to
  `docs/archive/plans/`
- Move after completion:
  `docs/plans/2026-07-28-native-dashboard-turn-card-stage-1-plan.md` to
  `docs/archive/plans/`

**Interfaces:**

- Consumes: green native/CTest/ARM tree and available Pi/phones.
- Produces: hardware-accepted SHA, one bounded Fable review, Stage 1 closure,
  and a precise Stage 2 evidence handoff.

- [ ] **Step 1: Deploy the exact ARM binary and restart Prodigy**

```bash
rsync -av build-pi/src/openauto-prodigy \
  matt@192.168.1.149:~/openauto-prodigy/build/src/
ssh matt@192.168.1.149 \
  'sudo systemctl restart openauto-prodigy.service'
sha256sum build-pi/src/openauto-prodigy
ssh matt@192.168.1.149 \
  'sha256sum ~/openauto-prodigy/build/src/openauto-prodigy; systemctl show openauto-prodigy.service -p MainPID -p NRestarts -p ActiveState'
```

Expected: local and remote hashes match, service is active, and one responsive
process owns the service.

- [ ] **Step 2: Validate Map and local switching without a timer**

Direct one user action at a time and wait for confirmation after each:

1. Connect AA and confirm Map renders.
2. Start music and an active route.
3. Select Turn card and confirm the change is immediate, AA does not reconnect,
   and audio does not skip.
4. Select Map and confirm immediate restoration.
5. Return to Turn card for semantic validation.

Do not batch these on a timer or claim simultaneous Map/card visibility on the
one-screen rig.

- [ ] **Step 3: Validate card readability, state clearing, and maneuvers**

Independently confirm:

- distance, unit, road/instruction, and header are readable at the installed
  viewing distance;
- theme colors and contrast are correct;
- disconnected and no-route states use the approved friendly copy;
- stopping navigation clears the maneuver and lanes;
- left, right, straight, ramp/fork/merge, roundabout, U-turn, and destination
  examples render correctly when practical routes produce them.

Automated exhaustive tests remain the acceptance evidence for known maneuver
values that cannot be forced safely in one drive.

- [ ] **Step 4: Validate live lane guidance**

Select a route/location that makes the phone publish lane data. Compare the
native card with the phone/AA guidance and confirm, one observation at a time:

- physical lane order;
- every direction within each lane;
- every recommended direction;
- muted alternatives remain visible;
- one continuous roadway band with subtle ticks and no button-like cells;
- the main 92-100 px distance hierarchy remains unchanged when lanes appear.

Repeat the core route/lane case on Pixel and Samsung when available. If a phone
does not publish lanes for the chosen route, record absence as delivery
evidence and choose another route; do not fabricate a rendering pass.

- [ ] **Step 5: Record the accepted SHA and run the one major review gate**

After the user accepts the live behavior, record `git rev-parse HEAD` as the
accepted tree. The newly authorized feature permits one gate reset:

```bash
bash scripts/review-gate.sh --reset
bash scripts/review-gate.sh --author codex --major \
  --base 4439c29 --accepted <accepted-sha>
```

Adjudicate every finding against a supported production entry point and the
accepted-tree rule. If remediation is needed, run the applicable native/CTest/
ARM and hardware checks again, then use only the one allowed remediation
review. Do not begin a third review cycle without explicit authorization.

- [ ] **Step 6: Close Stage 1 and preserve Stage 2**

Update the handoff with verification, hardware observations, accepted SHA,
and review counts. Mark Stage 1 complete in the roadmap. Keep Stage 2 as the
next evidence-gated item with the exact fields observed or still unobserved.

Set both design and plan status to `COMPLETED 2026-07-28`, move both files to
`docs/archive/plans/`, update `docs/INDEX.md` links, then run:

```bash
python3 scripts/check-doc-links.py --scope tracked-live
git diff --check
```

Commit the closure atomically:

```bash
git add -A docs/plans docs/archive/plans docs/roadmap-current.md \
        docs/INDEX.md docs/session-handoffs.md
git commit -m "docs(aa): close native turn-card stage 1"
```

Stop with Stage 2 unimplemented. Its implementation plan must be written from
the recorded live-delivery evidence under the approved design rather than from
speculation.
