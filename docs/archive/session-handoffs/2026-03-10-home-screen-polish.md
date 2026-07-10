# Session Handoff: Home Screen Polish

**Date:** 2026-03-10
**Branch:** `dev/0.5.2`
**Status:** All 7 plan tasks complete, deployed and running on Pi

## What Was Done

Implemented the full home screen polish plan (`docs/plans/2026-03-10-home-screen-polish-plan.md`):

### Glass-Effect Widget Cards
- WidgetHost renders semi-transparent `surfaceContainer` background + 1px `outlineVariant` border
- Per-pane opacity stored in `WidgetPlacement.config["opacity"]` (default 0.25)
- `paneOpacity()`/`setPaneOpacity()` on WidgetPlacementModel, YAML persistence
- Widget QML files (Clock, AAStatus, NowPlaying) now have `color: "transparent"` — WidgetHost provides the glass background

### Long-Press Context Menu
- Long-press pane → floating `WidgetContextMenu` popup (not direct picker)
- Three options: Change Widget, Opacity (expandable slider), Clear
- Context menu dismisses on scrim tap
- "Change Widget" opens the existing WidgetPicker (Clear button removed from picker)

### Force Dark Mode
- `ThemeService.forceDarkMode` property — when true, `nightMode()` returns true for QML
- `realNightMode()` accessor for AA SensorChannelHandler (reads real provider state)
- Config key: `display.force_dark_mode` (default: `true`)
- `toggleMode()` disables force-dark for the session when active
- Settings UI: "Always Use Dark Mode" toggle in Day/Night Mode section
- Night mode source controls dimmed (opacity 0.4) when force dark is on

## Bugs Fixed During Session
1. **Opacity not saving** — `placementsChanged` was updating in-memory YAML but never calling `save()`. Added `yamlConfig->save(yamlPath)` to the lambda in main.cpp.
2. **Context menu click overlap** — Items had transparent backgrounds and zero spacing. Fixed with `surfaceContainer` backgrounds + `UiMetrics.gap * 0.5` spacing.
3. **Opacity slider touch** — Restructured opacity section: header row and slider are separate Items, each with own MouseArea. No more overlapping MouseArea stealing slider events.
4. **`opacityChanged` signal collision** — `Item.opacity` auto-generates `opacityChanged`. Renamed to `paneOpacityAdjusted`.
5. **`onToggled` not a SettingsToggle property** — Used `Connections` on `ConfigService.onConfigChanged` instead.

## Test Coverage
- 3 new tests: `testDefaultPaneOpacity`, `testSetPaneOpacity`, `testPaneOpacity`
- 4 new tests: `forceDarkModeOverridesNightMode`, `forceDarkModeRealNightState`, `forceDarkModeSignals`, `forceDarkModeToggle`
- Full suite: 71/71 passing

## Files Changed (key files)
- `src/ui/WidgetPlacementModel.hpp/cpp` — paneOpacity/setPaneOpacity
- `src/core/YamlConfig.cpp` — config map read/write for placements, force_dark_mode default
- `src/core/services/ThemeService.hpp/cpp` — forceDarkMode property, nightMode() override
- `src/main.cpp` — save on placementsChanged, force dark mode startup
- `qml/components/WidgetHost.qml` — glass background + opacity binding
- `qml/components/WidgetContextMenu.qml` — new context menu component
- `qml/components/WidgetPicker.qml` — removed Clear button
- `qml/applications/home/HomeMenu.qml` — context menu flow
- `qml/applications/settings/DisplaySettings.qml` — force dark toggle
- `qml/widgets/{Clock,AAStatus,NowPlaying}Widget.qml` — transparent backgrounds
- `tests/test_widget_placement_model.cpp` — 3 new tests
- `tests/test_theme_service.cpp` — 4 new tests

## What's Next
- **Visual tuning on Pi** — Matt confirmed widgets load and work, but may want further visual polish (font sizes, spacing, widget content styling at various opacities)
- **Batch 2 plan tasks are all done** — no remaining tasks from the polish plan
- **Branch not merged** — `dev/0.5.2` has all widget system work. Ready for PR when Matt's satisfied.

## Gotchas Learned
- **`SettingsToggle` has no external `onToggled` signal** — the internal Switch's `onToggled` is private. Use `Connections` on `ConfigService.onConfigChanged` to react to toggle changes.
- **QML `Item.opacity` auto-generates `opacityChanged` signal** — don't name custom signals `opacityChanged` on Item subclasses.
- **Cross-build needs rebuild after C++ changes** — the Docker cross-build has its own build cache. Pushing QML-only changes + old binary = mismatched state.
- **`systemctl restart` rate limits after failures** — use `systemctl reset-failed` before retry.
