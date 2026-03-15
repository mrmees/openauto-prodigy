# Phase 07: Edit Mode - Research

**Researched:** 2026-03-12
**Domain:** QML touch-driven drag interactions, grid-based widget editing, Qt 6.4/6.8 dual-compat
**Confidence:** HIGH

## Summary

Phase 07 transforms the home screen from a static grid with per-widget context menus into a full-screen edit mode where all widgets become simultaneously movable, resizable, and removable. The C++ backend (`WidgetGridModel`) already provides every operation needed -- `moveWidget()`, `resizeWidget()`, `removeWidget()`, `canPlace()`, and occupancy tracking are fully tested with 20 unit tests. The work is almost entirely QML: building the edit mode overlay, drag/resize interactions, and visual feedback on top of the existing model.

The critical technical decision is drag implementation strategy. After investigation, **`MouseArea` with `drag.target` is the correct approach** -- it's available in Qt 6.4+, used elsewhere in this codebase (EqSettings), and provides the `pressed`/`released`/`position` properties needed for snap-on-drop behavior. `DragHandler` is also Qt 6.4 compatible but is designed for more complex multi-touch scenarios and provides less control over the drag-then-snap-on-release pattern needed here.

**Primary recommendation:** Build edit mode as a QML state on `HomeMenu.qml` with a `property bool editMode: false` that toggles visual overlays on all widget delegates. Use MouseArea drag for repositioning and a separate MouseArea on the resize handle. All operations go through the existing `WidgetGridModel` Q_INVOKABLE methods on release.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Long-press ANYWHERE on the grid (empty cell or widget) enters edit mode
- All widgets simultaneously show accent borders and resize handles
- Replaces the current behavior where long-press opens per-widget context menu outside edit mode
- Accent border (primary color) on all widgets when edit mode is active
- Small resize handle at bottom-right corner of each widget
- 'X' removal badge at top-right corner of each widget
- '+' FAB in bottom-right corner for adding widgets
- Subtle dotted grid lines showing cell boundaries (visible throughout edit mode)
- Tap any empty grid area exits edit mode
- 10-second inactivity timeout as safety net
- Auto-exits when AA fullscreen activates (EVIOCGRAB steals touch)
- No explicit "Done" button needed
- Tapping a widget in edit mode opens the existing context menu (change widget type, adjust opacity, clear)
- Short hold (~200ms) then drag initiates move -- distinguishes tap (context menu) from drag (reposition)
- Dragged widget follows finger at ~50% opacity, slightly scaled up
- Original position shows empty outlined placeholder
- Valid target cells highlight in primary/accent color as you drag over them
- Occupied/invalid cells flash red
- On release over invalid target, widget snaps back to original position
- On valid drop, widget snaps to nearest cell -- layout writes are atomic (on commit, not during drag)
- Single resize handle at bottom-right corner only
- Ghost dashed/outlined rectangle shows proposed new size, snapping to grid cells while dragging
- Bidirectional -- drag outward to grow, inward to shrink
- Ghost stops responding at min/max limits (brief color flash on boundary hit)
- Ghost turns red if size would overlap another widget
- Widget stays at original size until handle is released
- '+' FAB in bottom-right corner, visible only during edit mode
- Tapping FAB opens the existing bottom-sheet picker
- After selecting a widget, it auto-places in the first available grid spot at default size (2x2, clamped)
- 'X' circle badge at top-right corner of every widget during edit mode
- Tap 'X' to remove immediately -- no confirmation dialog
- Toast message at bottom: "No space available -- remove a widget first"

### Claude's Discretion
- Exact accent border width and color intensity
- FAB size and corner offset
- 'X' badge size and visual treatment
- Toast duration and styling
- Grid line dot spacing and opacity
- Drag threshold distance before activation
- Placeholder outline style (dashed vs dotted)
- Resize handle visual design (corner triangle, circle, etc.)
- MouseArea+Drag vs DragHandler implementation (Qt 6.4 compat -- resolved below)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| EDIT-01 | Long-press on empty grid area or widget background enters screen-wide edit mode | Existing long-press MouseArea pattern (z:-1, 500ms) in HomeMenu.qml. Change handler to set `editMode = true` instead of opening picker/context menu |
| EDIT-02 | Edit mode shows visual feedback on all widgets (accent borders + resize handles) | QML state binding: delegate shows border Rectangle + handle Items when `homeScreen.editMode`. ThemeService.primary for accent color |
| EDIT-03 | User can drag a widget to reposition it on the grid (snaps to cells on drop) | MouseArea `drag.target` on widget delegate. On release: compute target cell from position, call `WidgetGridModel.moveWidget()`. On fail: animate back |
| EDIT-04 | User can drag corner handles to resize within min/max span constraints | Separate MouseArea on resize handle item. Ghost rectangle tracks finger position snapped to cells. On release: call `WidgetGridModel.resizeWidget()` |
| EDIT-05 | "+" FAB opens widget catalog to add new widget | Existing pickerOverlay pattern. FAB visible when `editMode`. Auto-place via `findFirstAvailableCell()` helper (new) |
| EDIT-06 | "X" badge removes widget from grid | Existing `WidgetGridModel.removeWidget()`. Badge is a small circle with MaterialIcon at top-right, visible in editMode |
| EDIT-07 | Edit mode exits on tap outside or 10s timeout | Empty-area MouseArea calls `editMode = false`. Timer with `restart()` on any edit interaction |
| EDIT-08 | Edit mode auto-exits when AA fullscreen activates | `PluginModel.activePluginChanged` signal -- when `activePluginFullscreen` becomes true, set `editMode = false` |
| EDIT-09 | Layout writes are atomic -- persist on drop/commit, not during drag | `moveWidget()`/`resizeWidget()` already emit `placementsChanged()` which triggers YAML persist. Drag visual is position-only (no model writes until release) |
| EDIT-10 | Adding a widget when no space shows clear feedback | New Toast component. Check `WidgetRegistry.widgetsFittingSpace()` returns empty before opening picker, show toast instead |
</phase_requirements>

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt Quick | 6.4+ / 6.8 | QML UI framework | Project requirement -- dual-compat |
| MouseArea | built-in | Touch/drag interaction | Available Qt 6.4+, used in EqSettings drag pattern |
| Timer | built-in | Inactivity timeout | Standard QML timer |
| Behavior/NumberAnimation | built-in | Snap-back and transition animations | Already used throughout project |

### No New Dependencies
This phase requires zero new libraries. Everything is built with existing Qt Quick primitives and the already-complete `WidgetGridModel` C++ backend.

## Architecture Patterns

### Edit Mode State Machine

```
                    +----------+
                    |  Normal  |
                    +-----+----+
                          |
                   long-press (500ms)
                          |
                    +-----v----+
                    | EditMode |
                    +-----+----+
                          |
            +-------+-----+------+-------+
            |       |            |       |
         tap      timeout    AA grab  picker
        outside    10s       activate  open
            |       |            |       |
            v       v            v       |
         Normal   Normal      Normal     |
                                         v
                                    +--------+
                                    | Picker |
                                    +---+----+
                                        |
                                  select / cancel
                                        |
                                        v
                                    EditMode
```

### QML Structure (modified HomeMenu.qml)

```
HomeMenu.qml
  property bool editMode: false

  gridContainer
    // Grid lines overlay (visible in editMode)
    Repeater { model: gridColumns+1; delegate: vertical dotted line }
    Repeater { model: gridRows+1; delegate: horizontal dotted line }

    // Drop target highlight (visible during drag)
    Rectangle { id: dropHighlight }

    // Widget delegates
    Repeater {
      model: WidgetGridModel
      delegate: Item {
        // Normal widget content (existing)
        // Edit mode overlay (accent border, X badge, resize handle)
        // Drag MouseArea (short-hold threshold)
      }
    }

    // Resize ghost rectangle (visible during resize drag)
    Rectangle { id: resizeGhost }

  // FAB (visible in editMode)
  // Toast (transient, bottom-aligned)
  // Context menu overlay (existing, reused)
  // Picker overlay (existing, modified for auto-place)
```

### Drag-to-Reposition Pattern

```qml
// Inside widget delegate (editMode only)
MouseArea {
    id: dragArea
    anchors.fill: parent
    enabled: homeScreen.editMode

    property bool dragging: false
    property point pressPos: Qt.point(0, 0)
    property point originalPos: Qt.point(0, 0)

    // Short hold threshold before drag activates
    pressAndHoldInterval: 200

    onPressAndHold: {
        dragging = true
        originalPos = Qt.point(delegateItem.x, delegateItem.y)
        pressPos = Qt.point(mouse.x, mouse.y)
        // Reparent to gridContainer for free movement
    }

    onPositionChanged: {
        if (!dragging) return
        // Move visual proxy (NOT the model)
        // Compute target cell from current position
        // Update dropHighlight position and color
        inactivityTimer.restart()
    }

    onReleased: {
        if (!dragging) { /* tap = context menu */ return }
        dragging = false
        var targetCol = Math.round(proxy.x / cellWidth)
        var targetRow = Math.round(proxy.y / cellHeight)
        if (!WidgetGridModel.moveWidget(instanceId, targetCol, targetRow)) {
            // Animate back to originalPos
        }
    }
}
```

**Key insight:** The drag visual is a "proxy" -- either the delegate itself repositioned, or a separate overlay item. The model is NOT updated during drag. Only on release does `moveWidget()` get called, making writes atomic (EDIT-09).

### Resize Handle Pattern

```qml
// Bottom-right resize handle (visible in editMode)
Rectangle {
    id: resizeHandle
    width: UiMetrics.touchMin * 0.6
    height: width
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    visible: homeScreen.editMode

    MouseArea {
        anchors.fill: parent

        property point startPos: Qt.point(0, 0)
        property int startColSpan: 0
        property int startRowSpan: 0

        onPressed: {
            startPos = mapToItem(gridContainer, mouse.x, mouse.y)
            startColSpan = model.colSpan
            startRowSpan = model.rowSpan
            resizeGhost.visible = true
        }

        onPositionChanged: {
            var current = mapToItem(gridContainer, mouse.x, mouse.y)
            var deltaCol = Math.round((current.x - startPos.x) / cellWidth)
            var deltaRow = Math.round((current.y - startPos.y) / cellHeight)
            var newColSpan = Math.max(minCols, Math.min(maxCols, startColSpan + deltaCol))
            var newRowSpan = Math.max(minRows, Math.min(maxRows, startRowSpan + deltaRow))
            // Update ghost rectangle size and color
            inactivityTimer.restart()
        }

        onReleased: {
            resizeGhost.visible = false
            WidgetGridModel.resizeWidget(instanceId, newColSpan, newRowSpan)
        }
    }
}
```

### Auto-Place Algorithm (for FAB add)

```javascript
function findFirstAvailableCell(colSpan, rowSpan) {
    for (var r = 0; r < WidgetGridModel.gridRows; r++) {
        for (var c = 0; c < WidgetGridModel.gridColumns; c++) {
            if (WidgetGridModel.canPlace(c, r, colSpan, rowSpan))
                return { col: c, row: r }
        }
    }
    return null  // no space
}
```

### Toast Component (new, reusable)

```qml
// New file: qml/controls/Toast.qml
Rectangle {
    id: toast
    property string message: ""
    anchors.bottom: parent.bottom
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottomMargin: UiMetrics.gap * 2
    width: toastText.implicitWidth + UiMetrics.spacing * 2
    height: toastText.implicitHeight + UiMetrics.spacing
    radius: UiMetrics.radius
    color: ThemeService.inverseSurface
    opacity: 0

    NormalText {
        id: toastText
        text: toast.message
        color: ThemeService.inverseOnSurface
        anchors.centerIn: parent
    }

    function show(msg, durationMs) {
        message = msg
        opacity = 1.0
        hideTimer.interval = durationMs || 2500
        hideTimer.restart()
    }

    Behavior on opacity { NumberAnimation { duration: 200 } }
    Timer { id: hideTimer; onTriggered: toast.opacity = 0 }
}
```

### Anti-Patterns to Avoid
- **Updating WidgetGridModel during drag:** Every `moveWidget()` call triggers `dataChanged` + `placementsChanged` + YAML persist. Drag must be visual-only until release.
- **DragHandler for this use case:** DragHandler moves the target item directly and doesn't support the hold-before-drag threshold needed to distinguish tap from drag. MouseArea gives full control.
- **Animating widget width/height during resize:** Per STATE.md concern -- "ghost rectangle for resize preview, not live resize" is critical for Pi performance. Only the ghost rectangle changes size; the actual widget stays fixed until release.
- **Direct manipulation of Repeater delegate position:** Don't fight the Repeater's position binding (`x: model.column * cellWidth`). Instead, use a drag proxy overlay or temporarily unbind position during drag.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Occupancy/collision checking | Manual cell iteration in QML | `WidgetGridModel.canPlace()` | Already handles edge cases, tested with 20 unit tests |
| Widget min/max constraints | QML-side constraint checking | `WidgetGridModel.resizeWidget()` returns false | C++ checks against WidgetDescriptor constraints |
| Layout persistence | Manual YAML writes | `WidgetGridModel.placementsChanged()` signal | Already connected to ConfigService YAML persist |
| Widget descriptor lookup | QML property passing | `WidgetGridModel` data roles expose everything | `QmlComponentRole`, widget constraints via registry |

## Common Pitfalls

### Pitfall 1: Repeater Delegate Position Binding Conflict
**What goes wrong:** Delegate has `x: model.column * cellWidth`. During drag, you set `x` manually -- but the binding still fires on model changes, snapping the item back.
**Why it happens:** QML property bindings are re-evaluated whenever dependencies change.
**How to avoid:** Use a separate drag proxy Item (reparented to gridContainer at higher z) rather than moving the delegate itself. OR use `Qt.binding()` removal pattern -- but proxy is cleaner.
**Warning signs:** Widget "jumps" back to grid position during drag.

### Pitfall 2: Touch Coordinate Spaces
**What goes wrong:** Drag position computed in wrong coordinate space -- widget jumps on grab.
**Why it happens:** MouseArea reports `mouse.x/y` in local coordinates, but you need gridContainer coordinates.
**How to avoid:** Always use `mapToItem(gridContainer, mouse.x, mouse.y)` for position calculations. Store press offset to maintain finger position relative to widget.
**Warning signs:** Widget jumps to finger center instead of staying under finger.

### Pitfall 3: Long-Press Conflicts
**What goes wrong:** Edit mode long-press (500ms on grid) conflicts with widget interaction long-press.
**Why it happens:** Multiple MouseAreas at different z-levels compete for the press event.
**How to avoid:** When NOT in edit mode: long-press enters edit mode (replaces old per-widget context menu behavior). When IN edit mode: short hold (200ms) starts drag, tap opens context menu. Clear state machine.
**Warning signs:** Sometimes enters edit mode, sometimes opens context menu -- inconsistent.

### Pitfall 4: Inactivity Timer Reset
**What goes wrong:** Timer expires during active drag because drag events don't reset it.
**Why it happens:** Forgot to call `inactivityTimer.restart()` in `onPositionChanged`.
**How to avoid:** Reset timer on ANY touch interaction: drag move, resize move, context menu open, picker open, widget remove.
**Warning signs:** Edit mode exits while user is actively dragging.

### Pitfall 5: EVIOCGRAB Exit Race
**What goes wrong:** AA connects and grabs touch device while user is mid-drag. Widget stuck in drag state.
**Why it happens:** `PluginModel.activePluginChanged` fires on the main thread but drag state is only cleaned up on mouse release which never comes.
**How to avoid:** When setting `editMode = false`, also explicitly reset all drag state (dragging flags, proxy visibility, ghost visibility).
**Warning signs:** After AA connect during edit mode, ghost rectangles or drag proxies remain visible.

### Pitfall 6: Empty Grid Detection for Exit
**What goes wrong:** Tapping empty grid space doesn't exit edit mode because a widget delegate's MouseArea intercepts first.
**Why it happens:** Widget delegate MouseAreas are at higher z than the background.
**How to avoid:** In edit mode, widget delegate MouseAreas handle tap (context menu) and hold+drag (reposition). The background MouseArea handles tap-to-exit. Since widget delegates only cover occupied cells, tapping empty space naturally falls through to background.
**Warning signs:** Can't exit edit mode by tapping empty space.

## Code Examples

### Accessing Widget Constraints from QML

WidgetGridModel doesn't expose min/max constraints directly per-delegate. The resize handle needs these values. Two options:

**Option A (recommended): Add roles to WidgetGridModel**
```cpp
// In WidgetGridModel.hpp, add to Roles enum:
MinColsRole, MinRowsRole, MaxColsRole, MaxRowsRole

// In data():
case MinColsRole: {
    if (registry_) {
        auto desc = registry_->descriptor(p.widgetId);
        if (desc) return desc->minCols;
    }
    return 1;
}
```

**Option B: Q_INVOKABLE lookup**
```cpp
Q_INVOKABLE int widgetMinCols(const QString& widgetId) const;
```

Option A is better because it flows through the Repeater delegate naturally as `model.minCols`.

### AA Fullscreen Detection for Auto-Exit

```qml
// In HomeMenu.qml
Connections {
    target: PluginModel
    function onActivePluginChanged() {
        if (PluginModel.activePluginFullscreen && homeScreen.editMode) {
            homeScreen.editMode = false
        }
    }
}
```

This uses the existing `PluginModel.activePluginFullscreen` Q_PROPERTY. The AA plugin calls `requestActivation()` when connected, which triggers `PluginModel.setActivePlugin()`, which emits `activePluginChanged`.

### Dotted Grid Lines

```qml
// Vertical grid lines
Repeater {
    model: WidgetGridModel.gridColumns + 1
    delegate: Column {
        x: index * gridContainer.cellWidth
        y: 0
        height: gridContainer.height
        spacing: UiMetrics.spacing
        Repeater {
            model: Math.ceil(gridContainer.height / (UiMetrics.spacing + 2))
            delegate: Rectangle {
                width: 1
                height: 2
                color: ThemeService.outlineVariant
                opacity: 0.3
            }
        }
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Per-widget long-press context menu | Full-screen edit mode via long-press | This phase | All edit operations consolidated into one mode |
| WidgetPicker opened at long-press cell | Auto-place from FAB | This phase | Better for car touchscreen (no precision needed) |
| `DragHandler` recommended (initial review) | `MouseArea` with `drag.target` | Research resolved | Better control for hold-then-drag + snap-on-release |

## Open Questions

1. **Drag proxy vs delegate repositioning**
   - What we know: Both work, but proxy avoids binding conflicts
   - What's unclear: Whether creating/destroying a proxy Item per drag causes GC pressure on Pi
   - Recommendation: Start with moving the delegate itself (temporarily unbind x/y), fall back to proxy if binding issues arise. Simpler is better for Pi.

2. **Widget constraint roles in WidgetGridModel**
   - What we know: Resize handle needs min/max cols/rows per widget
   - What's unclear: Whether to add model roles or use Q_INVOKABLE lookup
   - Recommendation: Add MinColsRole/MaxColsRole/MinRowsRole/MaxRowsRole to WidgetGridModel -- small change, clean data flow through delegate

3. **Toast component location**
   - What we know: No existing toast in the project. Needed for EDIT-10 "no space" feedback
   - What's unclear: Whether to make it truly reusable (qml/controls/) or inline in HomeMenu
   - Recommendation: Create in qml/controls/ -- likely needed in future phases too

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (QtTest) |
| Config file | tests/CMakeLists.txt (`oap_add_test()` helper) |
| Quick run command | `cd build && ctest -R widget_grid -j$(nproc) --output-on-failure` |
| Full suite command | `cd build && ctest -j$(nproc) --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| EDIT-01 | Long-press enters edit mode | manual-only | N/A -- QML interaction test | N/A (QML state, visual) |
| EDIT-02 | Visual feedback (borders, handles) | manual-only | N/A -- QML visual test | N/A (QML state, visual) |
| EDIT-03 | Drag to reposition (snap + reject) | unit | `ctest -R widget_grid --output-on-failure` | Existing: moveWidget tests in test_widget_grid_model.cpp |
| EDIT-04 | Drag corner to resize within constraints | unit | `ctest -R widget_grid --output-on-failure` | Existing: resizeWidget tests in test_widget_grid_model.cpp |
| EDIT-05 | FAB opens catalog, auto-places | unit | `ctest -R widget_grid --output-on-failure` | Needs: findFirstAvailableCell test |
| EDIT-06 | X badge removes widget | unit | `ctest -R widget_grid --output-on-failure` | Existing: removeWidget test in test_widget_grid_model.cpp |
| EDIT-07 | Exit on tap outside or 10s timeout | manual-only | N/A -- QML interaction + timer | N/A (QML state) |
| EDIT-08 | Auto-exit on AA fullscreen | manual-only | N/A -- requires plugin activation | N/A (signal/slot, Pi-only) |
| EDIT-09 | Atomic layout writes (on commit) | unit | `ctest -R widget_grid --output-on-failure` | Existing: placementsChanged signal tests verify single emit per operation |
| EDIT-10 | No-space feedback | unit | `ctest -R widget_grid --output-on-failure` | Needs: canPlace returns false when grid full |

### Sampling Rate
- **Per task commit:** `cd build && ctest -R widget_grid -j$(nproc) --output-on-failure`
- **Per wave merge:** `cd build && ctest -j$(nproc) --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_widget_grid_model.cpp` -- add `testFindFirstAvailableCell` (if helper added to C++)
- [ ] `tests/test_widget_grid_model.cpp` -- add `testMinMaxConstraintRoles` (if constraint roles added)

Note: Most edit mode requirements (EDIT-01, 02, 07, 08) are QML-only state management that can only be verified manually on the device. The C++ model operations backing EDIT-03, 04, 05, 06, 09 are already well-tested (20 existing tests).

## Sources

### Primary (HIGH confidence)
- Project source code: `src/ui/WidgetGridModel.hpp/cpp` -- complete occupancy, move, resize, remove API
- Project source code: `qml/applications/home/HomeMenu.qml` -- current grid rendering + context menu
- Project source code: `src/ui/PluginModel.hpp` -- `activePluginFullscreen` property for EDIT-08
- Project source code: `src/core/aa/EvdevTouchReader.hpp` -- grab/ungrab lifecycle
- Project source code: `qml/applications/settings/EqSettings.qml` -- existing `MouseArea` `drag.target` pattern
- [Qt 6 DragHandler docs](https://doc.qt.io/qt-6/qml-qtquick-draghandler.html) -- confirmed Qt 6.4+ availability
- [Qt 6 MouseArea docs](https://doc.qt.io/qt-6/qml-qtquick-mousearea.html) -- drag.target, drag.axis properties

### Secondary (MEDIUM confidence)
- [Qt Wiki: Drag and Drop within a GridView](https://wiki.qt.io/Drag_and_Drop_within_a_GridView) -- community pattern for grid reordering
- [Raymii.org: QML Drag and Drop with C++ model](https://raymii.org/s/tutorials/Qml_Drag_and_Drop_example_including_reordering_the_Cpp_Model.html) -- reorder pattern reference

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, all Qt Quick built-ins
- Architecture: HIGH -- existing C++ model is complete, this is QML-layer work
- Pitfalls: HIGH -- most are from direct codebase analysis (binding conflicts, coordinate spaces, EVIOCGRAB lifecycle)
- Drag implementation: HIGH -- MouseArea+drag.target verified in Qt 6.4 docs and used in project

**Research date:** 2026-03-12
**Valid until:** 2026-04-12 (stable -- no external dependency changes expected)
