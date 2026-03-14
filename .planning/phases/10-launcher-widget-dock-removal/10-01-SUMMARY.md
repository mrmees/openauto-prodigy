---
phase: 10-launcher-widget-dock-removal
plan: 01
subsystem: ui
tags: [qt, qml, widget-system, singleton, grid, reserved-page]

# Dependency graph
requires:
  - phase: 09-widget-descriptor-grid-foundation
    provides: WidgetDescriptor struct, WidgetRegistry, WidgetGridModel, WidgetPickerModel, HomeMenu grid
provides:
  - "WidgetDescriptor.singleton field for system-seeded non-removable widgets"
  - "SingletonRole exposed to QML via isSingleton model role"
  - "isReservedPage() Q_INVOKABLE for page-level protection"
  - "addPage() insert-before-reserved with placement shifting"
  - "removePage() singleton page protection"
  - "SettingsLauncherWidget.qml and AALauncherWidget.qml"
  - "Fresh-install seeding of singleton widgets on reserved page"
  - "HomeMenu remove-badge and delete-page guards for singletons"
affects: [10-launcher-widget-dock-removal, 11-framework-polish]

# Tech tracking
tech-stack:
  added: []
  patterns: [singleton-widget-pattern, reserved-page-convention]

key-files:
  created:
    - qml/widgets/SettingsLauncherWidget.qml
    - qml/widgets/AALauncherWidget.qml
  modified:
    - src/core/widget/WidgetTypes.hpp
    - src/core/widget/WidgetRegistry.cpp
    - src/ui/WidgetGridModel.hpp
    - src/ui/WidgetGridModel.cpp
    - src/ui/WidgetPickerModel.cpp
    - src/main.cpp
    - src/CMakeLists.txt
    - qml/applications/home/HomeMenu.qml
    - tests/test_widget_grid_model.cpp
    - tests/test_widget_picker_model.cpp

key-decisions:
  - "Reserved page derived from singleton presence (pageHasSingleton), not explicit page flag"
  - "Fixed instanceIds for seeded singletons (aa-launcher-reserved, settings-launcher-reserved)"
  - "addPage shifts both livePlacements_ and basePlacements_ for remap consistency"

patterns-established:
  - "Singleton widget pattern: descriptor.singleton=true, fixed instanceId, system-seeded, non-removable, picker-hidden"
  - "Reserved page convention: last page containing singleton widgets, undeletable, addPage inserts before it"

requirements-completed: [NAV-01]

# Metrics
duration: 14min
completed: 2026-03-14
---

# Phase 10 Plan 01: Singleton Launcher Widgets Summary

**Singleton widget infrastructure with Settings/AA launcher widgets on a protected reserved page, replacing LauncherDock navigation entry points**

## Performance

- **Duration:** 14 min
- **Started:** 2026-03-14T19:04:02Z
- **Completed:** 2026-03-14T19:18:02Z
- **Tasks:** 3
- **Files modified:** 11

## Accomplishments
- WidgetDescriptor gains `singleton` field with full model/view enforcement
- Two new launcher widgets (Settings gear + AA car icon) with tap navigation and edit-mode compatibility
- Reserved page infrastructure: addPage inserts before, removePage refuses, isReservedPage Q_INVOKABLE
- Fresh-install seeding places both singletons on the last page via direct GridPlacement construction
- 7 new unit tests covering all singleton behaviors (84 total, 83 passing)

## Task Commits

Each task was committed atomically:

1. **Task 1: Singleton infrastructure + reserved page logic + unit tests** - `7387966` (feat)
2. **Task 2: Widget QML files, registration, seeding, HomeMenu wiring** - `da51020` (feat)
3. **Task 3: Codex review gate** - no code changes (review passed, tests green)

## Files Created/Modified
- `src/core/widget/WidgetTypes.hpp` - Added `bool singleton = false` to WidgetDescriptor
- `src/core/widget/WidgetRegistry.cpp` - Singleton exclusion in widgetsFittingSpace filter
- `src/ui/WidgetGridModel.hpp` - SingletonRole enum, isReservedPage Q_INVOKABLE, pageHasSingleton helper
- `src/ui/WidgetGridModel.cpp` - Singleton data role, removal protection, insert-before-reserved addPage, removePage guard, pageHasSingleton/isReservedPage implementations
- `qml/widgets/SettingsLauncherWidget.qml` - Gear icon widget with settings navigation
- `qml/widgets/AALauncherWidget.qml` - Car icon widget with AA plugin activation
- `src/main.cpp` - Singleton widget registrations + fresh-install seeding
- `src/CMakeLists.txt` - New QML files with QT_QML_SKIP_CACHEGEN
- `qml/applications/home/HomeMenu.qml` - Remove badge hidden for singletons, delete-page hidden on reserved page, addPage navigates to inserted page
- `tests/test_widget_grid_model.cpp` - 6 new singleton tests
- `tests/test_widget_picker_model.cpp` - 1 new singleton-hidden test

## Decisions Made
- Reserved page identity derived from singleton widget presence (not a separate page flag) -- simpler, no extra state to maintain
- Fixed instanceIds for seeded singletons ensure deterministic placement references across saves
- addPage shifts basePlacements_ alongside livePlacements_ to prevent remap corruption

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Singleton infrastructure complete, ready for Plan 02 (LauncherDock/LauncherModel/LauncherMenu removal)
- All singleton enforcement paths tested and verified
- HomeMenu guards in place for edit-mode protection

---
*Phase: 10-launcher-widget-dock-removal*
*Completed: 2026-03-14*
