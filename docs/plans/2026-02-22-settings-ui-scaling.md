# Settings UI Scaling Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace all hardcoded pixel values in settings UI with a centralized UiMetrics design token system so the UI scales correctly across different screen sizes.

**Architecture:** A QML singleton (`UiMetrics`) computes a clamped scale factor from the Window's shortest dimension vs a 600px baseline. All controls and settings pages reference tokens (`UiMetrics.rowH`, `UiMetrics.fontBody`, etc.) instead of magic numbers. Layout structure (Flickable + ColumnLayout) stays unchanged.

**Tech Stack:** Qt 6 QML, CMake (qt_add_qml_module)

---

### Task 1: Create UiMetrics singleton

**Files:**
- Create: `qml/controls/UiMetrics.qml`
- Modify: `src/CMakeLists.txt`

**Step 1: Create UiMetrics.qml**

```qml
pragma Singleton
import QtQuick
import QtQuick.Window

QtObject {
    // Scale from shortest screen edge vs 600px baseline.
    // Screen.height gives the physical display height even in fullscreen.
    readonly property real scale: {
        var shortest = Math.min(Screen.width, Screen.height);
        return Math.max(0.9, Math.min(1.35, shortest / 600));
    }

    // Touch targets
    readonly property int rowH:     Math.round(48 * scale)
    readonly property int touchMin: Math.round(44 * scale)

    // Spacing
    readonly property int marginPage: Math.round(16 * scale)
    readonly property int marginRow:  Math.round(8 * scale)
    readonly property int spacing:    Math.round(4 * scale)
    readonly property int gap:        Math.round(12 * scale)
    readonly property int sectionGap: Math.round(16 * scale)

    // Fonts
    readonly property int fontTitle:   Math.round(18 * scale)
    readonly property int fontBody:    Math.round(15 * scale)
    readonly property int fontSmall:   Math.round(13 * scale)
    readonly property int fontTiny:    Math.round(12 * scale)
    readonly property int fontHeading: Math.round(22 * scale)

    // Component sizing
    readonly property int headerH:     Math.round(40 * scale)
    readonly property int sectionH:    Math.round(32 * scale)
    readonly property int backBtnSize: Math.round(36 * scale)
    readonly property int iconSize:    Math.round(24 * scale)
    readonly property int iconSmall:   Math.round(16 * scale)

    // Radii
    readonly property int radius: Math.round(8 * scale)

    // Grid (settings menu tiles)
    readonly property int gridGap: Math.round(20 * scale)
}
```

**Step 2: Register in CMakeLists.txt**

Add `set_source_files_properties` block after SettingsPageHeader (line 93):

```cmake
set_source_files_properties(../qml/controls/UiMetrics.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "UiMetrics"
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS "UiMetrics.qml"
)
```

Add to QML_FILES list (after SettingsPageHeader.qml, line 199):

```
        ../qml/controls/UiMetrics.qml
```

**Step 3: Build and verify**

Run: `cd build && cmake .. && cmake --build . -j$(nproc)`
Expected: Builds clean with no errors.

**Step 4: Commit**

```
feat: add UiMetrics singleton for responsive UI scaling
```

---

### Task 2: Update settings controls (7 files)

**Files:**
- Modify: `qml/controls/SettingsComboBox.qml`
- Modify: `qml/controls/SettingsToggle.qml`
- Modify: `qml/controls/SettingsSpinBox.qml`
- Modify: `qml/controls/SettingsTextField.qml`
- Modify: `qml/controls/SettingsSlider.qml`
- Modify: `qml/controls/SettingsPageHeader.qml`
- Modify: `qml/controls/SectionHeader.qml`

**Step 1: Update SettingsComboBox.qml**

Replace all hardcoded values:
- `implicitHeight: 48` → `implicitHeight: UiMetrics.rowH`
- `anchors.leftMargin: 8` → `anchors.leftMargin: UiMetrics.marginRow`
- `anchors.rightMargin: 8` → `anchors.rightMargin: UiMetrics.marginRow`
- `spacing: 12` → `spacing: UiMetrics.gap`
- `font.pixelSize: 15` → `font.pixelSize: UiMetrics.fontBody`
- `size: 16` (MaterialIcon) → `size: UiMetrics.iconSmall`
- `Layout.preferredWidth: 160` → `Layout.preferredWidth: root.width * 0.35`

**Step 2: Update SettingsToggle.qml**

Same row pattern replacements:
- `implicitHeight: 48` → `implicitHeight: UiMetrics.rowH`
- `anchors.leftMargin: 8` → `anchors.leftMargin: UiMetrics.marginRow`
- `anchors.rightMargin: 8` → `anchors.rightMargin: UiMetrics.marginRow`
- `spacing: 12` → `spacing: UiMetrics.gap`
- `font.pixelSize: 15` → `font.pixelSize: UiMetrics.fontBody`
- `size: 16` → `size: UiMetrics.iconSmall`

**Step 3: Update SettingsSpinBox.qml**

Same row pattern + spinbox width:
- `implicitHeight: 48` → `implicitHeight: UiMetrics.rowH`
- `anchors.leftMargin: 8` → `anchors.leftMargin: UiMetrics.marginRow`
- `anchors.rightMargin: 8` → `anchors.rightMargin: UiMetrics.marginRow`
- `spacing: 12` → `spacing: UiMetrics.gap`
- `font.pixelSize: 15` → `font.pixelSize: UiMetrics.fontBody`
- `size: 16` → `size: UiMetrics.iconSmall`
- `Layout.preferredWidth: 140` → `Layout.preferredWidth: root.width * 0.3`

**Step 4: Update SettingsTextField.qml**

Same row pattern + text field font:
- `implicitHeight: 48` → `implicitHeight: UiMetrics.rowH`
- `anchors.leftMargin: 8` → `anchors.leftMargin: UiMetrics.marginRow`
- `anchors.rightMargin: 8` → `anchors.rightMargin: UiMetrics.marginRow`
- `spacing: 12` → `spacing: UiMetrics.gap`
- `font.pixelSize: 15` (label) → `font.pixelSize: UiMetrics.fontBody`
- `size: 16` → `size: UiMetrics.iconSmall`
- `font.pixelSize: 14` (TextField) → `font.pixelSize: UiMetrics.fontSmall`
- `radius: 4` → `radius: UiMetrics.radius / 2`
- `Layout.preferredWidth: root.width * 0.35` stays proportional (already good)

**Step 5: Update SettingsSlider.qml**

Same row pattern + value display:
- `implicitHeight: 48` → `implicitHeight: UiMetrics.rowH`
- `anchors.leftMargin: 8` → `anchors.leftMargin: UiMetrics.marginRow`
- `anchors.rightMargin: 8` → `anchors.rightMargin: UiMetrics.marginRow`
- `spacing: 12` → `spacing: UiMetrics.gap`
- `font.pixelSize: 15` (label) → `font.pixelSize: UiMetrics.fontBody`
- `size: 16` → `size: UiMetrics.iconSmall`
- `font.pixelSize: 14` (value) → `font.pixelSize: UiMetrics.fontSmall`
- `Layout.preferredWidth: 40` (value text) → `Layout.preferredWidth: UiMetrics.rowH` (scales with row)
- `Layout.preferredWidth: root.width * 0.3` stays proportional (already good)

**Step 6: Update SettingsPageHeader.qml**

- `implicitHeight: 40` → `implicitHeight: UiMetrics.headerH`
- `anchors.leftMargin: 8` → `anchors.leftMargin: UiMetrics.marginRow`
- `spacing: 8` → `spacing: UiMetrics.marginRow`
- `width: 36` → `width: UiMetrics.backBtnSize`
- `height: 36` → `height: UiMetrics.backBtnSize`
- `radius: 18` → `radius: UiMetrics.backBtnSize / 2`
- `size: 24` (back icon) → `size: UiMetrics.iconSize`
- `font.pixelSize: 18` → `font.pixelSize: UiMetrics.fontTitle`

**Step 7: Update SectionHeader.qml**

- `Layout.topMargin: 16` → `Layout.topMargin: UiMetrics.sectionGap`
- `Layout.bottomMargin: 4` → `Layout.bottomMargin: UiMetrics.spacing`
- `implicitHeight: 32` → `implicitHeight: UiMetrics.sectionH`
- `anchors.bottomMargin: 4` → `anchors.bottomMargin: UiMetrics.spacing`
- `font.pixelSize: 13` → `font.pixelSize: UiMetrics.fontSmall`

**Step 8: Build and verify**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Builds clean. UI should look identical at 1024x600 (scale = 1.0).

**Step 9: Commit**

```
feat: replace hardcoded pixels with UiMetrics tokens in settings controls
```

---

### Task 3: Update settings pages (7 files)

**Files:**
- Modify: `qml/applications/settings/SettingsMenu.qml`
- Modify: `qml/applications/settings/AudioSettings.qml`
- Modify: `qml/applications/settings/VideoSettings.qml`
- Modify: `qml/applications/settings/DisplaySettings.qml`
- Modify: `qml/applications/settings/ConnectionSettings.qml`
- Modify: `qml/applications/settings/SystemSettings.qml`
- Modify: `qml/applications/settings/AboutSettings.qml`

**Step 1: Update all pages — common pattern**

Every settings page (Audio, Video, Display, Connection, System) has:
```qml
anchors.margins: 16   →  anchors.margins: UiMetrics.marginPage
spacing: 4            →  spacing: UiMetrics.spacing
contentHeight: content.implicitHeight + 32  →  contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
```

**Step 2: Update SettingsMenu.qml**

- Grid `rowSpacing: 20` → `rowSpacing: UiMetrics.gridGap`
- Grid `columnSpacing: 20` → `columnSpacing: UiMetrics.gridGap`

**Step 3: Update AudioSettings.qml**

- Audio device ComboBox `Layout.preferredWidth: 220` → `Layout.preferredWidth: root.width * 0.45`
- Restart button `implicitHeight: 44` → `implicitHeight: UiMetrics.touchMin`
- Restart button `font.pixelSize: 14` → `font.pixelSize: UiMetrics.fontSmall`
- Restart button `radius: 8` → `radius: UiMetrics.radius`
- Restart button `Layout.topMargin: 12` → `Layout.topMargin: UiMetrics.gap`
- Help text `font.pixelSize: 12` → `font.pixelSize: UiMetrics.fontTiny` (if present)

**Step 4: Update VideoSettings.qml**

- Info text `font.pixelSize: 12` or `13` → `font.pixelSize: UiMetrics.fontTiny`
- Any remaining hardcoded spacing/margins → UiMetrics tokens

**Step 5: Update DisplaySettings.qml**

- Help text fonts → `UiMetrics.fontTiny`
- Any remaining hardcoded values → UiMetrics tokens

**Step 6: Update ConnectionSettings.qml**

- Same pattern as other pages (margins, spacing)

**Step 7: Update SystemSettings.qml**

- Same pattern. Plugin placeholder text font → `UiMetrics.fontSmall`

**Step 8: Update AboutSettings.qml**

- Title `font.pixelSize: 22` → `font.pixelSize: UiMetrics.fontHeading`
- Version `font.pixelSize: 15` → `font.pixelSize: UiMetrics.fontBody`
- Description `font.pixelSize: 13` → `font.pixelSize: UiMetrics.fontSmall`
- Exit dialog `width: 320` → `width: parent.width * 0.4`
- Dialog header height `48` → `UiMetrics.rowH`
- Dialog header font `18` → `UiMetrics.fontTitle`
- Dialog button height `48` → `UiMetrics.rowH`
- Dialog button font `16` → `UiMetrics.fontBody`
- Cancel button height `40` → `UiMetrics.headerH`
- Cancel font `14` → `UiMetrics.fontSmall`
- Content spacing `12` → `UiMetrics.gap`
- Spacer `24` → `UiMetrics.marginPage + UiMetrics.marginRow`
- Exit button `width: 200` → `width: parent.width * 0.3`
- Exit button `height: 48` → `height: UiMetrics.rowH`

**Step 9: Build and verify**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Builds clean. UI identical at 1024x600.

**Step 10: Commit**

```
feat: replace hardcoded pixels with UiMetrics tokens in settings pages
```

---

### Task 4: Deploy and test on Pi

**Step 1: Run tests locally**

Run: `ctest --output-on-failure`
Expected: 17/17 pass

**Step 2: Deploy to Pi**

```bash
rsync -av --exclude='build/' --exclude='.git/' --exclude='libs/aasdk/' \
  /home/matt/claude/personal/openautopro/openauto-prodigy/ \
  matt@192.168.1.149:/home/matt/openauto-prodigy/
```

**Step 3: Build on Pi**

```bash
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && cmake --build . -j3"
```

**Step 4: Launch and verify**

```bash
ssh -f matt@192.168.1.149 'cd /home/matt/openauto-prodigy && WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 ./build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'
```

Verify: Navigate through all settings pages. Everything should look identical to before (scale = 1.0 at 600px height). Check:
- All text is readable
- All controls are tappable
- ComboBox dropdowns open correctly
- SpinBox +/- buttons work
- Sliders drag smoothly
- Back button works on each page
- About dialog opens and buttons work

**Step 5: Commit**

```
chore: verify settings UI scaling on Pi target
```
