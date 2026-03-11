# Home Screen Polish Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add glass-effect widget containers with per-pane opacity, a long-press context menu, and a force-dark-mode override for the HU interface.

**Architecture:** Three independent features. (1) Per-pane opacity stored in `WidgetPlacement.config["opacity"]` and exposed to QML via `WidgetPlacementModel`. Widget QML backgrounds use that opacity value. (2) Long-press on a WidgetHost now opens a context popup (change/opacity/clear) instead of directly opening the picker. (3) ThemeService gains a `forceDarkMode` property that overrides `nightMode` for QML rendering while leaving AA's SensorChannelHandler reading the real provider state.

**Tech Stack:** Qt 6.4+/QML, C++17, yaml-cpp, QTest

---

### Task 1: Add pane opacity to WidgetPlacement config and persistence

**Files:**
- Modify: `src/core/YamlConfig.cpp:814-837` (setWidgetPlacements — persist config map)
- Modify: `src/core/YamlConfig.cpp:774-812` (widgetPlacements — read config map)
- Modify: `src/ui/WidgetPlacementModel.hpp`
- Modify: `src/ui/WidgetPlacementModel.cpp`
- Test: `tests/test_widget_placement_model.cpp`

**Step 1: Write failing tests for pane opacity**

Add to `tests/test_widget_placement_model.cpp`:

```cpp
void TestWidgetPlacementModel::testPaneOpacity();
void TestWidgetPlacementModel::testSetPaneOpacity();
void TestWidgetPlacementModel::testDefaultPaneOpacity();
```

```cpp
void TestWidgetPlacementModel::testDefaultPaneOpacity() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    // Default opacity should be 0.25
    QCOMPARE(model.paneOpacity("main"), 0.25);
}

void TestWidgetPlacementModel::testSetPaneOpacity() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    QSignalSpy spy(&model, &oap::WidgetPlacementModel::paneChanged);
    model.setPaneOpacity("main", 0.7);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.paneOpacity("main"), 0.7);

    // Verify it's stored in the placement config
    auto placement = model.placementForPane("main");
    QVERIFY(placement.has_value());
    QCOMPARE(placement->config.value("opacity").toDouble(), 0.7);
}

void TestWidgetPlacementModel::testPaneOpacity() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    p.config["opacity"] = 0.5;
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    QCOMPARE(model.paneOpacity("main"), 0.5);
}
```

**Step 2: Run tests to verify they fail**

Run: `cd build && cmake --build . --target test_widget_placement_model -j$(nproc) && ctest -R test_widget_placement_model -V`
Expected: FAIL — `paneOpacity` and `setPaneOpacity` don't exist yet.

**Step 3: Implement paneOpacity/setPaneOpacity in WidgetPlacementModel**

In `src/ui/WidgetPlacementModel.hpp`, add Q_INVOKABLEs:

```cpp
Q_INVOKABLE double paneOpacity(const QString& paneId) const;
Q_INVOKABLE void setPaneOpacity(const QString& paneId, double opacity);
```

In `src/ui/WidgetPlacementModel.cpp`:

```cpp
double WidgetPlacementModel::paneOpacity(const QString& paneId) const {
    auto placement = placementForPane(paneId);
    if (placement && placement->config.contains("opacity"))
        return placement->config.value("opacity").toDouble();
    return 0.25; // default
}

void WidgetPlacementModel::setPaneOpacity(const QString& paneId, double opacity) {
    for (auto& p : placements_) {
        if (p.pageId == activePageId_ && p.paneId == paneId) {
            p.config["opacity"] = qBound(0.0, opacity, 1.0);
            emit placementsChanged();
            emit paneChanged(paneId);
            return;
        }
    }
}
```

**Step 4: Update YAML persistence for config map**

In `src/core/YamlConfig.cpp`, update `widgetPlacements()` (around line 784) to read config:

```cpp
// After reading paneId, add:
if (node["config"] && node["config"].IsMap()) {
    for (auto it = node["config"].begin(); it != node["config"].end(); ++it) {
        QString key = QString::fromStdString(it->first.as<std::string>());
        if (it->second.IsScalar()) {
            // Try double first, then string
            try {
                double d = it->second.as<double>();
                p.config[key] = d;
            } catch (...) {
                p.config[key] = QString::fromStdString(it->second.as<std::string>());
            }
        }
    }
}
```

Update `setWidgetPlacements()` (around line 833) to write config:

```cpp
// After writing paneId, add:
if (!p.config.isEmpty()) {
    YAML::Node configNode;
    for (auto it = p.config.begin(); it != p.config.end(); ++it) {
        std::string key = it.key().toStdString();
        if (it.value().typeId() == QMetaType::Double || it.value().typeId() == QMetaType::Float)
            configNode[key] = it.value().toDouble();
        else
            configNode[key] = it.value().toString().toStdString();
    }
    n["config"] = configNode;
}
```

**Step 5: Run tests to verify they pass**

Run: `cd build && cmake --build . --target test_widget_placement_model -j$(nproc) && ctest -R test_widget_placement_model -V`
Expected: PASS

**Step 6: Commit**

```bash
git add src/ui/WidgetPlacementModel.hpp src/ui/WidgetPlacementModel.cpp src/core/YamlConfig.cpp tests/test_widget_placement_model.cpp
git commit -m "feat(widget): add per-pane opacity config with YAML persistence"
```

---

### Task 2: Glass-effect widget card backgrounds

**Files:**
- Modify: `qml/components/WidgetHost.qml`
- Modify: `qml/widgets/ClockWidget.qml`
- Modify: `qml/widgets/AAStatusWidget.qml`
- Modify: `qml/widgets/NowPlayingWidget.qml`

**Step 1: Update WidgetHost to render glass background with per-pane opacity**

Replace the empty-state Rectangle and add a glass card background in `qml/components/WidgetHost.qml`:

```qml
import QtQuick

Item {
    id: widgetHost

    property string paneId: ""
    property url widgetSource: ""
    property bool isMainPane: paneId === "main"
    property real paneOpacity: WidgetPlacementModel.paneOpacity(paneId)

    signal longPressed()

    // Glass card background — always visible
    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainer
        opacity: widgetHost.paneOpacity
    }

    // Subtle border for edge definition
    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: "transparent"
        border.width: 1
        border.color: ThemeService.outlineVariant
        opacity: 0.3
    }

    Loader {
        id: widgetLoader
        anchors.fill: parent
        source: widgetHost.widgetSource
        asynchronous: false

        onStatusChanged: {
            if (status === Loader.Error)
                console.warn("WidgetHost: failed to load", widgetHost.widgetSource)
        }
    }

    // Empty state hint
    NormalText {
        anchors.centerIn: parent
        text: "Hold to add widget"
        font.pixelSize: UiMetrics.fontSmall
        color: ThemeService.onSurfaceVariant
        opacity: 0.5
        visible: !widgetHost.widgetSource.toString()
    }

    // Long-press detector — sits behind widget content
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: widgetHost.longPressed()
    }
}
```

**Step 2: Remove opaque backgrounds from widget QML files**

Each widget currently has its own `Rectangle { color: ThemeService.surfaceContainer }` as a background. Since WidgetHost now provides the glass background, remove or make transparent the widget-level backgrounds.

In `qml/widgets/ClockWidget.qml`, change the root Rectangle:

```qml
Rectangle {
    anchors.fill: parent
    radius: UiMetrics.radius
    color: "transparent"  // WidgetHost provides glass background
    // ... children unchanged
}
```

Same change in `qml/widgets/AAStatusWidget.qml` and `qml/widgets/NowPlayingWidget.qml` — set `color: "transparent"` on their root Rectangle.

**Step 3: Refresh paneOpacity when placement changes**

In WidgetHost, add a Connections block to update opacity when the pane changes:

```qml
Connections {
    target: WidgetPlacementModel
    function onPaneChanged(changedPaneId) {
        if (changedPaneId === widgetHost.paneId)
            widgetHost.paneOpacity = WidgetPlacementModel.paneOpacity(widgetHost.paneId)
    }
}
```

**Step 4: Build and verify on VM**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Clean compile.

**Step 5: Commit**

```bash
git add qml/components/WidgetHost.qml qml/widgets/ClockWidget.qml qml/widgets/AAStatusWidget.qml qml/widgets/NowPlayingWidget.qml
git commit -m "feat(widget): glass-effect card backgrounds with per-pane opacity"
```

---

### Task 3: Widget context popup (replaces direct picker launch)

**Files:**
- Create: `qml/components/WidgetContextMenu.qml`
- Modify: `qml/applications/home/HomeMenu.qml`
- Modify: `CMakeLists.txt` (add QML file to resource list)

**Step 1: Create WidgetContextMenu component**

Create `qml/components/WidgetContextMenu.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: contextMenu
    anchors.fill: parent

    property string targetPaneId: ""
    property real anchorX: 0
    property real anchorY: 0

    signal changeRequested()
    signal opacityChanged(real value)
    signal clearRequested()
    signal dismissed()

    property bool _opacityExpanded: false
    property real _currentOpacity: WidgetPlacementModel.paneOpacity(targetPaneId)

    // Scrim
    Rectangle {
        anchors.fill: parent
        color: ThemeService.scrim
        opacity: 0.4
        MouseArea {
            anchors.fill: parent
            onClicked: contextMenu.dismissed()
        }
    }

    // Floating card positioned near the pane
    Rectangle {
        id: card
        width: Math.min(UiMetrics.tileW * 0.8, parent.width * 0.4)
        height: cardContent.implicitHeight + UiMetrics.spacing * 2
        x: Math.max(UiMetrics.spacing, Math.min(anchorX - width / 2, parent.width - width - UiMetrics.spacing))
        y: Math.max(UiMetrics.spacing, Math.min(anchorY - height / 2, parent.height - height - UiMetrics.spacing))
        radius: UiMetrics.radiusLarge
        color: ThemeService.surfaceContainerHighest
        opacity: 0.92

        // Border
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: ThemeService.outlineVariant
            opacity: 0.3
        }

        // Scale-in animation
        scale: 0.8
        Component.onCompleted: scale = 1.0
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        ColumnLayout {
            id: cardContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: UiMetrics.spacing
            spacing: 0

            // Change Widget option
            Rectangle {
                Layout.fillWidth: true
                height: UiMetrics.touchMin
                radius: UiMetrics.radiusSmall
                color: changeMa.pressed ? ThemeService.primaryContainer : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.spacing
                    anchors.rightMargin: UiMetrics.spacing
                    spacing: UiMetrics.spacing

                    MaterialIcon {
                        icon: "\ue41f"  // swap_horiz
                        size: UiMetrics.iconSmall
                        color: ThemeService.onSurface
                    }
                    NormalText {
                        text: "Change Widget"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurface
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: changeMa
                    anchors.fill: parent
                    onClicked: contextMenu.changeRequested()
                }
            }

            // Opacity option
            Rectangle {
                Layout.fillWidth: true
                height: _opacityExpanded ? UiMetrics.touchMin * 2 : UiMetrics.touchMin
                radius: UiMetrics.radiusSmall
                color: opacityMa.pressed ? ThemeService.primaryContainer : "transparent"
                clip: true

                Behavior on height { NumberAnimation { duration: 150 } }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.touchMin
                        Layout.leftMargin: UiMetrics.spacing
                        Layout.rightMargin: UiMetrics.spacing
                        spacing: UiMetrics.spacing

                        MaterialIcon {
                            icon: "\ue3a8"  // opacity
                            size: UiMetrics.iconSmall
                            color: ThemeService.onSurface
                        }
                        NormalText {
                            text: "Opacity"
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.onSurface
                            Layout.fillWidth: true
                        }
                        NormalText {
                            text: Math.round(contextMenu._currentOpacity * 100) + "%"
                            font.pixelSize: UiMetrics.fontSmall
                            color: ThemeService.onSurfaceVariant
                        }
                    }

                    // Slider (visible when expanded)
                    Slider {
                        Layout.fillWidth: true
                        Layout.leftMargin: UiMetrics.spacing
                        Layout.rightMargin: UiMetrics.spacing
                        Layout.preferredHeight: UiMetrics.touchMin
                        visible: _opacityExpanded
                        from: 0; to: 1; stepSize: 0.05
                        value: contextMenu._currentOpacity
                        onMoved: {
                            contextMenu._currentOpacity = value
                            WidgetPlacementModel.setPaneOpacity(contextMenu.targetPaneId, value)
                            contextMenu.opacityChanged(value)
                        }
                    }
                }

                MouseArea {
                    id: opacityMa
                    anchors.fill: parent
                    anchors.bottomMargin: _opacityExpanded ? UiMetrics.touchMin : 0
                    onClicked: _opacityExpanded = !_opacityExpanded
                }
            }

            // Clear option
            Rectangle {
                Layout.fillWidth: true
                height: UiMetrics.touchMin
                radius: UiMetrics.radiusSmall
                color: clearMa.pressed ? ThemeService.errorContainer : "transparent"
                visible: WidgetPlacementModel.qmlComponentForPane(contextMenu.targetPaneId).toString() !== ""

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.spacing
                    anchors.rightMargin: UiMetrics.spacing
                    spacing: UiMetrics.spacing

                    MaterialIcon {
                        icon: "\ue5cd"  // close
                        size: UiMetrics.iconSmall
                        color: ThemeService.error
                    }
                    NormalText {
                        text: "Clear"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.error
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: clearMa
                    anchors.fill: parent
                    onClicked: contextMenu.clearRequested()
                }
            }
        }
    }
}
```

**Step 2: Add WidgetContextMenu.qml to CMake resource list**

Find the QML resource list in the main `CMakeLists.txt` and add:
```
qml/components/WidgetContextMenu.qml
```

Ensure `QT_QML_SKIP_CACHEGEN TRUE` is set for it (same as other widget QMLs).

**Step 3: Update HomeMenu to use context menu flow**

Replace the direct-to-picker long-press flow in `qml/applications/home/HomeMenu.qml`. The widgetPicker Loader (lines 127-154) becomes two Loaders: one for the context menu, one for the picker. The picker opens only when "Change Widget" is selected from the context menu.

Replace the existing `widgetPicker` Loader with:

```qml
// Widget context menu overlay
Loader {
    id: contextMenuLoader
    anchors.fill: parent
    active: false
    z: 50
    property string targetPaneId: ""
    property real anchorX: 0
    property real anchorY: 0

    function openForPane(paneId) {
        targetPaneId = paneId
        // Position near center of the pane
        var pane = null
        if (homeScreen.isLandscape) {
            if (paneId === "main") pane = mainPaneLandscape
            else if (paneId === "sub1") pane = sub1PaneLandscape
            else if (paneId === "sub2") pane = sub2PaneLandscape
        } else {
            if (paneId === "main") pane = mainPanePortrait
            else if (paneId === "sub1") pane = sub1PanePortrait
            else if (paneId === "sub2") pane = sub2PanePortrait
        }
        if (pane) {
            var mapped = pane.mapToItem(homeScreen, pane.width / 2, pane.height / 2)
            anchorX = mapped.x
            anchorY = mapped.y
        }
        active = true
    }

    sourceComponent: WidgetContextMenu {
        targetPaneId: contextMenuLoader.targetPaneId
        anchorX: contextMenuLoader.anchorX
        anchorY: contextMenuLoader.anchorY
        onChangeRequested: {
            contextMenuLoader.active = false
            widgetPicker.openForPane(contextMenuLoader.targetPaneId)
        }
        onClearRequested: {
            WidgetPlacementModel.clearPane(contextMenuLoader.targetPaneId)
            contextMenuLoader.active = false
        }
        onDismissed: contextMenuLoader.active = false
    }
}

// Widget picker overlay (opened from context menu)
Loader {
    id: widgetPicker
    anchors.fill: parent
    active: false
    z: 51
    property string targetPaneId: ""

    function openForPane(paneId) {
        targetPaneId = paneId
        var sizeFlag = (paneId === "main") ? 1 : 2
        WidgetPickerModel.filterForSize(sizeFlag)
        active = true
    }

    sourceComponent: WidgetPicker {
        targetPaneId: widgetPicker.targetPaneId
        onWidgetSelected: function(widgetId) {
            WidgetPlacementModel.swapWidget(widgetPicker.targetPaneId, widgetId)
            widgetPicker.active = false
        }
        onCleared: {
            WidgetPlacementModel.clearPane(widgetPicker.targetPaneId)
            widgetPicker.active = false
        }
        onCancelled: widgetPicker.active = false
    }
}
```

Update all `onLongPressed` handlers to call `contextMenuLoader.openForPane(...)` instead of `widgetPicker.openForPane(...)`.

**Step 4: Remove "Clear" from WidgetPicker**

In `qml/components/WidgetPicker.qml`, remove the Clear button (lines 50-79) since clearing is now in the context menu. The picker becomes a pure widget selection list.

**Step 5: Build and verify on VM**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Clean compile.

**Step 6: Commit**

```bash
git add qml/components/WidgetContextMenu.qml qml/applications/home/HomeMenu.qml qml/components/WidgetPicker.qml CMakeLists.txt
git commit -m "feat(widget): long-press context menu with opacity slider and change/clear"
```

---

### Task 4: Force dark mode — ThemeService changes

**Files:**
- Modify: `src/core/services/ThemeService.hpp`
- Modify: `src/core/services/ThemeService.cpp`
- Test: `tests/test_theme_service.cpp`

**Step 1: Write failing tests**

Add to `tests/test_theme_service.cpp`:

```cpp
void forceDarkModeOverridesNightMode()
{
    oap::ThemeService service;
    service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

    QVERIFY(!service.nightMode());
    service.setForceDarkMode(true);
    QVERIFY(service.nightMode());

    // Even setting night mode off doesn't override force dark
    service.setNightMode(false);
    QVERIFY(service.nightMode());

    // Disabling force dark restores real state
    service.setForceDarkMode(false);
    QVERIFY(!service.nightMode());
}

void forceDarkModeRealNightState()
{
    oap::ThemeService service;
    service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

    service.setForceDarkMode(true);
    QVERIFY(service.nightMode()); // QML sees dark

    // Real state is still day
    QVERIFY(!service.realNightMode());

    // Set real night via setNightMode
    service.setNightMode(true);
    QVERIFY(service.realNightMode());

    // QML still sees dark (unchanged)
    QVERIFY(service.nightMode());
}
```

**Step 2: Run tests to verify they fail**

Run: `cd build && cmake --build . --target test_theme_service -j$(nproc) && ctest -R test_theme_service -V`
Expected: FAIL — `setForceDarkMode` and `realNightMode` don't exist.

**Step 3: Implement force dark mode**

In `src/core/services/ThemeService.hpp`, add:

```cpp
Q_PROPERTY(bool forceDarkMode READ forceDarkMode WRITE setForceDarkMode NOTIFY forceDarkModeChanged)

// In public section:
bool forceDarkMode() const { return forceDarkMode_; }
void setForceDarkMode(bool force);
bool realNightMode() const { return nightMode_; }

// In signals:
void forceDarkModeChanged();

// In private:
bool forceDarkMode_ = false;
```

Update `nightMode()` and `setNightMode()` in `ThemeService.cpp`:

```cpp
bool ThemeService::nightMode() const {
    return forceDarkMode_ || nightMode_;
}

void ThemeService::setNightMode(bool night) {
    if (nightMode_ == night) return;
    nightMode_ = night;
    emit modeChanged();
    emit colorsChanged();
}

void ThemeService::setForceDarkMode(bool force) {
    if (forceDarkMode_ == force) return;
    forceDarkMode_ = force;
    emit forceDarkModeChanged();
    emit modeChanged();
    emit colorsChanged();
}
```

Note: `nightMode()` was previously `{ return nightMode_; }` inline in the header. Move it to the .cpp file and update the header to just declare it.

**Step 4: Run tests to verify they pass**

Run: `cd build && cmake --build . --target test_theme_service -j$(nproc) && ctest -R test_theme_service -V`
Expected: PASS

**Step 5: Commit**

```bash
git add src/core/services/ThemeService.hpp src/core/services/ThemeService.cpp tests/test_theme_service.cpp
git commit -m "feat(theme): add forceDarkMode override for HU interface"
```

---

### Task 5: Wire force dark mode to config and main.cpp

**Files:**
- Modify: `src/main.cpp` (read config, apply force dark on startup)
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp` (use realNightMode for AA)

**Step 1: Read force_dark_mode from config on startup**

In `src/main.cpp`, after the theme is loaded and wallpaper is set (around line 203), add:

```cpp
// Apply force-dark-mode override (HU only — AA uses real night mode)
QVariant forceDark = yamlConfig->valueByPath("display.force_dark_mode");
if (forceDark.isValid())
    themeService->setForceDarkMode(forceDark.toBool());
else
    themeService->setForceDarkMode(true); // default: on
```

**Step 2: Update AA orchestrator to use realNightMode**

In `src/core/aa/AndroidAutoOrchestrator.cpp`, find the night mode provider connection (around line 549-550):

```cpp
connect(nightProvider_.get(), &NightModeProvider::nightModeChanged,
        &sensorHandler_, &oaa::hu::SensorChannelHandler::pushNightMode);
```

This is already correct — the night mode provider sends its own signal directly to SensorChannelHandler, not going through ThemeService. The provider's state is independent of forceDarkMode. No change needed here.

However, verify that the ThemeService connection also works. In main.cpp, find where the night mode provider connects to ThemeService. Check `AndroidAutoOrchestrator` for any `themeService->setNightMode()` calls. If the orchestrator calls `setNightMode` on the ThemeService, that's fine — `setNightMode` sets the real state, and `nightMode()` returns force || real.

**Step 3: Update toggleMode for force-dark session override**

In `ThemeService.cpp`, update `toggleMode()`:

```cpp
void ThemeService::toggleMode()
{
    if (forceDarkMode_) {
        // Temporarily disable force-dark for this session
        setForceDarkMode(false);
        return;
    }
    setNightMode(!nightMode_);
}
```

**Step 4: Set default in YamlConfig defaults**

In `src/core/YamlConfig.cpp`, find where defaults are set (the `setDefaults()` or similar function). Add:

```cpp
root_["display"]["force_dark_mode"] = true;
```

**Step 5: Build and run full test suite**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass.

**Step 6: Commit**

```bash
git add src/main.cpp src/core/services/ThemeService.cpp src/core/YamlConfig.cpp
git commit -m "feat(theme): wire force dark mode to config, default on"
```

---

### Task 6: Display Settings UI — force dark toggle

**Files:**
- Modify: `qml/applications/settings/DisplaySettings.qml`

**Step 1: Add toggle to Day/Night Mode section**

In `DisplaySettings.qml`, after the "Day / Night Mode" SectionHeader (line 270), add:

```qml
SettingsToggle {
    label: "Always Use Dark Mode"
    configPath: "display.force_dark_mode"
    onToggled: ThemeService.forceDarkMode = checked
}
```

**Step 2: Dim the night mode source controls when force dark is on**

Wrap the existing source picker, time fields, and GPIO controls in an Item with opacity:

```qml
Item {
    Layout.fillWidth: true
    implicitHeight: nightModeControls.implicitHeight
    opacity: ThemeService.forceDarkMode ? 0.4 : 1.0
    enabled: !ThemeService.forceDarkMode
    Behavior on opacity { NumberAnimation { duration: 200 } }

    ColumnLayout {
        id: nightModeControls
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: UiMetrics.spacing

        FullScreenPicker { /* existing source picker */ }
        ReadOnlyField { /* existing day start */ }
        ReadOnlyField { /* existing night start */ }
        SettingsSlider { /* existing GPIO pin */ }
        SettingsToggle { /* existing GPIO active high */ }
    }
}
```

**Step 3: Build and verify**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Clean compile.

**Step 4: Commit**

```bash
git add qml/applications/settings/DisplaySettings.qml
git commit -m "feat(settings): add 'Always Use Dark Mode' toggle with dimmed controls"
```

---

### Task 7: Integration test and push to Pi

**Step 1: Run full test suite**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass (including new tests from Tasks 1 and 4).

**Step 2: Push to dev branch and pull on Pi**

```bash
git push origin dev/0.5.2
ssh matt@192.168.1.152 'cd ~/openauto-prodigy && git pull && cd build && cmake --build . -j3'
```

**Step 3: Restart and take screenshot**

```bash
ssh matt@192.168.1.152 'sudo systemctl restart openauto-prodigy.service'
# Wait a few seconds for startup
ssh matt@192.168.1.152 'WAYLAND_DISPLAY=wayland-0 grim -o HDMI-A-1 /tmp/polish-test.png'
scp matt@192.168.1.152:/tmp/polish-test.png /tmp/polish-test.png
```

**Step 4: Verify visually**

- Home screen should show glass-effect cards with wallpaper visible through
- Dark mode should be active by default
- Long-press a widget pane → context menu should appear (not picker directly)
- Opacity slider in context menu should live-preview the pane transparency
- "Change Widget" should open the picker
- "Clear" should remove the widget

**Step 5: Commit any fixes needed, final push**

```bash
git push origin dev/0.5.2
```
