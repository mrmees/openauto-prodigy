# Settings UI Restructure Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the settings tile grid with a scrollable list and add plugin settings slot.

**Architecture:** Replace `SettingsMenu.qml` tile grid with a `ListView` using inline `ListModel`. Add `SettingsListItem.qml` control for row rendering. Add `settingsQml` role to `PluginModel` so plugins can contribute settings pages. Restructure AudioSettings section headers and convert AboutSettings to Flickable.

**Tech Stack:** Qt 6 QML, C++ (PluginModel), CMake, QtTest

**Design doc:** `docs/plans/2026-02-27-settings-restructure-design.md`

---

### Task 1: Add settingsQml role to PluginModel

**Files:**
- Modify: `src/ui/PluginModel.hpp:25-33`
- Modify: `src/ui/PluginModel.cpp:29-47` (data method) and `49-60` (roleNames method)
- Test: `tests/test_plugin_model.cpp`

**Step 1: Write the failing test**

Add to `tests/test_plugin_model.cpp`, new test slot in `TestPluginModel`:

```cpp
void testSettingsQmlRole()
{
    auto* p = new MockPlugin("test.a", "A", this);
    MockHostContext ctx;
    oap::PluginManager manager;
    QQmlEngine engine;

    manager.registerStaticPlugin(p);
    manager.initializeAll(&ctx);

    oap::PluginModel model(&manager, &engine);
    QModelIndex idx = model.index(0, 0);

    // MockPlugin returns empty QUrl from settingsComponent()
    QVariant val = model.data(idx, oap::PluginModel::SettingsQmlRole);
    QVERIFY(val.isValid());
    QCOMPARE(val.toUrl(), QUrl());
}
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . -j$(nproc) && ctest -R test_plugin_model --output-on-failure`
Expected: Compile error — `SettingsQmlRole` not defined.

**Step 3: Add the role to PluginModel**

In `src/ui/PluginModel.hpp`, add to the `Roles` enum after `WantsFullscreenRole`:

```cpp
WantsFullscreenRole,
SettingsQmlRole
```

In `src/ui/PluginModel.cpp`, add to the `data()` switch:

```cpp
case SettingsQmlRole:    return plugin->settingsComponent();
```

In `src/ui/PluginModel.cpp`, add to `roleNames()`:

```cpp
{SettingsQmlRole,     "settingsQml"}
```

**Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . -j$(nproc) && ctest -R test_plugin_model --output-on-failure`
Expected: All plugin model tests pass including the new one.

**Step 5: Commit**

```bash
git add src/ui/PluginModel.hpp src/ui/PluginModel.cpp tests/test_plugin_model.cpp
git commit -m "feat(settings): add settingsQml role to PluginModel"
```

---

### Task 2: Create SettingsListItem.qml control

**Files:**
- Create: `qml/controls/SettingsListItem.qml`

**Step 1: Create the component**

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string icon: ""
    property string label: ""
    property string subtitle: ""

    signal clicked()

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginPage
        anchors.rightMargin: UiMetrics.marginPage
        spacing: UiMetrics.gap

        MaterialIcon {
            icon: root.icon
            size: UiMetrics.iconSize
            color: ThemeService.normalFontColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                text: root.label
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.normalFontColor
                Layout.fillWidth: true
            }

            Text {
                text: root.subtitle
                font.pixelSize: UiMetrics.fontTiny
                color: ThemeService.descriptionFontColor
                Layout.fillWidth: true
                visible: root.subtitle !== ""
            }
        }

        MaterialIcon {
            icon: "\ue5cc"
            size: UiMetrics.iconSmall
            color: ThemeService.descriptionFontColor
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.marginPage
        anchors.rightMargin: UiMetrics.marginPage
        height: 1
        color: ThemeService.descriptionFontColor
        opacity: 0.15
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
```

**Step 2: Verify build**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Build succeeds. QML files are loaded at runtime so no compile-time validation, but ensure no CMake resource issues.

**Step 3: Commit**

```bash
git add qml/controls/SettingsListItem.qml
git commit -m "feat(settings): add SettingsListItem.qml control"
```

---

### Task 3: Replace tile grid with ListView in SettingsMenu.qml

**Files:**
- Modify: `qml/applications/settings/SettingsMenu.qml` (full rewrite)

**Context:** The current file uses a `GridLayout` of `Tile` components (lines 27-114) inside a `StackView`. The `StackView` and back-navigation `Connections` block stay. Only the `tileGrid` component and its contents are replaced.

**Step 1: Rewrite SettingsMenu.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: settingsMenu

    Component.onCompleted: ApplicationController.setTitle("Settings")

    Connections {
        target: ApplicationController
        function onBackRequested() {
            if (settingsStack.depth > 1) {
                settingsStack.pop()
                ApplicationController.setTitle("Settings")
                ApplicationController.setBackHandled(true)
            }
        }
    }

    StackView {
        id: settingsStack
        anchors.fill: parent
        initialItem: settingsList
    }

    Component {
        id: settingsList

        ListView {
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            model: ListModel {
                id: settingsListModel
            }

            delegate: Loader {
                width: ListView.view.width
                sourceComponent: type === "section" ? sectionDelegate : itemDelegate
                required property string type
                required property string icon
                required property string label
                required property string subtitle
                required property string pageId
            }

            Component.onCompleted: {
                // General
                settingsListModel.append({ type: "section", icon: "", label: "General", subtitle: "", pageId: "" })
                settingsListModel.append({ type: "item", icon: "\ueb97", label: "Display", subtitle: "Brightness, theme, night mode", pageId: "display" })
                settingsListModel.append({ type: "item", icon: "\ue050", label: "Audio", subtitle: "Volume, devices, microphone", pageId: "audio" })
                settingsListModel.append({ type: "item", icon: "\ue1d8", label: "Connection", subtitle: "WiFi, Bluetooth, Android Auto", pageId: "connection" })
                settingsListModel.append({ type: "item", icon: "\ue04b", label: "Video", subtitle: "Resolution, codecs, sidebar", pageId: "video" })
                settingsListModel.append({ type: "item", icon: "\uf8cd", label: "System", subtitle: "Identity, hardware, touch", pageId: "system" })

                // Companion
                settingsListModel.append({ type: "section", icon: "", label: "Companion", subtitle: "", pageId: "" })
                settingsListModel.append({ type: "item", icon: "\ue324", label: "Companion", subtitle: "Phone pairing, GPS, internet", pageId: "companion" })

                // Plugins — dynamic from PluginModel
                var pluginCount = 0
                if (typeof PluginModel !== "undefined") {
                    for (var i = 0; i < PluginModel.rowCount(); i++) {
                        var idx = PluginModel.index(i, 0)
                        var settingsUrl = PluginModel.data(idx, 264)  // SettingsQmlRole = UserRole+8 = 264
                        if (settingsUrl && settingsUrl.toString() !== "") {
                            if (pluginCount === 0) {
                                settingsListModel.append({ type: "section", icon: "", label: "Plugins", subtitle: "", pageId: "" })
                            }
                            var pluginName = PluginModel.data(idx, 258)  // PluginNameRole
                            var pluginIcon = PluginModel.data(idx, 260)  // PluginIconTextRole
                            var pluginId = PluginModel.data(idx, 257)    // PluginIdRole
                            settingsListModel.append({
                                type: "item",
                                icon: pluginIcon || "\ue8b8",
                                label: pluginName + " Settings",
                                subtitle: "",
                                pageId: "plugin:" + pluginId
                            })
                            pluginCount++
                        }
                    }
                }

                // About — always last
                settingsListModel.append({ type: "section", icon: "", label: "", subtitle: "", pageId: "" })
                settingsListModel.append({ type: "item", icon: "\ue88e", label: "About", subtitle: "Version, license, close app", pageId: "about" })
            }
        }
    }

    // --- Delegate components ---
    Component {
        id: sectionDelegate
        SectionHeader { text: label }
    }

    Component {
        id: itemDelegate
        SettingsListItem {
            icon: parent ? parent.icon : ""
            label: parent ? parent.label : ""
            subtitle: parent ? parent.subtitle : ""
            onClicked: openPage(parent ? parent.pageId : "")
        }
    }

    // --- Page components ---
    Component { id: displayPage; DisplaySettings {} }
    Component { id: audioPage; AudioSettings {} }
    Component { id: connectionPage; ConnectionSettings {} }
    Component { id: videoPage; VideoSettings {} }
    Component { id: systemPage; SystemSettings {} }
    Component { id: companionPage; CompanionSettings {} }
    Component { id: aboutPage; AboutSettings {} }

    function openPage(pageId) {
        var titles = {
            "display": "Display",
            "audio": "Audio",
            "connection": "Connection",
            "video": "Video",
            "system": "System",
            "companion": "Companion",
            "about": "About"
        }
        var pages = {
            "display": displayPage,
            "audio": audioPage,
            "connection": connectionPage,
            "video": videoPage,
            "system": systemPage,
            "companion": companionPage,
            "about": aboutPage
        }

        if (pageId.startsWith("plugin:")) {
            var pluginId = pageId.substring(7)
            // Find plugin settings URL from model
            for (var i = 0; i < PluginModel.rowCount(); i++) {
                var idx = PluginModel.index(i, 0)
                if (PluginModel.data(idx, 257) === pluginId) {
                    var settingsUrl = PluginModel.data(idx, 264)
                    ApplicationController.setTitle("Settings > " + PluginModel.data(idx, 258))
                    settingsStack.push(settingsUrl)
                    return
                }
            }
            return
        }

        if (pages[pageId]) {
            ApplicationController.setTitle("Settings > " + titles[pageId])
            settingsStack.push(pages[pageId])
        }
    }
}
```

**Important:** The magic numbers for role IDs (257, 258, 260, 264) are `Qt::UserRole + 1` through `Qt::UserRole + 8`. These match the `Roles` enum in `PluginModel.hpp`:
- `PluginIdRole` = 257 (UserRole+1)
- `PluginNameRole` = 258 (UserRole+2)
- `PluginIconTextRole` = 260 (UserRole+4)
- `SettingsQmlRole` = 264 (UserRole+8)

**Step 2: Verify build**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Build succeeds.

**Step 3: Run all tests**

Run: `cd build && ctest --output-on-failure`
Expected: All existing tests pass (QML changes are runtime, no compile impact).

**Step 4: Commit**

```bash
git add qml/applications/settings/SettingsMenu.qml
git commit -m "feat(settings): replace tile grid with scrollable ListView"
```

---

### Task 4: Restructure AudioSettings section headers

**Files:**
- Modify: `qml/applications/settings/AudioSettings.qml`

**Context:** Currently has "Volume" (brightness + mic gain) and "Devices" (output + input + restart banner). The mic gain is under "Volume" but logically belongs with the input device. Restructure to "Output" and "Microphone".

**Step 1: Rearrange sections**

Change the section headers and reorder controls:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SectionHeader { text: "Output" }

        SettingsSlider {
            label: "Master Volume"
            configPath: "audio.master_volume"
            from: 0; to: 100; stepSize: 1
            onMoved: {
                if (typeof AudioService !== "undefined")
                    AudioService.setMasterVolume(value)
            }
        }

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
                restartBanner.shown = true
            }
            Component.onCompleted: {
                if (typeof AudioOutputDeviceModel === "undefined") return
                var current = ConfigService.value("audio.output_device")
                if (!current) current = "auto"
                var idx = AudioOutputDeviceModel.indexOfDevice(current)
                if (idx >= 0) currentIndex = idx
            }
        }

        SectionHeader { text: "Microphone" }

        SettingsSlider {
            label: "Mic Gain"
            configPath: "audio.microphone.gain"
            from: 0.5; to: 4.0; stepSize: 0.1
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
                restartBanner.shown = true
            }
            Component.onCompleted: {
                if (typeof AudioInputDeviceModel === "undefined") return
                var current = ConfigService.value("audio.input_device")
                if (!current) current = "auto"
                var idx = AudioInputDeviceModel.indexOfDevice(current)
                if (idx >= 0) currentIndex = idx
            }
        }

        InfoBanner {
            id: restartBanner
            text: "Restart required to apply device changes"
            shown: false
        }
    }
}
```

**Step 2: Verify build**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Build succeeds.

**Step 3: Commit**

```bash
git add qml/applications/settings/AudioSettings.qml
git commit -m "refactor(settings): restructure AudioSettings into Output/Microphone sections"
```

---

### Task 5: Convert AboutSettings to Flickable

**Files:**
- Modify: `qml/applications/settings/AboutSettings.qml`

**Context:** Currently uses bare `Item` as root. Convert to `Flickable` + `ColumnLayout` for consistency with all other settings pages. Keep all existing content.

**Step 1: Convert to Flickable**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.gap

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: UiMetrics.sectionGap
            spacing: UiMetrics.marginRow

            Text {
                text: "OpenAuto Prodigy"
                font.pixelSize: UiMetrics.fontHeading
                font.bold: true
                color: ThemeService.normalFontColor
            }

            Text {
                text: "Version " + (ConfigService.value("identity.sw_version") || "0.0.0")
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.descriptionFontColor
            }

            Text {
                text: "Open-source Android Auto head unit"
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.descriptionFontColor
            }

            Text {
                text: "License: GPLv3"
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.descriptionFontColor
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: UiMetrics.marginPage + UiMetrics.marginRow }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.4
            Layout.preferredHeight: UiMetrics.rowH
            onClicked: exitDialog.open()
            contentItem: RowLayout {
                spacing: UiMetrics.marginRow
                Item { Layout.fillWidth: true }
                MaterialIcon {
                    icon: "\ue5cd"
                    size: UiMetrics.iconSize
                    color: ThemeService.normalFontColor
                }
                Text {
                    text: "Close App"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                }
                Item { Layout.fillWidth: true }
            }
            background: Rectangle {
                color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                radius: UiMetrics.radius
            }
        }
    }

    ExitDialog {
        id: exitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }
}
```

**Step 2: Verify build**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Build succeeds.

**Step 3: Commit**

```bash
git add qml/applications/settings/AboutSettings.qml
git commit -m "refactor(settings): convert AboutSettings to Flickable for consistency"
```

---

### Task 6: Run full test suite and verify

**Step 1: Build and run all tests**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass (47+ tests). `test_plugin_model` should include the new `testSettingsQmlRole` test.

**Step 2: Verify no regressions in test count**

Run: `cd build && ctest -N | tail -1`
Expected: "Total Tests: 48" (47 existing + 1 new settingsQml role test).

**Step 3: Commit plan doc and update session handoffs**

```bash
git add docs/plans/2026-02-27-settings-restructure.md
git commit -m "docs: add settings restructure implementation plan"
```
