# Phase 05: Static Grid Rendering & Widget Revision - Research

**Researched:** 2026-03-12
**Domain:** QML grid rendering, responsive widget layout, Qt 6.4/6.8 dual-compat
**Confidence:** HIGH

## Summary

This phase bridges the Phase 04 data model (WidgetGridModel with occupancy tracking) to visual rendering on the home screen. The core task is rewriting HomeMenu.qml to use a Repeater over WidgetGridModel with manual absolute positioning (Item with x/y/width/height computed from grid cell dimensions), and revising the three existing widgets to use pixel-based breakpoints instead of the `isMainPane` boolean.

The architecture is straightforward: WidgetGridModel already exposes all needed roles (column, row, colSpan, rowSpan, qmlComponent, opacity, visible). QML computes cell pixel dimensions from the available container size divided by grid columns/rows, then positions each widget using those values. No external libraries are needed -- this is pure Qt Quick with existing project patterns.

**Primary recommendation:** Use a flat Item container with a Repeater bound to WidgetGridModel. Each delegate computes its own x/y/width/height from model roles and cell dimensions. Widgets switch layout based on their delegate's pixel width/height, not model roles.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| GRID-03 | Widgets render at integer column/row coordinates with column/row span values | Repeater + manual positioning pattern; cell dimension computation from container size |
| GRID-06 | Fresh install ships a sensible default widget layout | Default placements already in main.cpp (Clock 2x2, NowPlaying 3x2, AAStatus 2x1); QML just renders them |
| REV-01 | All shipped v0.5.3 widgets use pixel-based breakpoints for responsive layout | Width/height properties from delegate, breakpoint thresholds per widget |
| REV-02 | Clock widget adapts content across grid sizes (1x1: time only, 2x1: time+date, 2x2+: full) | Pixel breakpoints on allocated width/height |
| REV-03 | AA Status widget adapts content across grid sizes (1x1: icon only, 2x1+: icon+text) | Pixel breakpoints on allocated width/height |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt Quick (QtQuick) | 6.4+ | QML rendering, Item positioning, Repeater | Already the project's UI framework |
| WidgetGridModel | in-tree | QAbstractListModel with grid roles | Built in Phase 04, has all needed roles |
| UiMetrics | in-tree (singleton) | DPI-aware scaling tokens | Project standard -- DO NOT hardcode pixel sizes |
| ThemeService | in-tree | Color tokens | Project standard for all colors |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| DisplayInfo | in-tree | gridColumns/gridRows Q_PROPERTYs | Derive cell pixel dimensions in QML |
| WidgetHost | in-tree (QML) | Glass card background + border | Adapted to wrap each grid cell's widget |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Repeater + Item (manual) | GridLayout | GridLayout fights you on multi-span cells and doesn't support arbitrary col/row placement. Repeater + manual positioning is explicit and predictable. |
| Repeater + Item (manual) | GridView | GridView is for uniform-cell scrollable lists, not fixed multi-span grid placement. Wrong abstraction entirely. |
| Repeater + Item (manual) | TableView | TableView is Qt 6.4+ but designed for data tables, not spatial widget grids. Overkill and wrong semantics. |

## Architecture Patterns

### Recommended Project Structure
```
qml/
  applications/home/
    HomeMenu.qml         # REWRITE: Repeater + manual positioning over WidgetGridModel
    GridCell.qml          # NEW: Per-cell delegate (positioning + WidgetHost + Loader)
  components/
    WidgetHost.qml        # ADAPT: Remove paneId/WidgetPlacementModel refs, accept opacity prop
  widgets/
    ClockWidget.qml       # REVISE: pixel breakpoints replace isMainPane
    AAStatusWidget.qml    # REVISE: pixel breakpoints replace isMainPane
    NowPlayingWidget.qml  # REVISE: pixel breakpoints replace isMainPane
```

### Pattern 1: Repeater + Manual Positioning Grid
**What:** A flat Item container with a Repeater bound to WidgetGridModel. Each delegate is an Item positioned absolutely using computed pixel coordinates.
**When to use:** Always for this grid -- it's the only pattern that cleanly handles multi-span cells at arbitrary positions.
**Example:**
```qml
// HomeMenu.qml — grid container
Item {
    id: gridContainer
    // Fill available space (parent minus margins and dock)

    // Cell dimensions derived from container and grid model
    readonly property real cellWidth: width / WidgetGridModel.gridColumns
    readonly property real cellHeight: height / WidgetGridModel.gridRows

    Repeater {
        model: WidgetGridModel

        // Each delegate is a GridCell
        delegate: GridCell {
            // Position from model roles * cell dimensions
            x: model.column * gridContainer.cellWidth
            y: model.row * gridContainer.cellHeight
            width: model.colSpan * gridContainer.cellWidth
            height: model.rowSpan * gridContainer.cellHeight
            visible: model.visible

            cellOpacity: model.opacity
            widgetSource: model.qmlComponent
            instanceId: model.instanceId
        }
    }
}
```

### Pattern 2: GridCell Delegate with WidgetHost
**What:** Each grid cell wraps WidgetHost (glass background) + Loader (widget QML). The cell provides pixel dimensions that the loaded widget reads for breakpoint logic.
**When to use:** Every widget placement in the grid.
**Example:**
```qml
// GridCell.qml
Item {
    id: cell
    property real cellOpacity: 0.25
    property url widgetSource: ""
    property string instanceId: ""

    // Inner area with gap/padding
    Item {
        id: innerCell
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing / 2  // half-gap for grid gutters

        // Glass card background
        Rectangle {
            anchors.fill: parent
            radius: UiMetrics.radius
            color: ThemeService.surfaceContainer
            opacity: cell.cellOpacity
        }

        // Subtle border
        Rectangle {
            anchors.fill: parent
            radius: UiMetrics.radius
            color: "transparent"
            border.width: 1
            border.color: ThemeService.outlineVariant
            opacity: 0.3
        }

        Loader {
            id: widgetLoader
            anchors.fill: parent
            source: cell.widgetSource
            asynchronous: false
        }
    }
}
```

### Pattern 3: Pixel-Based Widget Breakpoints
**What:** Widgets read their own `width` and `height` to decide layout tier. No model roles, no context properties -- just the widget's allocated pixel dimensions.
**When to use:** Every widget that adapts layout to its container size.
**Example:**
```qml
// ClockWidget.qml — breakpoint logic
Item {
    id: clockWidget

    // Breakpoint tiers based on allocated pixel size
    readonly property bool isCompact: width < 200 || height < 120
    readonly property bool isWide: width >= 200 && height < 200
    // Default: full layout (neither compact nor wide-only)

    // isCompact → time only (1x1 cell at ~150x125)
    // isWide → time + date (2x1 cell at ~300x125)
    // full → time + date + day (2x2+ at ~300x250+)
}
```

### Pattern 4: Container-to-Widget Dimension Flow
**What:** The widget's `anchors.fill: parent` inside the Loader means it inherits pixel dimensions from the grid cell. No explicit property passing needed -- QML's `width`/`height` properties propagate naturally through the parent chain.
**When to use:** Always. This is how Qt Quick works.

### Anti-Patterns to Avoid
- **Using GridLayout/GridView:** These fight multi-span placement. Manual positioning is 10 lines of code and does exactly what we need.
- **Passing isMainPane/paneSize:** The old pattern couples widgets to the 3-pane model. Pixel breakpoints are resolution-independent and grid-size-agnostic.
- **Computing cell size in C++:** Keep it in QML. The container's actual pixel size after layout is the ground truth. C++ cannot know the final rendered container dimensions after margins/dock.
- **Using WidgetInstanceContext.cellWidth/cellHeight:** These are C++ constants set at model construction time. The QML widget's actual width/height after layout is more accurate and reactive to window resizes.
- **Hardcoding breakpoint pixels:** Use UiMetrics-relative thresholds or compute from cell target sizes (150px cellW, 125px cellH at reference DPI).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Grid cell sizing | Custom C++ layout engine | QML property bindings (width / gridColumns) | QML bindings auto-update on resize, zero code |
| Widget responsive layout | Custom breakpoint framework | QML width/height property checks | Native QML, no overhead, already used in NowPlayingWidget's sub-pane pattern |
| Glass card background | New background component | Existing WidgetHost pattern (Rectangle + opacity) | Already exists, tested, themed |
| Grid gutter spacing | Margin arithmetic in each widget | `anchors.margins` on inner container in GridCell | One place, consistent spacing |
| Widget loading | Custom component factory | QML Loader with source URL | Existing pattern from WidgetHost, handles async errors |

**Key insight:** This phase is almost entirely QML work. The C++ model (WidgetGridModel) is already complete. The only C++ change is removing the old WidgetPlacementModel QML context property (already flagged as removed in Phase 04-02).

## Common Pitfalls

### Pitfall 1: WidgetPlacementModel References in QML
**What goes wrong:** HomeMenu.qml, WidgetHost.qml, and WidgetContextMenu.qml all reference `WidgetPlacementModel` which was removed as a QML context property in Phase 04-02.
**Why it happens:** The C++ context property was removed but QML files weren't updated yet (explicitly deferred to Phase 05).
**How to avoid:** Rewrite HomeMenu.qml from scratch. Adapt WidgetHost.qml to remove all paneId/WidgetPlacementModel references. WidgetContextMenu.qml and WidgetPicker.qml are edit-mode features (Phase 07) -- leave them broken for now or remove their Loaders from HomeMenu.
**Warning signs:** "ReferenceError: WidgetPlacementModel is not defined" in QML console at startup.

### Pitfall 2: QT_QML_SKIP_CACHEGEN on New QML Files
**What goes wrong:** Widget QML loaded via `Loader { source: url }` fails silently (blank widget area) on Qt 6.8 Pi.
**Why it happens:** Qt 6.8 cachegen replaces raw .qml with compiled C++ bytecode. Loader URL lookup can't find the .qml file anymore.
**How to avoid:** Every widget QML file (and any new file loaded via Loader) MUST have `QT_QML_SKIP_CACHEGEN TRUE` in CMakeLists.txt. The existing widgets already have this. Any new GridCell.qml needs it too.
**Warning signs:** Widget works on Qt 6.4 (dev VM) but shows blank on Pi.

### Pitfall 3: Grid Cell Rounding Errors
**What goes wrong:** Fractional pixels from `containerWidth / gridColumns` cause 1px gaps or overlaps between adjacent cells.
**Why it happens:** 984px (1024 - 40 margin) / 6 columns = 164.0 -- happens to be exact, but other resolutions won't be.
**How to avoid:** Use `Math.floor()` for cell dimensions and absorb the remainder in the last column/row, OR use `Math.round()` and accept sub-pixel alignment (Qt Quick handles this gracefully with anti-aliasing).
**Warning signs:** Visible hairline gaps between adjacent widget cards.

### Pitfall 4: Widget Context Menu / Picker Broken References
**What goes wrong:** HomeMenu.qml currently has Loaders for WidgetContextMenu and WidgetPicker that reference `WidgetPlacementModel.clearPane()`, `WidgetPlacementModel.swapWidget()`, etc.
**Why it happens:** These are edit-mode features that depend on the old pane model.
**How to avoid:** Remove the context menu and picker Loaders from the new HomeMenu.qml entirely. Edit mode (Phase 07) will reintroduce these with grid-aware versions.
**Warning signs:** QML errors about undefined methods on WidgetPlacementModel.

### Pitfall 5: Default Layout Widget ID Mismatch
**What goes wrong:** Fresh install places `org.openauto.bt-now-playing` at (2,0,3,2) but this widget is registered by BtAudioPlugin, which uses the old `NowPlayingWidget.qml`. Phase 06 will replace it with a unified now-playing widget.
**Why it happens:** The bt-now-playing descriptor was registered by BtAudioPlugin (still exists).
**How to avoid:** The default layout already works -- bt-now-playing is registered with its QML component. Just ensure NowPlayingWidget.qml also gets pixel breakpoints (REV-01 applies to all shipped widgets).
**Warning signs:** None -- just be aware of the widget ID when testing.

### Pitfall 6: anchors.fill vs Manual Sizing Conflict
**What goes wrong:** Using `anchors.fill: parent` on the Repeater delegate AND setting x/y/width/height causes binding loops or ignored properties.
**Why it happens:** `anchors.fill` and explicit x/y/width/height are mutually exclusive in QML.
**How to avoid:** Repeater delegates MUST use explicit x/y/width/height positioning. Do NOT use anchors on the delegate Item itself. Internal children of the delegate can use anchors normally.
**Warning signs:** "Cannot specify left, right, and horizontalCenter" binding warnings.

### Pitfall 7: Widgets Still Reference widgetContext.paneSize
**What goes wrong:** ClockWidget, AAStatusWidget, and NowPlayingWidget all check `widgetContext.paneSize === 1` for layout decisions. widgetContext is injected by WidgetInstanceContext which no longer provides paneSize.
**Why it happens:** Legacy from the 3-pane system.
**How to avoid:** Remove all `isMainPane` / `widgetContext.paneSize` logic from widgets. Replace with pixel-dimension breakpoints using the widget's own `width`/`height`.
**Warning signs:** "TypeError: Cannot read property 'paneSize' of undefined" in QML console.

### Pitfall 8: Dock Height Must Be Accounted For
**What goes wrong:** Grid container overlaps LauncherDock or grid cells are too small.
**Why it happens:** The old HomeMenu uses ColumnLayout to stack panes + dock. The new version must similarly reserve space for the dock.
**How to avoid:** Use a ColumnLayout with the grid area as `Layout.fillHeight: true` and LauncherDock at its natural height below. Or compute grid container height as `parent.height - dockHeight - margins`.
**Warning signs:** Widgets visually overlap the dock, or dock is pushed offscreen.

## Code Examples

### Cell Dimension Computation (QML)
```qml
// Available area = homeScreen size minus margins
// Grid container occupies the area above the dock
readonly property real cellWidth: gridContainer.width / WidgetGridModel.gridColumns
readonly property real cellHeight: gridContainer.height / WidgetGridModel.gridRows
```

For a 1024x600 display:
- Container: ~984 x ~484 (after 2*20px margins, 56px dock, spacing)
- Grid 6x4: cellWidth=164, cellHeight=121
- A 2x2 widget: 328 x 242 pixels
- A 1x1 widget: 164 x 121 pixels

### Clock Widget Breakpoints
```qml
// REV-02: Clock breakpoints
// 1x1 (~164x121): time only
// 2x1 (~328x121): time + date
// 2x2+ (~328x242+): time + date + day

readonly property bool showDate: width >= 250    // ~2 cells wide
readonly property bool showDay: height >= 200    // ~2 cells tall

NormalText {
    id: timeText
    font.pixelSize: showDay ? UiMetrics.fontHeading * 2.5
                   : showDate ? UiMetrics.fontHeading * 1.8
                   : UiMetrics.fontHeading * 1.5
    // ...
}

NormalText {
    id: dateText
    visible: showDate
    // ...
}

NormalText {
    id: dayText
    visible: showDay
    // ...
}
```

### AA Status Widget Breakpoints
```qml
// REV-03: AA Status breakpoints
// 1x1 (~164x121): icon only
// 2x1+ (~328x121+): icon + text

readonly property bool showText: width >= 250

MaterialIcon {
    // Always visible
    size: showText ? UiMetrics.iconSize : UiMetrics.iconSize * 1.5
}

NormalText {
    text: connected ? "Connected" : "Tap to connect"
    visible: showText
}
```

### Now Playing Widget Breakpoints
```qml
// Now Playing breakpoints (existing bt-now-playing)
// 2x1 (~300x121): compact horizontal strip (icon + title + play/pause)
// 3x2 (~450x242): full layout with all controls

readonly property bool isFullLayout: width >= 400 && height >= 200

// Full: centered column with icon, title, artist, prev/play/next
// Compact: horizontal row with icon, title column, play button
```

### CMakeLists.txt Pattern for New QML Files
```cmake
# Any new QML file loaded via Loader MUST have SKIP_CACHEGEN
set_source_files_properties(../qml/applications/home/GridCell.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "GridCell"
    QT_RESOURCE_ALIAS "GridCell.qml"
    QT_QML_SKIP_CACHEGEN TRUE
)
```

### HomeMenu.qml Rewrite Skeleton
```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: homeScreen
    anchors.fill: parent

    Component.onCompleted: ApplicationController.setTitle("Home")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        // Grid container — fills available space above dock
        Item {
            id: gridContainer
            Layout.fillWidth: true
            Layout.fillHeight: true

            readonly property real cellWidth: width / WidgetGridModel.gridColumns
            readonly property real cellHeight: height / WidgetGridModel.gridRows

            Repeater {
                model: WidgetGridModel

                delegate: Item {
                    x: model.column * gridContainer.cellWidth
                    y: model.row * gridContainer.cellHeight
                    width: model.colSpan * gridContainer.cellWidth
                    height: model.rowSpan * gridContainer.cellHeight
                    visible: model.visible

                    // Inner content with gutter margins
                    Item {
                        anchors.fill: parent
                        anchors.margins: UiMetrics.spacing / 2

                        // Glass card background
                        Rectangle {
                            anchors.fill: parent
                            radius: UiMetrics.radius
                            color: ThemeService.surfaceContainer
                            opacity: model.opacity
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: UiMetrics.radius
                            color: "transparent"
                            border.width: 1
                            border.color: ThemeService.outlineVariant
                            opacity: 0.3
                        }

                        Loader {
                            anchors.fill: parent
                            source: model.qmlComponent
                            asynchronous: false
                        }
                    }
                }
            }
        }

        // Launcher dock — fixed at bottom
        LauncherDock {
            Layout.fillWidth: true
        }
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| 3-pane fixed layout (main + sub1 + sub2) | Cell-based grid (NxM) | Phase 04 (this milestone) | HomeMenu must be completely rewritten |
| isMainPane boolean for widget sizing | Pixel-based breakpoints (width/height) | Phase 05 (this phase) | All 3 widgets need revision |
| WidgetPlacementModel (pane-based) | WidgetGridModel (grid-based) | Phase 04-02 | QML context property already swapped in main.cpp |
| WidgetInstanceContext.paneSize | Widget reads own width/height | Phase 05 (this phase) | Simpler, more accurate, reactive to resizes |

**Deprecated/outdated:**
- `WidgetPlacementModel`: C++ class still exists but QML context property removed. HomeMenu.qml and WidgetHost.qml still reference it (will break until rewritten).
- `widgetContext.paneSize`: WidgetInstanceContext no longer has paneSize -- only cellWidth/cellHeight. But widgets don't even need WidgetInstanceContext for breakpoints -- they can use their own width/height directly.
- `WidgetContextMenu` / `WidgetPicker` Loaders in HomeMenu: These reference the old pane model. Remove from HomeMenu; Phase 07 reintroduces with grid-aware versions.

## Open Questions

1. **GridCell.qml as Separate File vs Inline Delegate**
   - What we know: Inline delegates work fine for simple cases. Separate file is cleaner for testing and reuse.
   - What's unclear: Whether the delegate needs to be loaded via Loader (requiring SKIP_CACHEGEN) or can be a regular QML type.
   - Recommendation: Use an inline delegate for now (it's inside HomeMenu.qml which is already in the module). Extract to GridCell.qml only if complexity warrants it. If extracted, add SKIP_CACHEGEN.

2. **WidgetPlacementModel Removal Timing**
   - What we know: The QML context property was removed in Phase 04-02. The C++ class and its tests still exist.
   - What's unclear: Should the C++ class be deleted in this phase or left for later cleanup?
   - Recommendation: Do NOT delete the C++ class in this phase. It has tests and the old WidgetPicker references it. Just ensure QML no longer references it. Cleanup can happen after Phase 07 when edit mode is fully grid-aware.

3. **Breakpoint Threshold Calibration**
   - What we know: Target cells are ~150x125 at 1024x600. Actual rendered size depends on margins.
   - What's unclear: Exact breakpoint pixel values that look good on real hardware.
   - Recommendation: Start with thresholds derived from 1.5x cell width (225px for "show text") and 1.5x cell height (187px for "show full"). Tune on Pi.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (QTest) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build && ctest --output-on-failure -R "widget_grid_model\|display_info\|grid_dimensions"` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| GRID-03 | Widgets render at grid positions | manual (QML visual) | Visual inspection on dev VM + Pi | N/A |
| GRID-06 | Default layout shows on fresh install | unit | `ctest -R widget_grid_model` (existing testPlaceWidgetSuccess) | Yes |
| REV-01 | Pixel breakpoints replace isMainPane | manual (QML visual) | Visual inspection at multiple cell sizes | N/A |
| REV-02 | Clock breakpoints | manual (QML visual) | Visual inspection: `--geometry 800x480` vs `--geometry 1024x600` | N/A |
| REV-03 | AA Status breakpoints | manual (QML visual) | Visual inspection with geometry flag | N/A |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure -R "widget_grid\|display_info\|grid_dim"`
- **Per wave merge:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Phase gate:** Full suite green + visual verification on dev VM with `--geometry 1024x600` and `--geometry 800x480`

### Wave 0 Gaps
None -- existing test infrastructure covers data model requirements. Widget revision (REV-01/02/03) is visual-only and requires manual verification on the target display. No new automated tests needed for QML rendering.

## Sources

### Primary (HIGH confidence)
- Project codebase: WidgetGridModel.hpp/cpp, DisplayInfo.hpp/cpp, HomeMenu.qml, WidgetHost.qml, ClockWidget.qml, AAStatusWidget.qml, NowPlayingWidget.qml, main.cpp, UiMetrics.qml
- Phase 04 plans: 04-01-PLAN.md, 04-02-PLAN.md (grid model decisions, context property removal)
- REQUIREMENTS.md: GRID-03, GRID-06, REV-01, REV-02, REV-03

### Secondary (MEDIUM confidence)
- Qt Quick documentation: Repeater, Loader, Item positioning (verified against project's existing Qt 6.4/6.8 dual-compat patterns)

### Tertiary (LOW confidence)
- None -- this phase uses only project-internal patterns and standard Qt Quick features

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- entirely project-internal, no new dependencies
- Architecture: HIGH -- Repeater + manual positioning is the obvious and correct pattern for multi-span grids; confirmed by roadmap's explicit "not GridView/GridLayout" constraint
- Pitfalls: HIGH -- all pitfalls identified from direct codebase analysis (existing WidgetPlacementModel references, QT_QML_SKIP_CACHEGEN gotcha documented in project memory)

**Research date:** 2026-03-12
**Valid until:** Indefinite -- no external dependencies that could change
