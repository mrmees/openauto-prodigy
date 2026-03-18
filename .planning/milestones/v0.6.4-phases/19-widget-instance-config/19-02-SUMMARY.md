---
phase: 19-widget-instance-config
plan: 02
subsystem: ui
tags: [qt, qml, widget-config, config-sheet, schema-driven-ui, bottom-sheet]

requires:
  - phase: 19-widget-instance-config
    plan: 01
    provides: ConfigSchemaField types, per-instance config on GridPlacement, WidgetGridModel config methods, effectiveConfig pipeline, clock widget proof-of-concept
provides:
  - WidgetConfigSheet.qml bottom sheet with schema-driven controls (enum/bool/intrange)
  - configSchemaForWidget() and defaultConfigForWidget() Q_INVOKABLEs on WidgetGridModel
  - Gear icon in edit mode for widgets with configSchema
  - Override-only write path (preserves default/override distinction)
  - Multi-context fan-out for live config updates via WidgetContextFactory QSet tracking
affects: [21-config-dependent-widgets]

tech-stack:
  added: []
  patterns: [schema-driven-config-ui, override-only-persistence, multi-context-fan-out]

key-files:
  created:
    - qml/components/WidgetConfigSheet.qml
  modified:
    - src/ui/WidgetGridModel.hpp
    - src/ui/WidgetGridModel.cpp
    - src/ui/WidgetContextFactory.hpp
    - src/ui/WidgetContextFactory.cpp
    - src/CMakeLists.txt
    - qml/applications/home/HomeMenu.qml
    - qml/widgets/ClockWidget.qml
    - tests/test_widget_grid_model.cpp
    - tests/test_widget_instance_context.cpp

key-decisions:
  - "Schema exposed as plain QVariantList via Q_INVOKABLE rather than a separate QAbstractListModel -- avoids dead-weight C++ model class"
  - "WidgetContextFactory tracks all live contexts per instanceId via QSet (not single pointer) -- SwipeView keeps adjacent pages alive with multiple delegates"
  - "Malformed bool/int values rejected by validateConfig rather than silently coerced to false/0"

patterns-established:
  - "Schema-driven config UI: configSchemaForWidget() returns QVariantList of field descriptors, Repeater + Loader renders type-specific controls"
  - "Override-only persistence: config sheet tracks overrideKeys, writes delta vs defaults, values changed back to default are removed from overrides"
  - "Multi-context fan-out: WidgetContextFactory uses QHash<QString, QSet<WidgetInstanceContext*>> to deliver config changes to all live contexts for an instanceId"

requirements-completed: [WC-02]

duration: 24min
completed: 2026-03-16
---

# Phase 19 Plan 02: Widget Config Sheet UI Summary

**Schema-driven bottom sheet with gear icon in edit mode, enum/bool/intrange controls, override-only persistence, and multi-context fan-out for live widget config updates**

## Performance

- **Duration:** 24 min (including cross-build + Pi deploy + human verification)
- **Started:** 2026-03-16T22:51:39Z
- **Completed:** 2026-03-17T00:22:04Z
- **Tasks:** 2
- **Files modified:** 13

## Accomplishments
- WidgetConfigSheet.qml bottom sheet with schema-driven controls rendered via Repeater + Loader pattern
- Gear icon badge (top-left) in edit mode, visible only for widgets with configSchema (clock widget as proof)
- Override-only write path: sheet loads effectiveConfig for display, tracks user changes via overrideKeys, writes only delta vs defaults
- configSchemaForWidget() and defaultConfigForWidget() Q_INVOKABLEs expose schema as plain QVariantList (no separate model class)
- WidgetContextFactory multi-context fan-out fixed for SwipeView adjacent page pre-loading
- Stricter validateConfig rejects malformed bool/int values instead of silently coercing
- Pi hardware verified: gear visibility, default display, live update, persistence, per-instance independence all pass
- 87 test executables pass with zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: WidgetConfigSheet QML + gear icon + WidgetGridModel Q_INVOKABLEs** - `354ae57` (feat)
2. **Task 1 fixes: live config updates, malformed input rejection, int-range label** - `c9a609e` (fix)
3. **Task 2: Human verification on Pi hardware** - approved (no commit, verification-only)

## Files Created/Modified
- `qml/components/WidgetConfigSheet.qml` - Bottom sheet with schema-driven config controls (enum dropdown, bool switch, int-range slider)
- `src/ui/WidgetGridModel.hpp` - Added configSchemaForWidget() and defaultConfigForWidget() Q_INVOKABLEs
- `src/ui/WidgetGridModel.cpp` - Implementations of schema/default Q_INVOKABLEs, stricter validateConfig for malformed values
- `src/ui/WidgetContextFactory.hpp` - QSet-based multi-context tracking per instanceId
- `src/ui/WidgetContextFactory.cpp` - Fan-out config changes to all live contexts, cleanup on destroyed
- `src/CMakeLists.txt` - Registered WidgetConfigSheet.qml in QML module
- `qml/applications/home/HomeMenu.qml` - Gear icon badge in edit mode, WidgetConfigSheet instance
- `qml/widgets/ClockWidget.qml` - Declarative effectiveConfig binding instead of imperative Timer read
- `tests/test_widget_grid_model.cpp` - Added malformed bool/int rejection tests
- `tests/test_widget_instance_context.cpp` - Added context replacement race and multi-context fan-out tests

## Decisions Made
- Schema exposed as plain QVariantList via Q_INVOKABLE (configSchemaForWidget) rather than creating a separate QAbstractListModel -- the config sheet iterates with Repeater over modelData, no need for a full model class
- WidgetContextFactory tracks all live contexts per instanceId via QSet rather than a single pointer -- SwipeView pre-loads adjacent pages, creating multiple delegate instances for the same widget
- Malformed values (garbage strings for bool, non-numeric for int) are now rejected by validateConfig rather than silently coerced to false/0

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed multi-context fan-out in WidgetContextFactory**
- **Found during:** Task 1 (code review before Pi deploy)
- **Issue:** WidgetContextFactory tracked only one context per instanceId. SwipeView keeps adjacent pages alive, so multiple delegates exist for the same widget. Config changes only reached the last-registered context.
- **Fix:** Changed activeContexts_ from QHash<QString, WidgetInstanceContext*> to QHash<QString, QSet<WidgetInstanceContext*>>, fan out config changes to all live contexts
- **Files modified:** src/ui/WidgetContextFactory.hpp, src/ui/WidgetContextFactory.cpp
- **Verification:** Multi-context fan-out test added, Pi verification confirmed live updates work
- **Committed in:** c9a609e

**2. [Rule 1 - Bug] Stricter malformed input rejection in validateConfig**
- **Found during:** Task 1 (code review)
- **Issue:** validateConfig silently coerced garbage strings to false (bool) and 0 (int), masking data corruption
- **Fix:** Only accept "true"/"false" strings for bool coercion, reject non-numeric strings for int
- **Files modified:** src/ui/WidgetGridModel.cpp
- **Verification:** Malformed rejection tests added and passing
- **Committed in:** c9a609e

**3. [Rule 1 - Bug] Fixed int-range value label truthiness check**
- **Found during:** Task 1 (code review)
- **Issue:** Int-range label used JS truthiness (`|| fieldData.rangeMin`) which displays rangeMin when value is 0
- **Fix:** Used explicit `!== undefined` check matching the slider pattern
- **Files modified:** qml/components/WidgetConfigSheet.qml
- **Committed in:** c9a609e

---

**Total deviations:** 3 auto-fixed (3 bugs)
**Impact on plan:** All fixes necessary for correctness. No scope creep.

## Issues Encountered
- Pi git pull failed due to TLS errors (GnuTLS handshake failure). Worked around by rsyncing QML files directly. Binary already contained compiled QML via qt_add_qml_module.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Widget instance config system complete end-to-end: data layer (Plan 01) + config sheet UI (Plan 02)
- Any widget with a configSchema on its WidgetDescriptor will automatically get a gear icon and schema-driven config sheet
- Phase 20 (simple widgets) can proceed in parallel -- those widgets don't need per-instance config
- Phase 21 (config-dependent widgets like clock styles, weather) can now build on this config infrastructure

---
*Phase: 19-widget-instance-config*
*Completed: 2026-03-16*
