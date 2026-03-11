# Home Screen Polish — Design

**Date:** 2026-03-10
**Branch:** `dev/0.5.2`
**Status:** Approved

## Overview

Three changes to the home screen landing page:

1. **Glass-effect widget containers** with per-pane adjustable opacity (default ~25%)
2. **Long-press context menu** replacing direct widget picker launch
3. **"Always use dark mode" override** for the HU interface only (AA keeps real day/night)

## 1. Glass Widget Cards

**Current:** Widgets render inside fully opaque `surfaceContainer` rectangles.

**New:** Widget card backgrounds become semi-transparent with a subtle border.

- Default opacity: ~0.25 (heavy glass — wallpaper shows through strongly)
- Background color: `surfaceContainer` at the configured opacity
- Border: 1px `outlineVariant` at ~0.3 opacity for edge definition
- Corner radius: unchanged (`UiMetrics.radius`)
- No blur — plain transparency only. Blur is a future enhancement if Pi performance allows.
- Per-pane opacity stored in config: `widgets.panes.<paneId>.opacity` (float 0.0-1.0, default 0.25)
- Empty pane state: same glass card with "Hold to add widget" hint
- Readability at high transparency is the user's responsibility — opacity slider is accessible via long-press context menu

## 2. Long-Press Context Menu

**Current:** Long-press pane → widget picker bottom sheet immediately.

**New:** Long-press pane → floating context popup anchored to the pressed pane.

### Popup Design

- Small floating card (not full-width bottom sheet)
- Anchored to the center of the pane that was held
- Semi-transparent background matching glass style but higher opacity (~0.85) for readability
- Scrim behind it (tap scrim to dismiss)
- Vertical list of options, each row is icon + label:
  - **Change Widget** — opens the existing WidgetPicker bottom sheet
  - **Opacity** — expands inline to show a horizontal slider (0-100%). Live preview as you drag.
  - **Clear** — removes widget from pane

### Flow

```
Long-press pane → Context popup appears
  ├── Tap "Change Widget" → Popup dismisses, WidgetPicker opens
  ├── Tap "Opacity" → Slider expands inline, drag for live preview, tap away to confirm
  ├── Tap "Clear" → Widget removed, popup dismisses
  └── Tap scrim → Popup dismisses (cancel)
```

Opacity changes save to config immediately on slider release via `WidgetPlacementModel`.

## 3. Force Dark Mode

**Config key:** `display.force_dark_mode` (bool, default `true`)

### Behavior

- When `true`: `ThemeService.nightMode` returns `true` always for QML rendering
- Night mode providers (time/GPIO) continue running underneath
- `SensorChannelHandler` reads the **real** provider state (not the override) when reporting to AA
- Gesture overlay toggle and Ctrl+D temporarily disable the override for the session (resets on restart)

### Settings UI

- New toggle at the top of the Day/Night Mode section in DisplaySettings: "Always use dark mode"
- When enabled, the source picker, time fields, and GPIO controls are visually dimmed (still visible, not hidden)

### Default Config

`force_dark_mode: true` ships as the default.

## 4. Out of Scope

- Blur/frosted glass effects (performance risk on Pi)
- Theme rework (separate later step)
- Dock redesign
- New widgets
- Widget-specific settings beyond opacity
