# Touch-Friendly Settings Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Rework all settings pages for touch-friendly use on a 7" 1024x600 car touchscreen using Material Dark style, oversized controls, and purpose-built picker/segmented-button components.

**Architecture:** Switch to Qt Material Dark as the base style with a thin "HeadUnit" custom style fallback for oversized touch targets. Replace ComboBox with modal FullScreenPicker, SpinBox with Tumbler/Slider, TextField with ReadOnlyField. Migrate pages one at a time.

**Tech Stack:** Qt 6 QML, Qt Quick Controls Material style, existing UiMetrics singleton, ConfigService/ThemeService

**Design doc:** `docs/plans/2026-02-22-touch-friendly-settings-redesign.md`
**Rollback tag:** `v0.3.0-pre-touch-redesign`

---

## Phase 1: Foundation

### Task 1: Enable Material Dark Style

**Files:**
- Modify: `src/main.cpp:1-3` (add include)
- Modify: `src/main.cpp:125` (add style setup before engine)

**Step 1: Add QQuickStyle include to main.cpp**

Add after the existing includes (line 5):

```cpp
#include <QQuickStyle>
```

**Step 2: Set Material style before QQmlApplicationEngine creation**

Add before line 125 (`QQmlApplicationEngine engine;`):

```cpp
    QQuickStyle::setStyle("Material");
```

We start with plain Material (no HeadUnit fallback yet). HeadUnit overrides come in Task 4 after we build the style files.

**Step 3: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc)`
Expected: Clean compile, no errors.

**Step 4: Wire Material theme to ThemeService day/night**

In `qml/main.qml` (or wherever the root ApplicationWindow/Window is), add Material import and bind theme to ThemeService.nightMode:

```qml
import QtQuick.Controls.Material

// At the root Window/ApplicationWindow level:
Material.theme: ThemeService.nightMode ? Material.Dark : Material.Light
Material.accent: ThemeService.highlightColor
Material.background: ThemeService.backgroundColor
```

Note: Check what the root QML element is first. The Material attached properties must be set on a Window or ApplicationWindow to cascade to all children.

**Step 5: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile. App should launch with Material Dark appearance on all Qt Controls (buttons, sliders, switches).

**Step 6: Commit**

```bash
git add src/main.cpp qml/main.qml
git commit -m "feat: switch to Material Dark style with ThemeService integration"
```

---

### Task 2: Bump UiMetrics Baseline Values

**Files:**
- Modify: `qml/controls/UiMetrics.qml`

**Step 1: Update baseline pixel values**

Replace the token values in `UiMetrics.qml`. The file is at `qml/controls/UiMetrics.qml`. Change these specific lines:

```qml
    // Touch targets
    readonly property int rowH:     Math.round(64 * scale)
    readonly property int touchMin: Math.round(56 * scale)

    // Spacing
    readonly property int marginPage: Math.round(20 * scale)
    readonly property int marginRow:  Math.round(12 * scale)
    readonly property int spacing:    Math.round(8 * scale)
    readonly property int gap:        Math.round(16 * scale)
    readonly property int sectionGap: Math.round(20 * scale)

    // Fonts
    readonly property int fontTitle:   Math.round(22 * scale)
    readonly property int fontBody:    Math.round(20 * scale)
    readonly property int fontSmall:   Math.round(16 * scale)
    readonly property int fontTiny:    Math.round(14 * scale)
    readonly property int fontHeading: Math.round(28 * scale)

    // Component sizing
    readonly property int headerH:     Math.round(56 * scale)
    readonly property int sectionH:    Math.round(36 * scale)
    readonly property int backBtnSize: Math.round(44 * scale)
    readonly property int iconSize:    Math.round(28 * scale)
    readonly property int iconSmall:   Math.round(20 * scale)

    // Radii
    readonly property int radius: Math.round(10 * scale)

    // Grid (settings menu tiles)
    readonly property int gridGap: Math.round(24 * scale)
```

**Step 2: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile. All existing settings pages should render larger (fonts, spacing, row heights).

**Step 3: Commit**

```bash
git add qml/controls/UiMetrics.qml
git commit -m "feat: bump UiMetrics baselines for touch-friendly sizing"
```

---

### Task 3: Build FullScreenPicker Control

**Files:**
- Create: `qml/controls/FullScreenPicker.qml`
- Modify: `src/CMakeLists.txt` (register new QML file)

**Step 1: Create FullScreenPicker.qml**

Create `qml/controls/FullScreenPicker.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

Item {
    id: root

    property string label: ""
    property string configPath: ""
    property var options: []       // display strings
    property var values: []        // typed values to store (if empty, stores options strings)
    property bool restartRequired: false

    // Expose for conditional visibility (same pattern as old SettingsComboBox)
    readonly property int currentIndex: internal.selectedIndex
    readonly property var currentValue: {
        if (root.values.length > 0 && internal.selectedIndex >= 0 && internal.selectedIndex < root.values.length)
            return root.values[internal.selectedIndex]
        if (internal.selectedIndex >= 0 && internal.selectedIndex < root.options.length)
            return root.options[internal.selectedIndex]
        return undefined
    }

    // For model-driven pickers (audio devices etc.)
    property var model: null
    property string textRole: ""
    property string valueRole: ""
    signal activated(int index)

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    QtObject {
        id: internal
        property int selectedIndex: -1
        property string displayText: ""
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"  // restart indicator
            size: UiMetrics.iconSmall
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        // Tappable value display
        Rectangle {
            Layout.preferredWidth: Math.max(root.width * 0.35, implicitWidth + UiMetrics.gap * 2)
            Layout.preferredHeight: UiMetrics.touchMin
            color: ThemeService.controlBackgroundColor
            radius: UiMetrics.radius / 2

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.gap
                anchors.rightMargin: UiMetrics.gap / 2
                spacing: UiMetrics.spacing

                Text {
                    text: internal.displayText || "Select..."
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                MaterialIcon {
                    icon: "\ue5cf"  // expand_more (chevron down)
                    size: UiMetrics.iconSize
                    color: ThemeService.descriptionFontColor
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: pickerDialog.open()
            }
        }
    }

    // Modal bottom-sheet picker
    Dialog {
        id: pickerDialog
        modal: true
        dim: true
        // Position at bottom of window
        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: parent.height - height
        width: parent.width
        height: Math.min(parent.height * 0.6, pickerList.contentHeight + titleRow.height + UiMetrics.marginPage * 3)
        padding: 0

        background: Rectangle {
            color: ThemeService.controlBoxBackgroundColor
            radius: UiMetrics.radius
            // Only round top corners
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: parent.radius
                color: parent.color
            }
        }

        contentItem: ColumnLayout {
            spacing: 0

            // Title bar
            RowLayout {
                id: titleRow
                Layout.fillWidth: true
                Layout.leftMargin: UiMetrics.marginPage
                Layout.rightMargin: UiMetrics.marginPage
                Layout.topMargin: UiMetrics.marginPage
                Layout.bottomMargin: UiMetrics.spacing

                Text {
                    text: root.label
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }

                // Close button
                Rectangle {
                    width: UiMetrics.backBtnSize
                    height: UiMetrics.backBtnSize
                    radius: width / 2
                    color: closeArea.pressed ? ThemeService.controlBackgroundColor : "transparent"
                    MaterialIcon {
                        anchors.centerIn: parent
                        icon: "\ue5cd"  // close
                        size: UiMetrics.iconSize
                        color: ThemeService.descriptionFontColor
                    }
                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        onClicked: pickerDialog.close()
                    }
                }
            }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: ThemeService.descriptionFontColor
                opacity: 0.3
            }

            // Option list
            ListView {
                id: pickerList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                model: root.model ? root.model : root.options

                delegate: ItemDelegate {
                    width: pickerList.width
                    height: UiMetrics.rowH

                    contentItem: RowLayout {
                        anchors.leftMargin: UiMetrics.marginPage
                        anchors.rightMargin: UiMetrics.marginPage
                        spacing: UiMetrics.gap

                        Text {
                            text: {
                                if (root.model && root.textRole)
                                    return model[root.textRole] || modelData || ""
                                return modelData || ""
                            }
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.normalFontColor
                            Layout.fillWidth: true
                        }

                        MaterialIcon {
                            icon: "\ue876"  // check
                            size: UiMetrics.iconSize
                            color: ThemeService.highlightColor
                            visible: index === internal.selectedIndex
                        }
                    }

                    onClicked: {
                        internal.selectedIndex = index
                        if (root.model && root.textRole) {
                            internal.displayText = model[root.textRole] || modelData || ""
                        } else {
                            internal.displayText = modelData || ""
                        }

                        // Config-driven mode
                        if (root.configPath !== "") {
                            var writeVal = (root.values.length > 0 && index < root.values.length)
                                ? root.values[index] : internal.displayText
                            ConfigService.setValue(root.configPath, writeVal)
                            ConfigService.save()
                        }

                        // Model-driven mode (audio devices etc.)
                        root.activated(index)

                        pickerDialog.close()
                    }
                }
            }
        }
    }

    // Initialize from config
    Component.onCompleted: {
        if (root.configPath !== "") {
            var current = ConfigService.value(root.configPath)
            var idx = -1
            if (root.values.length > 0) {
                for (var i = 0; i < root.values.length; i++) {
                    if (root.values[i] == current) { idx = i; break }
                }
            }
            if (idx < 0)
                idx = root.options.indexOf(String(current))
            if (idx >= 0) {
                internal.selectedIndex = idx
                internal.displayText = root.options[idx] || ""
            }
        }
    }
}
```

**Step 2: Register in CMakeLists.txt**

Add to `src/CMakeLists.txt` — insert after the SettingsPageHeader block (after line 93):

```cmake
set_source_files_properties(../qml/controls/FullScreenPicker.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "FullScreenPicker"
    QT_RESOURCE_ALIAS "FullScreenPicker.qml"
)
```

And add to the `QML_FILES` list (after `../qml/controls/SettingsPageHeader.qml` on line 204):

```
        ../qml/controls/FullScreenPicker.qml
```

**Step 3: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc)`
Expected: Clean compile.

**Step 4: Commit**

```bash
git add qml/controls/FullScreenPicker.qml src/CMakeLists.txt
git commit -m "feat: add FullScreenPicker touch control (replaces ComboBox)"
```

---

### Task 4: Build SegmentedButton Control

**Files:**
- Create: `qml/controls/SegmentedButton.qml`
- Modify: `src/CMakeLists.txt` (register new QML file)

**Step 1: Create SegmentedButton.qml**

Create `qml/controls/SegmentedButton.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    property string label: ""
    property string configPath: ""
    property var options: []       // display strings: ["30", "60"]
    property var values: []        // typed values: [30, 60] — if empty, stores options strings
    property bool restartRequired: false

    readonly property int currentIndex: internal.selectedIndex
    readonly property var currentValue: {
        if (root.values.length > 0 && internal.selectedIndex >= 0 && internal.selectedIndex < root.values.length)
            return root.values[internal.selectedIndex]
        if (internal.selectedIndex >= 0 && internal.selectedIndex < root.options.length)
            return root.options[internal.selectedIndex]
        return undefined
    }

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    QtObject {
        id: internal
        property int selectedIndex: -1
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"
            size: UiMetrics.iconSmall
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        // Segmented button row
        Row {
            spacing: 0

            Repeater {
                model: root.options

                Rectangle {
                    width: Math.max(UiMetrics.touchMin * 1.5, segText.implicitWidth + UiMetrics.gap * 2)
                    height: UiMetrics.touchMin
                    color: index === internal.selectedIndex
                        ? ThemeService.highlightColor
                        : ThemeService.controlBackgroundColor
                    radius: index === 0 ? UiMetrics.radius / 2 : (index === root.options.length - 1 ? UiMetrics.radius / 2 : 0)

                    // Round only the appropriate corners
                    // Left segment: round left corners
                    // Right segment: round right corners
                    // Middle segments: no rounding
                    // This is approximated with overlapping rectangles
                    Rectangle {
                        visible: index === 0
                        anchors.right: parent.right
                        width: parent.radius
                        height: parent.height
                        color: parent.color
                    }
                    Rectangle {
                        visible: index === root.options.length - 1
                        anchors.left: parent.left
                        width: parent.radius
                        height: parent.height
                        color: parent.color
                    }

                    Text {
                        id: segText
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: UiMetrics.fontSmall
                        font.bold: index === internal.selectedIndex
                        color: index === internal.selectedIndex
                            ? ThemeService.backgroundColor
                            : ThemeService.normalFontColor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            internal.selectedIndex = index
                            if (root.configPath !== "") {
                                var writeVal = (root.values.length > 0 && index < root.values.length)
                                    ? root.values[index] : modelData
                                ConfigService.setValue(root.configPath, writeVal)
                                ConfigService.save()
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var current = ConfigService.value(root.configPath)
            var idx = -1
            if (root.values.length > 0) {
                for (var i = 0; i < root.values.length; i++) {
                    if (root.values[i] == current) { idx = i; break }
                }
            }
            if (idx < 0)
                idx = root.options.indexOf(String(current))
            if (idx >= 0) internal.selectedIndex = idx
            else internal.selectedIndex = 0
        }
    }
}
```

**Step 2: Register in CMakeLists.txt**

Add `set_source_files_properties` block and add to `QML_FILES` list (same pattern as Task 3).

**Step 3: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc)`
Expected: Clean compile.

**Step 4: Commit**

```bash
git add qml/controls/SegmentedButton.qml src/CMakeLists.txt
git commit -m "feat: add SegmentedButton touch control for 2-3 option selections"
```

---

### Task 5: Build ReadOnlyField Control

**Files:**
- Create: `qml/controls/ReadOnlyField.qml`
- Modify: `src/CMakeLists.txt`

**Step 1: Create ReadOnlyField.qml**

Create `qml/controls/ReadOnlyField.qml`:

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string label: ""
    property string configPath: ""
    property string value: ""          // explicit value override
    property string placeholder: "—"   // shown when value is empty
    property bool showWebHint: true    // show "Edit via web panel" hint

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.normalFontColor
            Layout.preferredWidth: root.width * 0.35
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: internal.displayValue || root.placeholder
                font.pixelSize: UiMetrics.fontBody
                color: internal.displayValue ? ThemeService.normalFontColor : ThemeService.descriptionFontColor
                Layout.fillWidth: true
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
            }

            Text {
                visible: root.showWebHint
                text: "Edit via web panel"
                font.pixelSize: UiMetrics.fontTiny
                font.italic: true
                color: ThemeService.descriptionFontColor
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    QtObject {
        id: internal
        property string displayValue: root.value
    }

    Component.onCompleted: {
        if (root.configPath !== "" && root.value === "") {
            var val = ConfigService.value(root.configPath)
            internal.displayValue = val ? String(val) : ""
        }
    }
}
```

**Step 2: Register in CMakeLists.txt** (same pattern)

**Step 3: Build and verify**

**Step 4: Commit**

```bash
git add qml/controls/ReadOnlyField.qml src/CMakeLists.txt
git commit -m "feat: add ReadOnlyField for display-only settings with web panel hint"
```

---

### Task 6: Build InfoBanner Control

**Files:**
- Create: `qml/controls/InfoBanner.qml`
- Modify: `src/CMakeLists.txt`

**Step 1: Create InfoBanner.qml**

Create `qml/controls/InfoBanner.qml`:

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string text: "Restart required to apply changes"
    property bool visible: false

    Layout.fillWidth: true
    implicitHeight: visible ? bannerContent.implicitHeight + UiMetrics.gap * 2 : 0

    Behavior on implicitHeight { NumberAnimation { duration: 200 } }

    Rectangle {
        id: bannerContent
        visible: root.visible
        anchors.fill: parent
        anchors.topMargin: UiMetrics.gap
        color: ThemeService.controlBoxBackgroundColor
        radius: UiMetrics.radius / 2

        RowLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.marginRow
            spacing: UiMetrics.gap

            MaterialIcon {
                icon: "\ue88e"  // info
                size: UiMetrics.iconSize
                color: ThemeService.highlightColor
            }

            Text {
                text: root.text
                font.pixelSize: UiMetrics.fontSmall
                font.italic: true
                color: ThemeService.descriptionFontColor
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }
}
```

**Step 2: Register in CMakeLists.txt** (same pattern)

**Step 3: Build and verify**

**Step 4: Commit**

```bash
git add qml/controls/InfoBanner.qml src/CMakeLists.txt
git commit -m "feat: add InfoBanner for restart-required notifications"
```

---

### Task 7: Visual Smoke Test

**Files:** None modified — verification only.

**Step 1: Full rebuild**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc)`
Expected: Clean compile with all new controls registered.

**Step 2: Run existing tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && ctest --output-on-failure`
Expected: All 17 tests pass. (Tests are C++ unit tests, not QML — they should be unaffected.)

**Step 3: Document any issues**

If the Material style or UiMetrics changes break anything visible in the non-settings UI (Shell, TopBar, BottomBar, NavStrip, launcher tiles), note the specific issues here before proceeding to Phase 2. These would need to be fixed before page migration.

**Step 4: Commit checkpoint**

No commit needed if nothing was changed. If fixes were required, commit them.

---

## Phase 2: Page Migration

### Task 8: Migrate Audio Settings

**Files:**
- Modify: `qml/applications/settings/AudioSettings.qml` (full rewrite)

This is the template page — sets the pattern for all others.

**Step 1: Rewrite AudioSettings.qml**

Replace entire contents of `qml/applications/settings/AudioSettings.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SettingsPageHeader {
            title: "Audio"
            stack: root.stackRef
        }

        SectionHeader { text: "Volume" }

        SettingsSlider {
            label: "Master Volume"
            configPath: "audio.master_volume"
            from: 0; to: 100; stepSize: 1
            onMoved: {
                if (typeof AudioService !== "undefined")
                    AudioService.setMasterVolume(value)
            }
        }

        SettingsSlider {
            label: "Mic Gain"
            configPath: "audio.microphone.gain"
            from: 0.5; to: 4.0; stepSize: 0.1
        }

        SectionHeader { text: "Devices" }

        FullScreenPicker {
            id: outputPicker
            label: "Output Device"
            model: typeof AudioOutputDeviceModel !== "undefined" ? AudioOutputDeviceModel : null
            textRole: "description"
            onActivated: function(index) {
                if (typeof AudioOutputDeviceModel === "undefined") return
                var nodeName = AudioOutputDeviceModel.data(
                    AudioOutputDeviceModel.index(index, 0), Qt.UserRole + 1)
                ConfigService.setValue("audio.output_device", nodeName)
                ConfigService.save()
                if (typeof AudioService !== "undefined")
                    AudioService.setOutputDevice(nodeName)
                restartBanner.visible = true
            }
            Component.onCompleted: {
                if (typeof AudioOutputDeviceModel === "undefined") return
                var current = ConfigService.value("audio.output_device")
                if (!current) current = "auto"
                var idx = AudioOutputDeviceModel.indexOfDevice(current)
                if (idx >= 0) {
                    internal.selectedIndex = idx
                    internal.displayText = AudioOutputDeviceModel.data(
                        AudioOutputDeviceModel.index(idx, 0), Qt.DisplayRole)
                }
            }
        }

        FullScreenPicker {
            id: inputPicker
            label: "Input Device"
            model: typeof AudioInputDeviceModel !== "undefined" ? AudioInputDeviceModel : null
            textRole: "description"
            onActivated: function(index) {
                if (typeof AudioInputDeviceModel === "undefined") return
                var nodeName = AudioInputDeviceModel.data(
                    AudioInputDeviceModel.index(index, 0), Qt.UserRole + 1)
                ConfigService.setValue("audio.input_device", nodeName)
                ConfigService.save()
                if (typeof AudioService !== "undefined")
                    AudioService.setInputDevice(nodeName)
                restartBanner.visible = true
            }
            Component.onCompleted: {
                if (typeof AudioInputDeviceModel === "undefined") return
                var current = ConfigService.value("audio.input_device")
                if (!current) current = "auto"
                var idx = AudioInputDeviceModel.indexOfDevice(current)
                if (idx >= 0) {
                    internal.selectedIndex = idx
                    internal.displayText = AudioInputDeviceModel.data(
                        AudioInputDeviceModel.index(idx, 0), Qt.DisplayRole)
                }
            }
        }

        InfoBanner {
            id: restartBanner
            text: "Restart required to apply device changes"
            visible: false
        }
    }
}
```

**Step 2: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile. Audio settings page should render with new controls.

**Step 3: Commit**

```bash
git add qml/applications/settings/AudioSettings.qml
git commit -m "feat: migrate AudioSettings to touch-friendly controls"
```

---

### Task 9: Migrate Video Settings

**Files:**
- Modify: `qml/applications/settings/VideoSettings.qml` (full rewrite)

**Step 1: Rewrite VideoSettings.qml**

Replace with touch-friendly controls:
- FPS → SegmentedButton (30/60)
- Resolution → FullScreenPicker (480p/720p/1080p)
- DPI → SettingsSlider (80–400)
- Show Sidebar → SettingsToggle
- Sidebar Position → SegmentedButton (Left/Right)
- Sidebar Width → SettingsSlider (80–300)

Key changes from current:
- SettingsComboBox instances → SegmentedButton/FullScreenPicker
- SettingsSpinBox instances → SettingsSlider
- Replace inline section header text with proper SectionHeader
- Add restart info note as InfoBanner or italic text

**Step 2: Build and verify**

**Step 3: Commit**

```bash
git add qml/applications/settings/VideoSettings.qml
git commit -m "feat: migrate VideoSettings to touch-friendly controls"
```

---

### Task 10: Migrate Display Settings

**Files:**
- Modify: `qml/applications/settings/DisplaySettings.qml` (full rewrite)

**Step 1: Rewrite DisplaySettings.qml**

Key changes:
- Brightness → SettingsSlider (keep)
- Theme → ReadOnlyField with web hint
- Orientation → SegmentedButton (Landscape/Portrait)
- Day/Night Source → FullScreenPicker (time/gpio/none) — **must preserve `currentValue` binding for conditional visibility**
- Day Start / Night Start → Tumbler pair (HH:MM). Use two Tumblers side by side: one for hours (0-23), one for minutes (0-59). Read from config as "HH:MM" string, parse/join.
- GPIO Pin → Tumbler (0-27) with `visibleItemCount: 3`
- GPIO Active High → SettingsToggle (keep)

**Critical:** The `FullScreenPicker.currentValue` property drives conditional visibility of the time and GPIO sections. Verify this binding works correctly after migration.

**Step 2: Build and verify**

**Step 3: Commit**

```bash
git add qml/applications/settings/DisplaySettings.qml
git commit -m "feat: migrate DisplaySettings to touch-friendly controls"
```

---

### Task 11: Migrate Connection Settings

**Files:**
- Modify: `qml/applications/settings/ConnectionSettings.qml` (full rewrite)

**Step 1: Rewrite ConnectionSettings.qml**

Key changes:
- Auto-connect → SettingsToggle (keep)
- TCP Port → ReadOnlyField with web hint (was SettingsSpinBox)
- **Remove** WiFi Interface, SSID, Password fields entirely (web panel only)
- WiFi Channel → FullScreenPicker with channel list (1-13 for 2.4GHz, extend for 5GHz)
- WiFi Band → SegmentedButton ("2.4 GHz" / "5 GHz") with values ["g", "a"]
- BT Discoverable → SettingsToggle (keep)

This page gets significantly shorter after removing 3 text fields.

**Step 2: Build and verify**

**Step 3: Commit**

```bash
git add qml/applications/settings/ConnectionSettings.qml
git commit -m "feat: migrate ConnectionSettings - remove text fields, add touch controls"
```

---

### Task 12: Migrate System Settings

**Files:**
- Modify: `qml/applications/settings/SystemSettings.qml` (full rewrite)

**Step 1: Rewrite SystemSettings.qml**

Key changes:
- All SettingsTextField instances → ReadOnlyField
- Software Version → ReadOnlyField with `showWebHint: false` (it's always read-only)
- Left-Hand Drive → SettingsToggle (keep)
- Plugins placeholder → keep as-is

**Step 2: Build and verify**

**Step 3: Commit**

```bash
git add qml/applications/settings/SystemSettings.qml
git commit -m "feat: migrate SystemSettings to ReadOnlyField controls"
```

---

### Task 13: Migrate About Settings

**Files:**
- Modify: `qml/applications/settings/AboutSettings.qml`

**Step 1: Update AboutSettings.qml**

Lighter touch than other pages — this one mostly works:
- Fix hardcoded `spacing: 10` → `UiMetrics.gap` (lines ~117, ~142)
- Fix hardcoded `"#F44336"` colors → use ThemeService color or a defined danger color
- Make exit dialog button heights consistent at `UiMetrics.rowH`
- Ensure fonts use UiMetrics tokens

**Step 2: Build and verify**

**Step 3: Commit**

```bash
git add qml/applications/settings/AboutSettings.qml
git commit -m "fix: AboutSettings use UiMetrics tokens, consistent button sizing"
```

---

## Phase 3: Cleanup

### Task 14: Remove Retired Controls

**Files:**
- Delete: `qml/controls/SettingsComboBox.qml`
- Delete: `qml/controls/SettingsSpinBox.qml`
- Delete: `qml/controls/SettingsTextField.qml`
- Modify: `src/CMakeLists.txt` (remove registrations and QML_FILES entries)

**Step 1: Verify no remaining references**

Search the entire codebase for any remaining usage of the three retired controls:

```bash
grep -r "SettingsComboBox\|SettingsSpinBox\|SettingsTextField" --include="*.qml" qml/
```

Expected: No matches (all pages migrated in Phase 2).

**Step 2: Remove from CMakeLists.txt**

Remove the `set_source_files_properties` blocks for:
- `SettingsComboBox.qml` (lines 78-81)
- `SettingsTextField.qml` (lines 82-85)
- `SettingsSpinBox.qml` (lines 86-89)

Remove from `QML_FILES` list:
- `../qml/controls/SettingsComboBox.qml` (line 201)
- `../qml/controls/SettingsTextField.qml` (line 202)
- `../qml/controls/SettingsSpinBox.qml` (line 203)

**Step 3: Delete the files**

```bash
rm qml/controls/SettingsComboBox.qml qml/controls/SettingsSpinBox.qml qml/controls/SettingsTextField.qml
```

**Step 4: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc)`
Expected: Clean compile with no missing QML type errors.

**Step 5: Run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && ctest --output-on-failure`
Expected: All 17 tests pass.

**Step 6: Commit**

```bash
git add -u qml/controls/ src/CMakeLists.txt
git commit -m "chore: remove retired SettingsComboBox, SettingsSpinBox, SettingsTextField"
```

---

### Task 15: Final Verification and Pi Test

**Files:** None modified — verification only.

**Step 1: Full clean rebuild on dev VM**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)
```

**Step 2: Run all tests**

```bash
ctest --output-on-failure
```

Expected: All 17 tests pass.

**Step 3: Deploy to Pi and test**

Copy source files to Pi (NOT the build directory):

```bash
rsync -av --exclude='build/' --exclude='.git/' --exclude='libs/aasdk/' \
    /home/matt/claude/personal/openautopro/openauto-prodigy/ \
    matt@192.168.1.149:/home/matt/openauto-prodigy/
```

Build on Pi:

```bash
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && cmake .. && make -j3"
```

Launch and verify:
- [ ] Material Dark theme active
- [ ] All settings pages render correctly
- [ ] Touch targets feel good on 7" screen
- [ ] FullScreenPicker opens/closes, selects items
- [ ] SegmentedButton taps register correctly
- [ ] ReadOnlyField shows current values
- [ ] Sliders are easy to grab and drag
- [ ] Toggles respond to touch
- [ ] Day/night theme switching works
- [ ] Scrolling works on pages with overflow
- [ ] Non-settings UI (Shell, TopBar, launcher, AA, BT audio) unaffected

**Step 4: Note any issues for follow-up**

Document any visual glitches, touch target problems, or style conflicts discovered during Pi testing.

**Step 5: Final commit if needed**

```bash
git commit -m "fix: Pi testing adjustments for touch-friendly settings"
```
