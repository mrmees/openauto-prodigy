---
phase: 19-widget-instance-config
verified: 2026-03-16T23:30:00Z
status: passed
score: 8/8 automated must-haves verified
re_verification: false
human_verification:
  - test: "Gear icon + config sheet on Pi hardware"
    expected: "Gear visible only on clock widget in edit mode. Config sheet opens, shows '24-hour' as default, changing to 12h updates clock immediately. Setting persists across restart. Two clock instances have independent config."
    why_human: "UI behavior, live widget update, Pi hardware validation — cannot verify programmatically"
    result: "passed — all 5 Pi test steps verified by user 2026-03-16"
---

# Phase 19: Widget Instance Config Verification Report

**Phase Goal:** Any widget can declare configuration options and users can set them per-placement
**Verified:** 2026-03-16T23:30:00Z
**Status:** passed (all automated checks pass, Pi hardware sign-off complete)
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 1 | A widget placement stores key-value config that is independent from other placements of the same widget | VERIFIED | `GridPlacement.config QVariantMap` in `WidgetTypes.hpp:62`; `setWidgetConfig` operates per `instanceId` in `WidgetGridModel.cpp:320-341` |
| 2 | Widget config survives app restart via YAML round-trip | VERIFIED | `YamlConfig.cpp` reads `node["config"]` (line 841) and writes it when non-empty (line 893); test `testGridPlacementsConfigRoundTrip` passes |
| 3 | Grid remap only affects geometry — config values are untouched | VERIFIED | `validateConfig` is called only on startup load and `setWidgetConfig`; remap path (`remapPlacements`) does not touch `p.config`; `testSetPlacementsValidatesLoadedConfig` and remap tests pass |
| 4 | Effective config merges persisted values over defaultConfig defaults | VERIFIED | `WidgetInstanceContext::effectiveConfig()` at `WidgetInstanceContext.cpp:59-62` merges `defaultConfig_` then overlays `instanceConfig_`; `testEffectiveWidgetConfig` passes |
| 5 | setWidgetConfig validates against configSchema before persisting (unknown keys rejected, types coerced, ranges clamped) | VERIFIED | `validateConfig` called from `setWidgetConfig` at `WidgetGridModel.cpp:324`; 5 validation tests pass (`testValidateConfigRejectsUnknownKeys`, `testValidateConfigClampsIntRange`, `testValidateConfigCoercesBool`, `testValidateConfigRejectsInvalidEnum`, malformed rejection) |
| 6 | Config loaded from YAML on startup is validated against schema | VERIFIED | `setPlacements` calls `validateConfig` per placement at `WidgetGridModel.cpp:784-787`; `testSetPlacementsValidatesLoadedConfig` and `testSetPlacementsStripsUnknownKeys` pass |
| 7 | User can open a config sheet from a gear icon in edit mode | VERIFIED (automated) | `configGear` in `HomeMenu.qml:461`, `visible: homeScreen.editMode && model.hasConfigSchema`; taps `configSheet.openConfig()`; `WidgetConfigSheet.qml` is 408 lines of substantive schema-driven UI |
| 8 | Clock widget reads effectiveConfig.format and renders 12h or 24h accordingly | VERIFIED | `ClockWidget.qml:24` — `readonly property string timeFormat: currentEffectiveConfig.format \|\| "24h"`; Timer uses `"h:mm AP"` for 12h, `"HH:mm"` for 24h at line 77 |

**Score:** 8/8 truths verified (automated)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/core/widget/WidgetTypes.hpp` | ConfigSchemaField struct, configSchema on WidgetDescriptor, config on GridPlacement | VERIFIED | `struct ConfigSchemaField` at line 13; `QList<ConfigSchemaField> configSchema` at line 39; `QVariantMap config` at line 62 |
| `src/core/YamlConfig.cpp` | YAML serialization/deserialization of per-instance config map | VERIFIED | gridPlacements reads `node["config"]` (line 841); setGridPlacements writes config map (line 893) |
| `src/ui/WidgetGridModel.hpp` | ConfigRole, HasConfigSchemaRole, setWidgetConfig, widgetConfig, effectiveWidgetConfig, widgetConfigChanged signal | VERIFIED | All roles at lines 39-43; all methods at lines 59-63; signal at line 105; `validateConfig` private at line 125 |
| `src/ui/WidgetInstanceContext.hpp` | effectiveConfig Q_PROPERTY | VERIFIED | `Q_PROPERTY(QVariantMap effectiveConfig READ effectiveConfig NOTIFY effectiveConfigChanged)` at line 25 |
| `qml/widgets/ClockWidget.qml` | Reads effectiveConfig.format | VERIFIED | Declarative binding at lines 23-24 |
| `qml/components/WidgetConfigSheet.qml` | Bottom sheet with schema-driven config controls | VERIFIED | 408 lines; enum/bool/intrange components; `applyConfig` override-only write path; reads `effectiveWidgetConfig`, `defaultConfigForWidget`, `configSchemaForWidget` |
| `qml/applications/home/HomeMenu.qml` | Gear icon in edit mode | VERIFIED | `configGear` Rectangle at line 461; `visible: homeScreen.editMode && model.hasConfigSchema`; `WidgetConfigSheet { id: configSheet }` at line 1037 |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `WidgetGridModel.cpp` | `WidgetTypes.hpp` | `p.config` field read/write | VERIFIED | `p.config = validated` (line 325); `return p.config` (line 44) |
| `WidgetGridModel.cpp` | `WidgetInstanceContext.cpp` | `widgetConfigChanged` signal triggers `setInstanceConfig` | VERIFIED | Signal emitted at line 341; `WidgetContextFactory` connects to it in constructor and calls `ctx->setInstanceConfig()` |
| `WidgetContextFactory.cpp` | `WidgetInstanceContext.cpp` | `activeContexts_` QSet tracking, fan-out on `widgetConfigChanged` | VERIFIED | Constructor connects `gridModel_->widgetConfigChanged` to lambda that iterates `activeContexts_.value(instanceId)` and calls `setInstanceConfig` on each |
| `YamlConfig.cpp` | `WidgetTypes.hpp` | Serializes `GridPlacement.config` to/from YAML | VERIFIED | Two separate YAML sections confirmed: `gridPlacements()` at line 841 and `setGridPlacements()` at line 893 |
| `ClockWidget.qml` | `WidgetInstanceContext.hpp` | `widgetContext.effectiveConfig.format` declarative binding | VERIFIED | `currentEffectiveConfig: widgetContext ? widgetContext.effectiveConfig : ({})` at line 23; Timer reads `clockWidget.timeFormat` at line 77 |
| `WidgetConfigSheet.qml` | `WidgetGridModel` | reads `effectiveWidgetConfig()`, `defaultConfigForWidget()`, `configSchemaForWidget()`; writes via `setWidgetConfig()` | VERIFIED | All four invokables called in `openConfig()` and `applyConfig()` functions |
| `HomeMenu.qml` | `WidgetConfigSheet.qml` | gear icon tap calls `configSheet.openConfig()` | VERIFIED | `onClicked: configSheet.openConfig(model.instanceId, model.widgetId, model.displayName, model.iconName)` at line 484 |
| `WidgetGridModel.cpp setPlacements` | `validateConfig` | startup load path validates persisted config | VERIFIED | `p.config = validateConfig(p.widgetId, p.config)` at line 785 inside `setPlacements` |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|---------|
| WC-01 | 19-01-PLAN | Widget placements support per-instance configuration (key-value map persisted in YAML) | SATISFIED | `GridPlacement.config` exists; YAML round-trip tested and passing |
| WC-02 | 19-02-PLAN | Widget config is editable via host-rendered config sheet opened from gear icon in edit mode | SATISFIED (automated) | `WidgetConfigSheet.qml` (408 lines) wired to `HomeMenu.qml` gear icon; Pi hardware verification pending |
| WC-03 | 19-01-PLAN | Widget config changes persist across restarts and survive grid remap | SATISFIED | YAML persistence verified; remap does not touch config; startup validation sanitizes stale values |

No orphaned requirements — all three WC requirements claimed by plans and verified.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| None | — | — | — | — |

No TODOs, FIXMEs, stubs, or placeholder returns found in any modified files.

### Human Verification Required

#### 1. Config Sheet on Pi Hardware

**Test:** Cross-build + deploy to Pi, enter edit mode. Tap the gear icon on the clock widget.
**Expected:**
- Gear visible only on clock widget (not AA Status, Now Playing, or other widgets)
- Config sheet slides up showing "Time Format" control with "24-hour" as current value (the default, not blank)
- Changing to "12-hour" updates the clock widget immediately (within 1 second, no sheet close required)
- Closing the sheet and restarting the app preserves the setting
- Adding a second clock widget shows it at "24-hour" (the default) independently
- Changing one clock's format does not affect the other

**Why human:** Pi display, touch input, animated slide-up, live visual widget update, actual YAML persistence on disk, and per-instance independence across a restart all require physical device verification.

### Gaps Summary

No automated gaps. All 8 observable truths verified, all artifacts substantive and wired, all key links confirmed in source code. 87/87 tests pass.

The only outstanding item is the Pi hardware smoke check for the config sheet UI (WC-02), which the plan itself designated as a blocking human checkpoint (19-02-PLAN Task 2).

---
_Verified: 2026-03-16T23:30:00Z_
_Verifier: Claude (gsd-verifier)_
