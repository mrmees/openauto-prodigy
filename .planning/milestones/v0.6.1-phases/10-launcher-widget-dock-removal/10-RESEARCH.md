# Phase 10: Launcher Widget & Dock Removal - Research

**Researched:** 2026-03-14
**Domain:** Qt 6 / QML widget system refactoring, C++ model modification
**Confidence:** HIGH

## Summary

Phase 10 is primarily a surgical removal and infrastructure addition within an already well-understood codebase. The work breaks into three domains: (1) adding singleton widget infrastructure to WidgetDescriptor/WidgetGridModel/WidgetPickerModel, (2) creating two trivial QML widgets (Settings gear + AA launcher icon), and (3) deleting LauncherDock, LauncherMenu, LauncherModel and all their references. The reserved page concept requires modifying `addPage()` and `removePage()` in WidgetGridModel and the corresponding QML in HomeMenu.qml.

All code involved is in-tree, well-tested C++ and QML following established patterns. No external libraries, no protocol work, no hardware dependencies. The primary risk is regression in home screen page management (add/delete/swipe) and ensuring the seeded default config correctly places singleton widgets on fresh installs.

**Primary recommendation:** Implement in two waves -- (1) add singleton infrastructure + new widgets + reserved page logic, (2) delete LauncherDock/LauncherModel/LauncherMenu and clean up references.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **No LauncherWidget with tiles** -- the roadmap's "quick-launch tiles" concept is replaced by a reserved utility page with two singleton system widgets (Settings + AA launcher)
- BT Audio and Phone are plumbing-only plugins -- no widgets, no launcher affordance, no home screen presence
- Reserved utility page is always the last home screen page; new pages insert before it, never after
- Reserved page is undeletable; singleton widgets on it are non-removable (X badge hidden), movable/resizable
- Singleton widgets are hidden from WidgetPickerModel (users cannot place additional copies)
- Settings widget: gear icon only, centered, no label, min 1x1, category "launcher", taps navigate to settings
- AA launcher widget: AA icon only, centered, no label, min 1x1, category "launcher", taps activate AA plugin
- Both widgets are system-seeded in YamlConfig defaults on fresh install
- Home-return behavior unchanged (returns to last-viewed page)
- LauncherDock, LauncherModel, LauncherMenu all deleted
- Widget picker serves as the conceptual "app drawer" replacement
- Doc/config cleanup for stale launcher references

### Claude's Discretion
- How reserved page is identified (last page by convention, explicit flag, or derived from singleton presence)
- AA launcher widget position on the reserved page
- Fresh-install default seeding in YamlConfig
- Cleanup scope for stale launcher references in docs and config schema
- Whether page-insert-before-reserved is enforced in WidgetGridModel (model-level) or HomeMenu (view-level)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| NAV-01 | LauncherWidget provides quick-launch tiles as a grid widget | **Reframed**: Two singleton icon widgets (Settings + AA) on a reserved utility page replace the "tiles" concept. WidgetDescriptor singleton flag + WidgetPickerModel filtering + reserved page seeding |
| NAV-02 | LauncherDock bottom bar removed from Shell | Delete LauncherDock.qml, remove from HomeMenu.qml line 562, remove from CMakeLists.txt lines 248-250 and 384 |
| NAV-03 | LauncherModel data model removed -- LauncherWidget uses PluginModel directly | Delete LauncherModel.hpp/.cpp, remove from CMakeLists.txt line 26, remove from main.cpp lines 52/604/697, remove YamlConfig::launcherTiles()/setLauncherTiles() |
</phase_requirements>

## Architecture Patterns

### Singleton Widget Infrastructure

The `WidgetDescriptor` struct (WidgetTypes.hpp:14-32) needs a `bool singleton = false` field. This flag drives behavior at three points:

1. **WidgetPickerModel** (WidgetPickerModel.cpp:86): `widgetsFittingSpace()` in WidgetRegistry already filters by minCols/minRows and qmlComponent -- add singleton exclusion either in WidgetRegistry::widgetsFittingSpace() or in WidgetPickerModel::filterByAvailableSpace(). Recommendation: filter in WidgetRegistry (single enforcement point) by adding a `bool includeSingletons = false` parameter or by having widgetsFittingSpace skip singletons.

2. **HomeMenu.qml remove badge** (HomeMenu.qml:393): Currently `visible: homeScreen.editMode` -- needs additional `&& !model.isSingleton` check. This requires exposing a `SingletonRole` from WidgetGridModel that looks up the descriptor's singleton flag.

3. **WidgetGridModel::removeWidget()** (WidgetGridModel.cpp:196): Add a guard that checks the descriptor's singleton flag and returns early if true. This is the model-level safety net.

### Reserved Page Implementation

**Recommendation: Derive reserved status from "page contains at least one singleton widget."** This avoids an explicit page-level flag in the YAML config and keeps the invariant self-enforcing. The reserved page is always `pageCount - 1` by convention.

Affected methods:
- `WidgetGridModel::addPage()` (line 279): Currently `pageCount_++`. Change to insert new page at `pageCount_ - 1` (shift all placements on the reserved page up by +1 page), then increment pageCount. This keeps the reserved page always last.
- `WidgetGridModel::removePage()` (line 285): Add a guard: if page is the reserved page (contains singletons), return false. Current guard already prevents removing page 0.
- HomeMenu.qml add-page FAB (line 699-703): After `addPage()`, navigate to the new page (which is now `pageCount - 2`, since reserved page shifted to last).
- HomeMenu.qml delete-page FAB (line 752): Add `&& !isReservedPage(pageView.currentIndex)` to visibility. Can check via a new Q_INVOKABLE `bool isReservedPage(int page)` on WidgetGridModel.

### New Widget QML Files

Both Settings and AA launcher widgets follow an identical pattern -- a centered MaterialIcon that responds to tap. Based on the ClockWidget.qml pattern (qml/widgets/ClockWidget.qml):

```
qml/widgets/SettingsLauncherWidget.qml  -- gear icon, tap -> navigateTo(6)
qml/widgets/AALauncherWidget.qml        -- AA icon, tap -> setActivePlugin("org.openauto.android-auto")
```

Both need:
- `QT_QML_SKIP_CACHEGEN TRUE` in CMakeLists.txt (established GOTCHA from widget system)
- `qrc:/OpenAutoProdigy/` prefix for resource paths
- No label, icon scales with parent size

### Widget Registration

In main.cpp (after line 518), register both new descriptors:

```cpp
oap::WidgetDescriptor settingsLauncher;
settingsLauncher.id = "org.openauto.settings-launcher";
settingsLauncher.displayName = "Settings";
settingsLauncher.iconName = "\ue8b8";  // settings gear
settingsLauncher.category = "launcher";
settingsLauncher.description = "Open settings";
settingsLauncher.singleton = true;
settingsLauncher.minCols = 1; settingsLauncher.minRows = 1;
settingsLauncher.maxCols = 3; settingsLauncher.maxRows = 3;
settingsLauncher.defaultCols = 1; settingsLauncher.defaultRows = 1;
settingsLauncher.qmlComponent = QUrl("qrc:/OpenAutoProdigy/SettingsLauncherWidget.qml");
widgetRegistry->registerWidget(settingsLauncher);

oap::WidgetDescriptor aaLauncher;
aaLauncher.id = "org.openauto.aa-launcher";
aaLauncher.displayName = "Android Auto";
aaLauncher.iconName = "\ueff7";  // directions_car
aaLauncher.category = "launcher";
aaLauncher.description = "Launch Android Auto";
aaLauncher.singleton = true;
aaLauncher.minCols = 1; aaLauncher.minRows = 1;
aaLauncher.maxCols = 3; aaLauncher.maxRows = 3;
aaLauncher.defaultCols = 1; aaLauncher.defaultRows = 1;
aaLauncher.qmlComponent = QUrl("qrc:/OpenAutoProdigy/AALauncherWidget.qml");
widgetRegistry->registerWidget(aaLauncher);
```

### Default Seeding in YamlConfig

YamlConfig.cpp line 190-194 currently seeds empty grid placements. Must add default placements for the two singleton widgets on the reserved page (page 1, assuming page_count remains 2):

- Settings: bottom-right of page 1 (e.g., col=gridCols-1, row=gridRows-1, 1x1)
- AA Launcher: next to settings or center of page 1

**Problem:** Grid dimensions aren't known at YamlConfig construction time (they depend on DisplayInfo which comes later). The current default placements section in YamlConfig uses the legacy pane system (lines 165-188), not the grid system (lines 191-194 are empty).

**Solution:** Seed grid placements in main.cpp after WidgetGridModel initialization, not in YamlConfig defaults. Check if `savedPlacements.isEmpty()` (line 543) and if so, programmatically place the two singleton widgets. Grid dimensions are known at that point.

### Deletion Inventory

Files to delete:
- `src/ui/LauncherModel.hpp`
- `src/ui/LauncherModel.cpp`
- `qml/components/LauncherDock.qml`
- `qml/applications/launcher/LauncherMenu.qml`

Files to modify (removal references):
- `src/CMakeLists.txt`: Remove `ui/LauncherModel.cpp` (line 26), LauncherDock QML properties (lines 248-250), LauncherDock in QML_FILES (line 384), LauncherMenu QML properties (lines 276-278), LauncherMenu in QML_FILES (line 390)
- `src/main.cpp`: Remove `#include "ui/LauncherModel.hpp"` (line 52), `LauncherModel` construction (line 604), context property registration (line 697)
- `qml/applications/home/HomeMenu.qml`: Remove `LauncherDock` block (lines 561-570)
- `src/core/YamlConfig.hpp`: Remove `launcherTiles()` and `setLauncherTiles()` declarations (lines 103-104)
- `src/core/YamlConfig.cpp`: Remove `launcherTiles()` implementation (lines 572-587) and `setLauncherTiles()` (lines 589+)
- `docs/development.md`: Remove LauncherMenu from directory tree (line 258)
- `docs/config-schema.md`: Remove launcher.tiles references (lines 177, 329-330)

### WidgetGridModel Role Addition

Add `SingletonRole` to the Roles enum (WidgetGridModel.hpp:37) and implement in data() to look up `descriptor->singleton`. This allows QML to conditionally hide the remove badge.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Reserved page detection | Explicit "reserved" flag in YAML per page | Derive from singleton widget presence on page | Self-enforcing invariant, no config migration needed |
| Widget placement on first boot | Complex migration logic | Programmatic seeding in main.cpp when placements are empty | Grid dimensions known, simple conditional |
| Singleton filtering in picker | Per-call filtering logic in QML | Filter in WidgetRegistry::widgetsFittingSpace() | Single enforcement point in C++ |

## Common Pitfalls

### Pitfall 1: Add-page shifts existing placements
**What goes wrong:** When inserting a new page before the reserved page, all placements on the reserved page must have their `page` field incremented by 1. Missing this causes reserved-page widgets to appear on the new user page.
**How to avoid:** In `addPage()`, iterate all livePlacements_ and basePlacements_, incrementing `page` for any placement where `page >= insertionIndex`. Then increment pageCount.

### Pitfall 2: Default seeding without grid dimensions
**What goes wrong:** YamlConfig defaults are constructed before DisplayInfo/grid dimensions exist. Placing singleton widgets at hardcoded positions (e.g., col=4, row=3) may exceed actual grid bounds on small screens.
**How to avoid:** Seed singleton defaults in main.cpp after grid dimensions are known, not in YamlConfig constructor. Use `findFirstAvailableCell()` or calculate positions from actual grid dims.

### Pitfall 3: Stale occupancy after page insertion
**What goes wrong:** WidgetGridModel occupancy array tracks only the active page. After shifting page numbers, if activePage is the new page, occupancy is empty -- fine. But if activePage is the reserved page, stale occupancy from the old page index remains.
**How to avoid:** Call `rebuildOccupancy()` after any page-number shifting operation.

### Pitfall 4: QML remove badge needs model role, not JS lookup
**What goes wrong:** Trying to check singleton status in QML by calling back into C++ via a Q_INVOKABLE for each widget. This is fragile and doesn't update reactively.
**How to avoid:** Expose `SingletonRole` as a model role on WidgetGridModel. QML binds naturally: `visible: homeScreen.editMode && !model.isSingleton`.

### Pitfall 5: LauncherModel removal breaks existing configs
**What goes wrong:** Existing `launcher.tiles` entries in user config YAML are now orphaned. YamlConfig still parses them silently (YAML is schema-less), but they serve no purpose.
**How to avoid:** This is benign -- stale YAML keys don't cause errors. No migration needed. Just remove the accessor methods.

### Pitfall 6: Widget QML needs QT_QML_SKIP_CACHEGEN
**What goes wrong:** New widget QML files loaded via Loader URL lookup fail silently when Qt 6.8 cachegen compiles them to C++.
**How to avoid:** Add `QT_QML_SKIP_CACHEGEN TRUE` to set_source_files_properties for both new QML files in CMakeLists.txt.

## Code Examples

### Singleton Widget QML (Settings)
```qml
// qml/widgets/SettingsLauncherWidget.qml
import QtQuick

Item {
    id: root

    MaterialIcon {
        anchors.centerIn: parent
        icon: "\ue8b8"  // settings
        size: Math.min(root.width, root.height) * 0.6
        opticalSize: Math.min(size, 48)
        color: ThemeService.onSurface
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            PluginModel.setActivePlugin("")
            ApplicationController.navigateTo(6)
        }
    }
}
```

### Singleton Widget QML (AA Launcher)
```qml
// qml/widgets/AALauncherWidget.qml
import QtQuick

Item {
    id: root

    MaterialIcon {
        anchors.centerIn: parent
        icon: "\ueff7"  // directions_car
        size: Math.min(root.width, root.height) * 0.6
        opticalSize: Math.min(size, 48)
        color: ThemeService.onSurface
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            PluginModel.setActivePlugin("org.openauto.android-auto")
        }
    }
}
```

### addPage() with reserved page shift
```cpp
void WidgetGridModel::addPage()
{
    // Insert new page before the reserved page (always last)
    int insertAt = pageCount_ - 1;  // reserved page index

    // Shift all placements on reserved page (and beyond) up by 1
    for (auto& p : livePlacements_) {
        if (p.page >= insertAt)
            p.page++;
    }
    for (auto& p : basePlacements_) {
        if (p.page >= insertAt)
            p.page++;
    }

    pageCount_++;
    rebuildOccupancy();
    emit pageCountChanged();
    emit placementsChanged();
}
```

### Singleton check in removeWidget()
```cpp
void WidgetGridModel::removeWidget(const QString& instanceId)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return;

    // Prevent removal of singleton widgets
    if (registry_) {
        auto desc = registry_->descriptor(livePlacements_[idx].widgetId);
        if (desc && desc->singleton) return;
    }

    // ... existing removal logic
}
```

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | QtTest (Qt 6.8.2) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build && ctest -R "widget_grid\|widget_picker" --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| NAV-01 | Singleton widgets seeded on reserved page | unit | `ctest -R test_widget_grid_model --output-on-failure` | Extend existing |
| NAV-01 | Singleton widgets hidden from picker | unit | `ctest -R test_widget_picker_model --output-on-failure` | Extend existing |
| NAV-01 | Singleton widgets cannot be removed | unit | `ctest -R test_widget_grid_model --output-on-failure` | Extend existing |
| NAV-02 | LauncherDock removed, no references remain | build | `cmake --build build -j$(nproc)` | N/A (compile gate) |
| NAV-03 | LauncherModel removed, no references remain | build | `cmake --build build -j$(nproc)` | N/A (compile gate) |
| NAV-01 | Reserved page cannot be deleted | unit | `ctest -R test_widget_grid_model --output-on-failure` | Extend existing |
| NAV-01 | addPage inserts before reserved page | unit | `ctest -R test_widget_grid_model --output-on-failure` | Extend existing |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . -j$(nproc) && ctest -R "widget_grid\|widget_picker" --output-on-failure`
- **Per wave merge:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Phase gate:** Full suite green (82+ tests) before verify

### Wave 0 Gaps
None -- existing test infrastructure (test_widget_grid_model.cpp, test_widget_picker_model.cpp) covers all phase requirements via extension. No new test files or framework setup needed.

## Open Questions

1. **Grid dimensions at seeding time**
   - What we know: Grid dimensions are computed from DisplayInfo after WidgetGridModel construction. Default placements need valid positions.
   - What's unclear: The exact col/row values for the reserved page depend on screen size. On the 1024x600 Pi display, the grid is approximately 6x3 or 7x4 depending on density bias.
   - Recommendation: Seed singleton widgets at (0,0) and (1,0) on the reserved page. These are always valid regardless of grid dimensions. Let the user move them to preferred positions.

2. **Legacy pane-based config (widget_config)**
   - What we know: YamlConfig still has `widget_config` with pane-based defaults (lines 155-188) alongside `widget_grid` (lines 191-194). The pane system is marked "Legacy -- do NOT use in new code."
   - What's unclear: Whether the legacy widget_config section should also be cleaned up in this phase.
   - Recommendation: Leave it alone -- it's separately scoped tech debt, not part of Phase 10.

## Sources

### Primary (HIGH confidence)
- Direct codebase analysis of all affected files (WidgetTypes.hpp, WidgetGridModel.cpp/hpp, WidgetPickerModel.cpp/hpp, WidgetRegistry.cpp/hpp, LauncherModel.cpp/hpp, LauncherDock.qml, LauncherMenu.qml, HomeMenu.qml, main.cpp, YamlConfig.cpp, CMakeLists.txt)
- Existing test files (test_widget_grid_model.cpp, test_widget_picker_model.cpp) for test patterns

### Secondary (MEDIUM confidence)
- CONTEXT.md decisions and code_context sections for line-number references (verified against current main branch)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all code is in-tree Qt 6 / C++, no new dependencies
- Architecture: HIGH -- singleton flag, reserved page, and widget QML all follow established patterns
- Pitfalls: HIGH -- identified from direct code analysis of addPage/removePage/seeding logic

**Research date:** 2026-03-14
**Valid until:** No expiration -- all findings are project-internal, not library-dependent
