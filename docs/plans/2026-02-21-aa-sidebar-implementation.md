# AA Sidebar Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a configurable sidebar alongside Android Auto video using protocol-level margin negotiation, with volume and home quick-action buttons.

**Architecture:** Calculate `margin_width`/`margin_height` from sidebar config and pass to phone during service discovery. Phone renders AA UI in a smaller centered region. QML crops the margin black bars and displays a sidebar strip with evdev-driven touch zones for volume/home controls.

**Tech Stack:** C++17, Qt 6 QML, yaml-cpp, aasdk protobuf, Linux evdev

**Design doc:** `docs/plans/2026-02-21-aa-sidebar-design.md`

---

### Task 1: Add sidebar config fields to YamlConfig

**Files:**
- Modify: `src/core/YamlConfig.hpp:52-58` (add after video section)
- Modify: `src/core/YamlConfig.cpp:42-44` (add defaults after video.dpi)
- Modify: `tests/test_yaml_config.cpp` (add test for sidebar defaults)
- Modify: `tests/test_config_key_coverage.cpp:52-54` (add sidebar keys to coverage)
- Modify: `tests/data/test_config.yaml` (add sidebar section)

**Step 1: Write the failing test**

Add to `tests/test_yaml_config.cpp` — new test slot declaration and implementation:

In the class declaration (after `testSetValueByPathRejectsUnknown`):
```cpp
void testSidebarDefaults();
```

Implementation:
```cpp
void TestYamlConfig::testSidebarDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.sidebarEnabled(), false);
    QCOMPARE(config.sidebarWidth(), 150);
    QCOMPARE(config.sidebarPosition(), QString("right"));
}
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_yaml_config -j$(nproc) 2>&1 | tail -5`
Expected: Compile error — `sidebarEnabled()` not declared

**Step 3: Add config defaults and accessors**

In `src/core/YamlConfig.hpp`, add after line 58 (after `setVideoDpi`):
```cpp
    // Video — sidebar
    bool sidebarEnabled() const;
    void setSidebarEnabled(bool v);
    int sidebarWidth() const;
    void setSidebarWidth(int v);
    QString sidebarPosition() const;
    void setSidebarPosition(const QString& v);
```

In `src/core/YamlConfig.cpp`, add defaults after line 44 (`root_["video"]["dpi"] = 140;`):
```cpp
    root_["video"]["sidebar"]["enabled"] = false;
    root_["video"]["sidebar"]["width"] = 150;
    root_["video"]["sidebar"]["position"] = "right";
```

Add getter/setter implementations (follow existing pattern, e.g. after the videoDpi getter/setter):
```cpp
bool YamlConfig::sidebarEnabled() const {
    return root_["video"]["sidebar"]["enabled"].as<bool>(false);
}
void YamlConfig::setSidebarEnabled(bool v) {
    root_["video"]["sidebar"]["enabled"] = v;
}

int YamlConfig::sidebarWidth() const {
    return root_["video"]["sidebar"]["width"].as<int>(150);
}
void YamlConfig::setSidebarWidth(int v) {
    root_["video"]["sidebar"]["width"] = v;
}

QString YamlConfig::sidebarPosition() const {
    return QString::fromStdString(root_["video"]["sidebar"]["position"].as<std::string>("right"));
}
void YamlConfig::setSidebarPosition(const QString& v) {
    root_["video"]["sidebar"]["position"] = v.toStdString();
}
```

**Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . --target test_yaml_config -j$(nproc) && ctest -R test_yaml_config --output-on-failure`
Expected: All tests PASS including `testSidebarDefaults`

**Step 5: Update config key coverage test**

In `tests/test_config_key_coverage.cpp`, add to the `keys` list in `testAllRuntimeKeys()` (after `"video.dpi"` around line 54):
```cpp
        "video.sidebar.enabled",
        "video.sidebar.width",
        "video.sidebar.position",
```

In `tests/data/test_config.yaml`, add sidebar section under `video:` (after `dpi: 160`):
```yaml
  sidebar:
    enabled: true
    width: 200
    position: "left"
```

**Step 6: Run all tests**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All 17+ tests PASS

**Step 7: Commit**

```bash
git add src/core/YamlConfig.hpp src/core/YamlConfig.cpp tests/test_yaml_config.cpp tests/test_config_key_coverage.cpp tests/data/test_config.yaml
git commit -m "feat: add sidebar config fields to YamlConfig (enabled, width, position)"
```

---

### Task 2: Calculate and apply margins in VideoService

**Files:**
- Modify: `src/core/aa/VideoService.cpp:55-97` (fillFeatures margin math)

**Step 1: Write margin calculation logic**

Replace `VideoService::fillFeatures()` lines 79-91 (the margin and fallback section). The new logic reads sidebar config from `yamlConfig_` and calculates margins using the formula from the design doc.

In `src/core/aa/VideoService.cpp`, replace the section from line 71 (`// Primary video config`) through line 96 (the log line) with:

```cpp
    // Resolve AA resolution dimensions
    int remoteW = 1280, remoteH = 720;
    if (res == "1080p") { remoteW = 1920; remoteH = 1080; }
    else if (res == "480p") { remoteW = 800; remoteH = 480; }

    // Calculate margins for sidebar (if enabled)
    int marginW = 0, marginH = 0;
    bool sidebarEnabled = yamlConfig_ && yamlConfig_->sidebarEnabled();
    int sidebarWidth = (yamlConfig_ && sidebarEnabled) ? yamlConfig_->sidebarWidth() : 0;

    if (sidebarEnabled && sidebarWidth > 0) {
        // Display dimensions from config (default 1024x600)
        int displayW = yamlConfig_->displayWidth();
        int displayH = yamlConfig_->displayHeight();
        int aaViewportW = displayW - sidebarWidth;

        float screenRatio = static_cast<float>(aaViewportW) / displayH;
        float remoteRatio = static_cast<float>(remoteW) / remoteH;

        if (screenRatio < remoteRatio) {
            marginW = std::round(remoteW - (remoteH * screenRatio));
        } else {
            marginH = std::round(remoteH - (remoteW / screenRatio));
        }

        BOOST_LOG_TRIVIAL(info) << "[VideoService] Sidebar: " << sidebarWidth << "px, "
                                << "AA viewport: " << aaViewportW << "x" << displayH
                                << ", margins: " << marginW << "x" << marginH;
    }

    // Primary video config — preferred resolution
    auto* primaryConfig = avChannel->add_video_configs();
    if (res == "1080p")
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_1080p);
    else if (res == "480p")
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_480p);
    else
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_720p);
    primaryConfig->set_video_fps(fpsEnum);
    primaryConfig->set_margin_width(marginW);
    primaryConfig->set_margin_height(marginH);
    primaryConfig->set_dpi(dpi);

    // Mandatory 480p fallback — production AA SDKs always include this
    if (res != "480p") {
        int fallbackMarginW = 0, fallbackMarginH = 0;
        if (sidebarEnabled && sidebarWidth > 0) {
            int displayW = yamlConfig_->displayWidth();
            int displayH = yamlConfig_->displayHeight();
            int aaViewportW = displayW - sidebarWidth;
            float screenRatio = static_cast<float>(aaViewportW) / displayH;
            float remoteRatio = 800.0f / 480.0f;
            if (screenRatio < remoteRatio)
                fallbackMarginW = std::round(800 - (480 * screenRatio));
            else
                fallbackMarginH = std::round(480 - (800 / screenRatio));
        }
        auto* fallbackConfig = avChannel->add_video_configs();
        fallbackConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_480p);
        fallbackConfig->set_video_fps(aasdk::proto::enums::VideoFPS::_30);
        fallbackConfig->set_margin_width(fallbackMarginW);
        fallbackConfig->set_margin_height(fallbackMarginH);
        fallbackConfig->set_dpi(dpi);
    }

    BOOST_LOG_TRIVIAL(info) << "[VideoService] Advertised video: " << res.toStdString()
                            << " @ " << fps << "fps, " << dpi << "dpi"
                            << ", margins: " << marginW << "x" << marginH
                            << (res != "480p" ? " + 480p fallback" : "");
```

Add `#include <cmath>` at the top of VideoService.cpp if not already present.

**Step 2: Build and verify**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -5`
Expected: Clean build, no errors

**Step 3: Run existing tests (no regressions)**

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS (VideoService isn't directly unit-tested, but no regressions in other tests)

**Step 4: Commit**

```bash
git add src/core/aa/VideoService.cpp
git commit -m "feat: calculate video margins from sidebar config in VideoService"
```

---

### Task 3: Create Sidebar.qml component

**Files:**
- Create: `qml/applications/android_auto/Sidebar.qml`
- Modify: `src/CMakeLists.txt` (register new QML file)

**Step 1: Create the sidebar QML component**

The sidebar is a vertical strip with volume slider and home button. It uses `AudioService` (already exposed to QML in main.cpp) and `ApplicationController` for the home action.

Create `qml/applications/android_auto/Sidebar.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: sidebar
    color: Qt.rgba(0, 0, 0, 0.85)

    property string position: "right"

    // Subtle border on the AA-facing edge
    Rectangle {
        width: 1
        height: parent.height
        color: ThemeService.borderColor
        anchors.left: position === "right" ? parent.left : undefined
        anchors.right: position === "left" ? parent.right : undefined
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 12

        // Volume up button
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue050"  // volume_up
                size: 28
                color: ThemeService.specialFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var vol = Math.min(100, AudioService.masterVolume + 5)
                    AudioService.setMasterVolume(vol)
                    ConfigService.setValue("audio.master_volume", vol)
                    ConfigService.save()
                }
            }
        }

        // Volume slider (vertical)
        Slider {
            id: volumeSlider
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical
            from: 0; to: 100
            value: AudioService.masterVolume
            onMoved: {
                AudioService.setMasterVolume(value)
                ConfigService.setValue("audio.master_volume", value)
                ConfigService.save()
            }

            background: Rectangle {
                x: volumeSlider.leftPadding + (volumeSlider.availableWidth - width) / 2
                y: volumeSlider.topPadding
                width: 4
                height: volumeSlider.availableHeight
                radius: 2
                color: ThemeService.borderColor

                Rectangle {
                    width: parent.width
                    height: volumeSlider.visualPosition * parent.height
                    radius: 2
                    color: ThemeService.specialFontColor
                }
            }

            handle: Rectangle {
                x: volumeSlider.leftPadding + (volumeSlider.availableWidth - width) / 2
                y: volumeSlider.topPadding + volumeSlider.visualPosition * (volumeSlider.availableHeight - height)
                width: 20; height: 20
                radius: 10
                color: volumeSlider.pressed ? ThemeService.specialFontColor : ThemeService.normalFontColor
            }
        }

        // Volume down button
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue04d"  // volume_down
                size: 28
                color: ThemeService.specialFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var vol = Math.max(0, AudioService.masterVolume - 5)
                    AudioService.setMasterVolume(vol)
                    ConfigService.setValue("audio.master_volume", vol)
                    ConfigService.save()
                }
            }
        }

        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: ThemeService.borderColor
        }

        // Home button
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 56

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue9b2"  // home (our verified codepoint)
                size: 32
                color: ThemeService.specialFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: ApplicationController.goHome()
            }
        }
    }
}
```

**Step 2: Register in CMakeLists.txt**

In `src/CMakeLists.txt`, add QML file metadata. Find the section where `AndroidAutoMenu.qml` is registered (around lines 130-133) and add after it:

```cmake
set_source_files_properties(${CMAKE_SOURCE_DIR}/qml/applications/android_auto/Sidebar.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME Sidebar
    QT_RESOURCE_ALIAS "applications/android_auto/Sidebar.qml"
)
```

Add the file to the `QML_FILES` list in `qt_add_qml_module` (around line 200, after AndroidAutoMenu.qml):
```cmake
        ${CMAKE_SOURCE_DIR}/qml/applications/android_auto/Sidebar.qml
```

**Step 3: Build and verify**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -10`
Expected: Clean build. Sidebar.qml compiles into QML module.

**Step 4: Commit**

```bash
git add qml/applications/android_auto/Sidebar.qml src/CMakeLists.txt
git commit -m "feat: add Sidebar.qml component with volume slider and home button"
```

---

### Task 4: Integrate sidebar into AndroidAutoMenu.qml

**Files:**
- Modify: `qml/applications/android_auto/AndroidAutoMenu.qml`

**Step 1: Replace fullscreen video layout with sidebar-aware layout**

The key change: wrap the VideoOutput and sidebar in a `RowLayout`. The video uses `PreserveAspectCrop` (not Fit) to fill its region and clip the margin black bars. Sidebar visibility and position are driven by config.

Replace the contents of `AndroidAutoMenu.qml` with:

```qml
import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: androidAutoMenu

    Component.onCompleted: ApplicationController.setTitle("Android Auto")

    // Read sidebar config via ConfigService
    readonly property bool sidebarEnabled: {
        var v = ConfigService.value("video.sidebar.enabled")
        return v === true || v === 1 || v === "true"
    }
    readonly property int sidebarWidth: ConfigService.value("video.sidebar.width") || 150
    readonly property string sidebarPosition: ConfigService.value("video.sidebar.position") || "right"
    readonly property bool projecting: AndroidAutoService.connectionState === 3

    RowLayout {
        anchors.fill: parent
        spacing: 0
        layoutDirection: androidAutoMenu.sidebarPosition === "left" ? Qt.RightToLeft : Qt.LeftToRight

        // AA Video area
        Item {
            id: videoArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Black background for letterbox bars
            Rectangle {
                anchors.fill: parent
                color: "black"
                visible: androidAutoMenu.projecting
            }

            // Video output — crops margin black bars to fill viewport
            VideoOutput {
                id: videoOutput
                anchors.fill: parent
                visible: androidAutoMenu.projecting
                fillMode: androidAutoMenu.sidebarEnabled
                         ? VideoOutput.PreserveAspectCrop
                         : VideoOutput.PreserveAspectFit
            }

            // Debug touch overlay
            Repeater {
                model: TouchHandler.debugOverlay ? TouchHandler.debugTouches : []
                delegate: Item {
                    readonly property real videoAspect: 1280 / 720
                    readonly property real displayAspect: videoArea.width / videoArea.height
                    readonly property real videoW: videoAspect > displayAspect ? videoArea.width : videoArea.height * videoAspect
                    readonly property real videoH: videoAspect > displayAspect ? videoArea.width / videoAspect : videoArea.height
                    readonly property real videoX0: (videoArea.width - videoW) / 2
                    readonly property real videoY0: (videoArea.height - videoH) / 2

                    x: videoX0 + modelData.x / 1280 * videoW - 15
                    y: videoY0 + modelData.y / 720 * videoH - 15
                    width: 30; height: 30
                    z: 100

                    Rectangle {
                        anchors.fill: parent
                        radius: 15
                        color: "transparent"
                        border.color: "#00FF00"
                        border.width: 2
                    }
                    Rectangle { anchors.centerIn: parent; width: 1; height: 20; color: "#00FF00" }
                    Rectangle { anchors.centerIn: parent; width: 20; height: 1; color: "#00FF00" }
                    Text {
                        anchors.top: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.x + "," + modelData.y
                        color: "#00FF00"
                        font.pixelSize: 10
                    }
                }
            }

            // Status overlay — hidden when projecting video
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 16
                visible: !androidAutoMenu.projecting

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        switch (AndroidAutoService.connectionState) {
                        case 0: return "Android Auto";
                        case 1: return "Waiting for Device";
                        case 2: return "Connecting...";
                        case 3: return "Android Auto Active";
                        default: return "Android Auto";
                        }
                    }
                    color: ThemeService.specialFontColor
                    font.pixelSize: 28
                    font.bold: true
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: AndroidAutoService.statusMessage
                    color: ThemeService.descriptionFontColor
                    font.pixelSize: 18
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.maximumWidth: videoArea.width * 0.8
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 12; height: 12; radius: 6
                    color: {
                        switch (AndroidAutoService.connectionState) {
                        case 0: return "#666666";
                        case 1: return "#FFA500";
                        case 2: return "#FFFF00";
                        case 3: return "#00FF00";
                        default: return "#666666";
                        }
                    }
                    SequentialAnimation on opacity {
                        running: AndroidAutoService.connectionState === 1 || AndroidAutoService.connectionState === 2
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
                    }
                }
            }
        }

        // Sidebar — only when enabled and projecting
        Sidebar {
            id: sidebarPanel
            Layout.preferredWidth: androidAutoMenu.sidebarWidth
            Layout.fillHeight: true
            visible: androidAutoMenu.sidebarEnabled && androidAutoMenu.projecting
            position: androidAutoMenu.sidebarPosition
        }
    }

    // Bind the video sink to C++ decoder
    Binding {
        target: VideoDecoder
        property: "videoSink"
        value: videoOutput.videoSink
    }
}
```

**Step 2: Build and verify**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -10`
Expected: Clean build

**Step 3: Run tests (no regressions)**

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

**Step 4: Commit**

```bash
git add qml/applications/android_auto/AndroidAutoMenu.qml
git commit -m "feat: integrate sidebar into AndroidAutoMenu with configurable position"
```

---

### Task 5: Update EvdevTouchReader for sidebar touch exclusion

**Files:**
- Modify: `src/core/aa/EvdevTouchReader.hpp`
- Modify: `src/core/aa/EvdevTouchReader.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp` (pass sidebar config)

**Step 1: Add sidebar config to EvdevTouchReader**

In `src/core/aa/EvdevTouchReader.hpp`, add sidebar members and signals. After the `gestureDetected()` signal (line 69), add:

```cpp
    /// Emitted when a touch occurs in the sidebar volume-up zone
    void sidebarVolumeUp();
    /// Emitted when a touch occurs in the sidebar volume-down zone
    void sidebarVolumeDown();
    /// Emitted when a touch occurs in the sidebar home zone
    void sidebarHome();
```

Add a method to configure sidebar after the constructor (after line 58):

```cpp
    /// Configure sidebar touch exclusion zone
    void setSidebar(bool enabled, int width, const std::string& position);
```

Add sidebar member variables at the end of the private section (before line 112):

```cpp
    // Sidebar touch exclusion
    bool sidebarEnabled_ = false;
    int sidebarPixelWidth_ = 0;
    std::string sidebarPosition_ = "right";
    float sidebarEvdevX0_ = 0;  // start of sidebar in evdev coords
    float sidebarEvdevX1_ = 0;  // end of sidebar in evdev coords
    // Sidebar hit zones (evdev Y ranges) — top half = vol up, bottom section = home
    float sidebarVolUpY0_ = 0, sidebarVolUpY1_ = 0;
    float sidebarVolDownY0_ = 0, sidebarVolDownY1_ = 0;
    float sidebarHomeY0_ = 0, sidebarHomeY1_ = 0;
```

**Step 2: Implement sidebar logic in EvdevTouchReader.cpp**

Add `setSidebar()` implementation and modify `computeLetterbox()` to account for sidebar offset.

Add `setSidebar()` (after `computeLetterbox()`):

```cpp
void EvdevTouchReader::setSidebar(bool enabled, int width, const std::string& position)
{
    sidebarEnabled_ = enabled;
    sidebarPixelWidth_ = width;
    sidebarPosition_ = position;

    if (!enabled || width <= 0) return;

    // Compute sidebar evdev X range
    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    if (position == "right") {
        int sidebarStartPx = displayWidth_ - width;
        sidebarEvdevX0_ = sidebarStartPx * evdevPerPixelX;
        sidebarEvdevX1_ = screenWidth_;
    } else {
        sidebarEvdevX0_ = 0;
        sidebarEvdevX1_ = width * evdevPerPixelX;
    }

    // Sidebar hit zones: divide sidebar height into regions
    // Top 40% = volume up, next 20% = volume down, bottom 25% = home (with 15% gap)
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;
    sidebarVolUpY0_ = 0;
    sidebarVolUpY1_ = displayHeight_ * 0.40f * evdevPerPixelY;
    sidebarVolDownY0_ = sidebarVolUpY1_;
    sidebarVolDownY1_ = displayHeight_ * 0.60f * evdevPerPixelY;
    sidebarHomeY0_ = displayHeight_ * 0.75f * evdevPerPixelY;
    sidebarHomeY1_ = screenHeight_;

    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Sidebar: " << position << " " << width << "px"
                            << ", evdev X range: " << sidebarEvdevX0_ << "-" << sidebarEvdevX1_;
}
```

Modify `computeLetterbox()` to offset the video area when sidebar is on the left. The existing letterbox computation already works for the AA viewport — we just need to shift it if the sidebar occupies the left side. Add at the end of `computeLetterbox()`, before the closing brace:

```cpp
    // Offset video area for sidebar position
    if (sidebarEnabled_ && sidebarPixelWidth_ > 0) {
        float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
        if (sidebarPosition_ == "left") {
            videoEvdevX0_ += sidebarPixelWidth_ * evdevPerPixelX;
        }
        // Reduce video evdev width to account for sidebar
        videoEvdevW_ = (displayWidth_ - sidebarPixelWidth_) * evdevPerPixelX
                       * (videoEvdevW_ / (displayWidth_ * evdevPerPixelX));
    }
```

Actually — this is getting complicated and the letterbox math needs to work correctly with the sidebar. A cleaner approach: `computeLetterbox()` should use the AA viewport dimensions (display minus sidebar) instead of full display dimensions. Modify the call order so `setSidebar()` is called before `computeLetterbox()`, and `computeLetterbox()` accounts for sidebar:

In `computeLetterbox()`, replace `displayWidth_` with an effective width:

```cpp
void EvdevTouchReader::computeLetterbox()
{
    int effectiveDisplayW = displayWidth_;
    int effectiveDisplayX0 = 0;  // pixel offset of video area
    if (sidebarEnabled_ && sidebarPixelWidth_ > 0) {
        effectiveDisplayW = displayWidth_ - sidebarPixelWidth_;
        if (sidebarPosition_ == "left") {
            effectiveDisplayX0 = sidebarPixelWidth_;
        }
    }

    float videoAspect = static_cast<float>(aaWidth_) / aaHeight_;
    float displayAspect = static_cast<float>(effectiveDisplayW) / displayHeight_;

    float videoPixelW, videoPixelH, videoPixelX0, videoPixelY0;

    if (videoAspect > displayAspect) {
        videoPixelW = effectiveDisplayW;
        videoPixelH = effectiveDisplayW / videoAspect;
        videoPixelX0 = effectiveDisplayX0;
        videoPixelY0 = (displayHeight_ - videoPixelH) / 2.0f;
    } else {
        videoPixelH = displayHeight_;
        videoPixelW = displayHeight_ * videoAspect;
        videoPixelX0 = effectiveDisplayX0 + (effectiveDisplayW - videoPixelW) / 2.0f;
        videoPixelY0 = 0;
    }

    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;

    videoEvdevX0_ = videoPixelX0 * evdevPerPixelX;
    videoEvdevY0_ = videoPixelY0 * evdevPerPixelY;
    videoEvdevW_ = videoPixelW * evdevPerPixelX;
    videoEvdevH_ = videoPixelH * evdevPerPixelY;

    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Letterbox: video pixel area "
                            << videoPixelW << "x" << videoPixelH
                            << " at (" << videoPixelX0 << "," << videoPixelY0 << ")"
                            << " evdev: (" << videoEvdevX0_ << "," << videoEvdevY0_ << ")"
                            << " " << videoEvdevW_ << "x" << videoEvdevH_;
}
```

Modify `processSync()` — add sidebar touch detection. Before the existing action logic (the `// Build pointer array` section), add a check:

```cpp
    // Check for sidebar touches — don't forward to AA
    if (sidebarEnabled_) {
        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (slots_[i].trackingId >= 0 && slots_[i].dirty) {
                float rawX = slots_[i].x;
                if (rawX >= sidebarEvdevX0_ && rawX <= sidebarEvdevX1_) {
                    // This touch is in the sidebar — check hit zones on DOWN only
                    if (prevSlots_[i].trackingId < 0) {
                        float rawY = slots_[i].y;
                        if (rawY >= sidebarVolUpY0_ && rawY < sidebarVolUpY1_)
                            emit sidebarVolumeUp();
                        else if (rawY >= sidebarVolDownY0_ && rawY < sidebarVolDownY1_)
                            emit sidebarVolumeDown();
                        else if (rawY >= sidebarHomeY0_ && rawY <= sidebarHomeY1_)
                            emit sidebarHome();
                    }
                    slots_[i].dirty = false;  // consume the touch — don't forward
                }
            }
        }
    }
```

**Step 3: Update AndroidAutoPlugin to pass sidebar config**

In `src/plugins/android_auto/AndroidAutoPlugin.cpp`, after the `touchReader_` construction (around line 85, after `touchReader_->start()`), add:

```cpp
        // Configure sidebar touch exclusion if sidebar is enabled
        if (hostContext_ && hostContext_->configService()) {
            auto* cs = hostContext_->configService();
            bool sidebarEnabled = cs->value("video.sidebar.enabled").toBool();
            if (sidebarEnabled) {
                int sidebarW = cs->value("video.sidebar.width").toInt();
                QString pos = cs->value("video.sidebar.position").toString();
                touchReader_->setSidebar(true, sidebarW, pos.toStdString());
                // Recompute letterbox with sidebar offset
                touchReader_->computeLetterbox();
            }
        }
```

Make `computeLetterbox()` public in the header (move from private to public section).

**Step 4: Build and verify**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -10`
Expected: Clean build

**Step 5: Run tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

**Step 6: Commit**

```bash
git add src/core/aa/EvdevTouchReader.hpp src/core/aa/EvdevTouchReader.cpp src/plugins/android_auto/AndroidAutoPlugin.cpp
git commit -m "feat: add sidebar touch exclusion zones to EvdevTouchReader"
```

---

### Task 6: Connect sidebar evdev signals to AudioService

**Files:**
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`

**Step 1: Wire sidebar signals to actions**

In `AndroidAutoPlugin.cpp`, after the sidebar configuration code added in Task 5 (the `touchReader_->setSidebar()` call), add signal connections:

```cpp
                connect(touchReader_, &oap::aa::EvdevTouchReader::sidebarVolumeUp,
                        this, [this]() {
                    if (auto* audio = hostContext_->audioService()) {
                        int vol = std::min(100, audio->masterVolume() + 5);
                        audio->setMasterVolume(vol);
                        BOOST_LOG_TRIVIAL(debug) << "[AAPlugin] Sidebar volume up: " << vol;
                    }
                }, Qt::QueuedConnection);

                connect(touchReader_, &oap::aa::EvdevTouchReader::sidebarVolumeDown,
                        this, [this]() {
                    if (auto* audio = hostContext_->audioService()) {
                        int vol = std::max(0, audio->masterVolume() - 5);
                        audio->setMasterVolume(vol);
                        BOOST_LOG_TRIVIAL(debug) << "[AAPlugin] Sidebar volume down: " << vol;
                    }
                }, Qt::QueuedConnection);

                connect(touchReader_, &oap::aa::EvdevTouchReader::sidebarHome,
                        this, [this]() {
                    BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Sidebar home pressed";
                    // goHome triggers AA exit-to-car flow
                    QMetaObject::invokeMethod(qApp, []() {
                        // ApplicationController::goHome() — need to find it via QML context
                    }, Qt::QueuedConnection);
                }, Qt::QueuedConnection);
```

Note: The home button action needs access to `ApplicationController`. Check how the AA plugin currently triggers exit-to-car. If there's an existing signal path (like the `videoFocusChanged` flow), use that. The sidebar home action should call the same code path as the existing "exit to car" gesture/button.

**Step 2: Build and verify**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -10`
Expected: Clean build

**Step 3: Commit**

```bash
git add src/plugins/android_auto/AndroidAutoPlugin.cpp
git commit -m "feat: wire sidebar touch signals to volume and home actions"
```

---

### Task 7: Add sidebar settings to Video Settings page

**Files:**
- Modify: `qml/applications/settings/VideoSettings.qml`

**Step 1: Add sidebar controls to VideoSettings.qml**

Add after the DPI SettingsSpinBox (after line 44):

```qml
        // Sidebar section
        Text {
            text: "Sidebar"
            font.pixelSize: 17
            font.bold: true
            color: ThemeService.normalFontColor
            Layout.topMargin: 16
        }

        SettingsToggle {
            label: "Show sidebar during Android Auto"
            configPath: "video.sidebar.enabled"
            restartRequired: true
        }

        SettingsComboBox {
            label: "Sidebar Position"
            configPath: "video.sidebar.position"
            options: ["Right", "Left"]
            values: ["right", "left"]
            restartRequired: true
        }

        SettingsSpinBox {
            label: "Sidebar Width (px)"
            configPath: "video.sidebar.width"
            from: 80; to: 300
            restartRequired: true
        }

        Text {
            text: "Sidebar changes take effect on next app restart."
            font.pixelSize: 12
            color: ThemeService.descriptionFontColor
            font.italic: true
            Layout.leftMargin: 8
        }
```

**Step 2: Build and verify**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -10`
Expected: Clean build

**Step 3: Run tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

**Step 4: Commit**

```bash
git add qml/applications/settings/VideoSettings.qml
git commit -m "feat: add sidebar settings (enable, position, width) to Video Settings"
```

---

### Task 8: Final build, test, and documentation commit

**Files:**
- Already created: `docs/aa-video-resolution.md`
- Modify: `CLAUDE.md` (update status and known limitations)

**Step 1: Full build and test**

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: Clean build, all tests PASS

**Step 2: Update CLAUDE.md current status**

Add to the "Working features" list:
```
- Configurable sidebar during AA (volume, home) using protocol-level margin negotiation
```

Add to "Known limitations / TODO":
```
- Sidebar touch zones use evdev coordinate math — not yet tested on Pi hardware
- Sidebar only takes effect on app restart (margins locked at AA session start)
- QML sidebar uses MouseArea (requires EVIOCGRAB release for sidebar area — may need evdev-only approach on Pi)
```

**Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: update status with sidebar feature and known limitations"
```

---

## Implementation Notes

### Critical gotcha: EVIOCGRAB vs QML MouseArea
The design doc discusses using evdev hit zones for sidebar touches (since EVIOCGRAB prevents Qt from receiving touches). However, Task 3 uses QML `MouseArea` in Sidebar.qml. **These two approaches conflict.** On the Pi with EVIOCGRAB active:
- The QML sidebar's MouseArea will NOT receive touches (grabbed by evdev reader)
- The evdev signals (Task 5-6) WILL fire

The QML sidebar is visual-only on Pi — the actual touch handling is the evdev signals. The QML MouseArea serves as a fallback for dev/testing on the VM where there's no evdev grab. This is acceptable for v1 but should be documented.

### Testing on Pi
After deploying to Pi, verify:
1. Sidebar appears when `video.sidebar.enabled: true`
2. AA video fills the viewport area (no visible black margin bars)
3. Touching sidebar area does NOT send touch events to AA
4. Volume up/down changes PipeWire volume
5. Home button exits AA to launcher
6. Touch coordinates in AA viewport are correctly mapped (use debug overlay)
