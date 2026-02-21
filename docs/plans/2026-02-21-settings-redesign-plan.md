# Settings Page Redesign Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the flat disabled-tile settings grid with a functional two-level settings UI (6 category tiles → subpages with working controls).

**Architecture:** StackView-based navigation in SettingsMenu.qml. ConfigService exposed to QML as a context property with Q_INVOKABLE value()/setValue() and a configChanged signal. Six reusable settings control components (toggle, slider, combo, text field, spinbox, section header) that bind to config paths. Six subpage QML files, one per category.

**Tech Stack:** Qt 6 / QML (must compile on Qt 6.4 dev VM and Qt 6.8 Pi), C++ (ConfigService modifications), yaml-cpp (config persistence)

---

## Context for Implementers

**Codebase:** OpenAuto Prodigy — a Qt 6 car head unit app. Source is at `/home/matt/claude/personal/openautopro/openauto-prodigy/`.

**Build:** `mkdir build && cd build && cmake .. && make -j$(nproc)` — test with `ctest --output-on-failure` from build dir.

**Key files you'll touch:**
- `src/core/services/ConfigService.hpp` / `.cpp` — add Q_OBJECT, Q_INVOKABLE, signal
- `src/core/services/IConfigService.hpp` — interface (do NOT modify — ConfigService adds Q_OBJECT on top of the interface)
- `src/main.cpp` — register ConfigService as QML context property
- `src/CMakeLists.txt` — add new QML files to the module
- `qml/applications/settings/SettingsMenu.qml` — rewrite to StackView
- `qml/controls/` — new reusable settings controls
- `qml/applications/settings/` — new subpage QML files

**Styling convention:** All QML controls use `ThemeService.*` for colors (normalFontColor, controlBackgroundColor, controlForegroundColor, barBackgroundColor, highlightColor, descriptionFontColor). See existing controls like `Tile.qml` and `MaterialIcon.qml`.

**QML module registration:** Every new QML file needs a `set_source_files_properties()` block in `src/CMakeLists.txt` AND an entry in the `QML_FILES` list of `qt_add_qml_module()`. Follow the exact pattern of existing files.

**Config paths:** Use dot-notation paths like `"display.brightness"`, `"audio.master_volume"`. Full list in `docs/config-schema.md`. Read with `ConfigService.value("path")`, write with `ConfigService.setValue("path", value)`.

---

### Task 1: Make ConfigService QML-accessible

Expose ConfigService to QML so settings controls can read/write config values.

**Files:**
- Modify: `src/core/services/ConfigService.hpp`
- Modify: `src/core/services/ConfigService.cpp`
- Modify: `src/main.cpp:73,135-141`

**Step 1: Add Q_OBJECT macro and Q_INVOKABLE to ConfigService**

In `src/core/services/ConfigService.hpp`, change the class to inherit QObject and add Q_OBJECT:

```cpp
#pragma once

#include <QObject>
#include "IConfigService.hpp"

namespace oap {

class YamlConfig;

/// Concrete IConfigService wrapping YamlConfig.
/// Single writer — only one ConfigService instance should exist.
/// Does NOT own the YamlConfig (caller manages lifetime).
class ConfigService : public QObject, public IConfigService {
    Q_OBJECT
public:
    explicit ConfigService(YamlConfig* config, const QString& configPath);

    Q_INVOKABLE QVariant value(const QString& key) const override;
    Q_INVOKABLE void setValue(const QString& key, const QVariant& value) override;
    Q_INVOKABLE QVariant pluginValue(const QString& pluginId, const QString& key) const override;
    Q_INVOKABLE void setPluginValue(const QString& pluginId, const QString& key, const QVariant& value) override;
    Q_INVOKABLE void save() override;

signals:
    void configChanged(const QString& path, const QVariant& value);

private:
    YamlConfig* config_;
    QString configPath_;
};

} // namespace oap
```

**Step 2: Update ConfigService.cpp — emit signal, QObject constructor**

In `src/core/services/ConfigService.cpp`:

```cpp
#include "ConfigService.hpp"
#include "core/YamlConfig.hpp"

namespace oap {

ConfigService::ConfigService(YamlConfig* config, const QString& configPath)
    : QObject(nullptr), config_(config), configPath_(configPath)
{
}

QVariant ConfigService::value(const QString& key) const
{
    return config_->valueByPath(key);
}

void ConfigService::setValue(const QString& key, const QVariant& val)
{
    if (config_->setValueByPath(key, val)) {
        emit configChanged(key, val);
    }
}

QVariant ConfigService::pluginValue(const QString& pluginId, const QString& key) const
{
    return config_->pluginValue(pluginId, key);
}

void ConfigService::setPluginValue(const QString& pluginId, const QString& key, const QVariant& value)
{
    config_->setPluginValue(pluginId, key, value);
}

void ConfigService::save()
{
    config_->save(configPath_);
}

} // namespace oap
```

**Step 3: Register ConfigService as QML context property in main.cpp**

In `src/main.cpp`, the existing `std::make_unique` on line 73 stays. No ownership change needed — `unique_ptr` works fine with QObject.

**Important:** Keep ConfigService as `std::unique_ptr`. Do NOT change to raw `new`. Pass `.get()` to both hostContext and setContextProperty. The line 75 `hostContext->setConfigService(configService.get())` stays exactly as-is.

Add the context property registration after line 141 (after the other setContextProperty calls):
```cpp
engine.rootContext()->setContextProperty("ConfigService", configService.get());
```

**Step 4: Build and run tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All 15 tests pass. The test_config_service test may need updating if it constructs ConfigService without a QObject parent — check and add `nullptr` as third arg if needed.

**Step 5: Commit**

```bash
git add src/core/services/ConfigService.hpp src/core/services/ConfigService.cpp src/main.cpp
git commit -m "feat: expose ConfigService to QML with Q_INVOKABLE and configChanged signal"
```

---

### Task 2: Create reusable settings controls

Build the 6 reusable QML controls that all settings subpages will use.

**Files:**
- Create: `qml/controls/SectionHeader.qml`
- Create: `qml/controls/SettingsToggle.qml`
- Create: `qml/controls/SettingsSlider.qml`
- Create: `qml/controls/SettingsComboBox.qml`
- Create: `qml/controls/SettingsTextField.qml`
- Create: `qml/controls/SettingsSpinBox.qml`
- Modify: `src/CMakeLists.txt` — register all 6 new QML files

**Step 1: Create SectionHeader.qml**

File: `qml/controls/SectionHeader.qml`

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string text: ""

    Layout.fillWidth: true
    Layout.topMargin: 16
    Layout.bottomMargin: 4
    implicitHeight: 32

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        text: root.text
        font.pixelSize: 13
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: ThemeService.descriptionFontColor
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: ThemeService.descriptionFontColor
        opacity: 0.3
    }
}
```

**Step 2: Create SettingsToggle.qml**

File: `qml/controls/SettingsToggle.qml`

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property bool restartRequired: false

    Layout.fillWidth: true
    implicitHeight: 48

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        Text {
            text: root.label
            font.pixelSize: 15
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"  // restart_alt
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        Switch {
            id: toggle
            checked: ConfigService.value(root.configPath) === true
            onToggled: {
                ConfigService.setValue(root.configPath, checked)
                ConfigService.save()
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "")
            toggle.checked = ConfigService.value(root.configPath) === true
    }
}
```

**Step 3: Create SettingsSlider.qml**

File: `qml/controls/SettingsSlider.qml`

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property bool restartRequired: false

    Layout.fillWidth: true
    implicitHeight: 48

    Timer {
        id: debounce
        interval: 300
        onTriggered: {
            ConfigService.setValue(root.configPath, slider.value)
            ConfigService.save()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        Text {
            text: root.label
            font.pixelSize: 15
            color: ThemeService.normalFontColor
            Layout.preferredWidth: root.width * 0.3
        }

        MaterialIcon {
            icon: "\ue86a"
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        Slider {
            id: slider
            Layout.fillWidth: true
            from: root.from
            to: root.to
            stepSize: root.stepSize
            onMoved: debounce.restart()
        }

        Text {
            text: Math.round(slider.value * 100) / 100
            font.pixelSize: 14
            color: ThemeService.descriptionFontColor
            Layout.preferredWidth: 40
            horizontalAlignment: Text.AlignRight
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                slider.value = v
        }
    }
}
```

**Step 4: Create SettingsComboBox.qml**

File: `qml/controls/SettingsComboBox.qml`

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property var options: []      // display strings: ["30", "60"] or ["time", "gpio", "none"]
    property var values: []       // typed values to store: [30, 60] — if empty, stores options strings
    property bool restartRequired: false

    // Expose current index so parent pages can drive conditional visibility
    property alias currentIndex: combo.currentIndex
    readonly property var currentValue: root.values.length > 0 ? root.values[combo.currentIndex] : root.options[combo.currentIndex]

    Layout.fillWidth: true
    implicitHeight: 48

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        Text {
            text: root.label
            font.pixelSize: 15
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        ComboBox {
            id: combo
            model: root.options
            Layout.preferredWidth: 160
            onActivated: {
                var writeVal = root.values.length > 0 ? root.values[currentIndex] : currentText
                ConfigService.setValue(root.configPath, writeVal)
                ConfigService.save()
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var current = ConfigService.value(root.configPath)
            // Match against typed values first, then display strings
            var idx = -1
            if (root.values.length > 0) {
                for (var i = 0; i < root.values.length; i++) {
                    if (root.values[i] == current) { idx = i; break }
                }
            }
            if (idx < 0)
                idx = root.options.indexOf(String(current))
            if (idx >= 0) combo.currentIndex = idx
        }
    }
}
```

**Usage with typed values (for int config keys like FPS):**
```qml
SettingsComboBox {
    label: "FPS"
    configPath: "video.fps"
    options: ["30", "60"]     // display strings
    values: [30, 60]          // typed int values stored to config
    restartRequired: true
}
```

**Usage with string values (default — values list empty):**
```qml
SettingsComboBox {
    label: "Source"
    configPath: "sensors.night_mode.source"
    options: ["time", "gpio", "none"]  // stores these strings directly
}
```

**Step 5: Create SettingsTextField.qml**

File: `qml/controls/SettingsTextField.qml`

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property string placeholder: ""
    property bool restartRequired: false
    property bool readOnly: false

    Layout.fillWidth: true
    implicitHeight: 48

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        Text {
            text: root.label
            font.pixelSize: 15
            color: ThemeService.normalFontColor
            Layout.preferredWidth: root.width * 0.35
        }

        MaterialIcon {
            icon: "\ue86a"
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: root.placeholder
            readOnly: root.readOnly
            color: ThemeService.normalFontColor
            font.pixelSize: 14
            background: Rectangle {
                color: ThemeService.controlBackgroundColor
                border.color: field.activeFocus ? ThemeService.highlightColor : ThemeService.controlForegroundColor
                border.width: 1
                radius: 4
            }
            onEditingFinished: {
                if (!root.readOnly) {
                    ConfigService.setValue(root.configPath, text)
                    ConfigService.save()
                }
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                field.text = String(v)
        }
    }
}
```

**Step 6: Create SettingsSpinBox.qml**

File: `qml/controls/SettingsSpinBox.qml`

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property int from: 0
    property int to: 99999
    property bool restartRequired: false

    Layout.fillWidth: true
    implicitHeight: 48

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        Text {
            text: root.label
            font.pixelSize: 15
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        SpinBox {
            id: spin
            from: root.from
            to: root.to
            editable: true
            Layout.preferredWidth: 140
            onValueModified: {
                ConfigService.setValue(root.configPath, value)
                ConfigService.save()
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                spin.value = Number(v)
        }
    }
}
```

**Step 7: Register all 6 new QML files in src/CMakeLists.txt**

Add these `set_source_files_properties` blocks after the existing Tile.qml block (after line 63):

```cmake
set_source_files_properties(../qml/controls/SectionHeader.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SectionHeader"
    QT_RESOURCE_ALIAS "SectionHeader.qml"
)
set_source_files_properties(../qml/controls/SettingsToggle.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsToggle"
    QT_RESOURCE_ALIAS "SettingsToggle.qml"
)
set_source_files_properties(../qml/controls/SettingsSlider.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsSlider"
    QT_RESOURCE_ALIAS "SettingsSlider.qml"
)
set_source_files_properties(../qml/controls/SettingsComboBox.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsComboBox"
    QT_RESOURCE_ALIAS "SettingsComboBox.qml"
)
set_source_files_properties(../qml/controls/SettingsTextField.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsTextField"
    QT_RESOURCE_ALIAS "SettingsTextField.qml"
)
set_source_files_properties(../qml/controls/SettingsSpinBox.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SettingsSpinBox"
    QT_RESOURCE_ALIAS "SettingsSpinBox.qml"
)
```

And add them to the `QML_FILES` list (after the Tile.qml entry):

```cmake
        ../qml/controls/SectionHeader.qml
        ../qml/controls/SettingsToggle.qml
        ../qml/controls/SettingsSlider.qml
        ../qml/controls/SettingsComboBox.qml
        ../qml/controls/SettingsTextField.qml
        ../qml/controls/SettingsSpinBox.qml
```

**Step 8: Build and verify**

Run: `cd build && cmake .. && make -j$(nproc)`
Expected: Builds without errors. Tests still pass.

**Step 9: Commit**

```bash
git add qml/controls/SectionHeader.qml qml/controls/SettingsToggle.qml qml/controls/SettingsSlider.qml qml/controls/SettingsComboBox.qml qml/controls/SettingsTextField.qml qml/controls/SettingsSpinBox.qml src/CMakeLists.txt
git commit -m "feat: add reusable settings control components (toggle, slider, combo, text, spinbox, header)"
```

---

### Task 3: Rewrite SettingsMenu.qml with StackView navigation

Replace the flat tile grid with a StackView. The tile grid becomes the initial item, and tapping a tile pushes the corresponding subpage.

**Files:**
- Modify: `qml/applications/settings/SettingsMenu.qml`

**Step 1: Rewrite SettingsMenu.qml**

Replace the entire contents of `qml/applications/settings/SettingsMenu.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: settingsMenu

    Component.onCompleted: ApplicationController.setTitle("Settings")

    StackView {
        id: settingsStack
        anchors.fill: parent
        initialItem: tileGrid

        // When stack pops back to tile grid, reset title
        onCurrentItemChanged: {
            if (depth === 1)
                ApplicationController.setTitle("Settings")
        }
    }

    Component {
        id: tileGrid

        Item {
            GridLayout {
                anchors.centerIn: parent
                columns: 3
                rowSpacing: 20
                columnSpacing: 20

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "Display"
                    tileIcon: "\ueb97"
                    onClicked: {
                        ApplicationController.setTitle("Settings > Display")
                        settingsStack.push(displayPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "Audio"
                    tileIcon: "\ue050"
                    onClicked: {
                        ApplicationController.setTitle("Settings > Audio")
                        settingsStack.push(audioPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "Connection"
                    tileIcon: "\ue63e"  // wifi
                    onClicked: {
                        ApplicationController.setTitle("Settings > Connection")
                        settingsStack.push(connectionPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "Video"
                    tileIcon: "\ue04b"  // videocam
                    onClicked: {
                        ApplicationController.setTitle("Settings > Video")
                        settingsStack.push(videoPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "System"
                    tileIcon: "\uf8cd"
                    onClicked: {
                        ApplicationController.setTitle("Settings > System")
                        settingsStack.push(systemPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "About"
                    tileIcon: "\ue88e"
                    onClicked: {
                        ApplicationController.setTitle("Settings > About")
                        settingsStack.push(aboutPage)
                    }
                }
            }
        }
    }

    Component { id: displayPage; DisplaySettings {} }
    Component { id: audioPage; AudioSettings {} }
    Component { id: connectionPage; ConnectionSettings {} }
    Component { id: videoPage; VideoSettings {} }
    Component { id: systemPage; SystemSettings {} }
    Component { id: aboutPage; AboutSettings {} }
}
```

**Step 2: Build** (will fail — subpage QML files don't exist yet)

This is expected. We'll create them in the next tasks. For now, create placeholder files so it compiles.

Create 6 placeholder files (each identical pattern):

`qml/applications/settings/DisplaySettings.qml`:
```qml
import QtQuick
Item { Text { text: "Display Settings"; color: ThemeService.normalFontColor; anchors.centerIn: parent } }
```

Repeat for `AudioSettings.qml`, `ConnectionSettings.qml`, `VideoSettings.qml`, `SystemSettings.qml`, `AboutSettings.qml` (change the text string in each).

**Step 3: Register all 6 new subpage QML files in src/CMakeLists.txt**

Add `set_source_files_properties` blocks:

```cmake
set_source_files_properties(../qml/applications/settings/DisplaySettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "DisplaySettings"
    QT_RESOURCE_ALIAS "DisplaySettings.qml"
)
set_source_files_properties(../qml/applications/settings/AudioSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AudioSettings"
    QT_RESOURCE_ALIAS "AudioSettings.qml"
)
set_source_files_properties(../qml/applications/settings/ConnectionSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ConnectionSettings"
    QT_RESOURCE_ALIAS "ConnectionSettings.qml"
)
set_source_files_properties(../qml/applications/settings/VideoSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "VideoSettings"
    QT_RESOURCE_ALIAS "VideoSettings.qml"
)
set_source_files_properties(../qml/applications/settings/SystemSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "SystemSettings"
    QT_RESOURCE_ALIAS "SystemSettings.qml"
)
set_source_files_properties(../qml/applications/settings/AboutSettings.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "AboutSettings"
    QT_RESOURCE_ALIAS "AboutSettings.qml"
)
```

And add to `QML_FILES`:

```cmake
        ../qml/applications/settings/DisplaySettings.qml
        ../qml/applications/settings/AudioSettings.qml
        ../qml/applications/settings/ConnectionSettings.qml
        ../qml/applications/settings/VideoSettings.qml
        ../qml/applications/settings/SystemSettings.qml
        ../qml/applications/settings/AboutSettings.qml
```

**Step 4: Build and verify**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: Builds and all tests pass. The settings page now shows a 3x2 tile grid, and tapping a tile pushes a placeholder page.

**Step 5: Commit**

```bash
git add qml/applications/settings/SettingsMenu.qml qml/applications/settings/DisplaySettings.qml qml/applications/settings/AudioSettings.qml qml/applications/settings/ConnectionSettings.qml qml/applications/settings/VideoSettings.qml qml/applications/settings/SystemSettings.qml qml/applications/settings/AboutSettings.qml src/CMakeLists.txt
git commit -m "feat: settings StackView navigation with 6 category tiles and placeholder subpages"
```

---

### Task 4: Implement subpage back navigation

Each subpage needs a back button to return to the tile grid. Create a shared back-button header component.

**Files:**
- Create: `qml/controls/SettingsPageHeader.qml`
- Modify: `src/CMakeLists.txt`

**Step 1: Create SettingsPageHeader.qml**

File: `qml/controls/SettingsPageHeader.qml`

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string title: ""
    property StackView stack: null

    Layout.fillWidth: true
    implicitHeight: 40

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        spacing: 8

        Rectangle {
            width: 36
            height: 36
            radius: 18
            color: backMouse.containsMouse ? ThemeService.highlightColor : "transparent"

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5c4"  // arrow_back
                size: 24
                color: ThemeService.normalFontColor
            }

            MouseArea {
                id: backMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (root.stack) root.stack.pop()
                }
            }
        }

        Text {
            text: root.title
            font.pixelSize: 18
            font.bold: true
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: ThemeService.descriptionFontColor
        opacity: 0.2
    }
}
```

**Step 2: Register in CMakeLists.txt**

Add `set_source_files_properties` and `QML_FILES` entry (same pattern as other controls).

**Step 3: Build and verify**

Run: `cd build && cmake .. && make -j$(nproc)`
Expected: Builds without errors.

**Step 4: Commit**

```bash
git add qml/controls/SettingsPageHeader.qml src/CMakeLists.txt
git commit -m "feat: add SettingsPageHeader with back navigation"
```

---

### Task 5: Implement DisplaySettings subpage

Replace the placeholder with the full Display settings page.

**Files:**
- Modify: `qml/applications/settings/DisplaySettings.qml`

**Step 1: Write DisplaySettings.qml**

Replace `qml/applications/settings/DisplaySettings.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // Access the parent StackView for back navigation
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 4

        SettingsPageHeader {
            title: "Display"
            stack: root.stackRef
        }

        SectionHeader { text: "General" }

        SettingsSlider {
            label: "Brightness"
            configPath: "display.brightness"
            from: 0; to: 100; stepSize: 1
        }

        SettingsComboBox {
            label: "Orientation"
            configPath: "display.orientation"
            options: ["landscape", "portrait"]
        }

        SectionHeader { text: "Day / Night Mode" }

        SettingsComboBox {
            id: nightSource
            label: "Source"
            configPath: "sensors.night_mode.source"
            options: ["time", "gpio", "none"]
        }

        SettingsTextField {
            label: "Day starts at"
            configPath: "sensors.night_mode.day_start"
            placeholder: "HH:MM"
            visible: nightSource.currentValue === "time"
        }

        SettingsTextField {
            label: "Night starts at"
            configPath: "sensors.night_mode.night_start"
            placeholder: "HH:MM"
            visible: nightSource.currentValue === "time"
        }

        SettingsSpinBox {
            label: "GPIO Pin"
            configPath: "sensors.night_mode.gpio_pin"
            from: 0; to: 40
            visible: nightSource.currentValue === "gpio"
        }

        SettingsToggle {
            label: "GPIO Active High"
            configPath: "sensors.night_mode.gpio_active_high"
            visible: nightSource.currentValue === "gpio"
        }
    }
}
```

**Note:** Conditional visibility for GPIO vs time fields is driven by `nightSource.currentValue`, which is a reactive binding to the combo's `currentIndex`. This re-evaluates immediately when the user changes the combo selection — no need for configChanged signals here.

**Step 2: Build and verify visually**

Run: `cd build && cmake .. && make -j$(nproc)`
Expected: Builds. Navigate to Settings > Display on the running app to verify the controls render.

**Step 3: Commit**

```bash
git add qml/applications/settings/DisplaySettings.qml
git commit -m "feat: implement Display settings subpage"
```

---

### Task 6: Implement AudioSettings subpage

**Files:**
- Modify: `qml/applications/settings/AudioSettings.qml`

**Step 1: Write AudioSettings.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 4

        SettingsPageHeader {
            title: "Audio"
            stack: root.stackRef
        }

        SettingsSlider {
            label: "Master Volume"
            configPath: "audio.master_volume"
            from: 0; to: 100; stepSize: 1
        }

        SettingsTextField {
            label: "Output Device"
            configPath: "audio.output_device"
            placeholder: "auto"
        }

        SettingsTextField {
            label: "Microphone"
            configPath: "audio.microphone.device"
            placeholder: "auto"
        }

        SettingsSlider {
            label: "Mic Gain"
            configPath: "audio.microphone.gain"
            from: 0.5; to: 4.0; stepSize: 0.1
        }
    }
}
```

**Step 2: Build, verify, commit**

```bash
cd build && cmake .. && make -j$(nproc)
git add qml/applications/settings/AudioSettings.qml
git commit -m "feat: implement Audio settings subpage"
```

---

### Task 7: Implement ConnectionSettings subpage

**Files:**
- Modify: `qml/applications/settings/ConnectionSettings.qml`

**Step 1: Write ConnectionSettings.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 4

        SettingsPageHeader {
            title: "Connection"
            stack: root.stackRef
        }

        SectionHeader { text: "Android Auto" }

        SettingsToggle {
            label: "Auto-connect"
            configPath: "connection.auto_connect_aa"
        }

        SettingsSpinBox {
            label: "TCP Port"
            configPath: "connection.tcp_port"
            from: 1024; to: 65535
            restartRequired: true
        }

        SectionHeader { text: "WiFi Access Point" }

        SettingsTextField {
            label: "Interface"
            configPath: "connection.wifi_ap.interface"
            restartRequired: true
        }

        SettingsTextField {
            label: "SSID"
            configPath: "connection.wifi_ap.ssid"
            restartRequired: true
        }

        SettingsTextField {
            label: "Password"
            configPath: "connection.wifi_ap.password"
            restartRequired: true
        }

        SettingsSpinBox {
            label: "Channel"
            configPath: "connection.wifi_ap.channel"
            from: 1; to: 165
            restartRequired: true
        }

        SettingsComboBox {
            label: "Band"
            configPath: "connection.wifi_ap.band"
            options: ["a", "g"]
            restartRequired: true
        }

        SectionHeader { text: "Bluetooth" }

        SettingsToggle {
            label: "Discoverable"
            configPath: "connection.bt_discoverable"
        }
    }
}
```

**Step 2: Build, verify, commit**

```bash
cd build && cmake .. && make -j$(nproc)
git add qml/applications/settings/ConnectionSettings.qml
git commit -m "feat: implement Connection settings subpage"
```

---

### Task 8: Implement VideoSettings subpage

**Files:**
- Modify: `qml/applications/settings/VideoSettings.qml`

**Step 1: Write VideoSettings.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 4

        SettingsPageHeader {
            title: "Video"
            stack: root.stackRef
        }

        SettingsComboBox {
            label: "FPS"
            configPath: "video.fps"
            options: ["30", "60"]
            values: [30, 60]          // typed int values — prevents string/int drift
            restartRequired: true
        }

        SettingsComboBox {
            label: "Resolution"
            configPath: "video.resolution"
            options: ["480p", "720p", "1080p"]
            restartRequired: true
        }

        SettingsSpinBox {
            label: "DPI"
            configPath: "video.dpi"
            from: 80; to: 400
            restartRequired: true
        }
    }
}
```

**Step 2: Build, verify, commit**

```bash
cd build && cmake .. && make -j$(nproc)
git add qml/applications/settings/VideoSettings.qml
git commit -m "feat: implement Video settings subpage"
```

---

### Task 9: Implement SystemSettings subpage

**Files:**
- Modify: `qml/applications/settings/SystemSettings.qml`

**Step 1: Write SystemSettings.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 4

        SettingsPageHeader {
            title: "System"
            stack: root.stackRef
        }

        SectionHeader { text: "Identity" }

        SettingsTextField {
            label: "Head Unit Name"
            configPath: "identity.head_unit_name"
        }

        SettingsTextField {
            label: "Manufacturer"
            configPath: "identity.manufacturer"
        }

        SettingsTextField {
            label: "Model"
            configPath: "identity.model"
        }

        SettingsTextField {
            label: "Software Version"
            configPath: "identity.sw_version"
            readOnly: true
        }

        SettingsTextField {
            label: "Car Model"
            configPath: "identity.car_model"
            placeholder: "(optional)"
        }

        SettingsTextField {
            label: "Car Year"
            configPath: "identity.car_year"
            placeholder: "(optional)"
        }

        SettingsToggle {
            label: "Left-Hand Drive"
            configPath: "identity.left_hand_drive"
        }

        SectionHeader { text: "Hardware" }

        SettingsTextField {
            label: "Hardware Profile"
            configPath: "hardware_profile"
        }

        SettingsTextField {
            label: "Touch Device"
            configPath: "touch.device"
            placeholder: "(auto-detect)"
        }
    }
}
```

**Step 2: Build, verify, commit**

```bash
cd build && cmake .. && make -j$(nproc)
git add qml/applications/settings/SystemSettings.qml
git commit -m "feat: implement System settings subpage"
```

---

### Task 10: Implement AboutSettings subpage

Move the exit dialog from the old SettingsMenu into the About page.

**Files:**
- Modify: `qml/applications/settings/AboutSettings.qml`

**Step 1: Write AboutSettings.qml**

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property StackView stackRef: StackView.view

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12

        SettingsPageHeader {
            title: "About"
            stack: root.stackRef
        }

        // Version info
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 16
            spacing: 8

            Text {
                text: "OpenAuto Prodigy"
                font.pixelSize: 22
                font.bold: true
                color: ThemeService.normalFontColor
            }

            Text {
                text: "Version " + (ConfigService.value("identity.sw_version") || "0.0.0")
                font.pixelSize: 15
                color: ThemeService.descriptionFontColor
            }

            Text {
                text: "Open-source Android Auto head unit"
                font.pixelSize: 13
                color: ThemeService.descriptionFontColor
            }

            Text {
                text: "License: GPLv3"
                font.pixelSize: 13
                color: ThemeService.descriptionFontColor
            }
        }

        // Exit section
        Item { Layout.fillWidth: true; Layout.preferredHeight: 24 }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 48
            onClicked: exitDialog.open()
            contentItem: RowLayout {
                spacing: 8
                Item { Layout.fillWidth: true }
                MaterialIcon {
                    icon: "\ue5cd"
                    size: 20
                    color: ThemeService.normalFontColor
                }
                Text {
                    text: "Exit App"
                    font.pixelSize: 16
                    color: ThemeService.normalFontColor
                }
                Item { Layout.fillWidth: true }
            }
            background: Rectangle {
                color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                radius: 8
            }
        }
    }

    // Exit confirmation dialog (moved from old SettingsMenu)
    Dialog {
        id: exitDialog
        anchors.centerIn: parent
        width: 320
        modal: true
        title: "Exit"

        background: Rectangle {
            color: ThemeService.controlBackgroundColor
            radius: 12
            border.color: ThemeService.controlForegroundColor
            border.width: 1
        }

        header: Item {
            height: 48
            Text {
                anchors.centerIn: parent
                text: "Exit OpenAuto Prodigy?"
                font.pixelSize: 18
                font.bold: true
                color: ThemeService.normalFontColor
            }
        }

        contentItem: ColumnLayout {
            spacing: 12

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: {
                    exitDialog.close()
                    ApplicationController.minimize()
                }
                contentItem: RowLayout {
                    spacing: 10
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\ue5cd"
                        size: 20
                        color: ThemeService.normalFontColor
                    }
                    Text {
                        text: "Minimize"
                        font.pixelSize: 16
                        color: ThemeService.normalFontColor
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                    radius: 8
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: ApplicationController.quit()
                contentItem: RowLayout {
                    spacing: 10
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\ue5cd"
                        size: 20
                        color: "#F44336"
                    }
                    Text {
                        text: "Close App"
                        font.pixelSize: 16
                        color: "#F44336"
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.pressed ? "#F44336" : ThemeService.barBackgroundColor
                    radius: 8
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                onClicked: exitDialog.close()
                contentItem: Text {
                    text: "Cancel"
                    font.pixelSize: 14
                    color: ThemeService.descriptionFontColor
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: "transparent"
                    radius: 8
                }
            }
        }

        footer: Item { height: 0 }
    }
}
```

**Step 2: Build, verify, commit**

```bash
cd build && cmake .. && make -j$(nproc)
git add qml/applications/settings/AboutSettings.qml
git commit -m "feat: implement About settings subpage with exit dialog"
```

---

### Task 11: Final verification and cleanup

Build, run all tests, deploy to Pi, and verify the full settings flow works.

**Step 1: Full build and test on dev VM**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass.

**Step 2: Deploy and build on Pi**

```bash
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy && git pull && cd build && cmake .. && cmake --build . -j3"
```

**Step 3: Run tests on Pi**

```bash
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && ctest --output-on-failure"
```
Expected: All tests pass.

**Step 4: Launch on Pi and test**

```bash
ssh -f matt@192.168.1.149 'cd /home/matt/openauto-prodigy && WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 ./build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'
```

Verify:
- Settings tile grid shows 3x2 layout (Display, Audio, Connection, Video, System, About)
- Tapping each tile navigates to the correct subpage
- Back button returns to tile grid
- Title bar updates correctly
- Controls render and read current config values
- Changing a value writes to config (check `~/.openauto/config.yaml` on Pi)
- Restart badges appear on WiFi/Video settings
- Exit dialog works from About page

**Step 5: Commit any fixes**

If any adjustments are needed, commit them with descriptive messages.
