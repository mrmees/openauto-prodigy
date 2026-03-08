---
phase: 03-theme-color-system
plan: 02
subsystem: ui
tags: [qml, theming, material-design, web-config]

requires:
  - phase: 03-theme-color-system/01
    provides: ThemeService AA token properties (primary, surface, scrim, success, red, etc.)
provides:
  - Zero hardcoded hex colors in QML (except debug overlays)
  - All 34 QML files using semantic AA token property names
  - Web config theme editor using 16 AA token key names
affects: [04-final-polish]

tech-stack:
  added: []
  patterns:
    - "ThemeService.scrim for overlay backgrounds"
    - "Qt.rgba(color.r, color.g, color.b, alpha) for alpha-tinted theme colors"
    - "Qt.darker(ThemeService.color, factor) for pressed-state variants"
    - "ThemeService.onRed/onSuccess for text on colored backgrounds"

key-files:
  created: []
  modified:
    - qml/components/GestureOverlay.qml
    - qml/components/NotificationArea.qml
    - qml/components/PairingDialog.qml
    - qml/components/ExitDialog.qml
    - qml/applications/phone/PhoneView.qml
    - qml/applications/phone/IncomingCallOverlay.qml
    - qml/applications/settings/CompanionSettings.qml
    - qml/applications/settings/ConnectionSettings.qml
    - qml/applications/settings/ConnectivitySettings.qml
    - qml/applications/settings/EqSettings.qml
    - qml/controls/MaterialIcon.qml
    - web-config/templates/themes.html

key-decisions:
  - "Used Qt.rgba() with ThemeService color components for alpha-tinted panel backgrounds"
  - "Used ThemeService.onRed/onSuccess instead of hardcoded 'white' for text on colored buttons"
  - "Replaced border.color '#0f3460' with ThemeService.outline for panel borders"

patterns-established:
  - "Overlay scrim: ThemeService.scrim (derived color with built-in alpha)"
  - "Alpha panels: Qt.rgba(ThemeService.X.r, ThemeService.X.g, ThemeService.X.b, 0.87)"
  - "Button pressed states: Qt.darker(ThemeService.color, 1.15-1.2)"

requirements-completed: [THM-03]

duration: 8min
completed: 2026-03-08
---

# Phase 3 Plan 2: QML Hardcoded Color Elimination Summary

**Replaced all 240+ old ThemeService property references and 50+ hardcoded hex colors across 34 QML files with semantic AA token names, achieving zero hardcoded colors in the UI**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-08T20:54:47Z
- **Completed:** 2026-03-08T21:02:26Z
- **Tasks:** 2
- **Files modified:** 45 (33 in Task 1, 12 in Task 2)

## Accomplishments
- All 240+ ThemeService property references renamed to AA token names (background, primary, textPrimary, etc.)
- All 50+ hardcoded hex color values replaced with ThemeService properties (scrim, success, red, yellow, etc.)
- Web config theme editor updated to 16 AA token key names
- Full build and all 64 tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Rename all ThemeService property references** - `a75b9b6` (feat)
2. **Task 2: Eliminate hardcoded hex colors + update web config** - `1b357ca` (feat)

## Files Created/Modified
- `qml/**/*.qml` (33 files) - Renamed all old property references to new AA token names
- `qml/components/GestureOverlay.qml` - 17 hardcoded colors replaced (scrim, panel bg, text, buttons)
- `qml/components/NotificationArea.qml` - Panel bg and text colors themed
- `qml/components/PairingDialog.qml` - Scrim, reject/confirm buttons themed with onRed/onSuccess
- `qml/applications/phone/PhoneView.qml` - Status dot, call buttons, disabled state themed
- `qml/applications/phone/IncomingCallOverlay.qml` - Scrim, call buttons themed
- `qml/applications/settings/CompanionSettings.qml` - Status indicators (success/warning/error) themed
- `qml/applications/settings/ConnectionSettings.qml` - Connected indicator, forget button themed
- `qml/applications/settings/ConnectivitySettings.qml` - Connected indicator, forget text themed
- `qml/applications/settings/EqSettings.qml` - Delete background themed
- `web-config/templates/themes.html` - colorKeys updated to 16 AA token names

## Decisions Made
- Used `Qt.rgba(ThemeService.surfaceContainerLow.r, .g, .b, 0.87)` for alpha-tinted panel backgrounds instead of new dedicated property -- keeps derived colors minimal
- Used `ThemeService.onRed`/`ThemeService.onSuccess` for text on colored buttons instead of hardcoded "white" -- ensures contrast works across theme variations
- Replaced `"#0f3460"` border colors with `ThemeService.outline` -- these were de facto borders, not brand colors
- IPC code (`IpcServer.cpp`) needed no changes -- it reads from `dayColors()`/`nightColors()` maps which already use the new YAML keys from Plan 01

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Theme color system is complete: all UI colors flow from ThemeService
- Phase 4 (Final Polish) can proceed -- consistent theming foundation is in place
- Custom themes will automatically affect the entire UI through the semantic token system

---
*Phase: 03-theme-color-system*
*Completed: 2026-03-08*
