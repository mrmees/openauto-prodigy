# Settings Row Back-Hold Ownership — Design

**Date:** 2026-03-11
**Branch:** `dev/0.5.2`
**Status:** Approved

## Overview

The current settings long-hold back behavior is split across multiple layers:

- `SettingsMenu.qml` owns blank-space hold through the full-screen overlay.
- Tap-driven controls such as toggles and pickers own their own hold through `SettingsHoldArea.qml`.
- `SettingsSlider.qml` owns a separate timer-driven hold path.

That split leaves dead zones. The clearest bug is slider rows: the actual `Slider` control can trigger long-hold back, but the label/title strip and other row padding do not. The row blocks the menu overlay, but no row-level owner arms the hold.

The intended behavior is:

- Any actual settings row should own long-hold back across the whole row surface.
- `SettingsMenu.qml` should remain responsible only for true whitespace, headers, and background.
- Short taps should still activate the row's normal control behavior.
- If long hold wins, the row must suppress the control action.
- Drag-driven controls such as sliders must be able to cancel row hold when real dragging starts and revert transient state if hold wins.

## Approaches Considered

### 1. Keep patching individual controls

- Pros: smallest immediate diff
- Cons: keeps the ownership model fragmented and continues to miss edge zones like slider labels, padding, and one-off custom rows

### 2. Make `SettingsRow.qml` the owner for every actual row

- Pros: one clear ownership boundary, fixes slider title/padding gaps, reduces gesture whack-a-mole, keeps `SettingsMenu.qml` focused on whitespace only
- Cons: controls need a small coordination contract so long-hold suppresses their normal click/commit behavior

### 3. Move the whole gesture into a C++ event filter

- Pros: centralized outside QML
- Cons: heavier, harder to validate, and more brittle than the QML-local ownership boundary

## Recommendation

Use approach 2.

`SettingsRow.qml` should own long-hold back for any real row. Controls inside the row should only handle their primary action and coordinate with the row when drag or suppression behavior matters.

## Architecture

### Ownership boundaries

- `SettingsMenu.qml` keeps the full-screen overlay for blank space, headers, and background only.
- `SettingsRow.qml` always marks itself as `blocksBackHold: true` so the menu overlay does not arm over real rows.
- `SettingsRow.qml` owns the row-level hold lifecycle, ripple hookup, and back request.

### Row/control contract

- `SettingsRow.qml` exposes row-hold state that child controls can query to suppress short-click actions after a long hold fires.
- `SettingsRow.qml` exposes a cancel/reset path so drag-driven controls can stop the row hold when the user begins a real drag.
- Tap-driven controls can keep their existing click surfaces, but they should no longer own long-hold back directly.

### Slider behavior

- `SettingsSlider.qml` stops owning its own long-hold timer.
- It records the press-time value when the slider is pressed.
- If the user starts a real drag, it cancels the row hold and continues normal slider behavior.
- If the row long-hold fires before a real drag, the slider restores the press-time value and suppresses commit behavior.

## Testing

- Replace the current regression expectation that `SettingsSlider.qml` owns its own dedicated hold timer.
- Add assertions that `SettingsRow.qml` owns the row hold API and blocks menu hold for all rows.
- Add assertions that `SettingsSlider.qml` coordinates with row-owned hold instead of implementing a separate timer path.
- Keep the overlay structure assertions so whitespace hold remains covered.

## Out of Scope

- Rewriting settings gestures in C++
- Changing hold timing or ripple visuals
- Refactoring unrelated settings controls outside this row-ownership boundary
