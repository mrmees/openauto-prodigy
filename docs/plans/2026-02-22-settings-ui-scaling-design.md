# Settings UI Scaling Design

**Date:** 2026-02-22
**Status:** Approved

## Problem

All settings UI uses hardcoded pixel values (48px rows, 16px margins, 15px fonts, 160px combo widths). This works on the primary 1024x600 display but won't scale correctly to other screen sizes or resolutions.

## Solution: UiMetrics Singleton

A centralized design token system via a QML singleton (`UiMetrics.qml`). One clamped scale factor computed from the shortest screen edge vs a 600px baseline. All QML files reference tokens instead of magic numbers.

### Scale Factor

```
scale = clamp(0.9, min(Screen.width, Screen.height) / 600, 1.35)
```

- Baseline: 600px (matches 1024x600 primary target where scale = 1.0)
- Clamped to prevent extreme scaling on very small or very large displays

### Design Tokens

| Token | Base Value | Purpose |
|-------|-----------|---------|
| `rowH` | 48 | Interactive row height (touch target) |
| `touchMin` | 44 | Minimum touch target size |
| `marginPage` | 16 | Outer page margins |
| `marginRow` | 8 | Internal row padding |
| `spacing` | 4 | Layout spacing between rows |
| `gap` | 12 | Spacing between label and control |
| `fontTitle` | 18 | Page titles |
| `fontBody` | 15 | Control labels |
| `fontSmall` | 13 | Section headers, descriptions |
| `fontTiny` | 12 | Help text, secondary info |
| `radius` | 8 | Corner radius for controls |

All tokens are `Math.round(baseValue * scale)`.

### Width Handling

Fixed-width controls (ComboBox, SpinBox, dialog) switch to proportional sizing:
- ComboBox: `Layout.fillWidth: true` or `parent.width * 0.35`
- SpinBox: `parent.width * 0.3`
- Exit dialog: `parent.width * 0.4`

## Scope

### New File
- `qml/controls/UiMetrics.qml` — singleton with all tokens

### Controls (7 files)
- `SettingsComboBox.qml`
- `SettingsToggle.qml`
- `SettingsSpinBox.qml`
- `SettingsTextField.qml`
- `SettingsSlider.qml`
- `SettingsPageHeader.qml`
- `SectionHeader.qml`

### Settings Pages (7 files)
- `AudioSettings.qml`
- `VideoSettings.qml`
- `DisplaySettings.qml`
- `ConnectionSettings.qml`
- `SystemSettings.qml`
- `AboutSettings.qml`
- `SettingsMenu.qml`

### No Structural Changes
Layout pattern (Flickable + ColumnLayout) stays the same. Only pixel values are replaced with token references.
