# Feature Landscape: Android-Style Widget Management Interactions

**Domain:** Widget management UX for automotive touchscreen (Qt 6.8 / QML)
**Researched:** 2026-03-21
**Overall confidence:** HIGH (based on official Android docs, Pixel Launcher behavior, Android 16 QPR3 Beta 2 changes, and direct codebase inspection of existing Prodigy implementation)

## Context: What Already Exists

This milestone replaces global edit mode with Android-style per-widget interactions. The grid, drag/drop, resize, widget picker, and config sheets all already work. The work here is about interaction model -- how the user engages with individual widgets rather than entering/exiting a global mode.

| Component | Status | v0.6.6 Change |
|-----------|--------|---------------|
| Global edit mode | Long-press anywhere enters mode; all widgets badge simultaneously | **Remove entirely.** Replace with per-widget selection. |
| FABs (add widget, add page, delete page) | Floating circles in edit mode | **Remove.** Replace with long-press-empty-space menu. |
| Badge buttons (X, gear) | 28px circles on every widget in edit mode | **Remove.** Move to navbar transformation. |
| Resize handle | Single bottom-right corner handle | **Expand** to 4-edge handles on selected widget only. |
| Widget picker | Bottom-sheet with categorized horizontal card list | **Keep.** New entry point from empty-space menu. |
| WidgetConfigSheet | Bottom-sheet dialog for per-instance config | **Keep.** New entry point from navbar settings button. |
| Drag-to-reposition | Press-and-hold in edit mode initiates drag | **Refactor.** Drag initiates from per-widget selected state. |
| Inactivity timer (10s) | Exits global edit mode | **Reuse.** Deselects widget instead. |
| Drop highlight + ghost preview | Valid/invalid placement and resize feedback | **Keep as-is.** Already works well. |
| Dotted grid overlay | Shows on all pages in edit mode | **Scope down.** Show only on active page during widget selection. |

---

## Table Stakes

Features users expect from an Android-like widget management experience. Missing any of these makes the interaction feel broken or unfamiliar to anyone who has used an Android phone.

| Feature | Why Expected | Complexity | Dependencies | Notes |
|---------|-------------|------------|--------------|-------|
| **Per-widget long-press to select** | Android's fundamental widget interaction. Long-press ONE widget to enter its management state. No global mode toggle. This is how every Android launcher has worked since Android 3.1. | Medium | Existing long-press detection in HomeMenu delegate MouseArea | Replaces `editMode` bool with `selectedWidgetId` string. Selected widget gets accent border + scale. Others remain normal. Only one widget selected at a time. |
| **Long-press lift feedback** | Visual confirmation the long-press registered. Android lifts the widget (scale ~1.05x, elevated shadow, slight transparency). Users need immediate feedback that their press was recognized. | Low | Existing scale/opacity animation in delegate | Already partially built (scale 1.05, opacity 0.5 during drag). Apply on selection, before drag starts. Accent border on selected widget only -- not all widgets. |
| **Drag to reposition from selected state** | After long-press selects a widget, continued hold + drag moves it. Release to place. Snap-to-grid with drop highlight. This is the core of Android widget management. | Low | Existing drag overlay, finalizeDrop(), snap-back animation | Already fully built. Refactor: drag initiates from per-widget selected state instead of global edit mode. All drag infrastructure (dropHighlight, dragPlaceholder, dragOverlay MouseArea, collision detection) is reusable as-is. |
| **Resize handles on selected widget** | When selected, a widget shows resize affordances on its edges. Android shows an outline with dots on each edge midpoint. User drags an edge to resize in that direction. | Medium | Existing single bottom-right resize handle + resizeGhost | Android uses 4-edge handles (dots at midpoint of top/bottom/left/right edges). Current system has only a bottom-right corner handle. Must expand to 4 edges. Each handle drags in one axis only (top/bottom = vertical, left/right = horizontal). |
| **Remove widget action** | Ability to delete a non-singleton widget. Must be discoverable and require minimal precision. On Android, long-press then drag to "Remove" area at top, or tap a remove option. | Low | Existing `WidgetGridModel.removeWidget()` | Currently a tiny 28px X badge. In a car at arm's length, that's too small. Move to navbar-hosted delete button during widget selection -- full navbar control size. |
| **Widget config access** | Ability to open per-widget configuration for widgets with config schemas (clock style, date format, etc.). | Low | Existing `WidgetConfigSheet` + `configSchemaForWidget()` | Currently a tiny 28px gear badge. Move to navbar-hosted settings button during widget selection. Only appears when selected widget has a config schema. |
| **Tap empty space to deselect** | Tapping anywhere outside the selected widget dismisses the selection state. Universal dismissal pattern on Android and every other touch UI. | Low | Existing background MouseArea in pageGridContent | Change handler from "exit edit mode" to "deselect widget" (set selectedWidgetId to ""). |
| **Widget picker for adding widgets** | Browsable list of available widgets with categories, icons, names. Tap to auto-place. | Low | Existing `WidgetPicker.qml` + `WidgetPickerModel` | Already built with categorized horizontal card rows. Needs new entry point from long-press-empty-space menu instead of FAB. |
| **Auto-placement on add** | Selecting a widget from picker auto-finds first available cell and places it. No drag-to-place. | Low | Existing `findFirstAvailableCell()` + `placeWidget()` | Already implemented. Android 15 added explicit "+ Add" tap-to-place alongside drag-to-place. Tap-to-auto-place is better for automotive. |

---

## Differentiators

Features that go beyond standard Android behavior. Not expected, but improve the experience -- especially for an automotive touchscreen.

| Feature | Value Proposition | Complexity | Dependencies | Notes |
|---------|-------------------|------------|--------------|-------|
| **Navbar control transformation** | When widget is selected, navbar volume/brightness controls morph into settings/delete buttons. Reuses existing touch-friendly navbar hit zones (full control-sized targets). Android puts actions as floating elements near the widget; Prodigy's fixed navbar provides larger, more predictable touch targets. | Medium | `Navbar.qml`, `NavbarControl.qml`, `NavbarController` C++, existing icon resolution logic | Volume becomes settings (gear icon) when selected widget has configSchema; brightness becomes delete (trash icon) for non-singleton widgets. Clock/home stays as "done"/deselect. Reverse transformation on deselect. This is the key automotive differentiation -- no tiny floating badges. |
| **Tap-to-resize buttons (Android 16 QPR3 style)** | Plus/minus buttons on widget edges for tap-to-expand/contract. Android 16 QPR3 Beta 2 (Jan 2026) added these alongside drag handles. Better accessibility -- no precision drag needed. Buttons disappear at min/max size. | Medium | 4-edge resize handle infrastructure | Layer on top of edge handles. Each edge gets +/- buttons. Tap + to grow one cell in that direction, - to shrink. Buttons themed with system colors. Auto-hide at size limits. Complementary to drag -- not a replacement. |
| **Long-press empty space context menu** | Long-pressing empty grid space shows a popup with "Add Widget" and "Add Page". Android shows Widgets/Wallpaper/Settings on empty-space long-press. Replaces FABs with a contextual popup near the touch point. | Low | Existing long-press detection on background MouseArea, existing `addPage()` and picker | Simple popup menu (2 items). Position near touch point. Dismiss on selection or outside tap. Much more discoverable than FABs for new users -- "long-press empty space" is an Android-universal gesture. |
| **Inactivity auto-deselect** | Widget selection auto-dismisses after 10s of no touch interaction. Prevents stuck selected state while driving. Android doesn't have this -- phone users can take their time. Essential for automotive safety. | Low | Existing `inactivityTimer` | Already implemented for global edit mode. Reuse for per-widget selection. Timer resets on any touch interaction with the selected widget. |
| **Grid overlay scoped to selection** | Show dotted grid lines only on the active page, only while a widget is selected. Provides spatial reference for placement/sizing without permanent visual clutter. | Low | Existing dotted grid overlay in pageGridContent | Already built. Change visibility binding from `editMode` to `selectedWidgetId !== ""`. |

---

## Anti-Features

Features to explicitly NOT build. Things that seem logical but would hurt the automotive UX or add unjustified complexity.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **Global edit mode** | Android doesn't have one. It's a desktop/iOS paradigm. Causes all widgets to badge simultaneously, cluttering a small screen. Forces "enter mode, do things, exit mode" workflow. Per-widget interaction is more direct -- one gesture to select and act. | Per-widget long-press to select one widget at a time. |
| **Floating badge buttons (X, gear, resize) on every widget** | 28px circles are too small for arm's-length automotive touch. They overlap widget content. They clutter the view when all widgets badge simultaneously. Android doesn't show actions on ALL widgets -- only the one being interacted with. | Navbar transformation for settings/delete. Edge resize handles on selected widget only. |
| **FABs (floating action buttons)** | Hover over content with no fixed spatial anchor. Hard to target while driving. Occlude widget content. Not how Android handles widget/page addition. | Long-press empty space context menu. Actions surface contextually. |
| **Drag-from-picker-to-homescreen** | Android's traditional flow of dragging from picker to home screen. Too much precision for automotive -- requires holding drag across screen regions. | Auto-placement: tap in picker, widget finds first open spot. Reposition after if needed. |
| **Widget jiggle/wobble animation** | iOS-style wobbling. Adds visual noise, no functional value, distracting while driving. Android doesn't do this. | Static selected state with clear accent border on the one selected widget. |
| **Multi-widget selection** | Selecting multiple widgets for batch move/delete. Over-engineering for a head unit with 4-8 widgets. Android doesn't support this either. | Single-widget selection only. |
| **Widget stacking/overlap** | Visual chaos on a small automotive screen. Android enforces grid occupancy. | Grid-based occupancy with collision detection (already enforced). |
| **Undo/redo** | State complexity for rare use. Android doesn't offer widget undo. | Snap-back animation on failed drop. Manual reposition. |
| **Resize animation (smooth size transition)** | Janky on Pi 4 GPU. Already discovered in v0.5.3. | Ghost rectangle preview during drag. Widget re-renders at new size after release. |

---

## Feature Dependencies

```
Per-widget long-press select (FOUNDATION -- build first)
  ├── Drag to reposition (requires selected state as entry point)
  ├── 4-edge resize handles (only appear on selected widget)
  │   └── Tap-to-resize +/- buttons (optional layer on edge handles)
  ├── Navbar transformation (triggered by selectedWidgetId !== "")
  │   ├── Delete button (navbar → removeWidget)
  │   └── Settings button (navbar → WidgetConfigSheet.openConfig)
  ├── Inactivity auto-deselect (timer runs during selection)
  └── Grid overlay scoped to selection (visible when widget selected)

Long-press empty space (INDEPENDENT -- can build in parallel)
  ├── Context menu popup (positioned near touch point)
  │   ├── "Add Widget" → Bottom-sheet picker → Auto-place
  │   └── "Add Page" → WidgetGridModel.addPage()
  └── Replaces FABs (remove FABs after menu works)

Cleanup pass (AFTER both above work)
  ├── Remove editMode property and all editMode-dependent logic
  ├── Remove FABs (addWidgetFab, addPageFab, deletePageFab)
  ├── Remove badge overlays (removeBadge, configGear)
  ├── Remove deletePageDialog (auto-cleanup handles empty pages)
  └── Verify auto-cleanup of empty pages runs on widget deselect

Auto-cleanup empty pages
  └── Runs on deselect (was: on edit mode exit)
```

---

## MVP Recommendation

Build in this order based on dependencies and incremental value:

1. **Per-widget long-press select** -- Foundation for everything. Replace `editMode: bool` with `selectedWidgetId: string`. Selected widget gets accent border + 1.05 scale. All others remain normal. Tap empty space deselects. Inactivity timer deselects after 10s. Drag-to-reposition works from selected state using existing infrastructure.

2. **Navbar transformation** -- Settings + delete move from tiny badges to full-size navbar buttons. Volume control becomes settings gear (when widget has configSchema). Brightness control becomes delete trash (when widget is non-singleton). Center clock/home becomes "done" or stays. Reverse on deselect. This is the biggest UX win over the current system.

3. **4-edge resize handles** -- Expand from single bottom-right corner to dot indicators on each edge midpoint. Each handle drags in one axis (top/bottom = vertical, left/right = horizontal). Ghost preview already built. Min/max constraint feedback already built. This is the most complex piece -- handle positioning, per-axis drag math, edge-vs-corner semantics.

4. **Long-press empty space menu** -- Simple 2-item popup near touch point. "Add Widget" opens existing picker. "Add Page" calls existing addPage(). Positioned contextually, more discoverable than FABs.

5. **Cleanup pass** -- Delete FABs, badge overlays, editMode property, deletePageDialog, global edit mode logic. Verify auto-cleanup of empty pages triggers on deselect. This is pure code removal -- satisfying.

**Defer:**
- **Tap-to-resize +/- buttons (Android 16 QPR3 style):** Nice accessibility feature but not MVP. Drag handles are the established pattern (Android has had them since 3.1). +/- buttons are brand new even on Android. Layer on later.
- **Bottom-sheet picker polish (M3 drag handle, pull-to-expand):** Current picker works. Polish is post-MVP.

---

## Interaction Flow Comparison

### Current (v0.6.5 -- global edit mode)
1. Long-press anywhere on home screen
2. ALL widgets show X/gear/resize badges simultaneously
3. FABs appear (add widget, add page, delete page)
4. Dotted grid shows on all pages
5. Tap a 28px badge to act on that widget
6. Tap empty space or wait 10s to exit edit mode

### Target (v0.6.6 -- per-widget interactions)
1. Long-press a specific widget
2. THAT widget shows accent border + resize handles
3. Navbar controls transform to settings/delete
4. Drag the widget to reposition, drag edge handles to resize
5. Tap navbar settings to configure, trash to remove
6. Tap empty space or wait 10s to deselect
7. Long-press empty space shows "Add Widget" / "Add Page" menu

### Key UX Wins
- **Faster:** One gesture to select and act (no enter-mode step)
- **Clearer:** Only the targeted widget changes appearance
- **Bigger targets:** Navbar buttons are full-size touch targets, not 28px floating circles
- **Less clutter:** No FABs, no badges on every widget, no global visual state change
- **More Android-like:** Matches the interaction model every Android user already knows

---

## Android Widget Interaction Reference

Detailed breakdown of how Android handles each interaction, for implementation reference.

### Long-Press to Select
- **Timing:** ~400-500ms press-and-hold (Android default `ViewConfiguration.getLongPressTimeout()`)
- **Feedback:** Haptic buzz (`LONG_PRESS` HapticFeedbackConstant) + visual lift (scale up, shadow increase)
- **State:** Widget enters "selected" state. Outline with resize dots appears. Other widgets dim slightly or stay normal.
- **Pixel Launcher:** Widget lifts (elevation increase), outline with 4-edge dot handles appears, "Remove" text appears at top of screen

### Drag to Reposition
- **Entry:** Continue holding after long-press feedback fires. Movement threshold triggers drag mode.
- **Visual:** Widget follows finger at ~0.5 opacity. Original position shows ghost/placeholder. Grid snap preview shows target position.
- **Drop:** Release finger. Widget snaps to nearest valid grid position. If invalid (overlap), snaps back to original.
- **Cross-page:** Drag to screen edge to move widget to adjacent page (Prodigy probably doesn't need this with SwipeView).

### Resize Handles
- **Appearance:** 4 dots at edge midpoints (top, bottom, left, right). Some launchers also show corner dots.
- **Behavior:** Drag any dot to resize in that direction. Widget snaps to grid cell boundaries.
- **Constraints:** Handles stop at `minResizeWidth`/`maxResizeWidth`. Visual feedback when hitting limits.
- **Android 16 QPR3 (new):** +/- buttons alongside dots for tap-to-resize. Buttons disappear at min/max. Theme-colored.

### Widget Removal
- **Pixel Launcher:** Long-press lifts widget. Drag to top of screen where "Remove" appears. Release over "Remove" to delete.
- **Samsung/Others:** Long-press shows context popup with "Remove" option.
- **Prodigy adaptation:** Navbar delete button is better for automotive -- fixed position, known location, no drag-to-top precision needed.

### Empty Space Long-Press
- **Android:** Long-press empty space shows popup: "Wallpapers", "Widgets", "Home settings"
- **Prodigy adaptation:** "Add Widget" (opens picker), "Add Page". No wallpaper/settings needed (those are in Settings app).

### Widget Picker
- **Android 15+:** Full-screen picker scrollable by app. Each app expandable to show its widgets with preview images.
- **Android 15 addition:** "+ Add" button for tap-to-auto-place (vs. drag from picker to home screen).
- **Prodigy adaptation:** Bottom-sheet with categorized card list. Tap to auto-place. Already built, just needs new entry point.

---

## Sources

- [Android Developers: Create a simple widget](https://developer.android.com/develop/ui/views/appwidgets) -- official widget API, manifest spec, lifecycle
- [Google Pixel Phone Help: Add widgets](https://support.google.com/pixelphone/answer/2781850) -- official user-facing interaction steps
- [Android Authority: Widget resize buttons in Android 16 QPR3 Beta 2](https://www.androidauthority.com/widget-resizing-3633307/) -- tap-to-resize feature details
- [Android Police: Android 16 QPR3 Beta 2 widget resizing](https://www.androidpolice.com/android-16-qpr3-beta-2-home-screen-widgets-resize-button/) -- +/- button UX details
- [Android Headlines: Accessible widget resizing controls](https://www.androidheadlines.com/2026/01/android-16-qpr3-beta-2-accessible-widget-resizing-controls.html) -- accessibility rationale for +/- buttons
- [Material Design 3: Bottom sheets](https://m3.material.io/components/bottom-sheets/overview) -- M3 bottom sheet component spec
- [Android Developers: Haptic feedback](https://developer.android.com/develop/ui/views/haptics/haptic-feedback) -- long-press haptic feedback constants
- [Android AOSP: Haptics UX design](https://source.android.com/docs/core/interaction/haptics/haptics-ux-design) -- haptic UX design principles
- [Android Developers: Drag and scale](https://developer.android.com/develop/ui/views/touch-and-input/gestures/scale) -- drag/scale gesture patterns
- [AOSP: Widgets and shortcuts](https://source.android.com/docs/core/display/widgets-shortcuts) -- AOSP widget system overview
- Codebase: `qml/applications/home/HomeMenu.qml`, `qml/components/WidgetHost.qml`, `qml/components/WidgetPicker.qml`, `qml/components/WidgetConfigSheet.qml`, `qml/components/Navbar.qml` -- direct source inspection

---

*Feature research for: OpenAuto Prodigy v0.6.6 Homescreen Layout & Widget Settings Rework*
*Researched: 2026-03-21*
