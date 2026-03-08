---
phase: 01-dpi-foundation
plan: 03
subsystem: installer
tags: [edid, screen-size, bash, dpi, hardware-detection]

requires: []
provides:
  - "EDID screen size probe in installer (probe_screen_size function)"
  - "display.screen_size config key persisted from installer"
  - "Interactive screen size prompt with accept/override flow"
affects: [01-dpi-foundation, 02-clock-scale-control]

tech-stack:
  added: []
  patterns: ["EDID binary parsing with od in bash", "Detailed timing descriptor + basic params fallback"]

key-files:
  created: []
  modified: ["install.sh"]

key-decisions:
  - "Pixel clock check at bytes 54-55 to distinguish timing vs monitor descriptors before reading physical size"
  - "SCREEN_SIZE variable initialized at top with other defaults for consistency"

patterns-established:
  - "EDID probe: check connected status, try detailed timing (mm), fall back to basic params (cm)"

requirements-completed: [DPI-01, DPI-02, DPI-04]

duration: 1min
completed: 2026-03-08
---

# Phase 1 Plan 3: Installer EDID Probe Summary

**EDID screen size detection in installer with detailed timing descriptor parsing, user confirm/override prompt, and config.yaml persistence as display.screen_size**

## Performance

- **Duration:** 1 min
- **Started:** 2026-03-08T17:34:34Z
- **Completed:** 2026-03-08T17:35:47Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments
- probe_screen_size() function parses EDID binary from /sys/class/drm for physical dimensions
- Detailed timing descriptor (mm precision) with pixel clock validation, basic params fallback (cm)
- Interactive prompt shows detected size with accept/override, or manual entry when EDID unavailable
- display.screen_size written to config.yaml in generate_config()
- Default 7.0" when EDID unavailable and user skips entry

## Task Commits

Each task was committed atomically:

1. **Task 1: Add EDID probe function and screen size prompt to installer** - `51faa6e` (feat)

## Files Created/Modified
- `install.sh` - Added SCREEN_SIZE default, probe_screen_size() function, screen size detection in setup_hardware(), screen_size in generate_config()

## Decisions Made
- Added pixel clock check (bytes 54-55 non-zero) before reading detailed timing descriptor physical dimensions, per research open question #1
- Placed screen size detection between touch device section and WiFi AP section in setup_hardware(), matching the plan's recommended position

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required. Matt tests the installer manually on the Pi.

## Next Phase Readiness
- Installer now persists display.screen_size to config.yaml
- Plans 01-01 and 01-02 provide the C++ and QML sides that consume this value
- Actual EDID testing requires Pi hardware (manual verification by Matt)

---
*Phase: 01-dpi-foundation*
*Completed: 2026-03-08*
