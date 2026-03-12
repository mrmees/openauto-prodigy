---
status: complete
phase: 06-content-widgets
source: [06-01-SUMMARY.md, 06-02-SUMMARY.md, 06-03-SUMMARY.md]
started: 2026-03-12T18:20:00Z
updated: 2026-03-12T18:25:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Navigation Widget Placement
expected: Long-press on empty grid space opens widget picker. "Navigation" appears in the list with a navigation icon. Selecting it places the widget on the home screen grid.
result: pass

### 2. Navigation Widget Inactive State
expected: When AA navigation is not active, the navigation widget shows a muted navigation icon and "No navigation" text at ~40% opacity. Widget remains visible, does not collapse.
result: pass

### 3. Navigation Widget Active State
expected: When AA navigation is active, the widget shows the distance with correct units (miles for US locale) and road name. Maneuver icon displays (phone PNG or Material fallback).
result: issue
reported: "maneuver icon never changes"
severity: minor

### 4. Navigation Widget Tap Action
expected: Tapping the navigation widget switches to AA fullscreen view.
result: pass

### 5. Now Playing Widget Placement
expected: "Now Playing" appears in widget picker. Selecting it places the unified now playing widget on the home screen grid.
result: pass

### 6. Now Playing Inactive State
expected: When no media is playing and no source connected, shows muted music icon and "No media" at ~40% opacity. Controls visible but dimmed.
result: pass

### 7. Now Playing Active State (AA)
expected: When AA is connected and playing music, widget shows track title, artist, and an AA source indicator icon. Playback controls (prev/play-pause/next) are visible and active.
result: pass

### 8. Now Playing Controls
expected: Prev, play/pause, and next buttons respond to taps and control the active media source.
result: pass

### 9. Old BT Widget Removed
expected: The old "BT Now Playing" widget no longer appears in the widget picker. Only the unified "Now Playing" widget is available.
result: pass

### 10. Source Switching (AA to BT)
expected: When AA disconnects while BT audio is playing, Now Playing widget switches to BT source with BT icon and shows BT track metadata. Controls route to AVRCP.
result: skipped
reason: Not easy to test right now

## Summary

total: 10
passed: 8
issues: 1
pending: 0
skipped: 1

## Gaps

- truth: "Maneuver icon displays phone PNG or Material fallback and changes with turn type"
  status: failed
  reason: "User reported: maneuver icon never changes"
  severity: minor
  test: 3
  root_cause: "AA 16.2 dropped NavigationTurnEvent (0x8004) which carried turn_icon PNG bytes. NavigationNotification (0x8006) does not include icon data. The bridge never receives icon updates, so hasManeuverIcon stays false and the Material fallback icon is static."
  artifacts:
    - path: "src/core/aa/NavigationDataBridge.cpp"
      issue: "Only receives icon from deprecated 0x8004 signal"
    - path: "libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp"
      issue: "handleNavStep does not extract maneuver type changes to update icon"
  missing:
    - "Map maneuverType from NavigationNotification steps to Material icons"
    - "Update Material fallback icon when maneuverType changes"
  debug_session: ""
