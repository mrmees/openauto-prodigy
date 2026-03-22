---
phase: 14-color-audit-m3-expressive-tokens
plan: 01
subsystem: ui
tags: [m3, theme, qcolor, night-mode, guardrail, semantic-tokens]

# Dependency graph
requires: []
provides:
  - surfaceTintHigh/surfaceTintHighest computed Q_PROPERTYs for centralized popup tint
  - warning/onWarning semantic tokens for amber status indicators
  - Night comfort guardrail clamping accent HSL saturation to 0.55
  - isAccentRole() helper identifying primary/secondary/tertiary groups
affects: [14-02-qml-audit]

# Tech tracking
tech-stack:
  added: []
  patterns: [night-guardrail-in-activeColor, computed-tint-blend, isAccentRole-set]

key-files:
  created: []
  modified:
    - src/core/services/ThemeService.hpp
    - src/core/services/ThemeService.cpp
    - tests/test_theme_service.cpp

key-decisions:
  - "HSL saturation clamp at 0.55 for night comfort guardrail"
  - "Guardrail applies inside activeColor() so tint blends use clamped primary automatically"
  - "Accent roles: primary, primary-container, secondary, secondary-container, tertiary, tertiary-container"

patterns-established:
  - "Night guardrail: isAccentRole check + HSL saturation clamp in activeColor()"
  - "Computed tint: 88/12 blend via QColor::fromRgbF in dedicated accessor"

requirements-completed: [CA-01, CA-03]

# Metrics
duration: 5min
completed: 2026-03-15
---

# Phase 14 Plan 01: ThemeService C++ Infrastructure Summary

**Centralized surface tint blending, M3 amber warning tokens, and night comfort guardrail clamping accent saturation at 0.55**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-15T20:55:41Z
- **Completed:** 2026-03-15T21:01:11Z
- **Tasks:** 1
- **Files modified:** 3

## Accomplishments
- Added surfaceTintHigh/surfaceTintHighest computed properties (88/12 blend) replacing inline QML math in 4 files
- Added warning/onWarning semantic tokens (M3 amber #FF9800) for status indicators
- Implemented night comfort guardrail that clamps accent role HSL saturation to 0.55 max
- 7 new tests plus 1 updated test; all 86 tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1 RED: Add failing tests** - `9cb290e` (test)
2. **Task 1 GREEN: Implement tint, warning, guardrail** - `67f4504` (feat)

## Files Created/Modified
- `src/core/services/ThemeService.hpp` - 4 new Q_PROPERTYs, 4 public method declarations, isAccentRole helper
- `src/core/services/ThemeService.cpp` - Tint blend implementation, warning tokens, isAccentRole set, guardrail in activeColor()
- `tests/test_theme_service.cpp` - 7 new test slots, updated nightModeColors for guardrail-adjusted primary

## Decisions Made
- HSL saturation clamp (not HCT chroma) at 0.55 threshold -- simpler, Qt-native, sufficient for comfort
- Guardrail inserted inside activeColor() so all consumers (including tint computations) get clamped values automatically -- avoids double-clamping issues since tint *should* use the clamped primary
- Accent role set: 6 roles (primary, primary-container, secondary, secondary-container, tertiary, tertiary-container) -- matches context decisions

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Updated existing nightModeColors test for guardrail**
- **Found during:** Task 1 GREEN phase
- **Issue:** Existing test expected raw night primary #c73650 (sat=0.573) but guardrail now clamps to ~#c43952 (sat=0.55)
- **Fix:** Changed QCOMPARE to tolerance-based check (QColor from setHslF has float precision differences from hex-parsed QColor)
- **Files modified:** tests/test_theme_service.cpp
- **Verification:** All 86 tests pass
- **Committed in:** 67f4504

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Necessary adaptation of existing test to account for new guardrail behavior. No scope creep.

## Issues Encountered
- QColor strict equality fails between hex-parsed and setHslF-constructed colors even when RGB values match -- floating point representation differs internally. Solved by using tolerance-based comparison (qAbs <= 1).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All 4 new ThemeService properties ready for QML binding in Plan 02
- Guardrail active and tested -- Plan 02 can reference theme colors knowing night mode is comfort-safe
- No blockers

## Self-Check: PASSED

---
*Phase: 14-color-audit-m3-expressive-tokens*
*Completed: 2026-03-15*
