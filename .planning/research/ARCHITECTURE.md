# Architecture Patterns

**Domain:** Android-style per-widget interactions for Qt 6.8 + QML homescreen (v0.6.6)
**Researched:** 2026-03-21
**Confidence:** HIGH (direct codebase analysis of all relevant components)

## Recommended Architecture

Replace the current global edit mode (enter edit mode -> all widgets editable -> FABs for add/delete/page) with a per-widget interaction model (long-press one widget -> that widget is "selected" -> navbar transforms to show actions -> edge resize handles on selected widget -> long-press empty space for add/page menu). This is an **interaction model change**, not an architecture rewrite. The existing data layer (WidgetGridModel, WidgetRegistry, WidgetPickerModel) stays intact. Changes are concentrated in QML layout and NavbarController state.

### System State Machine

The homescreen operates in three mutually exclusive states:

```
IDLE  ──long-press widget──>  WIDGET_SELECTED
IDLE  ──long-press empty──>   CONTEXT_MENU
WIDGET_SELECTED ──tap empty / tap navbar-center / timeout──>  IDLE
WIDGET_SELECTED ──begin drag──>  WIDGET_DRAGGING (sub-state)
WIDGET_SELECTED ──begin resize──>  WIDGET_RESIZING (sub-state)
CONTEXT_MENU ──select action / tap outside──>  IDLE
```

Key difference from current architecture: **no global `editMode` boolean as UI state**. The "selected widget" IS the state. `selectedInstanceId === ""` means IDLE. WidgetGridModel.setEditMode() is retained purely as a remap-deferral gate.

### Component Boundaries

| Component | Responsibility | Status | Communicates With |
|-----------|---------------|--------|-------------------|
| **HomeMenu.qml** | Grid rendering, widget delegates, drag/resize overlays, context menu, long-press detection | MODIFY heavily | WidgetGridModel, NavbarController, WidgetPickerModel |
| **WidgetGridModel (C++)** | Grid state, placement CRUD, collision detection | MODIFY lightly (add `resizeFromEdge`) | HomeMenu (via model roles), NavbarController (via properties) |
| **NavbarController (C++)** | Gesture dispatch, zone registration, control role mapping, popup management | MODIFY moderately (add selection state, contextual dispatch) | Navbar.qml, HomeMenu.qml (via signals/properties) |
| **Navbar.qml** | 3-control display with icons/clock, popup sliders, power menu | MODIFY moderately (conditional icons) | NavbarController |
| **NavbarControl.qml** | Individual control rendering (icon or clock) | MODIFY to support transformed state | NavbarController |
| **WidgetConfigSheet.qml** | Schema-driven config editor (bottom sheet) | KEEP as-is | WidgetGridModel |
| **WidgetPickerModel (C++)** | Filter registry by available space | KEEP as-is | BottomSheetPicker |
| **TouchRouter / EvdevCoordBridge** | Evdev zone dispatch under EVIOCGRAB | NO CHANGE | NavbarController |
| **ActionRegistry** | Named action dispatch | MINOR (register new actions) | NavbarController |
| **Shell.qml** | App shell layout | NO CHANGE | -- |

### New Components

| Component | Type | Purpose |
|-----------|------|---------|
| **BottomSheetPicker.qml** | QML Component | Categorized scrollable widget picker as a proper bottom sheet with drag-to-dismiss. Replaces current `pickerOverlay`. |
| **ContextMenu.qml** | QML Component | Small popup menu for long-press-empty (Add Widget / Add Page). Positioned at press location. |

### Components NOT Needed

| Rejected Component | Rationale |
|-------------------|-----------|
| **WidgetInteractionController (C++)** | Selection is a QML presentation concern. Adding C++ adds signal relay overhead for zero benefit. `selectedInstanceId` lives in HomeMenu.qml. NavbarController gets `selectedWidgetId` only for action dispatch context. |
| **ResizeHandle.qml (extracted)** | Four edge handles are simple enough to inline in the delegate. Extracting to a separate file adds file management overhead for ~20 lines of QML per handle. If they get complex, extract later. |

## Integration Analysis

### 1. Per-Widget Long-Press: Selection Model

**Current:** Long-press any widget OR background -> enters global `editMode`. All widgets show borders, badges, resize handles.

**New:** Long-press a specific widget -> selects THAT widget. Only selected widget shows resize handles.

**Integration point -- HomeMenu.qml:**
- Replace `property bool editMode` with `property string selectedInstanceId: ""`
- The `widgetMouseArea` `onPressAndHold` sets `selectedInstanceId = model.instanceId` instead of `editMode = true`
- Widget delegate conditionally shows selection UI based on `model.instanceId === homeScreen.selectedInstanceId`
- Drag initiation remains similar but only for the selected widget

**Integration point -- WidgetGridModel.setEditMode():**
- Still needed to gate dimension remap during drag/resize operations
- Call `setEditMode(true)` when `selectedInstanceId` becomes non-empty, `setEditMode(false)` when cleared
- Semantics unchanged -- it prevents remap during manipulation, not "visual edit mode"

**Why QML state, not C++ controller:** The selection state is purely a UI interaction concern. It drives QML visual changes (which widget shows handles, navbar appearance). WidgetGridModel doesn't need to know WHICH widget is selected -- it only needs to know whether any manipulation is happening (editMode). Adding a C++ WidgetInteractionController would add indirection without benefit. HomeMenu already manages drag/resize state in QML properties. `selectedInstanceId` is the same pattern.

### 2. Navbar Control Transformation

**Current:** Navbar always shows volume / clock / brightness. Gestures always dispatch volume/brightness/clock actions.

**New:** When a widget is selected, the driver and passenger controls transform:
- Driver-side control: Settings gear icon (opens config sheet for selected widget)
- Passenger-side control: Delete/trash icon (removes selected widget)
- Center control: Unchanged (clock, tap acts as "done" / deselect)

**Integration point -- NavbarController (C++):**

New properties:
```cpp
Q_PROPERTY(QString selectedWidgetId READ selectedWidgetId WRITE setSelectedWidgetId NOTIFY selectedWidgetChanged)
Q_PROPERTY(bool widgetInteractionMode READ widgetInteractionMode NOTIFY selectedWidgetChanged)
Q_PROPERTY(bool selectedWidgetHasConfig READ selectedWidgetHasConfig NOTIFY selectedWidgetChanged)
Q_PROPERTY(bool selectedWidgetIsSingleton READ selectedWidgetIsSingleton NOTIFY selectedWidgetChanged)
```

- `widgetInteractionMode` is derived: `return !selectedWidgetId_.isEmpty()`
- `selectedWidgetHasConfig` looks up the widget in WidgetGridModel/WidgetRegistry (needs registry pointer or model pointer)
- `selectedWidgetIsSingleton` same lookup -- singletons can't be deleted, so the delete control shows disabled

Action dispatch change in `dispatchAction()`:
```cpp
if (widgetInteractionMode()) {
    if (controlIndex == 0) actionId = "widget.settings";
    else if (controlIndex == 2) actionId = "widget.delete";
    else if (controlIndex == 1 && gesture == Tap) actionId = "widget.deselect";
    else return;  // suppress short/long hold during widget mode
} else {
    // existing logic unchanged
}
```

**Integration point -- Navbar.qml:**
- `control0Icon` and `control2Icon` conditionally switch:
  - Driver: `NavbarController.widgetInteractionMode ? "\ue8b8" : volumeIcon` (gear)
  - Passenger: `NavbarController.widgetInteractionMode ? "\ue872" : brightnessIcon` (delete)
- Guard: popup sliders should NOT show during widget interaction mode
- Optional: center clock area shows "Done" text or checkmark hint

**Integration point -- NavbarControl.qml:**
- Mostly unchanged -- it already shows `iconText` from its parent. The icon switch happens in Navbar.qml's property bindings.
- One change: suppress hold progress feedback during widget interaction mode (taps are instant actions, not hold-to-reveal)

**Integration point -- ActionRegistry:**
- Register `widget.settings`, `widget.delete`, `widget.deselect` handlers
- Handlers connect back to HomeMenu (via signals on NavbarController that HomeMenu listens to, or via global function calls)

**Data flow for navbar actions:**
```
User taps navbar control
  -> NavbarController.handleRelease() -> gestureTriggered signal
  -> NavbarController.dispatchAction() checks widgetInteractionMode
  -> ActionRegistry.dispatch("widget.delete")
  -> Handler emits NavbarController signal (e.g., widgetDeleteRequested)
  -> HomeMenu.qml Connections block catches signal
  -> WidgetGridModel.removeWidget(selectedInstanceId)
  -> homeScreen.deselectWidget()
```

### 3. Four-Edge Resize Handles

**Current:** Single resize handle at bottom-right corner of every widget in edit mode. Drag changes colSpan/rowSpan.

**New:** Four edge handles (top, bottom, left, right) on the selected widget only. Each handle resizes in one axis, anchored from the opposite edge.

**Integration point -- HomeMenu.qml widget delegate:**
- Remove the single `resizeHandle` Rectangle+MouseArea
- Add four handles positioned at each edge of `innerContent`, visible only when `model.instanceId === homeScreen.selectedInstanceId`
- Each handle constrains drag to one axis:
  - **Top handle:** Changes row position (moves top edge up/down), adjusts rowSpan inversely
  - **Bottom handle:** Changes rowSpan (grows/shrinks downward)
  - **Left handle:** Changes column position + colSpan inversely
  - **Right handle:** Changes colSpan (grows/shrinks rightward)

**Integration point -- WidgetGridModel (C++):**

New method (recommended over sequential moveWidget + resizeWidget):
```cpp
Q_INVOKABLE bool resizeFromEdge(const QString& instanceId,
                                 const QString& edge, int delta);
```

This atomically adjusts position + span for top/left edges, or just span for bottom/right edges. One method, one occupancy rebuild, one promoteToBase. Avoids race condition where intermediate state (after move but before resize) could be invalid.

Implementation sketch:
```cpp
bool WidgetGridModel::resizeFromEdge(const QString& instanceId,
                                      const QString& edge, int delta)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return false;
    auto p = livePlacements_[idx];  // copy for validation

    if (edge == "top") {
        p.row += delta;
        p.rowSpan -= delta;
    } else if (edge == "bottom") {
        p.rowSpan += delta;
    } else if (edge == "left") {
        p.col += delta;
        p.colSpan -= delta;
    } else if (edge == "right") {
        p.colSpan += delta;
    }

    // Validate: bounds, min/max constraints, collision
    if (!validatePlacement(p, instanceId)) return false;

    livePlacements_[idx] = p;
    rebuildOccupancy();
    // Emit appropriate role changes
    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {ColumnRole, RowRole, ColSpanRole, RowSpanRole});
    promoteToBase();
    emit placementsChanged();
    return true;
}
```

**Handle visual design:**
- Thin pill-shaped handle (4px wide, 40% of edge length) centered on each edge
- Color: `ThemeService.primary` at 70% opacity
- Touch target: extend MouseArea margins by `UiMetrics.spacing` for fat-finger tolerance
- Resize ghost (existing `resizeGhost` Rectangle) continues to show during resize drag

### 4. Bottom-Sheet Widget Picker

**Current:** `pickerOverlay` is a simple Item overlay with a fixed-height bottom panel, horizontal ListView.

**New:** Proper bottom sheet with:
- Drag handle at top (visual)
- Categorized vertical list (grouped by WidgetDescriptor.category)
- Widget rows with icon, name, description, size indicator
- Auto-placement on selection (existing logic from pickerOverlay works)
- Slide-up animation from bottom

**Integration point -- HomeMenu.qml:**
- Replace `pickerOverlay` Item with `BottomSheetPicker` component
- Trigger: context menu "Add Widget" action
- On widget selection: call existing `WidgetGridModel.placeWidget()` with `findFirstAvailableCell()`

**BottomSheetPicker.qml design:**
- Follow `WidgetConfigSheet.qml` pattern: `Dialog { modal: true; parent: Overlay.overlay }` with enter/exit `NumberAnimation` transitions
- Vertical `ListView` with section headers by category
- Each widget row: MaterialIcon + name + description text + size badge ("2x2")
- Touch to select -> places widget, closes sheet

### 5. Long-Press Empty Space Context Menu

**Current:** Long-press empty space -> enters global edit mode. Tap empty -> exits edit mode.

**New:** Long-press empty space -> shows small context menu at press coordinates:
- "Add Widget" -> opens bottom sheet picker
- "Add Page" -> calls WidgetGridModel.addPage()

**Integration point -- HomeMenu.qml `pageGridContent` MouseArea:**
- `onPressAndHold`: position and show `ContextMenu` at `(mouse.x, mouse.y)` instead of setting editMode
- Context menu: simple Column of buttons in a floating Rectangle with scrim
- Dismiss on selection or tap-outside

### 6. Auto-Delete Empty Pages

**Current:** Empty page cleanup runs on `exitEditMode()`.

**New:** Runs when `selectedInstanceId` clears (deselect) and when a widget is deleted.

**Integration:** Move cleanup logic from `exitEditMode()` to `function cleanupEmptyPages()` called from deselectWidget() and widget delete handler.

### 7. Removing Global Edit Mode UI

**Components to DELETE from HomeMenu.qml:**
| Component | What It Is | Replacement |
|-----------|-----------|-------------|
| `addPageFab` | FAB for adding pages | Context menu "Add Page" |
| `fab` | FAB for adding widgets | Context menu "Add Widget" -> bottom sheet |
| `deletePageFab` | FAB for deleting current page | Auto-cleanup + context menu |
| `inactivityTimer` | 10s global edit timeout | `selectionTimer` (5-7s per-widget) |
| `removeBadge` (per widget) | X button badge | Navbar delete control |
| `configGear` (per widget) | Gear badge | Navbar settings control |
| Dotted grid lines | Full-page grid overlay | Optional: only around selected widget |
| Accent border on ALL widgets | Global edit mode indicator | Only on selected widget |

**Components to KEEP:**
| Component | Why |
|-----------|-----|
| `dragOverlay` MouseArea (z:200) | Still needed for drag capture |
| `dropHighlight` Rectangle | Still needed for drop target preview |
| `dragPlaceholder` Rectangle | Still needed for original position indicator |
| `resizeGhost` Rectangle | Still needed for resize preview |
| `deletePageDialog` | May still need confirmation for pages with widgets |

## Data Flow Changes

### New Flow: Select Widget
```
Long-press specific widget
  -> homeScreen.selectedInstanceId = model.instanceId
  -> WidgetGridModel.setEditMode(true)  // gates remap
  -> NavbarController.selectedWidgetId = instanceId
  -> Navbar transforms controls (gear/delete icons)
  -> Selected widget shows edge resize handles + selection highlight
  -> selectionTimer starts (5-7s)

Tap navbar settings (driver side)
  -> ActionRegistry.dispatch("widget.settings")
  -> NavbarController emits widgetSettingsRequested(instanceId)
  -> HomeMenu: configSheet.openConfig(selectedInstanceId, ...)
  -> selectionTimer pauses while config sheet is open

Tap navbar delete (passenger side)
  -> ActionRegistry.dispatch("widget.delete")
  -> NavbarController emits widgetDeleteRequested(instanceId)
  -> HomeMenu: WidgetGridModel.removeWidget(selectedInstanceId)
  -> homeScreen.deselectWidget()
  -> cleanupEmptyPages()

Tap navbar center (clock / "done")
  -> ActionRegistry.dispatch("widget.deselect")
  -> NavbarController emits widgetDeselectRequested()
  -> homeScreen.deselectWidget()
```

### New Flow: Empty Space Context Menu
```
Long-press empty grid cell
  -> Show ContextMenu at (mouse.x, mouse.y)
  -> "Add Widget" -> open BottomSheetPicker
  -> "Add Page" -> WidgetGridModel.addPage()
  -> Tap outside -> dismiss menu
```

## Patterns to Follow

### Pattern 1: Contextual Navbar Actions (existing pattern)
**What:** NavbarController already maps control index + gesture to named actions via ActionRegistry. The `dispatchAction()` method resolves `controlRole()` and builds action IDs like `navbar.volume.tap`.
**When:** Widget interaction mode is a second dimension in the same dispatch logic.
**Example:**
```cpp
void NavbarController::dispatchAction(int controlIndex, int gesture) {
    if (widgetInteractionMode()) {
        QString actionId;
        if (controlIndex == 0) actionId = "widget.settings";
        else if (controlIndex == 2) actionId = "widget.delete";
        else if (controlIndex == 1 && gesture == Tap) actionId = "widget.deselect";
        else return;
        actionRegistry_->dispatch(actionId);
    } else {
        // existing logic
        QString role = controlRole(controlIndex);
        QString actionId = QStringLiteral("navbar.%1.%2").arg(role, gestureStr);
        actionRegistry_->dispatch(actionId);
    }
}
```

### Pattern 2: Popup Region Registration NOT Needed
**What:** QML popups report interactive regions to NavbarController for evdev zones under EVIOCGRAB.
**When this applies:** Only during AA mode when EVIOCGRAB is active.
**Why NOT needed here:** Widget management is a home-screen-only activity. AA is fullscreen and auto-deselects any widget. EVIOCGRAB is NOT engaged when on the homescreen. Standard QML MouseAreas work for the context menu, bottom sheet picker, and resize handles. No new evdev zones needed.

### Pattern 3: Selection Timeout (adapt inactivity timer)
**What:** 5-7s timeout deselects the widget. Timer resets on any interaction (drag start, resize drag, config sheet open).
**Key behavior:** Timer PAUSES while config sheet or picker is visible (check `configSheet.visible || bottomSheetPicker.visible`).
```qml
Timer {
    id: selectionTimer
    interval: 7000
    running: homeScreen.selectedInstanceId !== "" && !configSheet.visible && !bottomSheetPicker.visible
    onTriggered: homeScreen.deselectWidget()
}
```

### Pattern 4: Bottom Sheet as Dialog (existing pattern)
**What:** WidgetConfigSheet.qml uses `Dialog { modal: true; parent: Overlay.overlay }` with `enter`/`exit` NumberAnimation transitions for slide-up behavior.
**When:** BottomSheetPicker should follow the exact same pattern. Don't invent a new bottom sheet abstraction.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Dual Edit Mode + Selection State
**What:** Keeping `editMode` AND `selectedInstanceId` as separate independent boolean/string state.
**Why bad:** Two sources of truth for "are we editing?" leads to inconsistent UI states (edit mode on but nothing selected, or widget selected but edit mode off). Every guard condition doubles in complexity.
**Instead:** `selectedInstanceId` IS the state. `WidgetGridModel.setEditMode()` is a derived gate set mechanically when selection changes -- NOT independent UI state.

### Anti-Pattern 2: C++ Controller for Pure UI State
**What:** Creating a `WidgetInteractionController` C++ class to hold selection state.
**Why bad:** Selection is a QML presentation concern. WidgetGridModel doesn't need to know which widget is visually selected. Adding C++ adds a signal relay layer and makes it harder to iterate on interaction feel.
**Instead:** `selectedInstanceId` lives in HomeMenu.qml. NavbarController gets `selectedWidgetId` only because it needs it for action dispatch context (and it's a simple string property set from QML).

### Anti-Pattern 3: Mouse Events in Resize Handles During AA
**What:** Trying to make resize handles work under EVIOCGRAB via evdev zones.
**Why bad:** Widget manipulation is a home-screen activity. AA is fullscreen. The two never overlap. Adding evdev zones for resize handles adds complexity for zero value.
**Instead:** Resize handles use standard QML MouseAreas. They're only visible when AA is not active. AA fullscreen auto-deselects (like current AA auto-exits edit mode).

### Anti-Pattern 4: Sequential Move + Resize for Edge Handles
**What:** QML calling `moveWidget()` then `resizeWidget()` for top/left edge drag.
**Why bad:** The intermediate state (after move, before resize) may fail collision detection. Two separate model mutations, two occupancy rebuilds, two dataChanged emissions.
**Instead:** Single `resizeFromEdge()` method validates and applies the combined position+span change atomically.

## Suggested Build Order

Build order follows dependency chains. Each phase produces a testable intermediate state.

### Phase 1: Selection Model Foundation
**What:** Replace global `editMode` with `selectedInstanceId`. Remove FABs, badges, global edit UI. Wire selection to WidgetGridModel.setEditMode().
**Why first:** Everything else depends on the selection state machine.
**Scope:**
- HomeMenu.qml: replace `editMode` property with `selectedInstanceId`, add `deselectWidget()` function
- Remove: `addPageFab`, `fab`, `deletePageFab`, `removeBadge`, `configGear`, `inactivityTimer`, dotted grid lines (all widgets)
- Add: `selectionTimer`, selected widget gets accent border + slight scale bump
- Long-press widget -> select it. Tap empty -> deselect. Timeout -> deselect.
- WidgetGridModel: `setEditMode(true)` when selected, `setEditMode(false)` when deselected
- AA fullscreen auto-deselect (replaces auto-exit edit mode)
- Drag still works: select -> long-press again initiates drag (current pattern)
**Testable:** Long-press a widget, it highlights. Tap away, it unhighlights. Drag works for selected widget. No FABs, no badges.

### Phase 2: Navbar Transformation
**What:** NavbarController gains selection awareness and contextual action dispatch. Navbar.qml switches icons.
**Why second:** With selection working, navbar can react to it.
**Scope:**
- NavbarController.hpp/cpp: add `selectedWidgetId`, `widgetInteractionMode`, `selectedWidgetHasConfig`, `selectedWidgetIsSingleton` properties
- NavbarController: add `widgetSettingsRequested`, `widgetDeleteRequested`, `widgetDeselectRequested` signals
- NavbarController::dispatchAction(): branch on widgetInteractionMode
- ActionRegistry: register `widget.settings`, `widget.delete`, `widget.deselect` handlers that emit the signals
- Navbar.qml: conditional icons (gear/delete vs volume/brightness)
- NavbarControl.qml: suppress hold progress during widget interaction mode
- HomeMenu.qml: Connections block on NavbarController for widget actions
- Guard popup sliders: don't show during widget interaction mode
**Testable:** Select widget -> navbar shows gear and trash. Tap trash -> widget removed, navbar reverts. Tap gear -> config sheet opens. Tap clock -> deselects.

### Phase 3: Edge Resize Handles
**What:** Replace single bottom-right resize handle with four edge handles on selected widget.
**Why third:** Independent of navbar transformation. Needs selection model from Phase 1.
**Scope:**
- WidgetGridModel: add `resizeFromEdge(instanceId, edge, delta)` method
- HomeMenu.qml: remove old single `resizeHandle`, add four edge handles on selected widget delegate
- Each handle: axis-constrained drag, ghost preview, snap-to-grid, min/max enforcement
- Top/left handles call `resizeFromEdge()`. Bottom/right call existing `resizeWidget()`.
- Unit test for `resizeFromEdge()`
**Testable:** Select widget -> four edge handles appear. Drag right edge -> widget grows right. Drag top edge -> widget grows upward.

### Phase 4: Bottom-Sheet Widget Picker
**What:** Replace `pickerOverlay` with categorized bottom-sheet picker.
**Why fourth:** Independent of resize. Needs triggering mechanism.
**Scope:**
- New `BottomSheetPicker.qml` following WidgetConfigSheet pattern
- Vertical ListView with section headers by category
- Widget rows: icon + name + description + size badge
- On selection: existing `placeWidget()` + `findFirstAvailableCell()` logic
- Remove old `pickerOverlay` from HomeMenu.qml
**Testable:** Open picker -> slides up. Browse categories. Tap widget -> placed on grid, picker closes.

### Phase 5: Context Menu + Page Auto-Management
**What:** Long-press empty space shows context menu. Auto-delete empty pages.
**Why last:** Least critical path. Depends on picker (Phase 4).
**Scope:**
- Inline `ContextMenu` in HomeMenu.qml: small floating Rectangle at press coords
- "Add Widget" -> opens BottomSheetPicker. "Add Page" -> WidgetGridModel.addPage()
- `cleanupEmptyPages()` called on deselect and widget delete
- `deletePageDialog` retained for explicit page deletion with widgets
- HomeMenu.qml `pageGridContent` MouseArea: `onPressAndHold` shows context menu
**Testable:** Long-press empty cell -> menu appears. "Add Widget" -> picker. "Add Page" -> new page. Delete last widget -> page auto-removed on deselect.

## Scalability Considerations

| Concern | Current | After v0.6.6 |
|---------|---------|--------------|
| Widget count per page | ~10-15 typical, occupancy O(cols*rows) | Unchanged -- selection doesn't change data model |
| Touch responsiveness | Direct evdev in AA, QML on homescreen | Unchanged -- no new evdev zones for homescreen interactions |
| Navbar state transitions | Static 3 controls | +1 string property binding -- negligible |
| Memory | Widget QML instances per visible page | Unchanged -- no new per-widget allocations |
| Page management | Manual via FABs | Auto-cleanup reduces dead pages -- slight improvement |

## Sources

- Existing codebase analysis:
  - `qml/applications/home/HomeMenu.qml` -- current edit mode, drag, resize, FABs, picker overlay
  - `src/ui/WidgetGridModel.hpp/cpp` -- grid model, CRUD, occupancy, page management, editMode gate
  - `src/ui/NavbarController.hpp/cpp` -- gesture dispatch, zone registration, popup management, action dispatch
  - `qml/components/Navbar.qml` -- control layout, icon bindings, popup management, zone registration
  - `qml/components/NavbarControl.qml` -- individual control rendering, hold progress
  - `qml/components/NavbarPopup.qml` -- popup slider pattern, evdev zone reporting
  - `qml/components/WidgetConfigSheet.qml` -- bottom sheet Dialog pattern (model for picker)
  - `qml/components/Shell.qml` -- app shell structure
  - `src/core/aa/TouchRouter.hpp` -- zone-based touch dispatch
  - `src/core/aa/EvdevCoordBridge.hpp` -- pixel-to-evdev coordinate bridge
  - `src/core/services/ActionRegistry.hpp` -- named action dispatch
  - `src/ui/WidgetPickerModel.hpp` -- picker model (unchanged)
- `.planning/PROJECT.md` -- v0.6.6 milestone specification
- `CLAUDE.md` -- architectural context, gotchas, touch routing constraints
