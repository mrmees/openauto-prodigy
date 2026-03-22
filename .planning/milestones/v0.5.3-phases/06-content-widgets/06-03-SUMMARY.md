---
phase: 06-content-widgets
plan: 03
status: complete
---

# Plan 06-03 Summary: QML Widgets, Wiring & BT Widget Removal

## What was built
- **NavigationWidget.qml**: Responsive nav turn-by-turn widget with pixel-based breakpoints (2x1 icon+distance, 3x1 adds road name, 3x2+ expanded layout). Muted "No navigation" placeholder when inactive. Tap opens AA fullscreen.
- **NowPlayingWidget.qml**: Unified now-playing widget replacing BT-only version. Shows title, artist, source indicator icon (BT/AA), prev/play-pause/next controls. Muted "No media" placeholder when inactive. Responsive breakpoints (2x1 compact, 3x1 strip, 3x2 full).
- **main.cpp wiring**: NavigationDataBridge, MediaDataBridge, and ManeuverIconProvider instantiated, wired to orchestrator/plugin, exposed as root QML context properties. Image provider registered for `image://navicon`.
- **Widget descriptor stubs filled**: Nav and Now Playing descriptors now have qmlComponent URLs.
- **BtAudioPlugin widget removal**: `widgetDescriptors()` returns empty list — unified widget handles both sources.
- **Long-press widget picker**: Added to HomeMenu grid (was missing from Phase 05 grid rewrite).

## Bug fixes during verification
- **AA 16.2 nav data**: Phone doesn't send deprecated NavigationTurnEvent (0x8004). Bridge now also connects to NavigationNotification (0x8006) for road/maneuver data and NavigationNextTurnDistanceEvent (0x8007) for pre-formatted distance.
- **Distance unit mapping**: Corrected from AA APK source analysis. AA Distance.displayUnit: 0=unknown, 1=m, 2/3=km, 4/5=mi, 6=ft, 7=yd. Values 3 and 5 are P1 variants (one decimal place formatting).

## Key decisions
- **Phone's display_text preferred**: Bridge uses pre-formatted distance text from NavigationNextTurnDistanceEvent with unit suffix, falling back to computed distance only for legacy phones.
- **NavigationNotification as primary**: Modern phones (AA 16.2) send 0x8006+0x8007 instead of deprecated 0x8004. Bridge handles both paths.
- **No background tap on Now Playing**: Controls only through explicit buttons, per user decision.
- **Resize testing deferred**: Edit mode (Phase 07) needed for runtime resize; verified at default placement sizes only.

## Pi verification results
- Navigation widget: active state showing correct distance in miles, muted placeholder when inactive
- Now Playing widget: title, artist, controls functional with source indicator
- Old BT Now Playing widget: removed from picker
- Widget picker: long-press placement working on touchscreen

## Artifacts
| File | Lines | Purpose |
|------|-------|---------|
| qml/widgets/NavigationWidget.qml | ~130 | Nav turn-by-turn widget |
| qml/widgets/NowPlayingWidget.qml | ~180 | Unified now playing widget |
| src/main.cpp | (modified) | Bridge instantiation, wiring, context properties |
| src/plugins/bt_audio/BtAudioPlugin.cpp | (modified) | Empty widgetDescriptors() |
| src/core/aa/NavigationDataBridge.hpp/.cpp | (modified) | Added modern nav message support |

## Commits
- 23fe9f3 feat(06-03): wire navigation and now-playing content widgets
- e5d8c51 feat(06-03): add long-press widget picker to home grid
- f06152c fix(nav): wire bridge to modern AA 16.2 nav messages
- 0a3d691 fix(nav): combine phone display_text with unit suffix
- 510529f fix(nav): correct distance unit enum from AA source analysis
