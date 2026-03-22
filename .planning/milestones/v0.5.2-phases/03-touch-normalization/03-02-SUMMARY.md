---
phase: 03-touch-normalization
plan: 02
subsystem: ui
tags: [qml, settings, alternating-rows, touch, material-design, accordion]

requires:
  - phase: 03-touch-normalization
    provides: SettingsRow component and flat-mode controls from Plan 01
provides:
  - All 9 settings pages normalized with SettingsRow alternating-row styling
  - Debug AA Protocol Test accordion toggle
  - Simplified Bluetooth Forget button (no shadow)
affects: []

tech-stack:
  added: []
  patterns: [accordion-toggle-settings, codec-sub-row-indentation, status-dot-indicator]

key-files:
  created: []
  modified:
    - qml/applications/settings/DisplaySettings.qml
    - qml/applications/settings/AudioSettings.qml
    - qml/applications/settings/ConnectionSettings.qml
    - qml/applications/settings/DebugSettings.qml

key-decisions:
  - "Codec sub-rows use index*3 stride for rowIndex to reserve slots for sub-rows"
  - "AA Protocol Test uses accordion pattern (inline expand/collapse) not sub-page navigation"
  - "Connection status shown as small colored dot in AA test row (green=connected, gray=not)"
  - "InfoBanner removed from Audio -- restartRequired icon on controls is sufficient"
  - "Forget button simplified to outlined rectangle (no MultiEffect shadow, no scale animation)"

patterns-established:
  - "Accordion toggle: SettingsRow interactive:true toggles root property, ColumnLayout visible binds to it"
  - "Sub-row indentation: Item inside SettingsRow with anchors.leftMargin for nested controls"
  - "Repeater + SettingsRow offset: rowIndex: index + N to account for static rows above"

requirements-completed: [TCH-01, TCH-02, TCH-03]

duration: 3min
completed: 2026-03-11
---

# Phase 3 Plan 02: Settings Row Complex Pages Summary

**SettingsRow alternating-row styling applied to Display, Audio, Bluetooth, and Debug pages with accordion AA test toggle and simplified controls**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-11T04:53:00Z
- **Completed:** 2026-03-11T04:56:07Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Applied SettingsRow wrappers to all controls on Display, Audio, Bluetooth, and Debug pages
- Replaced Debug AA Protocol Test inline button grid with accordion toggle (tap to expand/collapse) with connection status dot
- Removed all helper/subtitle text and fontCaption usage across all settings pages
- Simplified Bluetooth Forget button (plain outlined rectangle, no shadow or press animation)
- Removed InfoBanner from Audio page
- All 9 settings pages now use consistent SettingsRow alternating-row styling

## Task Commits

Each task was committed atomically:

1. **Task 1: Apply SettingsRow to Display, Audio, and Bluetooth pages** - `5c393f8` (feat)
2. **Task 2: Apply SettingsRow to Debug page with AA test accordion** - `e1d6b0e` (feat)

## Files Created/Modified
- `qml/applications/settings/DisplaySettings.qml` - SettingsRow wrappers on Screen, UI Scale, Brightness, Navbar Position, Show Navbar controls
- `qml/applications/settings/AudioSettings.qml` - SettingsRow wrappers, flat pickers, InfoBanner removed
- `qml/applications/settings/ConnectionSettings.qml` - SettingsRow wrappers, simplified Forget button, removed dividers and MultiEffect
- `qml/applications/settings/DebugSettings.qml` - SettingsRow wrappers on all sections, codec sub-row indentation, accordion AA test toggle, removed helper text

## Decisions Made
- Codec sub-rows use `index * 3` stride for rowIndex (0=codec, 1=hw/sw, 2=decoder name) to give each codec group consistent alternation
- AA Protocol Test uses accordion pattern (expand/collapse in-place) rather than StackView sub-page -- simpler, no navigation complexity
- Connection status dot (green/gray) embedded directly in the AA test row header
- InfoBanner removed entirely from Audio page -- the restartRequired icon on individual controls already communicates the restart need
- Forget button stripped to minimal outlined style (border only, no MultiEffect shadow, no scale animation)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All settings pages fully normalized with consistent SettingsRow styling
- Phase 3 (Touch Normalization) complete -- all plans executed
- Ready for visual testing on Pi hardware

---
*Phase: 03-touch-normalization*
*Completed: 2026-03-11*
