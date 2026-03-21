# Domain Pitfalls

**Domain:** Per-widget interactions, navbar control swapping, edge resize handles, bottom sheet overlays in Qt/QML widget grid
**Researched:** 2026-03-21
**Milestone:** v0.6.6 — Replacing global edit mode with Android-style per-widget long-press interactions
**Confidence:** HIGH (codebase analysis of current touch routing, gesture handling, navbar zone system, widget grid + known project gotchas)

---

## Critical Pitfalls

Mistakes that cause broken interaction, touch routing failures, or forced rewrites.

### Pitfall 1: Long-Press-to-Select Transition Breaks Widget Content Interactivity

**What goes wrong:** The current system uses `widgetMouseArea` at `z: -1` behind widget content (normal mode) and `z: 10` above content (edit mode). The v0.6.6 model needs a per-widget "selected" state WITHOUT a global edit mode toggle. If the long-press MouseArea stays behind content, interactive widgets (Now Playing play/pause, AA Focus toggle, launcher tiles) consume the press and `pressAndHold` never fires on the host. If it sits above content, widget taps die completely.

**Why it happens:** The current binary edit/non-edit toggle flips z-order globally. Per-widget selection requires disambiguation PER WIDGET PER GESTURE: the same press could be a widget tap, widget long-press (select), or content interaction. QML MouseArea has no built-in "pass through tap but catch long-press" behavior.

**Consequences:** Either widgets stop responding to taps (z above content), or long-press never fires on interactive widgets (z below content). Both are shipping regressions from current behavior.

**Prevention:**
- Use the existing `requestContextMenu()` pattern already established in the codebase: interactive widgets call `widgetLoader.requestContextMenu()` from their own `onPressAndHold`. Non-interactive widgets fall through to the background MouseArea at `z: -1`. This pattern is proven and works today.
- The long-press transitions the individual delegate to "selected" state (visual lift, scale, border) rather than toggling a global boolean. This is per-delegate state.
- Critical: `pressAndHoldInterval` must be identical (500ms) in both the background MouseArea and any widget-internal MouseArea to avoid timing surprises.
- For future-proofing against third-party widgets that miss the pattern: consider the `SettingsInputBoundary` approach (C++ `childMouseEventFilter`) already proven in this project for detecting long-press across a widget subtree without stealing child events.

**Detection:** After the change, tap Now Playing play/pause, tap AA Focus toggle, tap launcher tile. All must still respond. Then long-press each -- must select.

### Pitfall 2: Navbar Zone Registration Races with Visual Control Swap

**What goes wrong:** The plan transforms navbar controls from volume/clock/brightness to settings/delete when a widget is selected. The C++ `NavbarController` registers evdev touch zones via `registerZones()` based on control geometry. Zone callbacks reference the current control role ("volume") at REGISTRATION time, not dispatch time. If the visual QML swap happens asynchronously from zone re-registration, touches land on stale zone callbacks -- tapping "delete" adjusts volume instead.

**Why it happens:** Zone registration is triggered by `Qt.callLater(registerZones)` connected to geometry change signals (`onXChanged`, `onWidthChanged`). QML property animations cause multiple intermediate geometry updates. The zone callback closures capture the role at bind time.

**Consequences:** Tapping "delete widget" adjusts volume. Tapping "settings" adjusts brightness. Race condition manifests intermittently depending on animation timing and touch speed. On Pi with EVIOCGRAB active, the evdev zones are the ONLY touch path -- this is not a cosmetic issue.

**Prevention:**
- Swap control roles BEFORE any animation, not during. Use a `NavbarController.mode` property (`"normal"` vs `"widgetEdit"`) that switches the action dispatch table atomically. The visual animation is cosmetic; the logical role swap must be instant.
- Unregister ALL navbar zones during the swap transition and re-register only after new roles are committed. A 200ms gap where navbar touch does nothing is better than wrong actions firing.
- The existing zone system uses string IDs (`"navbar-ctrl-0"`, etc.) -- the callbacks should check current mode at dispatch time rather than capturing the role at registration time.
- Test by triggering rapid select/deselect cycles and tapping navbar controls at each transition.

**Detection:** Tap the "delete" control area immediately after widget selection triggers the navbar swap. If volume changes instead, the zone registration raced.

### Pitfall 3: SwipeView Page Swipe Conflicts with Widget Drag Gesture

**What goes wrong:** Currently `SwipeView.interactive = !homeScreen.editMode` prevents page swiping during edit. Without global edit mode, there is no binary flag. If SwipeView remains interactive, horizontal widget drags near page boundaries trigger page swipes instead of widget movement. If SwipeView is disabled whenever any widget is selected, the user cannot swipe to see other pages.

**Why it happens:** SwipeView's internal Flickable consumes horizontal drag gestures at its own level. QML gesture disambiguation gives priority to the outer container (SwipeView) over inner MouseAreas unless `preventStealing: true` is set. But blanket `preventStealing` on the drag overlay prevents SwipeView from EVER swiping.

**Consequences:** Widget drag near edges causes accidental page changes, or pages become non-swipeable during any widget interaction.

**Prevention:**
- Disable SwipeView interactive only when `draggingInstanceId !== ""` (active drag in progress), not on selection alone. The binding becomes: `interactive: homeScreen.draggingInstanceId === ""`.
- Keep `preventStealing: true` ONLY on the drag overlay MouseArea (already present in current code). Only enable the drag overlay when actual dragging begins (finger moved past threshold after long-press).
- The state transitions must be: `Idle -> Selected (SwipeView enabled) -> Dragging (SwipeView disabled) -> Drop (SwipeView re-enabled)`.

**Detection:** Long-press widget to select, try to swipe to next page. Should work. Start dragging widget horizontally -- page should NOT swipe.

### Pitfall 4: Drag Overlay at z:200 Can Steal Touch from Navbar During Widget Drag

**What goes wrong:** The current `dragOverlay` MouseArea sits at `z: 200` with `anchors.fill: parent` covering the entire HomeMenu. The navbar sits at `z: 100` in Shell. Because these are in different parent contexts (HomeMenu inside `pluginContentHost`, navbar in Shell), QML z-ordering is scoped per-parent and should not conflict. BUT: if the overlay is accidentally reparented to Shell for "full coverage" during refactoring, it eats navbar touches.

**Why it happens:** During the refactor, the temptation to parent overlays at the shell level (for "clicking outside the content area dismisses selection") creates z-order conflicts with the navbar. The navbar's z:100 is lower than a shell-level overlay at z:200.

**Consequences:** User cannot adjust volume or brightness while dragging a widget. On Pi with EVIOCGRAB active, evdev zones bypass QML entirely so this only matters on the home screen -- but that is exactly when widget drag happens.

**Prevention:**
- Keep the drag overlay strictly inside HomeMenu / `pluginContentHost`. Never parent it to Shell.
- For "tap outside widget to deselect" behavior, use a HomeMenu-level background MouseArea (the existing page background MouseArea already does `onClicked → exitEditMode`). Adapt this to clear selection instead.
- The "selected widget" state should NOT deploy a full-screen overlay at all. Only activate the drag overlay when actual finger movement begins.

**Detection:** During widget drag, try tapping a navbar control (volume/brightness). Must still respond.

### Pitfall 5: Edge Resize Handles Unusable at Automotive Touch Targets

**What goes wrong:** The current single bottom-right resize handle is `UiMetrics.touchMin * 0.5` (~24px visual) with expanded touch margins. Moving to 4-edge resize handles creates narrow strips (~8-12px) along widget borders that are nearly impossible to hit with a finger in a moving car. Corners where two edge handles meet create ambiguous hit zones.

**Why it happens:** Edge handles look obvious in mockups and on desktop. At 7" 1024x600 with finger-based input and vehicle vibration, the touch target is physically smaller than a fingertip (~7mm at this DPI). The existing corner handle with negative margins is already at the minimum usable size.

**Consequences:** Users cannot resize widgets. They accidentally drag instead of resize. Corner ambiguity means resizing from a corner changes both axes unpredictably.

**Prevention:**
- **Keep the single bottom-right resize handle.** It works, is discoverable, and allows both single-axis and dual-axis resize from one control. Four edge handles add complexity without proportional value on a 7" touchscreen.
- If 4-edge handles are required: make them INVISIBLE with large touch margins (`anchors.margins: -UiMetrics.touchMin * 0.5` = ~24px extension). Show them only after widget selection. Corners resize both axes; edges resize one axis only.
- Minimum effective touch target: `UiMetrics.touchMin` (48dp equivalent). Anything smaller will fail in-car use.
- Use `preventStealing: true` on resize handle MouseAreas (already done in current code) to prevent the drag overlay from claiming the resize gesture.

**Detection:** On Pi touchscreen, try to resize a 1x1 widget by its edge handle. If it takes more than 2 attempts, the touch target is too small.

---

## Moderate Pitfalls

### Pitfall 6: Selected Widget State Persists Across Page Swipes

**What goes wrong:** User long-presses a widget on page 0 to select it (visual lift, border, navbar transforms to settings/delete). User swipes to page 1. The selected widget is off-screen but selection state persists -- navbar still shows settings/delete, and tapping delete would remove a widget the user cannot see.

**Why it happens:** Selection state is stored as a homeScreen-level property (`selectedInstanceId`) which is page-independent. The visual feedback (border, scale) is on the delegate which becomes invisible off-screen, but the logical state and navbar transformation remain.

**Consequences:** User deletes a widget they did not intend to delete. Navbar shows wrong controls for the current context.

**Prevention:**
- Clear widget selection when `pageView.currentIndex` changes. Add a `Connections` watching `pageView.onCurrentIndexChanged` that calls deselect.
- Alternatively, disable page swiping while a widget is selected. This matches Android launcher behavior where widget selection locks the current page.
- Show the selected widget's name in the navbar center (replacing the clock) as visual confirmation of what is selected.

**Detection:** Select widget on page 0, swipe to page 1, tap delete. Should either do nothing or auto-deselect on swipe.

### Pitfall 7: Long-Press Empty Space Fails Due to SwipeView Gesture Stealing

**What goes wrong:** Long-pressing empty grid space should show an "Add Widget / Add Page" context menu. But SwipeView treats the initial press as a potential swipe start. If the finger moves even ~8px during the 500ms hold interval (trivial with vehicle vibration), SwipeView begins its drag gesture and cancels the MouseArea's `pressAndHold`.

**Why it happens:** SwipeView's Flickable has a low drag threshold (~8px). Any movement beyond this converts the gesture to a flick, and the background MouseArea receives `onCanceled`, not `onPressAndHold`.

**Consequences:** Long-press on empty space rarely triggers on a touchscreen in a car. The "Add Widget" entry point becomes unreliable.

**Prevention:**
- Use `preventStealing: true` on the background MouseArea during the press interval. Release the steal prevention if the user clearly swipes (movement > 30px threshold).
- Keep a secondary entry point: the navbar or a persistent "+" indicator that does not rely on the long-press gesture at all. Belt-and-suspenders.
- The current FAB approach works reliably -- consider keeping a minimal "+" FAB visible only when on a page with available grid space, without requiring edit mode.

**Detection:** On Pi, try to long-press empty grid space. If it fails > 30% of the time, the gesture is unreliable.

### Pitfall 8: Bottom Sheet Widget Picker Overlaps or Hides Behind Navbar

**What goes wrong:** The current picker panel uses `height: Math.min(parent.height * 0.35, 200)` anchored to `parent.bottom`. If the navbar is at the bottom edge (`navbar.edge === "bottom"`), the picker either overlaps the navbar (if it renders above the navbar's z) or is hidden behind it (if below). If the navbar is transformed to show settings/delete controls, the picker covers those controls.

**Why it happens:** The picker's position calculation does not account for navbar geometry. The navbar is in Shell space, the picker is in HomeMenu space -- they are in different parent contexts with different coordinate origins.

**Consequences:** Picker and navbar overlap. Touch targets collide. User cannot dismiss picker because the scrim behind the navbar is unreachable.

**Prevention:**
- Calculate picker height as `Math.min(parent.height * 0.35, 200)` with a bottom margin of `navbar.barThick` when `navbar.edge === "bottom"`. Use the Shell's content margins that are already applied to the ColumnLayout.
- Since HomeMenu already lives inside `pluginContentHost` which has navbar margins applied, the picker anchored to `parent.bottom` within HomeMenu should clear the navbar automatically. Verify this is still true after refactoring.
- If using `Overlay.overlay` as parent (like the current `configSheet`), the picker escapes the content margins and the navbar overlap returns. Keep the picker inside HomeMenu bounds.

**Detection:** Open widget picker with navbar at bottom edge. Picker and navbar should not overlap.

### Pitfall 9: Bottom Sheet / Picker Unreachable During AA (EVIOCGRAB)

**What goes wrong:** If the widget picker is somehow open when AA connects (race condition), or if a user triggers the picker via a QML action right before AA takes over, the bottom sheet renders but cannot receive touch to dismiss or select. EVIOCGRAB routes all touch through evdev, bypassing Qt's event loop entirely. QML Popups become untouchable zombies.

**Why it happens:** EVIOCGRAB is toggled on AA connect. Any QML overlay that was visible at the moment of grab becomes permanently stuck.

**Consequences:** Stuck overlay that can only be cleared by restarting the app.

**Prevention:**
- The AA fullscreen auto-exit pattern already exists (`EDIT-08` in current code: `PluginModel.activePluginChanged` → `exitEditMode`). Extend this to dismiss any widget selection state, picker visibility, and config sheet.
- Guard picker open: `if (PluginModel.activePluginId) return` before showing. Same for config sheet.
- The existing `configSheet` (Dialog with Overlay.overlay parent) has the same vulnerability -- add auto-dismiss there too.

**Detection:** Open widget picker, then immediately connect phone for AA. Picker should auto-dismiss.

### Pitfall 10: Navbar Control Swap Animation Causes GPU Jank on Pi

**What goes wrong:** Crossfading navbar controls (volume icon to settings icon, brightness icon to delete icon) during the transformation. The Pi 4's VideoCore VI GPU struggles with multiple simultaneous opacity layers -- this was already discovered with `FullScreenPicker` delegate shadows causing GPU freezes in v0.6.2.

**Why it happens:** Each opacity-animated item creates a separate render layer. Crossfading two sets of 3 controls = 6 simultaneous opacity layers in the navbar area. The Pi's GPU has limited layer composition bandwidth.

**Consequences:** Visible flicker, frame drops, or momentary freeze during navbar transformation. In a car, any UI freeze during touch interaction is a safety concern.

**Prevention:**
- Use instant content swap (no animation) for navbar control icons and labels. The navbar is small and peripheral -- snap changes are acceptable and feel responsive.
- If any animation is desired, limit to a single property (e.g., a brief background color flash on the control that changed) rather than crossfading the entire control content.
- Test on Pi hardware before committing to any animation approach. Desktop dev VM has no GPU constraints.

**Detection:** Trigger navbar swap rapidly by selecting/deselecting widgets in quick succession. Watch for frame drops on Pi.

### Pitfall 11: `requestContextMenu()` Call Breaks During Loader Source Swap

**What goes wrong:** Widgets call `parent.requestContextMenu()` to forward long-press. This works because the Loader exposes the function. If the widget picker replaces a widget at the same grid position (remove old + place new), the Loader's `source` changes, the old item is destroyed, and during the brief transition `parent` may be null. A long-press during this window crashes or silently fails.

**Why it happens:** QML Loader source changes destroy the old component before the new one loads. During this gap, `parent` references are unstable.

**Consequences:** Crash on rare timing, or first long-press after widget replacement silently fails.

**Prevention:**
- Guard `requestContextMenu()` calls: `if (parent && typeof parent.requestContextMenu === "function") parent.requestContextMenu()`.
- Set a brief `interactionLocked` flag (200ms) after any widget add/remove/move operation to suppress long-press gestures during transitions.

**Detection:** Rapidly add a widget and immediately long-press it. If it crashes or does not respond, the timing window exists.

---

## Minor Pitfalls

### Pitfall 12: `opacityChanged` Signal Name Collision (Known Gotcha)

**What goes wrong:** QML `Item.opacity` auto-generates `opacityChanged`. Any new "selected widget opacity" property or signal using the name `opacity` or `opacityChanged` on a delegate will shadow the built-in and cause unexpected behavior.

**Prevention:** Name selection-related opacity properties explicitly: `selectionOpacity`, `liftOpacity`. Never shadow built-in Item properties.

### Pitfall 13: Widget Scale Transform During Selection Breaks Position Math

**What goes wrong:** Applying `scale: 1.05` for the "lift" visual effect. Scale transforms around `transformOrigin` (default: center). The delegate's `x`/`y` bindings use top-left corner coordinates. If the scale animation runs while position bindings are active, the visual position jumps because the transform origin is not the same as the binding coordinate origin.

**Prevention:** Apply scale only to the `innerContent` child (which already exists inside the delegate), not the delegate itself. Or explicitly set `transformOrigin: Item.Center` and ensure position bindings are not re-evaluated during scale changes.

### Pitfall 14: Auto-Delete Empty Pages Races with Widget Placement

**What goes wrong:** Auto-deleting empty pages on widget removal. If the last widget on page 2 is deleted, page 2 auto-deletes. If the user then places a new widget targeting page index 2 (from a queued action or auto-place), the page no longer exists.

**Prevention:** Auto-delete empty pages only when leaving widget interaction (deselection), not immediately on widget removal. This matches the current `exitEditMode()` cleanup pattern.

### Pitfall 15: Config Sheet and Bottom Sheet Picker Can Stack

**What goes wrong:** User selects a widget, taps settings (config sheet opens), then somehow triggers the widget picker (e.g., via a "replace widget" action). Two overlays stack, creating confusing z-order and touch routing.

**Prevention:** Close any existing overlay before opening a new one. Enforce mutual exclusivity: `configSheet.visible` and `pickerOverlay.visible` should be mutually exclusive, gated by a single `activeOverlay` enum.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Per-widget long-press selection | MouseArea z-ordering breaks interactive widgets (#1) | Use existing requestContextMenu pattern; test all interactive widgets |
| Navbar control swap | Zone registration races with visual animation (#2) | Atomic mode swap before animation; unregister during transition |
| Navbar control swap | GPU jank on Pi (#10) | Instant swap, no opacity animation |
| Edge resize handles | Touch targets too small for automotive (#5) | Keep bottom-right handle; minimum touchMin touch area |
| Bottom sheet picker | Overlaps navbar at bottom edge (#8) | Account for navbar margins in positioning |
| Bottom sheet picker | Unreachable during AA EVIOCGRAB (#9) | Auto-dismiss on AA activation, guard open action |
| Long-press empty space | SwipeView steals gesture (#7) | preventStealing during hold; keep secondary entry point |
| Widget drag | SwipeView page swipe conflicts (#3) | Disable SwipeView only during active drag, not selection |
| Widget drag | Drag overlay steals navbar touch (#4) | Keep overlay strictly in HomeMenu, not Shell |
| Widget selection state | Persists across page swipes (#6) | Clear selection on page change |
| Selection visual | Scale transform breaks position (#13) | Scale innerContent, not delegate |
| Page management | Auto-delete races placement (#14) | Defer auto-delete to deselection |

---

## Integration Risk Summary

The highest-risk integration point is the **touch event layering**. This project has FOUR distinct touch input paths that must cooperate:

1. **QML MouseArea** -- normal home screen widget taps, long-press, drag, resize
2. **QML SwipeView Flickable** -- page swipe gestures (steals horizontal drags)
3. **C++ NavbarController** -- QML-side `handlePress`/`handleRelease` for launcher mode
4. **Evdev TouchRouter** -- bypasses Qt entirely during AA (EVIOCGRAB active, priority-based zone dispatch)

Adding per-widget selection, navbar mode swapping, and a bottom sheet overlay means paths 1-3 must negotiate gesture ownership for every touch on the home screen. The existing architecture handles this for the global edit mode case (binary toggle), but per-widget selection creates a **partial-edit state** where some gestures should be forwarded (page swipe when selected-but-not-dragging) and others captured (drag when finger moves past threshold), and the decision depends on which widget is selected and where exactly the touch lands.

**Recommendation:** Implement the widget interaction state machine in C++ (new `WidgetInteractionController` or extension of existing `NavbarController`) rather than in QML properties scattered across HomeMenu. QML property bindings update asynchronously and create race conditions when multiple gesture handlers read the same state on the same frame. A C++ state machine with explicit transitions (`Idle -> Selected -> Dragging -> Resizing`) and `Q_PROPERTY` notifications gives deterministic ordering and is testable with unit tests. The `NavbarController` already proves this pattern works for the gesture state machine -- apply the same approach for widget interaction.

---

## Sources

- **Codebase analysis** (HIGH confidence):
  - `qml/applications/home/HomeMenu.qml` -- current edit mode, drag overlay z:200, resize handle, widget MouseArea z toggle, SwipeView interactive binding, page cleanup
  - `qml/components/Shell.qml` -- navbar z:100, content area layout with navbar margins, GestureOverlay
  - `qml/components/Navbar.qml` -- zone registration via Qt.callLater, edge states, power menu popup session, 4-edge anchor states
  - `qml/components/NavbarControl.qml` -- control role mapping, hold progress, QML handlePress/Release
  - `qml/components/WidgetHost.qml` -- z:-1 MouseArea pattern, requestContextMenu() convention
  - `qml/components/WidgetConfigSheet.qml` -- Dialog with Overlay.overlay parent, bottom sheet animation
  - `src/ui/NavbarController.hpp` -- gesture state machine, evdev zone callbacks, popup session API, zone registration
  - `src/core/aa/TouchRouter.hpp` -- priority-based zone dispatch, finger stickiness, mutex-guarded zone updates
  - `src/ui/WidgetGridModel.hpp` -- grid model API, canPlace, editMode flag, page management
- **Project gotchas** (HIGH confidence):
  - CLAUDE.md/MEMORY.md: EVIOCGRAB steals all touch from Qt during AA
  - CLAUDE.md/MEMORY.md: Qt 6 TapHandler overlays steal ALL touch from child controls
  - CLAUDE.md/MEMORY.md: WidgetHost long-press MouseArea must be at z:-1
  - CLAUDE.md/MEMORY.md: FullScreenPicker GPU freeze with multiple MultiEffect layers on Pi
  - CLAUDE.md/MEMORY.md: SettingsInputBoundary childMouseEventFilter for subtree long-press
  - CLAUDE.md/MEMORY.md: QML Item.opacity auto-generates opacityChanged signal
  - CLAUDE.md/MEMORY.md: Plugin view self-destruction when calling setActivePlugin("") from within own view
- **Web research** (MEDIUM confidence):
  - [Qt 6 MouseArea documentation](https://doc.qt.io/qt-6/qml-qtquick-mousearea.html) -- preventStealing behavior
  - [Qt 6 DragHandler documentation](https://doc.qt.io/qt-6/qml-qtquick-draghandler.html) -- drag threshold semantics
  - [QML Swipe gesture overrides other components](https://www.qtcentre.org/threads/60772-QML-Swipe-gesture-overrides-other-components) -- SwipeView gesture stealing is a known platform issue
  - [Qt 6 SwipeView documentation](https://doc.qt.io/Qt-6/qml-qtquick-controls-swipeview.html) -- interactive property, Flickable behavior

---
*Pitfalls research for: OpenAuto Prodigy v0.6.6 Homescreen Layout & Widget Settings Rework*
*Researched: 2026-03-21*
