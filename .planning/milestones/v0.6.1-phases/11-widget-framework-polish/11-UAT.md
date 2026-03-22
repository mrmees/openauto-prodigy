---
status: complete
phase: 11-widget-framework-polish
source: [11-01-SUMMARY.md, 11-02-SUMMARY.md]
started: 2026-03-15T22:00:00Z
updated: 2026-03-15T22:15:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Clock widget displays time
expected: Clock widget shows current time. If in a wide pane (2+ columns), date is visible below the time.
result: pass

### 2. Widget long-press enters edit mode
expected: Long-press on any widget pane. Edit mode activates — widgets show remove badges (X) and resize handles.
result: pass

### 3. Tap exits edit mode
expected: While in edit mode, tap on a widget (not the X or resize handle). Edit mode exits — badges and handles disappear.
result: pass

### 4. Remove badge works in edit mode
expected: In edit mode, tap the X badge on a non-singleton widget. Widget is removed from the pane.
result: pass

### 5. Resize handle works in edit mode
expected: In edit mode, drag the resize handle (bottom-right corner) on a widget. Widget resizes to the new span. Releasing snaps to grid.
result: pass

### 6. Widget picker shows categories
expected: Long-press an empty pane or use Change from context menu. Widget picker appears with widgets grouped by category (Status, Media, Navigation, Launcher) — not each widget in its own group.
result: pass

### 7. AA Launcher widget launches AA
expected: Tap the AA launcher widget. Android Auto plugin activates (fullscreen AA view or "waiting for connection" state).
result: pass

### 8. Settings Launcher widget opens settings
expected: Tap the Settings launcher widget. Settings view opens.
result: pass

### 9. Now Playing placeholder state
expected: With no media playing (no BT audio, no AA), the Now Playing widget shows a dimmed music note icon. If the pane is 2+ columns wide, "No media" text appears below the icon.
result: pass

### 10. Context menu via long-press
expected: Long-press on a widget. Context menu appears with options (Change / Opacity / Clear).
result: pass

## Summary

total: 10
passed: 10
issues: 0
pending: 0
skipped: 0

## Gaps

[none]
