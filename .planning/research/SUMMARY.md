# Project Research Summary

**Project:** OpenAuto Prodigy v0.6.6 — Homescreen Layout & Widget Settings Rework
**Domain:** Android-style per-widget interaction model for Qt 6.8 / QML automotive touchscreen
**Researched:** 2026-03-21
**Confidence:** HIGH

## Executive Summary

v0.6.6 is an interaction model rework, not a technology or architecture expansion. The goal is to replace the current global-edit-mode design (long-press anywhere -> all widgets badge -> FABs appear) with the Android-launcher per-widget model: long-press one widget to select it, then act on it via transformed navbar controls or drag handles. Every feature in this milestone maps directly to QML primitives and interaction patterns already proven in the codebase. There are zero new dependencies, zero new Qt modules, and zero new C++ classes required at minimum viable scope.

The recommended approach is to build in strict dependency order: selection model first (everything depends on `selectedInstanceId`), navbar transformation second (requires selection signal), edge resize handles third (independent of navbar, requires selection), bottom-sheet picker fourth (requires a trigger mechanism from the context menu), and context menu + cleanup last. This ordering gives a testable intermediate state after each phase and avoids building atop an unstable foundation. The core data layer (WidgetGridModel, WidgetRegistry, WidgetPickerModel) is unchanged — changes are concentrated in HomeMenu.qml and NavbarController.

The dominant risk in this milestone is touch event layering. Four distinct input paths coexist on the home screen: QML MouseArea, SwipeView Flickable, C++ NavbarController, and the evdev TouchRouter (active during AA). The per-widget selection model introduces a partial-edit state (selected-but-not-dragging) where SwipeView should remain enabled, but active dragging must disable it. Navbar zone registration has a documented race condition risk with animation timing. The pitfalls research found 5 critical issues, all with clear prevention strategies rooted in established patterns already in the codebase (requestContextMenu, SettingsInputBoundary, instant icon swap without opacity animation).

---

## Key Findings

### Recommended Stack

No new dependencies are needed. The entire milestone runs on Qt 6.8 QML primitives already used in this codebase: `MouseArea.pressAndHold`, `NumberAnimation`, `State`/`PropertyChanges`, `Dialog` for the bottom sheet, `Flickable` for scrollable content. The "What NOT to Add" list from the stack research is as important as the additions — `DragHandler`/`TapHandler` conflict with existing MouseArea architecture, `MultiEffect` layers freeze the Pi 4 GPU, and `Drawer` has the wrong gesture trigger semantics for a button-activated picker.

**Core technologies (all unchanged):**
- Qt 6.8.2 / QML: all rendering, animation, state management — no version change, no new modules
- C++17: minor property additions to NavbarController and WidgetGridModel (~45 lines total, no new classes)
- CMake 3.22+: no changes to build system, no new source files beyond BottomSheetPicker.qml

**Critical "do not add" technologies:**
- `DragHandler`/`TapHandler`: steals touch from child controls — documented crash in v0.6 SettingsInputBoundary work
- `MultiEffect`/`GraphicalEffects`: GPU freeze on Pi 4 with multiple simultaneous layers — v0.6.2 FullScreenPicker fix
- `Qt.Quick.Controls.Drawer`: wrong gesture trigger for button-activated picker; edge-swipe detection is unwanted

### Expected Features

See `FEATURES.md` for the full feature table with complexity, dependency, and Android reference analysis.

**Must have (table stakes — the Android interaction model core):**
- Per-widget long-press to select (replaces `editMode: bool` with `selectedInstanceId: string`)
- Long-press lift feedback: scale 1.05, accent border on selected widget only
- Drag-to-reposition from selected state (infrastructure already built, wiring changes only)
- Remove widget action via navbar delete control (replaces 28px X badge — automotive-critical touch target upgrade)
- Widget config access via navbar settings control (replaces 28px gear badge)
- Tap empty space to deselect (existing background MouseArea, behavior change only)
- Widget picker for adding widgets (existing WidgetPicker.qml, new entry point only)
- Auto-placement on add (existing `findFirstAvailableCell` + `placeWidget`, unchanged)

**Should have (differentiators for automotive use):**
- Navbar control transformation (volume/brightness to settings/delete during selection) — key automotive differentiation, replaces tiny badges with full-size navbar hit targets
- 4-edge resize handles on selected widget only (expands from current single bottom-right handle)
- Long-press empty space context menu (Add Widget / Add Page, replaces FABs)
- Inactivity auto-deselect at 5-7s (existing timer, repurposed for per-widget selection)
- Grid overlay scoped to selected widget (visibility change only, existing dotted grid component)

**Defer to later:**
- Tap-to-resize +/- buttons (Android 16 QPR3 style) — drag handles are established; +/- are brand new even on Android
- Bottom-sheet picker M3 polish (drag handle, pull-to-expand) — current picker works, polish is post-MVP

**Remove entirely (anti-features):**
- Global `editMode` boolean and all dependent logic
- FABs (addWidgetFab, addPageFab, deletePageFab)
- Badge overlays (removeBadge, configGear on every widget simultaneously)
- `deletePageDialog` (auto-cleanup replaces explicit deletion)
- 10s inactivity timer in global-mode form

### Architecture Approach

This is an interaction model change concentrated in QML and minor C++ property additions. The data layer is untouched. The system state machine has three states: IDLE, WIDGET_SELECTED (with DRAGGING and RESIZING sub-states), and CONTEXT_MENU. The key architectural decision: `selectedInstanceId` IS the state — no separate `editMode` boolean alongside it. `WidgetGridModel.setEditMode()` becomes a derived gate called mechanically when selection changes, not independent UI state.

**Major components and their v0.6.6 changes:**
1. HomeMenu.qml — heavy modification: replaces editMode with selectedInstanceId, removes FABs/badges, adds 4-edge handles, adds ContextMenu inline component, triggers BottomSheetPicker
2. NavbarController (C++) — moderate modification: adds `selectedWidgetId`, `widgetInteractionMode`, `selectedWidgetHasConfig` properties; contextual action dispatch branch; 3 new signals
3. WidgetGridModel (C++) — light modification: adds `resizeFromEdge()` atomic method for top/left edge resize (avoids the intermediate-invalid-state bug of sequential move + resize calls)
4. BottomSheetPicker.qml — new component, follows existing WidgetConfigSheet Dialog pattern exactly
5. ContextMenu in HomeMenu.qml — inline component, follows existing power menu popup pattern in Navbar.qml

**Components explicitly NOT needed:**
- WidgetInteractionController (C++): selection is a QML presentation concern; no benefit to adding C++ indirection (see gap note below re: PITFALLS disagreement)
- ResizeHandle.qml (extracted): 4 edge handles are simple enough to inline in the delegate

### Critical Pitfalls

Full detail in `PITFALLS.md`. Top 5 requiring explicit mitigation in the roadmap:

1. **Long-press-to-select breaks interactive widget content** — The MouseArea z-order toggle that works globally in edit mode doesn't transfer cleanly to per-widget selection. Use the existing `requestContextMenu()` pattern in WidgetHost; interactive widgets forward their own long-press. Test Now Playing, AA Focus toggle, and launcher tiles after every interaction change.

2. **Navbar zone registration races with visual control swap** — Zone callbacks capture control role at registration time. Visual swap before zone re-registration causes tapping "delete" to adjust volume instead. Prevention: swap action dispatch table in NavbarController atomically BEFORE any animation; use instant icon swap (no opacity crossfade); unregister all zones during transition.

3. **SwipeView page swipe conflicts with widget drag** — Without global editMode, there is no binary flag to gate `SwipeView.interactive`. Bind it to `draggingInstanceId === ""`, NOT `selectedInstanceId === ""`. Selected-but-not-dragging must still allow page swiping.

4. **Edge resize handles unusable at automotive touch targets** — 4-edge handles are physically smaller than a fingertip on 7" 1024x600, worse with vehicle vibration. Minimum MouseArea hit size of `UiMetrics.touchMin` (~48dp) with negative margins extending into the widget body. Test on Pi hardware before committing.

5. **Selected widget state persists across page swipes** — Selection is page-independent; swiping away leaves navbar showing settings/delete for an off-screen widget. Clear `selectedInstanceId` when `pageView.currentIndex` changes.

---

## Implications for Roadmap

Based on the dependency graph from FEATURES.md and the build order from ARCHITECTURE.md, a 5-phase roadmap is recommended. Each phase produces a testable, shippable intermediate state.

### Phase 1: Selection Model Foundation
**Rationale:** Everything in this milestone depends on `selectedInstanceId` replacing `editMode`. Build this first so subsequent phases have a stable signal to react to. This is the largest single conceptual change but involves the simplest code — a string property replacing a boolean, with corresponding visual changes.
**Delivers:** Working per-widget long-press select with visual lift (accent border + scale 1.05 on innerContent), drag-to-reposition from selected state, tap-to-deselect, inactivity auto-deselect at 5-7s. FABs, badges, and global editMode removed.
**Addresses:** Per-widget long-press select (table stakes #1-3), remove/config via deselect path (#5-6), inactivity auto-deselect (differentiator)
**Avoids:** Dual edit mode + selection state anti-pattern; SwipeView conflict requires `draggingInstanceId` binding here (Pitfall #3); scale transform on innerContent not delegate (Pitfall #13); `opacityChanged` signal name collision (Pitfall #12); page swipe clears selection (Pitfall #6)

### Phase 2: Navbar Transformation
**Rationale:** With selection working and emitting a signal, NavbarController can react to it. This is the highest-value UX change in the milestone — it moves widget actions from 28px badges to full-size automotive touch targets.
**Delivers:** Navbar controls morph to settings/delete when widget selected. Tap settings opens config sheet. Tap delete removes widget and reverts navbar. Tap center clock deselects. Navbar reverts cleanly on deselect or AA activation.
**Addresses:** Navbar control transformation (differentiator #1), remove widget action (table stakes), widget config access (table stakes)
**Avoids:** Zone registration race (Pitfall #2) — atomic mode swap before any animation, instant icon swap not crossfade; GPU jank on Pi (Pitfall #10) — no opacity animation on navbar controls; popup sliders guarded during widget interaction mode; AA activation auto-dismisses (Pitfall #9)

### Phase 3: Edge Resize Handles
**Rationale:** Independent of navbar transformation. Requires only the selection model from Phase 1. The most complex implementation in this milestone (axis-constrained drag math, 4-handle positioning) but well-contained in HomeMenu.qml and one new WidgetGridModel method.
**Delivers:** Four edge handles (top/bottom/left/right) on selected widget only. Each drags in one constrained axis. Ghost preview continues working. `resizeFromEdge()` atomic model method with unit test. Min/max constraint enforcement at handle boundaries.
**Addresses:** Resize handles on selected widget (table stakes #4)
**Avoids:** Sequential move+resize anti-pattern — use atomic `resizeFromEdge()` (ARCHITECTURE anti-pattern #4); touch targets too small (Pitfall #5) — `UiMetrics.touchMin` minimum with negative margins, Pi hardware validation required; handle drag during AA (irrelevant — home screen inactive during AA)
**Pi gate:** Test on Pi hardware before merging. If any edge handle requires more than 2 attempts to hit, widen MouseArea margins.

### Phase 4: Bottom-Sheet Widget Picker
**Rationale:** Requires a trigger mechanism (long-press empty space from Phase 5) but can be built and tested independently by calling it directly. Follows the existing WidgetConfigSheet Dialog pattern exactly — this is essentially a copy-adapt.
**Delivers:** New BottomSheetPicker.qml — categorized vertical scrollable list of available widgets by category, slide-up from bottom, tap-to-auto-place, slide-down dismiss.
**Addresses:** Widget picker for adding widgets (table stakes #7), auto-placement on add (table stakes #8)
**Avoids:** Picker overlapping navbar at bottom edge (Pitfall #8) — keep inside HomeMenu bounds, not Overlay.overlay parent; picker unreachable during AA EVIOCGRAB (Pitfall #9) — auto-dismiss on AA activation, guard open action; config sheet + picker stacking (Pitfall #15) — mutual exclusivity via single `activeOverlay` guard; `requestContextMenu()` race on Loader source swap (Pitfall #11) — `interactionLocked` flag 200ms after placement

### Phase 5: Context Menu + Page Auto-Management
**Rationale:** Least critical path. Depends on picker (Phase 4) for "Add Widget". Completes FAB removal and makes empty-space long-press the canonical entry point for page and widget management.
**Delivers:** Long-press empty space shows inline ContextMenu at press coordinates with "Add Widget" and "Add Page" options. Auto-delete empty pages on deselect/widget removal. Global edit mode infrastructure fully removed.
**Addresses:** Long-press empty space menu (differentiator #3), auto-delete empty pages (milestone requirement), complete removal of global edit mode
**Avoids:** SwipeView stealing long-press gesture (Pitfall #7) — `preventStealing: true` on background MouseArea during hold interval; keep secondary entry point (navbar or persistent indicator) as belt-and-suspenders; page auto-delete races placement (Pitfall #14) — defer cleanup to deselection, not immediate on removal

### Phase Ordering Rationale

- Phase 1 before all others: `selectedInstanceId` is the prerequisite signal for every other phase
- Phase 2 before Phase 3: navbar transformation validates the signal flow end-to-end before adding resize complexity; higher user-visible value per effort
- Phase 3 independent of Phase 2: could be swapped if preferred, but navbar wins on UX impact
- Phase 4 before Phase 5: picker must exist before the context menu can open it
- Phase 5 last: cleanup and secondary entry point; no dependencies from other phases

### Research Flags

Phases with established patterns (no additional research needed):
- **Phase 1:** Direct rework of existing editMode logic. All QML primitives proven in codebase.
- **Phase 2:** NavbarController dispatch extension. Follows existing 4-state edge pattern exactly.
- **Phase 4:** Dialog bottom sheet. WidgetConfigSheet is the template; copy-adapt.
- **Phase 5:** Inline context menu. Power menu popup in Navbar.qml is the template.

Phases requiring Pi hardware validation before sign-off:
- **Phase 3:** Edge resize handles. Touch target sizes cannot be validated on desktop VM. Plan a Pi smoke test after implementation: try resizing a 1x1 widget from each edge handle. If > 2 attempts needed, widen margins.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Direct codebase inspection — every primitive mapped to an existing usage in this codebase |
| Features | HIGH | Official Android docs, Android Authority/Police QPR3 coverage, direct codebase inspection |
| Architecture | HIGH | Full analysis of all relevant source files: HomeMenu.qml, WidgetGridModel, NavbarController, Navbar.qml, WidgetConfigSheet.qml, TouchRouter |
| Pitfalls | HIGH | Majority from documented project gotchas (CLAUDE.md/MEMORY.md) + codebase analysis; SwipeView threshold research is MEDIUM |

**Overall confidence:** HIGH

### Gaps to Address

- **4-edge handle usability on Pi hardware:** Research recommends `UiMetrics.touchMin` minimum and negative margins, but actual usability can only be proven with a finger on the 1024x600 DFRobot screen. Plan a Pi smoke test after Phase 3 before proceeding to Phase 4.

- **Navbar zone re-registration timing:** The exact timing gap between icon swap and zone re-registration depends on Pi GPU performance. The prevention strategy (instant swap, unregister during transition) is sound but needs verification on Pi. Test with rapid select/deselect cycles.

- **PITFALLS vs ARCHITECTURE disagreement on C++ state machine:** PITFALLS.md's integration risk section recommends a C++ WidgetInteractionController for deterministic gesture state, while ARCHITECTURE.md argues against it (QML presentation concern, unnecessary indirection). Resolution: start with QML state in HomeMenu.qml. If gesture race conditions emerge during Phase 1-2 implementation, escalate to C++ state machine as the PITFALLS recommendation suggests.

- **SwipeView long-press steal threshold:** Documented as ~8px in Qt forums, but the exact value on the Pi's actual touch driver has not been verified. If the Pitfall #7 long-press empty space proves unreliable, the fallback is a persistent "+" indicator that does not rely on gesture at all.

---

## Sources

### Primary (HIGH confidence — direct codebase inspection)
- `qml/applications/home/HomeMenu.qml` — current editMode, drag overlay z:200, resize handle, widget MouseArea z toggle, SwipeView interactive binding, page cleanup
- `qml/components/Navbar.qml` — zone registration, edge states, power menu popup pattern
- `qml/components/NavbarControl.qml` — control role mapping, hold progress
- `qml/components/WidgetConfigSheet.qml` — Dialog bottom sheet pattern (model for BottomSheetPicker)
- `qml/components/WidgetHost.qml` — z:-1 MouseArea pattern, requestContextMenu() convention
- `qml/components/Shell.qml` — navbar z:100, content area layout with margins
- `src/ui/WidgetGridModel.hpp/cpp` — grid model, CRUD, occupancy, page management, editMode gate
- `src/ui/NavbarController.hpp/cpp` — gesture state machine, evdev zone callbacks, action dispatch
- `src/core/aa/TouchRouter.hpp` — zone-based touch dispatch, EVIOCGRAB interaction
- `src/core/services/ActionRegistry.hpp` — named action dispatch pattern
- `.planning/PROJECT.md` — v0.6.6 milestone specification and prior milestone history
- CLAUDE.md and MEMORY.md — established gotchas: TapHandler grab conflict, MultiEffect GPU freeze, EVIOCGRAB behavior, WidgetHost z:-1 long-press pattern, opacityChanged signal collision

### Secondary (MEDIUM confidence)
- [Android Developers: Create a simple widget](https://developer.android.com/develop/ui/views/appwidgets) — interaction model reference
- [Google Pixel Phone Help: Add widgets](https://support.google.com/pixelphone/answer/2781850) — user-facing interaction steps
- [Android Authority: Android 16 QPR3 Beta 2 widget resize buttons](https://www.androidauthority.com/widget-resizing-3633307/) — tap-to-resize feature details
- [Material Design 3: Bottom sheets](https://m3.material.io/components/bottom-sheets/overview) — M3 bottom sheet spec
- [Qt 6 MouseArea documentation](https://doc.qt.io/qt-6/qml-qtquick-mousearea.html) — preventStealing behavior
- [Qt 6 SwipeView documentation](https://doc.qt.io/Qt-6/qml-qtquick-controls-swipeview.html) — interactive property, Flickable behavior

### Tertiary (LOW confidence — needs hardware validation)
- SwipeView gesture threshold (~8px) causing long-press cancellation: documented as platform issue in Qt forums; specific threshold on Pi's DFRobot touch driver not verified

---
*Research completed: 2026-03-21*
*Ready for roadmap: yes*
