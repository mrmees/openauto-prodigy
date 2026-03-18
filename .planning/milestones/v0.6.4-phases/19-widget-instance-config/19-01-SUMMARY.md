---
phase: 19-widget-instance-config
plan: 01
subsystem: ui
tags: [qt, qml, yaml, widget-config, schema-validation, tdd]

requires:
  - phase: 18-grid-home-screen
    provides: WidgetGridModel, GridPlacement, WidgetInstanceContext, WidgetContextFactory
provides:
  - ConfigSchemaField type definitions (Enum, Bool, IntRange)
  - Per-instance config map on GridPlacement with YAML persistence
  - Schema validation (unknown key rejection, range clamping, type coercion, enum validation)
  - Startup load validation (stale/malformed YAML config sanitized)
  - WidgetGridModel config roles, methods, and widgetConfigChanged signal
  - WidgetInstanceContext.effectiveConfig Q_PROPERTY (defaults merged with overrides)
  - WidgetContextFactory live config change notification pipeline
  - Clock widget proof-of-concept reading effectiveConfig.format for 12h/24h
affects: [19-02-config-sheet-ui, 21-config-dependent-widgets]

tech-stack:
  added: []
  patterns: [schema-validated-config, effective-config-merge, live-context-update-pipeline]

key-files:
  created: []
  modified:
    - src/core/widget/WidgetTypes.hpp
    - src/core/YamlConfig.cpp
    - src/ui/WidgetGridModel.hpp
    - src/ui/WidgetGridModel.cpp
    - src/ui/WidgetInstanceContext.hpp
    - src/ui/WidgetInstanceContext.cpp
    - src/ui/WidgetContextFactory.hpp
    - src/ui/WidgetContextFactory.cpp
    - src/main.cpp
    - qml/widgets/ClockWidget.qml
    - tests/test_widget_config.cpp
    - tests/test_widget_grid_model.cpp
    - tests/test_widget_instance_context.cpp
    - tests/test_widget_contract_qml.cpp
    - tests/data/test_widget_config.yaml

key-decisions:
  - "Constructor signature: added defaultConfig and instanceConfig as optional params with defaults rather than overloads"
  - "Validation rejects unknown keys silently (dropped, not errored) -- default fills missing values"
  - "widgetConfigChanged signal carries effective config (merged) for convenience, but setInstanceConfig receives raw overrides (context merges internally)"

patterns-established:
  - "Schema validation pattern: validateConfig() used both by setWidgetConfig (runtime) and setPlacements (startup load)"
  - "Effective config merge: defaults from descriptor, overrides from placement -- instance wins, defaults fill gaps"
  - "Live config pipeline: WidgetGridModel.widgetConfigChanged -> WidgetContextFactory -> WidgetInstanceContext.setInstanceConfig -> effectiveConfigChanged -> QML rebind"

requirements-completed: [WC-01, WC-03]

duration: 16min
completed: 2026-03-16
---

# Phase 19 Plan 01: Widget Instance Config Data Layer Summary

**Per-instance widget config with schema validation, YAML persistence, effective config merging, and live notification pipeline -- clock widget reads effectiveConfig.format for 12h/24h display**

## Performance

- **Duration:** 16 min
- **Started:** 2026-03-16T22:31:36Z
- **Completed:** 2026-03-16T22:47:36Z
- **Tasks:** 2
- **Files modified:** 15

## Accomplishments
- ConfigSchemaField struct with Enum/Bool/IntRange types, configSchema on WidgetDescriptor, config QVariantMap on GridPlacement
- Full YAML round-trip for per-instance config (empty config omitted for backward compat)
- Schema validation: rejects unknown keys, clamps IntRange, coerces bool strings, validates enum values
- Startup load validation in setPlacements sanitizes stale/malformed YAML config
- WidgetInstanceContext.effectiveConfig Q_PROPERTY merges descriptor defaults with instance overrides
- WidgetContextFactory tracks live contexts and wires widgetConfigChanged to setInstanceConfig for immediate QML updates
- Clock widget reads effectiveConfig.format to render 12h ("h:mm AP") or 24h ("HH:mm") time
- 20 new tests across 3 test files, 87 total tests pass with zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: ConfigSchemaField, GridPlacement config, YAML persistence, model roles, validation, clock widget** - `675cc7c` (feat)
2. **Task 2: effectiveConfig on WidgetInstanceContext, live config wiring via WidgetContextFactory** - `aa51723` (feat)

## Files Created/Modified
- `src/core/widget/WidgetTypes.hpp` - ConfigSchemaField struct, ConfigFieldType enum, configSchema on WidgetDescriptor, config on GridPlacement
- `src/core/YamlConfig.cpp` - Serialize/deserialize config map in gridPlacements/setGridPlacements
- `src/ui/WidgetGridModel.hpp` - ConfigRole, HasConfigSchemaRole, DisplayNameRole, IconNameRole, config methods, widgetConfigChanged signal, validateConfig, registry() accessor
- `src/ui/WidgetGridModel.cpp` - Implementation of config roles, setWidgetConfig, widgetConfig, effectiveWidgetConfig, validateConfig, startup load validation in setPlacements
- `src/ui/WidgetInstanceContext.hpp` - effectiveConfig Q_PROPERTY, setInstanceConfig, effectiveConfigChanged signal, defaultConfig_/instanceConfig_ members
- `src/ui/WidgetInstanceContext.cpp` - effectiveConfig merger, setInstanceConfig with equality check
- `src/ui/WidgetContextFactory.hpp` - activeContexts_ hash for live context tracking
- `src/ui/WidgetContextFactory.cpp` - widgetConfigChanged connection in constructor, createContext passes defaultConfig/instanceConfig, cleanup on destroyed
- `src/main.cpp` - Clock descriptor configSchema with format field, defaultConfig
- `qml/widgets/ClockWidget.qml` - Reads effectiveConfig.format for 12h/24h time format
- `tests/test_widget_config.cpp` - Config round-trip and empty config omission tests
- `tests/test_widget_grid_model.cpp` - 13 new config tests (CRUD, validation, signal, roles, startup validation, remap preservation)
- `tests/test_widget_instance_context.cpp` - 5 new effectiveConfig tests (defaults, override, merge, signal, empty)
- `tests/test_widget_contract_qml.cpp` - Updated constructor calls for new signature
- `tests/data/test_widget_config.yaml` - Added config sub-map for deserialization testing

## Decisions Made
- Constructor signature extended with optional defaultConfig and instanceConfig params rather than creating overloads -- cleaner API, required updating existing call sites
- Validation drops unknown keys silently rather than erroring -- defaults fill gaps via effectiveConfig merge
- widgetConfigChanged signal carries effective config (merged defaults+overrides) for convenience, but the factory passes raw overrides to setInstanceConfig (context handles merge internally)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed testGridPlacementsEmptyConfigOmitted false positive**
- **Found during:** Task 1
- **Issue:** Test checked `!text.contains("config:")` on full YAML output, but legacy WidgetPlacement defaults also serialize `config:` keys
- **Fix:** Narrowed check to widget_grid section only using regex extraction
- **Files modified:** tests/test_widget_config.cpp
- **Verification:** Test passes correctly
- **Committed in:** 675cc7c (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Minor test fix, no scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Config data layer complete and tested -- Plan 02 (config sheet UI) can now call WidgetGridModel.setWidgetConfig() to write validated config
- configSchema on descriptors enables Plan 02 to dynamically generate config sheet UI from schema fields
- effectiveConfig pipeline proven end-to-end with clock widget proof-of-concept
- All 87 tests pass

---
*Phase: 19-widget-instance-config*
*Completed: 2026-03-16*
