# Swipe-to-Go-Back — Settings

## Summary

Add threshold-based right-swipe gesture to SettingsMenu for navigating back. Works at both levels: sub-page → grid, and grid → launcher.

## Detection

- `DragHandler` (or MouseArea with drag tracking) on SettingsMenu StackView
- Minimum horizontal distance: ~80px (UiMetrics-scaled)
- Horizontal dominance check: dx > 2×dy (avoid false triggers on vertical scrolls)
- Threshold-based: no interactive drag-follow, fires existing pop/back animation

## Behavior

| Context | Swipe Action |
|---------|-------------|
| Sub-page (depth > 1) | `settingsStack.pop()` + update title |
| Settings grid (depth = 1) | `ApplicationController.navigateBack()` |

## Scope

- Single file: `qml/applications/settings/SettingsMenu.qml`
- No changes to sub-pages, ApplicationController, or Shell
- Reuses existing StackView slide-out transition animations
