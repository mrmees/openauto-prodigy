# Settings Back-Hold Interactive Controls — Design

**Date:** 2026-03-11
**Branch:** `dev/0.5.2`
**Status:** Approved

## Overview

The current settings long-press back gesture works on empty space, headers, and simple button-style rows, but not reliably on interactive control surfaces such as dropdowns, sliders, and toggles.

The intended behavior is:

- Short press/release keeps the control's normal action.
- Long hold (500 ms) triggers back navigation, even when the finger starts on the interactive part of the control.
- Long hold must suppress the control action so a toggle does not flip, a picker does not open, and a slider does not commit a value change.

## Approaches Considered

### 1. Keep pushing the global overlay harder

Extend the full-screen overlay/TapHandler path and try to win more pointer-grab conflicts.

- Pros: one central implementation
- Cons: this is already the flaky path on Pi touch, and it depends on Qt pointer delivery details that vary by control type

### 2. Shared hold-aware behavior inside interactive controls

Each interactive control owns its own short-tap vs long-hold decision. The existing overlay remains only for blank space and non-interactive areas.

- Pros: uses the event path that already works for each control, explicit ownership, easy to reason about, aligns with desired semantics
- Cons: requires touching multiple reusable controls

### 3. Window-level C++ event filter

Capture the press sequence above QML and decide globally whether the gesture is tap or back-hold.

- Pros: most centralized
- Cons: highest risk, most invasive, and harder to validate quickly for this bug

## Recommendation

Use approach 2.

Interactive controls should own long-hold back themselves. The overlay should stop trying to service those surfaces and continue handling only blank or non-interactive regions.

## Gesture Contract

### Toggle

- Short tap toggles and saves as it does now.
- Long hold requests back and must not change the toggle state.

### Full-screen picker / dropdown row

- Short tap opens the picker dialog.
- Long hold requests back and must not open the picker.

### Slider

- Press and hold without meaningful movement requests back.
- If the user starts dragging before the long-hold threshold, cancel long-hold detection and continue normal slider behavior.
- If long hold fires, the slider must not commit a changed value.

### Button-style interactive rows

- `SettingsListItem` and `SettingsRow { interactive: true }` keep their current short-tap behavior.
- Long hold requests back and suppresses the row click.

### Non-interactive space

- The existing `SettingsMenu.qml` overlay remains responsible for headers, gaps, and other non-interactive surfaces.

## Architecture

### Ownership model

- Reusable interactive controls mark themselves as `blocksBackHold: true`.
- `SettingsMenu.blocksBackHoldAt()` uses that marker to keep the overlay from arming over controls that now own the gesture.
- Those controls call `ApplicationController.requestBack()` on long hold so the existing settings-stack back handling remains authoritative.

### Shared control behavior

- Add a reusable hold-aware mouse/tap helper for tap-driven settings controls.
- Use it in `SettingsToggle.qml`, `FullScreenPicker.qml`, `SettingsListItem.qml`, and interactive `SettingsRow.qml`.
- Keep the control visuals, but route the actual short-tap action through the hold-aware layer so long hold can suppress it.

### Slider behavior

- `SettingsSlider.qml` keeps the real slider drag path.
- It adds a dedicated timer/suppression path for long hold:
  - arm on press
  - cancel on meaningful movement
  - if the timer completes first, request back and suppress commit behavior

## Testing

- Replace the current regression expectation that toggle/slider/picker roots should not block the overlay. That assumption is wrong for the clarified interaction model.
- Add failing regression coverage for the new ownership model:
  - overlay still exists for non-interactive space
  - interactive controls explicitly own back-hold
  - slider has dedicated long-hold suppression logic
- Validate short-tap behavior is preserved and long hold triggers back without activating the control.

## Out of Scope

- Rewriting the settings screen gesture system in C++
- Replacing the overlay path for non-interactive surfaces
- Broader gesture changes outside the settings UI
