# Settings Page Redesign — Design Document
> **Status:** COMPLETED

## Goal

Reorganize the settings page from a flat grid of mostly-disabled tiles into a functional two-level settings UI with 6 category subpages, reusable controls, and a QML-config bridge.

## Constraints

- 1024x600 touchscreen — no room for deep nesting or tiny controls
- Must work on Qt 6.4 (dev VM) and Qt 6.8 (Pi target)
- Settings that require restart get a visual badge, no auto-restart prompts
- Quick settings (volume, brightness, day/night) handled by existing 3-finger gesture overlay — not duplicated here

## Architecture

### Navigation

Two-level structure using a `StackView` inside `SettingsMenu.qml`:

1. **Tile grid** (initial view) — 3x2 grid of category tiles
2. **Subpages** — pushed onto the stack when a tile is tapped, back arrow returns to grid

Each subpage sets the title via `ApplicationController.setTitle("Settings > Category")`. No nesting deeper than one level — subpages use section headers for logical grouping within a single scrollable page.

### QML-Config Bridge

Expose `ConfigService` as a QML context property (registered in `main.cpp`). Two `Q_INVOKABLE` methods provide generic read/write:

```cpp
Q_INVOKABLE QVariant value(const QString& path);
Q_INVOKABLE bool setValue(const QString& path, const QVariant& value);
```

A `configChanged(QString path, QVariant value)` signal is emitted on successful writes for future reactivity needs.

Slider controls debounce writes (~300ms via QML `Timer`) to avoid hammering YAML saves during drag.

### Restart Badge

Pure QML — each control that needs it sets `restartRequired: true`, which shows a small restart icon next to the label. No C++ involvement.

## Categories & Content

### 1. Display (8 settings)

**Section: General**
- Brightness — slider (0-100)
- Theme — combo box (list from ~/.openauto/themes/)
- Orientation — combo box (landscape/portrait)

**Section: Day/Night**
- Source — combo box (time/gpio/none)
- Day start time — text field (HH:MM)
- Night start time — text field (HH:MM)
- GPIO pin — spin box (conditional: only visible when source=gpio)
- GPIO active high — toggle (conditional: only visible when source=gpio)

### 2. Audio (4 settings)

- Master volume — slider (0-100)
- Output device — text field ("auto" or PipeWire device name)
- Microphone device — text field ("auto" or PipeWire device name)
- Microphone gain — slider (0.5-4.0)

### 3. Connection (7 settings, restart badge on WiFi + TCP port)

**Section: Android Auto**
- Auto-connect — toggle
- TCP port — spin box (restart badge)

**Section: WiFi Access Point**
- Interface — text field (restart badge)
- SSID — text field (restart badge)
- Password — text field (restart badge)
- Channel — spin box (restart badge)
- Band — combo box a/g (restart badge)

**Section: Bluetooth**
- BT discoverable — toggle

### 4. Video (3 settings, restart badge on all)

- FPS — combo box (30/60) (restart badge)
- Resolution — combo box (480p/720p/1080p) (restart badge)
- DPI — spin box (restart badge)

### 5. System (9 settings)

**Section: Identity**
- Head unit name — text field
- Manufacturer — text field
- Model — text field
- Software version — read-only text display
- Car model — text field
- Car year — text field
- Left-hand drive — toggle

**Section: Hardware**
- Hardware profile — text field
- Touch device — text field

**Section: Plugins**
- Enabled plugins list — read-only display (placeholder for future management UI)

### 6. About

- Version info — read-only display
- Exit button — opens exit dialog (minimize / close app / cancel)

## Reusable Controls

Six new QML controls in `qml/controls/`:

| Control | Properties | Behavior |
|---------|-----------|----------|
| `SettingsToggle` | `label`, `configPath`, `restartRequired` | Label left, Switch right, reads/writes ConfigService |
| `SettingsSlider` | `label`, `configPath`, `from`, `to`, `stepSize`, `restartRequired` | Label + value left, Slider right, 300ms debounced write |
| `SettingsComboBox` | `label`, `configPath`, `model`, `restartRequired` | Label left, ComboBox right |
| `SettingsTextField` | `label`, `configPath`, `restartRequired`, `placeholder` | Label left, TextField right |
| `SettingsSpinBox` | `label`, `configPath`, `from`, `to`, `restartRequired` | Label left, SpinBox right |
| `SectionHeader` | `text` | Styled text divider with bottom border |

Each control (except SectionHeader) binds to a config path and handles its own read on `Component.onCompleted` and write on value change.

## File Changes

### New Files
```
qml/applications/settings/DisplaySettings.qml
qml/applications/settings/AudioSettings.qml
qml/applications/settings/ConnectionSettings.qml
qml/applications/settings/VideoSettings.qml
qml/applications/settings/SystemSettings.qml
qml/applications/settings/AboutSettings.qml
qml/controls/SettingsToggle.qml
qml/controls/SettingsSlider.qml
qml/controls/SettingsComboBox.qml
qml/controls/SettingsTextField.qml
qml/controls/SettingsSpinBox.qml
qml/controls/SectionHeader.qml
```

### Modified Files
```
qml/applications/settings/SettingsMenu.qml  — GridLayout → StackView with tile grid as initial item
src/core/services/ConfigService.hpp          — Add Q_INVOKABLE, add configChanged signal
src/core/services/ConfigService.cpp          — Emit configChanged on setValue
src/main.cpp                                 — Register ConfigService as QML context property
```

### No New C++ Classes

All 6 reusable controls are pure QML. ConfigService gets minimal additions (Q_INVOKABLE markers + one signal).

## Design Decisions

1. **StackView over Loader** — StackView gives us push/pop transitions and back navigation for free. Loader would require manual state management.
2. **ConfigService in QML over ApplicationController methods** — Keeps config concerns separate from app control (Codex recommendation). ApplicationController already has enough responsibility.
3. **Generic path API over per-setting Q_PROPERTYs** — We have ~45 settings. Per-setting properties would be massive boilerplate. The path system already has schema validation; QML just needs to call it.
4. **Section headers over nested pages** — At 1024x600, a third navigation level would be confusing. Scrollable sections within a single subpage is the right compromise.
5. **Debounced slider writes** — Prevents writing YAML to disk on every pixel of a slider drag. 300ms timer resets on each change, fires once after user stops moving.
6. **Conditional visibility for GPIO settings** — GPIO pin and active-high toggle only shown when night mode source is "gpio". Reduces clutter for users using time-based or no night mode.
