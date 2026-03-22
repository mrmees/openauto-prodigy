# Phase 08: Multi-Page - Research

**Researched:** 2026-03-13
**Domain:** Qt QML SwipeView multi-page layout, page-scoped widget grid model
**Confidence:** HIGH

## Summary

Phase 08 adds horizontal swipe navigation between multiple widget pages on the home screen. The existing `HomeMenu.qml` currently renders a single grid of widgets via `WidgetGridModel` (a flat `QAbstractListModel`) inside a `gridContainer` Item, with a `LauncherDock` pinned below it. The core work is: (1) wrapping the grid in a `SwipeView` with per-page content, (2) adding a `page` field to `GridPlacement` for page-scoped filtering, (3) page indicator dots, and (4) page management (add/delete) in edit mode.

Qt's `SwipeView` and `PageIndicator` are both available in Qt 6.4+ (verified on dev machine) and provide exactly the navigation + indicator behavior needed. The `interactive` property disables swipe gesture when set to `false`, which is the mechanism for PAGE-04 (disable during edit mode). The main architectural decision is whether to use multiple `WidgetGridModel` instances (one per page) or a single model with page filtering. A single model with filtering is simpler and aligns with the existing auto-save wiring.

**Primary recommendation:** Use `SwipeView` with `Repeater`-based page generation, a single `WidgetGridModel` extended with page awareness, and `PageIndicator` with custom dot styling. Keep the `LauncherDock` outside the `SwipeView` in the parent `ColumnLayout`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Horizontal swipe between pages with smooth glide animation (~350ms)
- Rubber band overscroll at edges (elastic snap-back at first/last page)
- Standard filled/unfilled dot indicators centered between grid and dock
- Active dot = primary/accent color, inactive = muted
- Page indicator dots always visible, even with a single page
- Explicit "add page" button in edit mode (FAB, separate from add-widget FAB) -- no auto-create on overflow
- No maximum page limit -- user can create as many as they want
- Empty pages auto-cleaned on edit mode exit
- Explicit "delete page" button also available in edit mode -- removes page and all its widgets
- Delete-page requires confirmation dialog ("Delete page and N widgets?")
- Page 1 cannot be deleted (always at least one page)
- Edit mode is page-scoped -- only affects the currently visible page
- No cross-page navigation during edit mode (exit edit, swipe, long-press to edit another page)
- Swipe gesture disabled during edit mode (prevents conflict with drag-to-reposition)
- Add-page FAB only visible during edit mode
- Full page behavior: toast "No space available -- remove a widget first" (same as Phase 07)
- No migration needed (alpha, no current users)
- Add `page` field to GridPlacement (schema v3)
- Fresh install: 2 blank pages

### Claude's Discretion
- SwipeView vs Flickable vs custom page container implementation
- Add-page FAB placement relative to add-widget FAB
- Delete-page button/FAB placement and styling
- Confirmation dialog visual treatment
- Dot indicator sizing (visible but appropriately small for 7" screen)
- Lazy instantiation strategy for non-visible pages
- Page transition easing curve
- Whether dots are tappable for page navigation in normal mode (not required)

### Deferred Ideas (OUT OF SCOPE)
- Dedicated system widgets page replacing the navbar -- future milestone
- Page reordering via drag on page indicator dots (already in REQUIREMENTS.md as POLISH-03)
- Drag widgets between pages (explicitly out of scope in REQUIREMENTS.md)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PAGE-01 | Home screen supports multiple widget pages with horizontal swipe navigation | SwipeView component (Qt 6.4+), `interactive` property for gesture control |
| PAGE-02 | Page indicator dots show current page position and total count | PageIndicator component with custom delegate for M3 styling |
| PAGE-03 | Launcher dock remains fixed across all pages | ColumnLayout structure: SwipeView + PageIndicator + LauncherDock as siblings |
| PAGE-04 | Page swipe is disabled during edit mode | `SwipeView.interactive: !editMode` binding |
| PAGE-05 | Maximum 5 pages supported | CONTEXT.md overrides: no maximum page limit |
| PAGE-06 | Explicit page creation and removal | Add-page FAB + delete-page button; `addPage()`/`removePage()` on WidgetGridModel |
| PAGE-07 | Page navigation available during edit mode via non-swipe mechanism | Dots remain tappable (PageIndicator `interactive: true` always); `setCurrentIndex()` method |
| PAGE-08 | Non-visible pages are lazily instantiated | SwipeView `Loader` pattern with `active: isCurrentItem \|\| isNextItem \|\| isPreviousItem` |
| PAGE-09 | Page assignments persist in YAML config across restarts | `page` field on GridPlacement, `page_count` in widget_grid YAML section |
</phase_requirements>

## Standard Stack

### Core
| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| SwipeView | Qt 6.4+ (QtQuick.Controls) | Horizontal page container with snap navigation | Built-in, handles gesture physics, snap behavior, orientation. Verified present on both dev (6.4.2) and Pi (6.8) |
| PageIndicator | Qt 6.4+ (QtQuick.Controls) | Dot indicator for page position | Built-in, customizable delegate for M3 styling, supports `interactive` for tap navigation |

### Already In Use
| Component | Purpose | How It Extends |
|-----------|---------|----------------|
| WidgetGridModel | Grid widget management | Add `page` field filtering, page count tracking |
| YamlConfig | YAML persistence | Add `page` field to grid placement serialization, add `page_count` |
| GridPlacement | Widget position struct | Add `int page = 0` field |
| HomeMenu.qml | Home screen layout | Wrap grid in SwipeView, add PageIndicator, add page FABs |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| SwipeView | PathView / Flickable | SwipeView provides snap-to-page, page index tracking, interactive toggle for free. Flickable requires manual snap logic. PathView is overkill. |
| PageIndicator | Custom Row + Repeater dots | PageIndicator already handles sizing, animation, tap interaction, accessibility. Custom gains nothing. |
| Multiple WidgetGridModel instances | Single model with page filter | Multiple models would require duplicating auto-save wiring, occupancy grids, instance ID tracking. Single model with page awareness is far simpler. |

## Architecture Patterns

### Recommended Approach: Single Model, Page-Filtered Views

```
HomeMenu.qml
  ColumnLayout
    SwipeView (fills available space)
      Repeater (model: pageCount)
        Loader (lazy: active when current/adjacent)
          PageGridContent.qml (per-page grid)
            Repeater (model: WidgetGridModel, filtered by page)
    PageIndicator (between SwipeView and dock)
    LauncherDock (fixed at bottom)
```

**Key insight:** The `WidgetGridModel` stores ALL placements across all pages. Each page's QML Repeater shows only placements where `model.page === pageIndex`. This keeps the C++ model simple (one occupancy grid per page) and auto-save writes the full list.

### Pattern 1: Page-Scoped Occupancy

**What:** The C++ `WidgetGridModel` needs per-page occupancy tracking. Currently `occupancy_` is a single flat grid. With pages, `canPlace()` and `findFirstAvailableCell()` must scope to a specific page.

**When to use:** All grid operations (place, move, resize, collision check).

**Approach:**
```cpp
// GridPlacement gets page field
struct GridPlacement {
    // ... existing fields ...
    int page = 0;
};

// WidgetGridModel gets page-aware methods
Q_PROPERTY(int activePage READ activePage WRITE setActivePage NOTIFY activePageChanged)
Q_PROPERTY(int pageCount READ pageCount WRITE setPageCount NOTIFY pageCountChanged)

// Occupancy becomes per-page: QMap<int, QVector<QString>>
// Or rebuild occupancy for active page on demand
```

### Pattern 2: SwipeView with Lazy Loading

**What:** Qt docs recommend lazy loading for SwipeView pages. Use `Loader` with `active` bound to SwipeView attached properties.

**Implementation:**
```qml
SwipeView {
    id: pageView
    interactive: !homeScreen.editMode

    Repeater {
        model: WidgetGridModel.pageCount

        Loader {
            active: SwipeView.isCurrentItem || SwipeView.isNextItem || SwipeView.isPreviousItem
            sourceComponent: pageGridComponent
            // Pass page index to loaded content
        }
    }
}

Component {
    id: pageGridComponent
    // Grid container with Repeater filtered by page
}
```

### Pattern 3: Rubber Band Overscroll

**What:** SwipeView's underlying ListView uses `boundsBehavior: Flickable.StopAtBounds` by default. For rubber-band effect, override to `Flickable.DragAndOvershootBounds`.

**Implementation:**
```qml
SwipeView {
    id: pageView
    Component.onCompleted: {
        // SwipeView's contentItem is a ListView
        contentItem.boundsBehavior = Flickable.DragAndOvershootBounds
    }
}
```

### Pattern 4: Page Model Filtering in QML

**What:** Filter widgets per page without a proxy model. Use delegate `visible` binding.

**Why not QSortFilterProxyModel:** Would require one proxy per page, each connected to the same source model. Overhead for simple integer filtering.

**Implementation:**
```qml
Repeater {
    model: WidgetGridModel
    delegate: Item {
        // Only show widgets belonging to this page
        visible: model.page === pageIndex
        // ... grid positioning ...
    }
}
```

**Caveat:** Invisible delegates still exist in the scene graph. For large widget counts this is fine (typically <30 widgets total across all pages). The lazy `Loader` on the page level handles the real performance win.

### Pattern 5: Edit Mode Page Isolation

**What:** Edit mode FABs, drag overlay, dotted grid, and all edit interactions are scoped to the current page only. SwipeView `interactive: false` prevents page switching.

**Implementation:** The existing edit mode infrastructure in HomeMenu.qml already operates on the visible grid content. By placing the grid content inside per-page items within the SwipeView, edit mode naturally scopes to the current page. The drag overlay (`z: 200`) stays at the SwipeView level (or the homeScreen level) since it needs to capture touches across the full area.

### Anti-Patterns to Avoid
- **Multiple WidgetGridModel instances:** Creates sync nightmares for auto-save, instance ID uniqueness, and widget picker queries.
- **Anchors on SwipeView children:** SwipeView takes over geometry of direct children. Use `width`/`height` bindings or let SwipeView manage sizing.
- **Direct `currentIndex` assignment with dynamic `interactive`:** Known Qt bug (QTBUG-80750) -- toggling `interactive` can break `currentIndex` binding. Use `setCurrentIndex()` method or `Qt.callLater()` for safety.
- **Putting LauncherDock inside SwipeView:** Dock must be page-independent. Keep it as a sibling in the parent ColumnLayout.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Page snap physics | Custom Flickable + snap logic | SwipeView | Handles velocity-based snap, overshoot damping, orientation |
| Dot indicators | Row of Rectangles + manual index tracking | PageIndicator with custom delegate | Handles animation, pressed states, accessibility |
| Rubber band overscroll | Custom spring animation | `boundsBehavior: Flickable.DragAndOvershootBounds` on SwipeView contentItem | Built into Flickable physics |
| Page navigation during edit | Custom tap handler on dots | PageIndicator `interactive: true` + `onCurrentIndexChanged` | Already supports tap navigation out of the box |

**Key insight:** SwipeView + PageIndicator is Qt's built-in answer to this exact problem. Customization is in styling (dot colors, sizes), not behavior.

## Common Pitfalls

### Pitfall 1: SwipeView interactive + currentIndex Binding Break
**What goes wrong:** Toggling `interactive` dynamically can cause `currentIndex` to lose its binding, resulting in pages that won't change.
**Why it happens:** Qt bug QTBUG-80750 -- the internal ListView's highlight positioning gets confused.
**How to avoid:** Use property binding `interactive: !editMode` rather than imperative assignment. If issues arise, use `setCurrentIndex()` method calls instead of direct binding. Test on both Qt 6.4 and 6.8.
**Warning signs:** After exiting edit mode, swipe no longer works or currentIndex stays stuck.

### Pitfall 2: Drag Overlay Z-Order with SwipeView
**What goes wrong:** The drag overlay (`z: 200`) needs to capture touches during widget drag, but it's inside a SwipeView page that clips its content.
**Why it happens:** SwipeView pages clip to their bounds by default.
**How to avoid:** Keep the drag overlay, drop highlight, resize ghost, and FABs at the homeScreen level (outside the SwipeView), not inside page content. They overlay the entire home screen, not just one page.
**Warning signs:** Drag doesn't follow finger past page edges, or drag artifacts appear on wrong page.

### Pitfall 3: Empty Page Auto-Cleanup Timing
**What goes wrong:** Auto-cleaning empty pages on edit mode exit while the SwipeView is animating causes visual glitches or index jumps.
**Why it happens:** Removing pages from the model while SwipeView is settling its animation.
**How to avoid:** Use `Qt.callLater()` or a short delay timer after edit mode exit to perform cleanup. Adjust `currentIndex` before removing pages to avoid index-out-of-bounds.
**Warning signs:** Flash of wrong page, index jump, or SwipeView showing blank content after edit exit.

### Pitfall 4: Page Count vs Repeater Model Updates
**What goes wrong:** Adding/removing pages doesn't update the SwipeView content because the Repeater model is a simple integer (`pageCount`), and QML doesn't re-evaluate when an integer property changes.
**Why it happens:** `Repeater { model: intValue }` creates delegates once; changing `intValue` triggers a full reset.
**How to avoid:** This is actually fine for small page counts (<10). The `Repeater` with an integer model does react to changes. But the reset destroys and recreates all page items, which resets scroll position and any in-progress state. For safety, track `currentIndex` before the reset and restore it after.
**Warning signs:** After adding a page, current page content flickers or reloads.

### Pitfall 5: Page Field Missing from canPlace/findFirstAvailableCell
**What goes wrong:** Adding a widget checks occupancy globally instead of per-page, so occupied cells on page 0 block placement on page 1.
**Why it happens:** The current `canPlace()` and `findFirstAvailableCell()` don't have a page parameter.
**How to avoid:** Add `int page` parameter to `canPlace()`, `findFirstAvailableCell()`, and rebuild occupancy per-page. Use `activePage` property to scope operations.
**Warning signs:** "No space available" when there's clearly empty space on the current page.

### Pitfall 6: SKIP_CACHEGEN and Loader source for Page Content
**What goes wrong:** If page grid content is extracted to a separate QML file loaded via `Loader { source: url }`, it needs `QT_QML_SKIP_CACHEGEN TRUE` in CMake on Qt 6.8.
**Why it happens:** Qt 6.8 cachegen replaces raw .qml with compiled C++. Loader URL lookup fails without SKIP_CACHEGEN. This is a known project gotcha.
**How to avoid:** Either use inline `Component {}` (no separate file) or add the new file to `QT_QML_SKIP_CACHEGEN` list. Inline Component is simpler and avoids this entirely.
**Warning signs:** Works on dev (Qt 6.4), fails on Pi (Qt 6.8) with blank page content.

## Code Examples

### SwipeView + PageIndicator Basic Structure
```qml
// Source: Qt 6 documentation + project patterns
import QtQuick.Controls

ColumnLayout {
    anchors.fill: parent
    anchors.margins: UiMetrics.marginPage
    spacing: UiMetrics.spacing

    SwipeView {
        id: pageView
        Layout.fillWidth: true
        Layout.fillHeight: true
        interactive: !homeScreen.editMode
        clip: true

        Component.onCompleted: {
            // Enable rubber band overscroll
            contentItem.boundsBehavior = Flickable.DragAndOvershootBounds
            // Customize transition duration (~350ms per user decision)
            contentItem.highlightMoveDuration = 350
        }

        Repeater {
            model: WidgetGridModel.pageCount

            Loader {
                active: SwipeView.isCurrentItem
                        || SwipeView.isNextItem
                        || SwipeView.isPreviousItem
                sourceComponent: pageGridComponent

                property int pageIndex: index
            }
        }
    }

    PageIndicator {
        id: pageIndicator
        Layout.alignment: Qt.AlignHCenter
        count: pageView.count
        currentIndex: pageView.currentIndex
        interactive: true  // Tappable always, even during edit mode

        delegate: Rectangle {
            implicitWidth: UiMetrics.spacing * 1.2
            implicitHeight: implicitWidth
            radius: width / 2
            color: index === pageIndicator.currentIndex
                   ? ThemeService.primary
                   : ThemeService.onSurfaceVariant
            opacity: index === pageIndicator.currentIndex ? 1.0 : 0.4

            required property int index
            Behavior on opacity { OpacityAnimator { duration: 150 } }
        }

        onCurrentIndexChanged: {
            if (currentIndex !== pageView.currentIndex)
                pageView.setCurrentIndex(currentIndex)
        }
    }

    LauncherDock {
        Layout.fillWidth: true
    }
}
```

### GridPlacement Schema v3 Serialization
```cpp
// YamlConfig.cpp -- add page field
void YamlConfig::setGridPlacements(const QList<GridPlacement>& placements)
{
    root_["widget_grid"]["version"] = 3;  // bumped from 2

    YAML::Node node(YAML::NodeType::Sequence);
    for (const auto& p : placements) {
        YAML::Node n;
        n["instance_id"] = p.instanceId.toStdString();
        n["widget_id"] = p.widgetId.toStdString();
        n["page"] = p.page;  // NEW
        n["col"] = p.col;
        n["row"] = p.row;
        n["col_span"] = p.colSpan;
        n["row_span"] = p.rowSpan;
        n["opacity"] = p.opacity;
        node.push_back(n);
    }
    root_["widget_grid"]["placements"] = node;
    root_["widget_grid"]["page_count"] = pageCount;  // persist page count
}
```

### Page-Scoped Occupancy
```cpp
// WidgetGridModel -- per-page occupancy
void WidgetGridModel::rebuildOccupancyForPage(int page)
{
    clearOccupancy();
    for (const auto& p : placements_) {
        if (p.visible && p.page == page)
            markOccupied(p);
    }
}

bool WidgetGridModel::canPlace(int col, int row, int colSpan, int rowSpan,
                                int page, const QString& excludeInstanceId) const
{
    if (col < 0 || row < 0) return false;
    if (col + colSpan > cols_ || row + rowSpan > rows_) return false;

    // Check only placements on this page
    for (const auto& p : placements_) {
        if (p.page != page || p.instanceId == excludeInstanceId || !p.visible)
            continue;
        // Rectangle overlap test
        if (col < p.col + p.colSpan && col + colSpan > p.col &&
            row < p.row + p.rowSpan && row + rowSpan > p.row)
            return false;
    }
    return true;
}
```

### Confirmation Dialog Pattern
```qml
// Delete page confirmation -- follows existing overlay pattern
Item {
    id: deletePageDialog
    anchors.fill: parent
    visible: false
    z: 300

    property int targetPage: -1
    property int widgetCount: 0

    Rectangle {
        anchors.fill: parent
        color: ThemeService.scrim
        opacity: 0.4
        MouseArea { anchors.fill: parent; onClicked: deletePageDialog.visible = false }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.6, 400)
        height: contentCol.implicitHeight + UiMetrics.spacing * 3
        radius: UiMetrics.radiusLarge
        color: ThemeService.surfaceContainerHighest

        ColumnLayout {
            id: contentCol
            anchors.fill: parent
            anchors.margins: UiMetrics.spacing * 1.5
            spacing: UiMetrics.spacing

            NormalText {
                text: "Delete page and " + deletePageDialog.widgetCount + " widget"
                      + (deletePageDialog.widgetCount !== 1 ? "s" : "") + "?"
                font.pixelSize: UiMetrics.fontTitle
                color: ThemeService.onSurface
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: UiMetrics.spacing

                // Cancel / Delete buttons
            }
        }
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Single grid (no pages) | SwipeView multi-page | Phase 08 | Widgets organized across swipeable pages |
| Schema v2 (no page field) | Schema v3 (page field on GridPlacement) | Phase 08 | No migration needed (alpha) |
| Flat occupancy grid | Per-page occupancy | Phase 08 | canPlace/findFirstAvailableCell scoped to page |

## Open Questions

1. **Repeater + SwipeView vs explicit page Items**
   - What we know: Using `Repeater { model: pageCount }` inside SwipeView works but resets all pages when count changes
   - What's unclear: Whether the reset causes noticeable visual disruption on Pi with lazy Loaders
   - Recommendation: Use Repeater approach. Page count changes are rare (edit mode only). If reset is jarring, save/restore currentIndex across the reset.

2. **Occupancy strategy: per-page map vs on-demand rebuild**
   - What we know: Current occupancy is a single flat QVector. Multi-page needs page scoping.
   - What's unclear: Whether a `QMap<int, QVector<QString>>` (per-page map) or on-demand rebuild for active page is more appropriate
   - Recommendation: On-demand rebuild is simpler. `canPlace()` iterates placements for the target page. Occupancy grid only needed for the active page's collision checks. With <30 widgets total, iteration is negligible.

3. **PAGE-05 conflict: REQUIREMENTS.md says max 5, CONTEXT.md says no maximum**
   - What we know: CONTEXT.md (user decisions) explicitly says "No maximum page limit -- user can create as many as they want"
   - Recommendation: Follow CONTEXT.md (user's locked decision). The planner should note PAGE-05 is overridden by user decision.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test 6.4+ |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && ctest -R test_widget_grid --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PAGE-01 | Multi-page navigation | manual-only | Pi touchscreen test | N/A |
| PAGE-02 | Page indicator dots | manual-only | Visual verification | N/A |
| PAGE-03 | Dock fixed across pages | manual-only | Visual verification | N/A |
| PAGE-04 | Swipe disabled in edit mode | manual-only | Pi touchscreen test | N/A |
| PAGE-05 | No page limit (overridden) | unit | `ctest -R test_widget_grid -x` | Wave 0 |
| PAGE-06 | Page add/remove | unit | `ctest -R test_widget_grid -x` | Wave 0 |
| PAGE-07 | Dot tap navigation in edit | manual-only | Pi touchscreen test | N/A |
| PAGE-08 | Lazy instantiation | manual-only | Performance check on Pi | N/A |
| PAGE-09 | Page persistence | unit | `ctest -R test_yaml_config -x` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Per wave merge:** Full suite
- **Phase gate:** Full suite green + Pi touchscreen verification

### Wave 0 Gaps
- [ ] `tests/test_widget_grid_model.cpp` -- covers PAGE-05, PAGE-06 (page-scoped canPlace, addPage, removePage, page count)
- [ ] Update `tests/test_yaml_config.cpp` -- covers PAGE-09 (page field serialization roundtrip)

## Sources

### Primary (HIGH confidence)
- Qt 6 official docs: [SwipeView QML Type](https://doc.qt.io/qt-6/qml-qtquick-controls-swipeview.html) -- properties, lazy loading example, interactive toggle
- Qt 6 official docs: [PageIndicator QML Type](https://doc.qt.io/qt-6/qml-qtquick-controls-pageindicator.html) -- delegate customization, interactive tap
- Local filesystem verification: SwipeView.qml and PageIndicator.qml present at `/usr/lib/x86_64-linux-gnu/qt6/qml/QtQuick/Controls/Basic/` (Qt 6.4.2)
- Existing codebase: `src/ui/WidgetGridModel.hpp/cpp`, `qml/applications/home/HomeMenu.qml`, `src/core/YamlConfig.cpp`

### Secondary (MEDIUM confidence)
- [QTBUG-80750](https://bugreports.qt.io/browse/QTBUG-80750) -- SwipeView interactive + currentIndex binding issue (status unclear, workaround documented)
- [Qt Forum SwipeView interactive discussion](https://forum.qt.io/topic/109694/swipeview-with-dynamic-interactive-breaks-currentindex-binding) -- community workarounds

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- SwipeView and PageIndicator verified present on both Qt 6.4 (dev) and expected on Qt 6.8 (Pi). Core QML controls, stable across versions.
- Architecture: HIGH -- Single-model-with-page-filter is a straightforward extension of existing code. No new libraries or complex patterns.
- Pitfalls: HIGH -- Known issues documented (QTBUG-80750, SKIP_CACHEGEN, drag overlay z-order). All have clear mitigations.

**Research date:** 2026-03-13
**Valid until:** 2026-04-13 (stable Qt components, no external dependencies)
