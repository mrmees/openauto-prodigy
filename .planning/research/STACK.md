# Technology Stack

**Project:** OpenAuto Prodigy v0.6.6 — Homescreen Layout & Widget Settings Rework
**Researched:** 2026-03-21
**Confidence:** HIGH — all findings from direct codebase inspection of existing patterns

---

## Stack Verdict: No New Dependencies

Every feature in this milestone is implementable with existing Qt 6.8 QML primitives already used throughout the codebase. This is a UI interaction rework, not a technology expansion. Zero new libraries, zero new Qt modules, zero new build dependencies.

---

## Locked Stack (Unchanged)

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| Qt 6.8 | 6.8.2 | UI framework, QML engine | Locked |
| C++17 | gcc 12+ | Core logic, models, services | Locked |
| CMake | 3.22+ | Build system | Locked |

### Relevant Qt Modules (Already Linked)
| Module | Role in This Milestone |
|--------|----------------------|
| `Qt6::Quick` | All QML rendering — Item, MouseArea, Rectangle, NumberAnimation |
| `Qt6::QuickControls2` | Dialog, Popup, Overlay (WidgetConfigSheet pattern) |
| `Qt6::QuickLayouts` | RowLayout, ColumnLayout for bottom sheet and menu content |

---

## Feature-by-Feature Stack Analysis

### 1. Per-Widget Long-Press Lift + Drag + Select

**No new primitives.** The existing `HomeMenu.qml` already implements long-press-to-drag with scale/opacity feedback, snap-back animation, and drop highlight. The change is behavioral: long-press selects a widget (showing resize handles + navbar transformation) rather than entering global edit mode.

| QML Primitive | Purpose | Already Used In |
|---------------|---------|-----------------|
| `MouseArea.pressAndHold` | Detect long-press | HomeMenu delegate (`pressAndHoldInterval: 500`) |
| `NumberAnimation` on `scale` | Lift animation (1.0 to 1.05) | Drag state in HomeMenu |
| `Behavior on opacity` | Dim non-selected widgets | Drag state already does `opacity = 0.5` |
| `z` property | Lift selected above siblings | Drag delegate already uses `z: 100` |

**Key behavioral change:** Currently long-press enters global `editMode` boolean. New model: long-press sets `selectedInstanceId` on `WidgetGridModel`. No global edit mode toggle — each widget is individually selectable. Tap-elsewhere deselects.

### 2. Navbar Control Transformation

**No new primitives.** Navbar already uses QML `states` for edge positioning (top/bottom/left/right) with `PropertyChanges`. Adding a "widget interaction" state follows the exact same pattern.

| QML Primitive | Purpose | Already Used In |
|---------------|---------|-----------------|
| `State` + `PropertyChanges` | Swap icon text and click handlers | Navbar edge states (4 states) |
| `Transition` + `NumberAnimation` | Crossfade during icon swap | Theme color transitions throughout |
| C++ `Q_PROPERTY(bool)` | `widgetInteractionMode` on NavbarController | `popupVisible`, `edge` properties |

**Approach:** Add `widgetInteractionMode` bool property to `NavbarController`. Navbar.qml adds a state:
- Control 0 (driver side): icon becomes settings gear, tap opens WidgetConfigSheet for selected widget
- Control 2 (passenger side): icon becomes delete, tap calls `removeWidget(selectedId)`
- Control 1 (center): shows "Done" text or keeps clock — tap deselects

**Signal flow:**
```
WidgetGridModel.selectedInstanceId changes
  -> NavbarController observes (connect in main.cpp or QML Connections)
  -> NavbarController.widgetInteractionMode = (id != "")
  -> Navbar.qml State activates -> icons swap
```

### 3. 4-Edge Resize Handles

**No new primitives.** A single bottom-right resize handle with `MouseArea` + `preventStealing` already exists. Extending to 4 edge handles is copy-paste of the same pattern with axis-constrained drag math.

| QML Primitive | Purpose | Already Used In |
|---------------|---------|-----------------|
| `Rectangle` | Handle visual | `resizeHandle` (bottom-right corner) |
| `MouseArea` with `preventStealing: true` | Drag detection | Resize handle MouseArea |
| `mapToItem` | Coordinate transforms | Resize ghost positioning |

**Math change per handle:**
- Left edge: only changes column position and colSpan (row/rowSpan frozen)
- Right edge: only changes colSpan (column/row/rowSpan frozen) — current behavior
- Top edge: only changes row position and rowSpan (col/colSpan frozen)
- Bottom edge: only changes rowSpan (column/row/colSpan frozen) — current behavior minus col

**Handle visibility:** Only on the selected widget (when `selectedInstanceId === model.instanceId`), not on all widgets simultaneously like current edit mode.

### 4. Bottom-Sheet Widget Picker

**No new primitives.** `WidgetConfigSheet.qml` already implements a full bottom-sheet pattern with:
- Scrim overlay
- Rounded-top-corner panel
- Slide-up/down `NumberAnimation` on `y`
- Scrollable content via `Flickable`

The current picker is a horizontal `ListView` in a simple overlay. Replace with the WidgetConfigSheet bottom-sheet pattern but using `WidgetPickerModel` as the data source, organized by category with `SectionHeader` components.

| QML Primitive | Purpose | Already Used In |
|---------------|---------|-----------------|
| `Item` with y-animation | Slide-up panel | WidgetConfigSheet Dialog transitions |
| `Rectangle` as scrim | Modal dim | pickerOverlay, deletePageDialog |
| `Flickable` | Vertical scroll | WidgetConfigSheet contentItem |
| `ListView` / `Repeater` | Widget list | Current picker, settings pages |
| `SectionHeader` | Category headers | Settings pages |

**Why not Qt Quick Controls `Drawer`:** `Drawer` handles edge-swipe detection which is unwanted (picker opens via button/menu, not swipe). Manual bottom-sheet matches existing WidgetConfigSheet pattern.

### 5. Long-Press Empty Space Menu

**No new primitives.** The power menu popup in `Navbar.qml` implements exactly this pattern: a `Column` of `ElevatedButton`s inside a positioned `Rectangle`, with scrim dismiss.

| QML Primitive | Purpose | Already Used In |
|---------------|---------|-----------------|
| `MouseArea.pressAndHold` | Detect empty-space long-press | Background MouseArea in page delegate |
| `Rectangle` + `Column` | Popup menu container | Power menu in Navbar.qml |
| `mapToItem` | Position at press location | Power menu positioning |
| `MouseArea` scrim | Tap-outside dismiss | Power menu dismiss overlay |

**Menu items:** "Add Widget" (opens bottom-sheet picker) and "Add Page" (calls `WidgetGridModel.addPage()`).

### 6. Auto-Delete Empty Pages

**Already implemented.** `exitEditMode()` in HomeMenu.qml already iterates pages in reverse and removes empty ones. The change is just calling this cleanup at the right time (on widget removal, on deselect) instead of only on edit-mode exit.

---

## C++ Changes Needed (Additions to Existing Classes)

### WidgetGridModel — New Selection State

```cpp
// New property
Q_PROPERTY(QString selectedInstanceId READ selectedInstanceId
           WRITE setSelectedInstanceId NOTIFY selectedChanged)

// Convenience accessors for navbar to query selected widget metadata
Q_INVOKABLE bool selectedHasConfigSchema() const;
Q_INVOKABLE bool selectedIsSingleton() const;
Q_INVOKABLE QString selectedDisplayName() const;
Q_INVOKABLE QString selectedIconName() const;
```

This is ~30 lines of trivial getter/setter code. No new data structures.

### NavbarController — Widget Interaction Mode

```cpp
Q_PROPERTY(bool widgetInteractionMode READ widgetInteractionMode
           WRITE setWidgetInteractionMode NOTIFY widgetInteractionModeChanged)
```

Single bool property with getter/setter/signal. ~15 lines.

### No New Classes

No new C++ class, model, service, or controller is warranted. The selection concept lives on `WidgetGridModel` (it owns widget state), and the navbar mode flag lives on `NavbarController` (it owns navbar state). Communication is via signal/slot or QML Connections.

---

## Animation Specifications

All use existing Qt 6.8 QML animation types. No custom C++ animation code.

| Animation | Type | Duration | Easing | Precedent |
|-----------|------|----------|--------|-----------|
| Widget lift (select) | `Behavior on scale` | 150ms | OutCubic | Drag lift |
| Widget drop (deselect) | `NumberAnimation on scale` | 200ms | OutCubic | Snap-back |
| Non-selected dim | `Behavior on opacity` | 200ms | Linear | Drag opacity |
| Navbar icon swap | `Transition { NumberAnimation on opacity }` | 150ms | Linear | Theme transitions |
| Bottom sheet slide-in | `NumberAnimation on y` | 250ms | OutCubic | WidgetConfigSheet |
| Bottom sheet slide-out | `NumberAnimation on y` | 200ms | InCubic | WidgetConfigSheet |
| Resize handle appear | `NumberAnimation on opacity` | 100ms | Linear | Edit mode overlays |
| Context menu appear | Instant | 0ms | N/A | Power menu |

---

## What to Remove

This milestone explicitly removes UI elements. No new dependencies replace them.

| Removed | Reason | Replacement |
|---------|--------|-------------|
| Global `editMode` boolean | Per-widget selection replaces it | `selectedInstanceId` on model |
| FABs (add widget, add page, delete page) | Cluttered, tiny touch targets | Bottom sheet picker + navbar actions + context menu |
| X-badge remove buttons | Hard to tap at car speed | Navbar delete control |
| Gear-badge config buttons | Hard to tap, visually noisy | Navbar settings control |
| 10s inactivity timer | No global mode to time out | Selection clears on tap-elsewhere |
| Edit-mode dotted grid lines | No global mode to show them in | Possibly show around selected widget only |

---

## What NOT to Add

| Technology | Why Not |
|------------|---------|
| `DragHandler` / `PointHandler` / `TapHandler` | Qt Quick Input Handlers conflict with existing `MouseArea` + evdev architecture. Mixing grab types causes touch steal issues (documented: TapHandler overlay stole all touch from child controls in v0.6 SettingsInputBoundary work) |
| `MultiEffect` / `Qt5Compat.GraphicalEffects` | Pi 4 GPU freezes with multiple effect layers (FullScreenPicker fix, v0.6.2). No drop shadows on lifted widgets |
| `SwipeDelegate` | Swipe-to-delete is not the interaction model |
| `Drawer` (QtQuick.Controls) | Edge-swipe trigger is wrong UX for button-triggered picker |
| `SpringAnimation` / `SmoothedAnimation` | Over-engineered; `NumberAnimation` + `Easing.OutCubic` matches existing patterns |
| Any haptic/vibration library | No haptic hardware on target Pi |
| `QtQuick.Particles` | Tempting for lift effect, terrible on Pi GPU |
| New npm/pip/cargo dependency | Pure Qt/QML work |

---

## Touch Routing Considerations

**Home screen uses QML MouseArea, not evdev TouchRouter.** When viewing the home screen, EVIOCGRAB is not active (only during AA projection). All long-press, drag, and menu interactions go through standard QML touch handling.

**During AA, home screen is invisible.** The `onActivePluginChanged` connection already handles cleanup when AA activates. No touch routing changes needed.

**The existing `dragOverlay` MouseArea at z:200** remains the correct pattern for capturing drag movement across the full screen during widget repositioning. No changes needed to this mechanism.

---

## Installation

No changes. Existing build command works:

```bash
# Dev VM
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH=/opt/qt/6.8.2/gcc_64 ..
make -j$(nproc)

# Cross-compile for Pi
./cross-build.sh
```

---

## Confidence Assessment

| Area | Confidence | Reason |
|------|------------|--------|
| No new dependencies | HIGH | Every feature maps to QML primitives already used in this codebase |
| QML animation approach | HIGH | All proposed animations replicate existing patterns |
| C++ model additions | HIGH | ~45 lines of trivial property code on existing classes |
| Navbar transformation | HIGH | State-based icon swap matches existing 4-state edge pattern |
| Bottom sheet pattern | HIGH | WidgetConfigSheet already implements this exact pattern |
| Touch routing unchanged | HIGH | Home screen uses QML MouseArea; AA auto-exits interaction state |
| Removal of global edit mode | HIGH | Simplification — less code, not more |

---

## Sources

- Direct codebase inspection: `HomeMenu.qml` (widget grid, drag, resize, edit mode, picker, FABs), `Navbar.qml` (control layout, state management, power menu popup), `NavbarControl.qml` (icon display, gesture handling), `WidgetConfigSheet.qml` (bottom sheet pattern), `WidgetGridModel.hpp` (model API surface), `NavbarController.hpp` (property pattern), `TouchRouter.hpp` (touch architecture) -- HIGH confidence (primary source)
- Project CLAUDE.md and MEMORY.md for established patterns, gotchas (TapHandler grab conflict, MultiEffect GPU freeze, EVIOCGRAB behavior) -- HIGH confidence
- Qt 6.8 QML type documentation (MouseArea, NumberAnimation, State, Dialog) -- HIGH confidence (stable APIs since Qt 6.0+)

---

*Stack research for: OpenAuto Prodigy v0.6.6 -- Homescreen Layout & Widget Settings Rework*
*Researched: 2026-03-21*
