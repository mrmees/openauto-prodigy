---
phase: 03-theme-color-system
plan: 01
subsystem: ui
tags: [qt, qml, theme, material-design, aa-wire-tokens]

# Dependency graph
requires: []
provides:
  - 16 base AA wire token Q_PROPERTYs on ThemeService
  - 5 derived color Q_PROPERTYs (scrim, pressed, barShadow, success, onSuccess)
  - 4 bundled theme YAMLs with all 16 token keys
affects: [03-02 QML migration, 03-03 Connected Device theme]

# Tech tracking
tech-stack:
  added: []
  patterns: [AA wire token naming for theme colors, derived colors computed not stored in YAML]

key-files:
  created: []
  modified:
    - src/core/services/ThemeService.hpp
    - src/core/services/ThemeService.cpp
    - config/themes/default/theme.yaml
    - config/themes/amoled/theme.yaml
    - config/themes/ocean/theme.yaml
    - config/themes/ember/theme.yaml
    - tests/data/themes/default/theme.yaml
    - tests/test_theme_service.cpp

key-decisions:
  - "Derived colors (scrim, pressed, barShadow, success, onSuccess) are computed constants, not stored in theme YAML"
  - "Old property names completely removed (no backwards compat shim) -- clean break per plan"

patterns-established:
  - "AA wire token naming: theme YAML keys use snake_case (on_surface), Q_PROPERTY accessors use camelCase (onSurface)"
  - "Derived colors: fixed values or computed from base tokens (pressed = onSurface at 10% alpha)"

requirements-completed: [THM-01, THM-02]

# Metrics
duration: 8min
completed: 2026-03-08
---

# Phase 3 Plan 1: Token Rename Summary

**16 base + 5 derived AA wire token Q_PROPERTYs on ThemeService, 4 bundled themes migrated to new key names**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-08T20:44:50Z
- **Completed:** 2026-03-08T20:52:33Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Replaced all 16 old OAP-derived Q_PROPERTYs with AA wire token names (primary, onSurface, surface, etc.)
- Added 5 derived color Q_PROPERTYs (scrim, pressed, barShadow, success, onSuccess)
- Migrated all 4 bundled theme YAMLs (default, amoled, ocean, ember) to new token keys with 6 new keys per theme
- Added comprehensive tests: allBaseTokensPresent, derivedColors, dayNightWithNewAccessors

## Task Commits

Each task was committed atomically:

1. **Task 1: Rename ThemeService properties and add derived colors (TDD)**
   - `762cfdc` test(03-01): add failing tests for AA token property rename
   - `3644917` feat(03-01): rename ThemeService to AA wire token properties + derived colors
2. **Task 2: Migrate all 4 bundled theme YAMLs to new token keys** - `69f1fc4`

## Files Created/Modified
- `src/core/services/ThemeService.hpp` - 16 base + 5 derived Q_PROPERTYs with AA token names
- `src/core/services/ThemeService.cpp` - Derived color implementations (scrim, pressed, barShadow, success, onSuccess)
- `config/themes/default/theme.yaml` - Default theme with AA token keys, v2.0.0
- `config/themes/amoled/theme.yaml` - AMOLED theme with AA token keys, v2.0.0
- `config/themes/ocean/theme.yaml` - Ocean theme with AA token keys, v2.0.0
- `config/themes/ember/theme.yaml` - Ember theme with AA token keys, v2.0.0
- `tests/data/themes/default/theme.yaml` - Test theme migrated to AA token keys
- `tests/test_theme_service.cpp` - Updated tests + 3 new test cases

## Decisions Made
- Derived colors are computed constants, not stored in theme YAML -- keeps themes simple, derived values consistent
- Old property names completely removed (no backwards compat shim) -- clean break as specified in plan

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- ThemeService AA token foundation complete
- Ready for Plan 02 (QML migration to new property names) and Plan 03 (Connected Device theme)
- Old Configuration class still uses old color struct names -- separate concern, not part of this plan

---
*Phase: 03-theme-color-system*
*Completed: 2026-03-08*
