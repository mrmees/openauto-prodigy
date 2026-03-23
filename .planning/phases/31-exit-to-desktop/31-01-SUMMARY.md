---
phase: 31-exit-to-desktop
plan: 01
subsystem: ui
tags: [dm-tool, lightdm, session-switch, gesture-overlay, kiosk]

# Dependency graph
requires:
  - phase: 28-kiosk-session-infrastructure
    provides: XDG kiosk session and rpd-labwc desktop session coexistence
provides:
  - ApplicationController::exitToDesktop() method with dm-tool session switch
  - app.exitToDesktop ActionRegistry action
  - Desktop button in GestureOverlay (3-finger tap quick controls)
affects: [32-installer-integration]

# Tech tracking
tech-stack:
  added: []
  patterns: [dm-tool switch-to-user for atomic LightDM session switching]

key-files:
  created: []
  modified:
    - src/ui/ApplicationController.hpp
    - src/ui/ApplicationController.cpp
    - src/main.cpp
    - qml/components/GestureOverlay.qml

key-decisions:
  - "dm-tool switch-to-user for session switch -- atomic, no auth prompt for same user, LightDM handles VT management"
  - "500ms delayed quit after dm-tool to ensure LightDM registers the switch before kiosk session ends"
  - "Desktop button placed between Day/Night and Close in GestureOverlay -- natural position before the dismiss action"

patterns-established:
  - "Session switch via dm-tool: startDetached for process survival, delayed quit for registration"

requirements-completed: []

# Metrics
duration: 3min
completed: 2026-03-22
---

# Phase 31 Plan 01: Exit-to-Desktop Summary

**dm-tool session switch from kiosk to rpd-labwc desktop via ApplicationController + GestureOverlay Desktop button**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-23T02:14:51Z
- **Completed:** 2026-03-23T02:18:00Z
- **Tasks:** 3
- **Files modified:** 4

## Accomplishments
- ApplicationController gains exitToDesktop() using dm-tool switch-to-user for atomic LightDM session switch
- ActionRegistry wired with app.exitToDesktop action for decoupled invocation
- GestureOverlay has Desktop button (desktop_windows icon) in quick controls panel
- All 88 tests pass, build clean

## Task Commits

Each task was committed atomically:

1. **Task 1: Add exitToDesktop() to ApplicationController** - `ef5c65b` (feat)
2. **Task 2: Register action and add GestureOverlay button** - `ccf7cb5` (feat)
3. **Task 3: Build verification and tests** - `89a31ec` (chore)

## Files Created/Modified
- `src/ui/ApplicationController.hpp` - Added Q_INVOKABLE exitToDesktop() declaration
- `src/ui/ApplicationController.cpp` - Implemented exitToDesktop() with dm-tool + 500ms delayed quit
- `src/main.cpp` - Registered app.exitToDesktop action in ActionRegistry
- `qml/components/GestureOverlay.qml` - Added Desktop button between Day/Night and Close

## Decisions Made
- Used dm-tool switch-to-user (Pattern 3 from ARCHITECTURE.md) for atomic session switch -- no authentication prompt for same user, LightDM handles VT management
- 500ms delayed quit ensures LightDM registers the switch before the kiosk labwc session terminates (prevents autologin race back into kiosk)
- QProcess::startDetached so dm-tool survives even if the app exits immediately
- Clean exit (code 0) prevents systemd Restart=on-failure from restarting the app
- Desktop button uses Material Icons desktop_windows codepoint (\uef6e)
- Button order: Home, Day/Night, Desktop, Close -- Desktop before Close is natural (action before dismiss)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Exit-to-desktop functionality is complete at the app level
- Phase 32 (Installer Integration) can wire kiosk session creation into install.sh
- Hardware validation needed: dm-tool with Wayland sessions on Pi is MEDIUM confidence per ROADMAP

## Self-Check: PASSED

All 4 modified files exist. All 3 task commits verified (ef5c65b, ccf7cb5, 89a31ec).

---
*Phase: 31-exit-to-desktop*
*Completed: 2026-03-22*
