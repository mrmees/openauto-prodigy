# Settings UI Restructure — Design

**Date:** 2026-02-27
**Scope:** QML on-device settings only (no web panel changes)
**Approach:** Hardcoded QML pages with shared controls (Option A from brainstorming)

---

## Problem

The settings UI has two issues:

1. **Entry point doesn't scale.** The 3x3 tile grid is full at 7 categories. Adding plugin settings or future categories has no room.
2. **Pages lack internal structure.** Most settings pages are flat lists of controls without section groupings. Hard to scan on a 1024x600 touchscreen.

## Design

### 1. Settings List Entry Point

Replace the tile grid (`GridLayout` of `Tile` components) with a `ListView` backed by a `ListModel`.

```
ListView
  ├── GENERAL section header
  │   ├── Display         (icon + label + chevron)
  │   ├── Audio
  │   ├── Connection
  │   ├── Video
  │   └── System
  ├── COMPANION section header
  │   └── Companion
  ├── PLUGINS section header (dynamic, only if any plugin has settings)
  │   ├── <plugin 1>      (from IPlugin::settingsComponent())
  │   └── <plugin 2>
  └── About (no section header, bottom of list)
```

**New component: `SettingsListItem.qml`**
- Row layout: Material icon (left) + label + optional subtitle + chevron (right)
- Touch-friendly height (~60px via UiMetrics token)
- Themed via ThemeService colors
- `onClicked` signal for StackView push

**Model construction:** `SettingsMenu.qml` builds a `ListModel` on `Component.onCompleted`:
- Core entries are hardcoded (Display, Audio, Connection, Video, System, Companion, About)
- Plugin entries are appended by iterating `PluginModel` and filtering for non-empty `settingsQml` role
- Each entry has: `icon`, `label`, `section`, `component` (QML Component reference or URL)

**Section headers** are rendered inline using `ListView.delegate` role-checking — items with `type: "section"` render as `SectionHeader`, items with `type: "item"` render as `SettingsListItem`.

Tapping a row pushes the corresponding page onto the existing `StackView` (same navigation pattern as today).

### 2. Settings Page Structure

6 of 7 pages already use `Flickable` + `ColumnLayout`. Add `SectionHeader` dividers to organize controls into logical groups:

| Page | Proposed Sections |
|------|-------------------|
| DisplaySettings | "General", "Day / Night Mode" (already done) |
| AudioSettings | "Output", "Microphone" |
| ConnectionSettings | "Android Auto", "WiFi AP", "Bluetooth" |
| VideoSettings | "Playback", "Codecs", "Sidebar" |
| SystemSettings | "Identity", "Hardware" |
| CompanionSettings | "Status", "Pairing", "Configuration" |
| AboutSettings | Convert from `Item` to `Flickable` for consistency, no sections needed |

No new controls. Existing `SectionHeader`, `SettingsSlider`, `SettingsToggle`, `FullScreenPicker`, `ReadOnlyField`, `SegmentedButton`, and `InfoBanner` are reused as-is.

No read-only → editable conversions. Fields like night mode times, identity, and WiFi credentials remain read-only on device (editable via web panel).

### 3. Plugin Settings Integration

`IPlugin::settingsComponent()` already exists in the interface. All three current plugins return empty `QUrl`.

**PluginModel change:** Add `settingsQml` role (reads `IPlugin::settingsComponent()`). This lets QML query which plugins have settings pages.

**Settings list integration:**
- `SettingsMenu.qml` iterates `PluginModel` on load
- Plugins with non-empty `settingsQml` get a row under the PLUGINS section header
- Icon and label come from the plugin's existing `pluginIconText` and `pluginName` roles
- Tapping pushes a `Loader { source: settingsQml }` onto the StackView

**No IPlugin interface changes.** Plugins start providing settings by returning a QUrl from `settingsComponent()` — that's future per-plugin work, not part of this effort.

## What Changes

| File | Change |
|------|--------|
| `qml/applications/settings/SettingsMenu.qml` | Replace tile grid with ListView + ListModel |
| `qml/controls/SettingsListItem.qml` | **New.** Reusable row: icon + label + subtitle + chevron |
| `src/ui/PluginModel.cpp` | Add `settingsQml` role |
| `src/ui/PluginModel.hpp` | Add `SettingsQmlRole` enum value |
| `qml/applications/settings/AudioSettings.qml` | Add section headers |
| `qml/applications/settings/ConnectionSettings.qml` | Add section headers |
| `qml/applications/settings/VideoSettings.qml` | Add section headers |
| `qml/applications/settings/SystemSettings.qml` | Add section headers |
| `qml/applications/settings/CompanionSettings.qml` | Add section headers |
| `qml/applications/settings/AboutSettings.qml` | Convert `Item` → `Flickable` |

## What Doesn't Change

- No new settings fields or config keys
- No `IPlugin` interface changes (method already exists)
- No new plugin settings pages (plugins still return empty URL)
- No web panel work
- No read-only → editable conversions
- All existing QML controls reused as-is

## Testing

- Existing ctest suite (47 tests) must still pass
- `PluginModel` role addition covered by existing plugin model tests (add assertion for new role)
- QML changes are visual — verify on device at 1024x600
