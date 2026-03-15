# Domain Pitfalls

**Domain:** Widget framework refinement, launcher dock removal, DPI grid layout in Qt 6.8 QML car head unit
**Researched:** 2026-03-14
**Confidence:** HIGH (codebase analysis of current widget/grid/launcher implementation + known Qt/QML behaviors + project-specific gotchas)

---

## Critical Pitfalls

Mistakes that cause broken navigation, invisible widgets, or forced rewrites.

### Pitfall 1: Removing LauncherDock before replacement navigation paths exist

**What goes wrong:** `LauncherDock` is currently the only way to reach AA, BT Audio, Phone, and Settings from the home screen. The navbar provides volume/clock-home/brightness only -- no plugin activation. Removing the dock without equivalent entry points strands users with no way to open Settings or start AA.

**Why it happens:** The dock looks like redundant UI clutter once widgets exist, so it gets removed early. But the widgets that could replace its navigation functions (a "launch AA" tap on AAStatusWidget, a settings shortcut widget, launcher tile widgets) don't fully cover all destinations.

**Consequences:** Users stuck on home screen. Settings unreachable without SSH. Literally bricked UI for non-technical users on a car-mounted Pi.

**Prevention:**
- Audit every `LauncherModel` tile action (`plugin:org.openauto.android-auto`, `plugin:org.openauto.bt-audio`, `plugin:org.openauto.phone`, `navigate:settings`) and verify each has a non-dock entry point before removal.
- Current coverage: AAStatusWidget tap covers AA. Nothing covers Settings, BT Audio, or Phone.
- Ship a "launcher tile" widget type that can be configured as a shortcut to any plugin/view. Place default launcher tiles on the grid to replace dock functions.
- Alternative: add a navbar gesture (e.g., long-press clock) that opens Settings. But this is undiscoverable.
- QA gate: "Can I reach every view in the app without SSH?" must pass before dock removal merges.

**Detection:** Manual test on Pi: navigate to every plugin and Settings starting from a fresh boot. If any destination requires SSH or YAML editing, the dock was removed too early.

**Phase:** Launcher tile widget must ship BEFORE dock removal. Do not combine in the same commit.

---

### Pitfall 2: Grid dimension constant changes silently break saved widget layouts

**What goes wrong:** `DisplayInfo::updateGridDimensions()` computes grid cols/rows from hardcoded pixel constants (`targetCellW = 150`, `targetCellH = 125`). Refining these constants changes the grid dimensions for the same display resolution. Saved `GridPlacement` entries with `col + colSpan > newCols` or `row + rowSpan > newRows` become invalid. Widgets either disappear (the `visible = false` clamping path) or overlap.

**Why it happens:** YAML schema v3 stores absolute grid coordinates. There is no migration path when the grid itself changes dimensions. The code handles the "widget too big for new grid" case via the `visible` flag, but the user experience is "my widgets vanished after an update."

**Consequences:** Users update the app, their home screen layout is gone. On a car head unit, this triggers a dangerous "what happened to my stuff" reaction at the wheel.

**Prevention:**
- Add a `grid_version` or `grid_dimensions` field to the YAML layout section. On load, compare stored dimensions to current dimensions. If they differ, run a migration pass.
- Migration pass: for each placement, clamp `col` and `row` to fit. If `colSpan > newCols`, reduce span. If widget still can't fit (min constraints violated), mark `visible = false` and emit a notification.
- Test matrix: save layout on 1024x600 (6x3), load on 800x480 (3x2). Verify widgets are relocated, not lost.
- Consider storing layouts as proportional positions (fraction of grid) rather than absolute cell indices. This makes resolution changes a smooth remapping rather than a hard break. Tradeoff: snapping to cells after remap needs extra logic.

**Detection:** Widget count on home screen decreases after app update with no user action. `grep "visible: false"` in saved YAML shows widgets the user didn't hide.

**Phase:** Address BEFORE any changes to `targetCellW`/`targetCellH` constants.

---

### Pitfall 3: Widget size constraint enforcement diverges between C++ model and QML drag logic

**What goes wrong:** `WidgetDescriptor` has `minCols/maxCols/minRows/maxRows` enforced by `WidgetGridModel::resizeWidget()` in C++. The QML drag-to-resize overlay in `HomeMenu.qml` does its own clamping math for the ghost rectangle preview. These are two independent implementations of the same constraint logic. When they diverge, the ghost rectangle shows a valid resize but the model rejects it (or vice versa).

**Why it happens:** The QML drag code reads constraints from model roles (`MinColsRole`, etc.) but does its own `Math.max/Math.min` clamping. The C++ model also clamps. During refactoring, only one side gets updated.

**Consequences:** Resize preview ghost shows 1x1 but widget snaps to 2x1 after release. Or ghost shows 4x4 but nothing happens because model rejects it. User perceives drag-resize as buggy and unreliable.

**Prevention:**
- Add a `Q_INVOKABLE QVariantMap clampedSize(const QString& instanceId, int proposedCols, int proposedRows)` to `WidgetGridModel`. QML calls this during drag to get the actual allowed size, rather than duplicating the constraint logic.
- QML ghost rectangle uses the clamped result directly. No independent min/max logic in QML.
- Unit test: attempt `resizeWidget()` below min and above max for each widget type. Verify both rejection and the clamped values.

**Detection:** Ghost rectangle and final widget size disagree. Resize appears to "snap" unexpectedly after release.

**Phase:** Address during widget framework convention formalization (size constraints).

---

### Pitfall 4: Sub-pixel cell dimensions cause visible rendering artifacts

**What goes wrong:** `cellWidth = pageView.width / WidgetGridModel.gridColumns` produces fractional pixel values. Example: 984px usable width / 6 cols = 164.0px (clean), but with navbar left edge eating 50px: 934 / 6 = 155.67px. On the Pi's non-HiDPI 1024x600 display, fractional positioning produces visible 1px gaps between adjacent widget glass cards, blurry card borders, and inconsistent spacing.

**Why it happens:** QML positions items at floating-point coordinates. On HiDPI displays this is fine (subpixel rendering is invisible). On 1024x600 at native DPI, every fractional pixel is visible.

**Consequences:** Hairline gaps or double-thick borders between adjacent widgets. Users perceive it as unpolished. The effect varies with navbar position (each edge position changes usable area and produces different remainders).

**Prevention:**
- Use `Math.floor()` for cell dimensions. Distribute remainder pixels as extra padding on the grid container edges (visually invisible).
- Alternative: round cell dimensions and add a single-pixel padding to the outer grid margins.
- Test with navbar in all 4 edge positions at 1024x600. Each position produces a different usable area and different remainder.
- Consider adding a `cellPadding` constant (2-4px) that absorbs rounding error while providing intentional spacing between widgets.

**Detection:** Zoom in on widget boundaries on Pi. Look for inconsistent gap widths between adjacent widgets. Compare left-edge vs right-edge widgets.

**Phase:** Address during DPI-based grid layout refinement.

---

### Pitfall 5: Interactive widget MouseAreas prevent edit mode entry

**What goes wrong:** Widgets with interactive controls (NowPlayingWidget play/pause/skip, AAStatusWidget tap-to-connect, NavigationWidget tap-to-open-AA) have MouseAreas that consume press events. The WidgetHost's long-press detector sits at `z: -1`, behind widget content. For interactive widgets, the only way to enter edit mode is for each widget to independently implement `onPressAndHold → parent.requestContextMenu()`. Third-party plugin widgets will miss this pattern.

**Why it happens:** The current design relies on convention: each interactive widget must manually forward long-press to the WidgetHost. There is no framework enforcement. Existing widgets do it correctly (hard-won knowledge from v0.5.2), but the pattern is invisible to new widget developers.

**Consequences:** User long-presses on a third-party widget: nothing happens. Edit mode appears broken. Only workaround is finding a different widget or empty space to long-press.

**Prevention:**
- Move long-press detection to the framework level. Use `childMouseEventFilter` in WidgetHost (the C++ approach already proven with TapHandler overlays) to intercept long-press regardless of widget internals.
- If keeping the QML-only approach: make `requestContextMenu()` part of a required widget base component (e.g., `WidgetBase.qml` that all widgets must extend) rather than a convention.
- Document prominently in widget developer docs: "ALL widgets with MouseAreas MUST forward `pressAndHold` via `parent.requestContextMenu()`."
- Provide a reference widget template that demonstrates the pattern.

**Detection:** Long-press on widget does nothing. Only discoverable through manual testing of each widget type.

**Phase:** Address during plugin-widget contract documentation. Framework-level fix preferred over documentation-only.

---

## Moderate Pitfalls

### Pitfall 6: Hardcoded pixel breakpoints in widget QML don't scale with DPI

**What goes wrong:** Existing widgets use raw pixel breakpoints: `width >= 250` (ClockWidget, AAStatusWidget), `width >= 400` (NowPlayingWidget, NavigationWidget), `height >= 180`. These were tuned for 1024x600 at 7". On a higher-DPI display at the same physical size (more pixels, same inches), breakpoints fire at physically smaller widget sizes, showing the "full" layout crammed into space that is visually identical to the "compact" layout on the original display.

**Why it happens:** The project invested heavily in `UiMetrics` for font/spacing/icon scaling, but widget-internal layout breakpoints bypass it. Each widget author picks a pixel value that looks right on their test display.

**Prevention:**
- Define a convention: widget breakpoints use grid span thresholds (colSpan >= 2, rowSpan >= 2) rather than pixel widths. The widget can read its span from `WidgetGridModel` role data.
- Alternative: express breakpoints as `width >= UiMetrics.tileW * 1.5` so they scale with the DPI-derived tile dimensions.
- Document the convention with before/after examples showing why raw pixels break.
- Refactor existing 4 widgets to use span-based breakpoints as the reference implementation.

**Detection:** Widget layout looks wrong on a display with different DPI than 1024x600 at 7". Specifically: "full" layout appears at physically small sizes.

**Phase:** Address during DPI-based grid layout refinement. Refactor existing widgets as proof of the convention.

### Pitfall 7: Widget lifecycle -- no deactivation signal when page scrolls off-screen

**What goes wrong:** Widgets with running timers (ClockWidget 1s Timer) or active D-Bus signal connections continue running when their SwipeView page is not visible. Multiple copies of the same widget across pages all run simultaneously.

**Why it happens:** `WidgetInstanceContext` has no `active` property tied to page visibility. SwipeView may keep off-screen pages instantiated (depending on `cacheItemCount`), so widget Items exist but aren't visible.

**Prevention:**
- Add an `active` property to `WidgetInstanceContext` that tracks `activePage === placement.page`.
- Document the convention: widget timers should bind `running: widgetContext.active`.
- For a clock timer this is negligible. For widgets doing network polling or D-Bus subscriptions, it prevents unnecessary resource use.

**Detection:** CPU usage scales linearly with page count. D-Bus traffic from off-screen widgets.

### Pitfall 8: LauncherModel config remains as dead YAML after dock removal

**What goes wrong:** `LauncherModel` reads `launcher.tiles` from YAML config. Removing the `LauncherDock` component doesn't remove the config section. Users editing YAML manually see a `launcher` block that does nothing, or worse, assume it still controls something.

**Prevention:**
- Add a config migration step: if launcher tiles exist AND new launcher-tile-widgets are available, auto-migrate dock entries into widget placements on the grid.
- Mark the `launcher` config section as deprecated in comments, or remove it with a logged migration message.
- Update `docs/development.md` and any config reference to reflect the change.

**Detection:** Users report config settings that "don't do anything."

### Pitfall 9: WidgetPicker doesn't explain why some widgets are missing

**What goes wrong:** `WidgetRegistry::widgetsFittingSpace()` filters out widgets whose `minCols > availCols` or `minRows > availRows`. On 800x480 (3x2 grid), widgets with `minCols = 4` silently disappear from the picker. User doesn't know they exist.

**Prevention:**
- Show filtered-out widgets in the picker as disabled/greyed with a size requirement note ("Needs 4x2 or larger").
- Alternative: show all widgets but disable the "Add" action with a tooltip explaining the constraint.

**Detection:** User on small display asks "where is the navigation widget?" -- it was filtered out.

### Pitfall 10: QT_QML_SKIP_CACHEGEN omission for new widget QML files

**What goes wrong:** New widget QML files added without `QT_QML_SKIP_CACHEGEN TRUE` in their `qt_add_qml_module` get compiled by Qt 6.8 cachegen. WidgetHost's `Loader { source: url }` can't find the compiled version by URL. Widget appears as blank glass card.

**Why it happens:** This is a known gotcha (documented in CLAUDE.md) but easily forgotten when adding new files to CMakeLists.txt.

**Prevention:**
- Consider a dedicated CMake function (`oap_add_widget_qml()`) that wraps `qt_add_qml_module` with SKIP_CACHEGEN always set.
- Add to the widget developer docs as step 1 of the build setup.
- CI check (when CI exists): verify all files under `qml/widgets/` have SKIP_CACHEGEN.

**Detection:** Blank glass card where widget should be. `Loader.status === Loader.Error` in console.

### Pitfall 11: Drag coordinate mapping breaks when navbar changes edge position

**What goes wrong:** HomeMenu.qml computes grid cell from mouse position relative to the drag overlay. The navbar's edge position changes Shell's layout margins, which changes the SwipeView's position and size. If the drag math uses raw `mouseX/mouseY` without mapping through `mapToItem(pageView, ...)`, widgets land one cell off when navbar is on left or top edge.

**Why it happens:** Drag coordinates are relative to the overlay's parent, but the grid origin depends on navbar-induced offsets. The existing code likely handles this correctly (v0.4.5 solved analogous issues for evdev touch routing), but refactoring the grid layout can reintroduce it.

**Prevention:**
- Always use `mapToItem()` for coordinate transforms, never raw position offsets.
- Test drag-drop with navbar in all 4 positions after any layout changes.

**Detection:** Widget lands one cell off from where it was dropped, only when navbar is on left or top.

---

## Minor Pitfalls

### Pitfall 12: Widget unregistration path untested

**What goes wrong:** `WidgetRegistry::unregisterWidget()` exists but is never called. If dynamic plugin hot-reload is ever added, unloading a plugin leaves its widget descriptors in the registry. Placing them creates empty WidgetHosts.

**Prevention:** Wire `unregisterWidget()` into plugin shutdown. Add a guard in `WidgetGridModel::placeWidget()` to verify the widgetId exists in the registry.

### Pitfall 13: typeof guards for context properties are stringly-typed and fragile

**What goes wrong:** Widgets use `typeof MediaStatus !== "undefined"` to guard against missing context properties. If a context property is renamed in C++, the guard silently evaluates to true (preventing the fallback from hiding the error) or false (silently showing fallback state when data actually exists).

**Prevention:** Document every stable context property name in the widget API contract. Consider a `WidgetDataBridge` singleton that wraps all widget-relevant data behind stable names.

### Pitfall 14: Multiple WidgetHost z:-1 MouseAreas at boundaries

**What goes wrong:** Adjacent widgets both have z:-1 long-press MouseAreas. Touch events near the boundary may route to the wrong widget's area depending on QML hit-testing order.

**Prevention:** Framework-level `childMouseEventFilter` (Pitfall 5) eliminates this class of issue.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Launcher dock removal | Navigation paths lost (Pitfall 1) | Ship launcher tile widget first, QA "reach every view" |
| DPI grid constant refinement | Saved layouts break (Pitfall 2) | Grid version in YAML, migration pass, 3-resolution test |
| DPI grid constant refinement | Sub-pixel artifacts (Pitfall 4) | Floor cell dimensions, test all navbar positions |
| Widget size constraint formalization | C++/QML enforcement divergence (Pitfall 3) | Single source of truth via Q_INVOKABLE clampedSize() |
| Widget breakpoint conventions | Pixel breakpoints don't scale (Pitfall 6) | Define span-based or UiMetrics-based breakpoints |
| Widget lifecycle documentation | Long-press forwarding missed (Pitfall 5) | Framework-level childMouseEventFilter |
| Widget lifecycle hooks | Off-screen widgets active (Pitfall 7) | active property bound to page visibility |
| Plugin-widget contract docs | QML resource path gotcha (Pitfall 10) | CMake helper function + prominent docs |
| Drag/resize interaction | Navbar edge mapping (Pitfall 11) | mapToItem() always, test all 4 positions |

---

## Sources

- **Codebase analysis** (HIGH confidence):
  - `src/ui/DisplayInfo.cpp` -- grid dimension computation from pixel constants (targetCellW=150, targetCellH=125, clamp 3-8 cols / 2-6 rows)
  - `src/ui/WidgetGridModel.hpp` -- placement model with MinCols/MaxCols/MinRows/MaxRows roles
  - `src/core/widget/WidgetTypes.hpp` -- WidgetDescriptor size constraints, GridPlacement with page/visible fields
  - `src/core/widget/WidgetRegistry.hpp` -- widgetsFittingSpace() filter, unregisterWidget() present but unused
  - `qml/components/WidgetHost.qml` -- z:-1 MouseArea pattern, requestContextMenu() convention
  - `qml/components/LauncherDock.qml` -- LauncherModel-backed tile buttons (AA, BT, Phone, Settings)
  - `qml/widgets/ClockWidget.qml` -- pixel breakpoints (width>=250, height>=180), 1s Timer
  - `qml/widgets/NowPlayingWidget.qml` -- pixel breakpoints (width>=400, height>=180), typeof guards
  - `qml/widgets/AAStatusWidget.qml` -- typeof guards, pressAndHold forwarding
  - `qml/widgets/NavigationWidget.qml` -- pixel breakpoints, typeof guards, pressAndHold forwarding
  - `qml/applications/home/HomeMenu.qml` -- drag/resize overlay, cell dimension computation, edit mode
  - `qml/components/Shell.qml` -- navbar margin offsets, LauncherDock + HomeMenu layout
- **Project memory** (HIGH confidence):
  - CLAUDE.md: QT_QML_SKIP_CACHEGEN gotcha for Loader-loaded QML
  - CLAUDE.md: TapHandler/childMouseEventFilter approach for touch event interception
  - CLAUDE.md: Widget QML resource path prefix requirement (qrc:/OpenAutoProdigy/)
  - SESSION MEMORY: Widget system v0.5.2 WidgetHost long-press z:-1 fix
  - SESSION MEMORY: v0.5.3 grid density auto-derived from UiMetrics

---
*Pitfalls research for: OpenAuto Prodigy v0.6.1 Widget Framework & Layout Refinement*
*Researched: 2026-03-14*
