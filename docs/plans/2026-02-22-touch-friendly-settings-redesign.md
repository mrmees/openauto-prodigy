# Touch-Friendly Settings Redesign

**Date:** 2026-02-22
**Rollback tag:** `v0.3.0-pre-touch-redesign` (pushed to GitHub)
**Status:** Design approved, pending implementation

## Problem

The settings UI was built with desktop-style Qt controls — small fonts, tiny dropdown arrows, SpinBox +/- buttons, and text fields with no on-screen keyboard. On the target hardware (7" 1024x600 touchscreen at arm's length in a car), everything is too small, too close together, and some controls are functionally unusable.

## Approach

Material style as foundation, thin custom "HeadUnit" style layer on top for car-scale sizing, purpose-built replacements for the worst touch offenders (ComboBox, SpinBox, TextField). Controls-first implementation, then page-by-page migration.

## Style Foundation

### Material Dark + HeadUnit Fallback

Switch from Basic/default to Material Dark as the application-wide style:

```cpp
QQuickStyle::setStyle("HeadUnit");
QQuickStyle::setFallbackStyle("Material");
```

- Material configured: `Material.theme: Material.Dark`, accent color matched to current theme blue
- `styles/HeadUnit/` directory contains overrides only for controls that need car-scale sizing
- All other controls inherit Material look and behavior automatically
- Wire `Material.theme` to `ThemeService` — day mode = `Material.Light`, night mode = `Material.Dark`

### UiMetrics Baseline Bump

UiMetrics stays as the scaling/token system. Baseline values increase:

| Token | Old (scale 1.0) | New (scale 1.0) |
|-------|-----------------|-----------------|
| `rowH` | 48px | 64px |
| `touchMin` | 44px | 56px |
| `fontBody` | 15px | 20px |
| `fontSmall` | 13px | 16px |
| `fontTitle` | 18px | 22px |
| `fontHeading` | 22px | 28px |
| `marginPage` | 16px | 20px |
| `marginRow` | 8px | 12px |
| `spacing` | 4px | 8px |
| `gap` | 12px | 16px |

Existing `UiMetrics.*` references throughout QML automatically pick up the larger values.

### Non-Settings Impact

Material style applies app-wide. Our Shell, TopBar, BottomBar, and nav strip use custom components (not Qt Quick Controls), so they should be unaffected. The launcher tiles and exit dialog use some Qt Controls and need visual verification after the switch.

## Control Replacements

### SpinBox -> Tumbler

Qt's built-in `Tumbler` — flick-to-scroll wheel picker (like iOS date/time pickers).

- `visibleItemCount: 3` (current value + one above/below)
- Used for: DPI, sidebar width, WiFi channel, GPIO pin
- TCP port (1024-65535) becomes read-only on touchscreen — set via web panel

### ComboBox -> FullScreenPicker

Tappable row showing current value. Tap opens a modal Dialog filling ~60% of screen height from the bottom, containing a ListView of 64px-tall ItemDelegate rows. Single tap selects and closes.

- Used for: resolution, day/night source, audio devices, orientation (if >2 options)
- For 2-option choices (FPS 30/60, band 2.4/5 GHz, sidebar left/right): use inline SegmentedButton instead

### TextField -> ReadOnlyField + Web Panel

Most text fields become read-only displays with a "Edit via web panel" hint. WiFi SSID, password, and other keyboard-dependent settings are removed from the touchscreen entirely — web config panel only.

- ReadOnly fields: head unit name, manufacturer, model, car model, car year, hardware profile, touch device, TCP port, software version
- Removed from touchscreen: WiFi interface, SSID, password, theme name

### Slider -> Material Slider (oversized)

Material Slider is already touch-friendly. HeadUnit style override increases handle to 48px diameter with thicker track.

- Used for: brightness, master volume, mic gain, DPI, sidebar width

### Switch -> Material Switch (oversized)

Material Switch bumped up in size via HeadUnit style override.

- Used for: auto-connect, BT discoverable, show sidebar, left-hand drive, GPIO active high

## Page Layout Pattern

Every settings page follows a single-column, full-width list layout:

```
┌──────────────────────────────────┐
│ <-  Page Title                   │  header (64px, tappable back)
├──────────────────────────────────┤
│ SECTION NAME                     │  section header (32px, uppercase)
├──────────────────────────────────┤
│ Label                   Control >│  setting row (64px min)
├──────────────────────────────────┤
│ Label              ━━━━●━━━━ 75% │  slider row (64px min)
├──────────────────────────────────┤
│                                  │
│  i  Restart required to apply    │  info banner (when applicable)
│     device changes               │
└──────────────────────────────────┘
```

Rules:
- Label on left, control on right — always
- Flickable wrapping everything below the header
- Section headers as visual separators (uppercase, smaller text, divider line)
- No nested columns or grids within settings pages
- "Restart required" shown as a persistent banner at bottom (not per-row icons)

## Page-by-Page Changes

### Settings Menu (landing page)

Keep the 3x2 tile grid. Works well on touch. Sizes scale up with UiMetrics bump.

### Audio Settings

| Setting | Old Control | New Control |
|---------|-------------|-------------|
| Master Volume | SettingsSlider | Slider (oversized) |
| Output Device | ComboBox (manual) | FullScreenPicker |
| Input Device | ComboBox (manual) | FullScreenPicker |
| Mic Gain | SettingsSlider | Slider (oversized) |
| Restart button | Button | InfoBanner (passive) |

### Display Settings

| Setting | Old Control | New Control |
|---------|-------------|-------------|
| Brightness | SettingsSlider | Slider |
| Theme | SettingsTextField | ReadOnlyField + web hint |
| Orientation | SettingsComboBox | SegmentedButton (landscape/portrait) |
| Day/Night Source | SettingsComboBox | FullScreenPicker (time/gpio/none) |
| Day Start | SettingsTextField | Tumbler pair (HH:MM) — visible when source=time |
| Night Start | SettingsTextField | Tumbler pair (HH:MM) — visible when source=time |
| GPIO Pin | SettingsSpinBox | Tumbler (0-27) — visible when source=gpio |
| GPIO Active High | SettingsToggle | Toggle — visible when source=gpio |

### Connection Settings

| Setting | Old Control | New Control |
|---------|-------------|-------------|
| Auto-connect | SettingsToggle | Toggle |
| TCP Port | SettingsSpinBox | ReadOnlyField + web hint |
| WiFi Interface | SettingsTextField | **Removed** (web panel only) |
| WiFi SSID | SettingsTextField | **Removed** (web panel only) |
| WiFi Password | SettingsTextField | **Removed** (web panel only) |
| WiFi Channel | SettingsSpinBox | FullScreenPicker or Tumbler |
| WiFi Band | SettingsComboBox | SegmentedButton (2.4 GHz / 5 GHz) |
| BT Discoverable | SettingsToggle | Toggle |

### Video Settings

| Setting | Old Control | New Control |
|---------|-------------|-------------|
| FPS | SettingsComboBox | SegmentedButton (30 / 60) |
| Resolution | SettingsComboBox | FullScreenPicker (480p/720p/1080p) |
| DPI | SettingsSpinBox | Slider (80-400) |
| Show Sidebar | SettingsToggle | Toggle |
| Sidebar Position | SettingsComboBox | SegmentedButton (Left / Right) |
| Sidebar Width | SettingsSpinBox | Slider (80-300px) |

### System Settings

| Setting | Old Control | New Control |
|---------|-------------|-------------|
| Head Unit Name | SettingsTextField | ReadOnlyField + web hint |
| Manufacturer | SettingsTextField | ReadOnlyField + web hint |
| Model | SettingsTextField | ReadOnlyField + web hint |
| Software Version | SettingsTextField (readOnly) | ReadOnlyField |
| Car Model | SettingsTextField | ReadOnlyField + web hint |
| Car Year | SettingsTextField | ReadOnlyField + web hint |
| Left-Hand Drive | SettingsToggle | Toggle |
| Hardware Profile | SettingsTextField | ReadOnlyField + web hint |
| Touch Device | SettingsTextField | ReadOnlyField + web hint |
| Plugins | Placeholder text | Placeholder text (unchanged) |

### About Settings

- Info block: bigger fonts from UiMetrics bump
- Exit App button: Material-styled, 64px tall
- Exit dialog: bigger buttons, more spacing, consistent heights

## New Components

| Component | Type | Purpose |
|-----------|------|---------|
| `FullScreenPicker.qml` | Custom control | Modal bottom dialog with 64px tappable list items. Replaces ComboBox. |
| `SegmentedButton.qml` | Custom control | 2-3 inline toggle buttons for small option sets. |
| `SettingsRow.qml` | Custom control | Full-width row container, 64px min height, label-left/control-right. |
| `InfoBanner.qml` | Custom control | Bottom-anchored "restart required" banner. |
| `ReadOnlyField.qml` | Custom control | Displays current value with "web panel" hint. |
| `styles/HeadUnit/Slider.qml` | Style override | Material Slider with 48px handle, thicker track. |
| `styles/HeadUnit/Switch.qml` | Style override | Material Switch, bumped up. |
| `styles/HeadUnit/Button.qml` | Style override | Material Button, 64px min height. |
| `styles/HeadUnit/ItemDelegate.qml` | Style override | 64px tall list items for pickers. |

## Retired Controls

- `SettingsComboBox.qml` — replaced by FullScreenPicker / SegmentedButton
- `SettingsSpinBox.qml` — replaced by Tumbler / Slider
- `SettingsTextField.qml` — replaced by ReadOnlyField or removed entirely

## Kept Controls (with UiMetrics bump)

- `SettingsSlider.qml` — verify Material styling, may refactor into SettingsRow
- `SettingsToggle.qml` — wraps Material Switch
- `SettingsPageHeader.qml` — bump to 64px
- `SectionHeader.qml` — keep as-is
- `Tile.qml` — keep for settings menu grid

## Implementation Phases

### Phase 1: Foundation

1. Add Material style + HeadUnit fallback to build system and `main.cpp`
2. Bump UiMetrics baseline values
3. Build new controls (FullScreenPicker, SegmentedButton, SettingsRow, ReadOnlyField, InfoBanner)
4. Build HeadUnit style overrides (Slider, Switch, Button, ItemDelegate)
5. Verify non-settings UI (Shell, TopBar, launcher) isn't broken by Material switch

### Phase 2: Page Migration (one page per commit)

1. Audio Settings — sliders, pickers, banner
2. Display Settings — conditional sections, Tumbler for time
3. Video Settings — segmented buttons, sliders
4. Connection Settings — biggest change, removing fields
5. System Settings — mostly ReadOnly conversion
6. About Settings — minor tweaks

### Phase 3: Cleanup

1. Remove retired controls if fully unused
2. Test on Pi at 1024x600
3. Verify day/night theme switching with Material
4. Verify web config panel still works for settings pushed there

## Compatibility Notes

- Material style available in both Qt 6.4 (dev VM) and Qt 6.8 (Pi target)
- `roundedScale` and `containerStyle` properties are Qt 6.5+ only — avoid these
- Core Material properties (theme, accent, primary, foreground, background, elevation) and Dense/Normal variant available in Qt 6.4
- Tumbler available since Qt 5.7 — no compatibility concern
- Fallback style mechanism (`QQuickStyle::setFallbackStyle`) available since Qt 5.8
