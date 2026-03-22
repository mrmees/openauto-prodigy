---
phase: 29-compositor-splash-handoff
plan: 01
subsystem: infra
tags: [kiosk, swaybg, splash, wayland, labwc, frameSwapped]

# Dependency graph
requires:
  - phase: 28-kiosk-session-infrastructure
    provides: "Kiosk autostart file and labwc config directory"
provides:
  - swaybg splash line in kiosk autostart (branded image visible from compositor start)
  - frameSwapped-based splash dismissal in main.cpp (kills swaybg after first rendered frame)
affects: [30-boot-splash, 32-installer-integration]

# Tech tracking
tech-stack:
  added: []
  patterns: [compositor-owned splash with app-owned dismissal via frameSwapped one-shot]

key-files:
  created: []
  modified:
    - docs/pi-config/kiosk/labwc/autostart
    - src/main.cpp

key-decisions:
  - "500ms delay after frameSwapped before pkill -- ensures compositor has presented the frame (Pitfall 6 mitigation)"
  - "pkill -f 'swaybg.*splash' pattern -- targets only kiosk splash, not any desktop swaybg instance"
  - "QMetaObject::Connection heap-allocated for clean one-shot disconnect -- avoids static bool or dangling lambda captures"

patterns-established:
  - "Compositor-owned splash with app-owned dismissal: swaybg launched by labwc autostart, killed by app after first frame"
  - "One-shot signal pattern: heap-allocated QMetaObject::Connection with disconnect+delete in lambda"

requirements-completed: []

# Metrics
duration: 6min
completed: 2026-03-22
---

# Phase 29 Plan 01: Compositor Splash Handoff Summary

**swaybg splash in kiosk autostart with frameSwapped-based dismissal in main.cpp -- eliminates black gap between compositor start and app first frame**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-22T23:24:00Z
- **Completed:** 2026-03-22T23:30:40Z
- **Tasks:** 3 (2 code, 1 verification-only)
- **Files modified:** 2

## Accomplishments

- Kiosk autostart now launches swaybg with branded splash as first Wayland surface after compositor starts
- main.cpp gains one-shot frameSwapped handler that kills swaybg 500ms after first rendered frame
- Full build (88 tests) passes cleanly with the new code

## Task Commits

Each task was committed atomically:

1. **Task 1: Add swaybg splash to kiosk autostart** - `4cd1af6` (feat)
2. **Task 2: Implement frameSwapped splash dismissal in main.cpp** - `92df7fc` (feat)
3. **Task 3: Build verification** - (verification only, no commit)

## Files Created/Modified

- `docs/pi-config/kiosk/labwc/autostart` - Added `swaybg -i /usr/share/openauto-prodigy/splash.png -m fill &` as first command
- `src/main.cpp` - Added `#include <QProcess>` and frameSwapped one-shot handler block (~20 lines) after DisplayInfo wiring

## Decisions Made

- Used 500ms delay (not 200ms from ARCHITECTURE.md suggestion) after frameSwapped before killing swaybg. Per PITFALLS.md (Pitfall 6), frameSwapped doesn't mean "frame is visible on screen" -- the extra margin ensures the compositor has composited and presented the frame at vblank before the background layer process is killed. Since swaybg is on the background layer and auto-occluded by the app window anyway, the delay is purely cleanup timing.
- Used heap-allocated `QMetaObject::Connection` for the one-shot pattern instead of a `static bool` flag. This is cleaner -- the lambda disconnects itself and frees the connection object on first fire. No persistent state in the enclosing scope.
- Used `pkill -f "swaybg.*splash"` rather than bare `pkill swaybg` to avoid killing any swaybg instance that might be running in a concurrent desktop session (per ARCHITECTURE.md guidance).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Initialized git submodule for proto headers**
- **Found during:** Task 3 (build verification)
- **Issue:** Worktree submodule not initialized -- proto headers missing, build failed on `PingRequestMessage.pb.h`
- **Fix:** `git submodule update --init --recursive` + CMake reconfigure
- **Files modified:** None (submodule checkout, no code changes)
- **Verification:** Full build passes (100%), all 88 tests pass
- **Committed in:** N/A (submodule state is already tracked)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Submodule init was necessary for build verification. No scope creep.

## Issues Encountered

- The worktree had uninitialized submodules (same as Phase 28). Required `git submodule update --init --recursive` + CMake reconfigure to generate proto headers. This is a worktree-specific issue, not a codebase problem.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Splash artwork exists at `assets/splash.png` -- Phase 30 (RPi Boot Splash) will convert it to TGA for kernel splash
- Phase 32 (Installer Integration) will deploy `splash.png` to `/usr/share/openauto-prodigy/` and wire the autostart + main.cpp changes into the installer flow
- No blockers

## Self-Check: PASSED

All modified files verified present:
- `docs/pi-config/kiosk/labwc/autostart` -- FOUND, contains swaybg line
- `src/main.cpp` -- FOUND, contains frameSwapped handler

Both task commits verified:
- `4cd1af6` -- FOUND in git log
- `92df7fc` -- FOUND in git log

---
*Phase: 29-compositor-splash-handoff*
*Completed: 2026-03-22*
