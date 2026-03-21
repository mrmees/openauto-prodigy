---
phase: 25-selection-model-interaction-foundation
verified: 2026-03-21T19:00:00Z
status: passed
score: 13/13 must-haves verified
re_verification: false
---

# Phase 25: Selection Model & Interaction Foundation Verification Report

**Phase Goal:** Users interact with individual widgets via long-press rather than entering a global edit mode
**Verified:** 2026-03-21
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | User can long-press a widget to see it visually lift (scale + shadow during hold) then show accent border on release | VERIFIED | `innerContent.scale = 1.05 + liftShadow.visible = true` in `onPressAndHold`; accent border at `visible: delegateItem.isSelected` (line 514) |
| 2 | User can drag a selected widget to a new grid position with snap-to-cell | VERIFIED | `initiateDrag()` called from selectionTapInterceptor `onPressAndHold` when `delegateItem.isSelected`; `WidgetGridModel.setWidgetSelected(true)` defers remap until drop |
| 3 | User can release a long-pressed widget without dragging to see selected-state indicator (accent border, NO scale) | VERIFIED | `onReleased: if (!dragging) { innerContent.scale = 1.0; liftShadow.visible = false }` — scale returns to 1.0 on release; accent border gated on `isSelected` only |
| 4 | User can tap empty space, tap another widget, or tap clock-home to deselect | VERIFIED | Background MouseArea `onClicked: deselectWidget()`; selectionTapInterceptor `onClicked: homeScreen.deselectWidget()`; `navbar.clock.tap` calls `widgetGridModel->setWidgetSelected(false)` |
| 5 | Tapping another widget while one is selected deselects ONLY — does NOT fire tapped widget's action | VERIFIED | selectionTapInterceptor MouseArea at z:15, `enabled: homeScreen.selectedInstanceId !== ""`, swallows all clicks via `onClicked: homeScreen.deselectWidget()` with no propagation |
| 6 | Tapping the SELECTED widget itself also deselects | VERIFIED | selectionTapInterceptor has no `&& !delegateItem.isSelected` guard — covers all delegates including the selected one |
| 7 | Selection auto-clears after 10 seconds of inactivity | VERIFIED | `Timer { id: selectionTimer; interval: 10000; running: homeScreen.selectedInstanceId !== "" && !configSheet.isOpen && !pickerOverlay.visible; onTriggered: homeScreen.deselectWidget() }` (line 117) |
| 8 | SwipeView page swiping is locked while any widget is selected | VERIFIED | `interactive: homeScreen.selectedInstanceId === ""` (line 144) |
| 9 | AA fullscreen activation force-deselects any selected widget and dismisses config sheet | VERIFIED | Connections on `PluginModel.onActivePluginChanged`: `configSheet.closeConfig(); homeScreen.deselectWidget()` (lines 127-129) |
| 10 | Removing a widget via X badge clears selection if it was the selected widget | VERIFIED | X badge `onClicked: if (wasSelected) homeScreen.deselectWidget()` (lines 542-548) |
| 11 | Navigating to settings (any navbar short-hold) clears selection | VERIFIED | All 3 short-hold actions (`navbar.volume.shortHold`, `navbar.clock.shortHold`, `navbar.brightness.shortHold`) call `widgetGridModel->setWidgetSelected(false)` in `src/main.cpp` |
| 12 | Navigating via launcher widget actions (app.openSettings, app.launchPlugin) clears selection | VERIFIED | Both actions in `src/main.cpp` call `widgetGridModel->setWidgetSelected(false)` before navigating |
| 13 | Add-page FAB creates a new page without deselect-on-page-change self-destructing it | VERIFIED | `_addingPage = true` set before `addPage()`, cleared after `setCurrentIndex()`; `onCurrentIndexChanged` checks `!homeScreen._addingPage` (lines 154-155, 838-842) |

**Score:** 13/13 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/ui/WidgetGridModel.hpp` | `setWidgetSelected(bool)` replacing `setEditMode(bool)` | VERIFIED | Line 87: `Q_INVOKABLE void setWidgetSelected(bool selected);` — no `setEditMode` |
| `src/ui/WidgetGridModel.cpp` | Remap-deferral gate using `widgetSelected_` flag | VERIFIED | Lines 547, 804-805: gate uses `widgetSelected_`; `setWidgetSelected` body present |
| `src/main.cpp` | All 6 navigation actions call `setWidgetSelected(false)` | VERIFIED | Exactly 6 occurrences at lines 793, 797, 815, 834, 840, 858 |
| `qml/applications/home/HomeMenu.qml` | Per-widget selection model with `selectedInstanceId`, tap-intercept overlay, `_addingPage` guard | VERIFIED | `property string selectedInstanceId: ""` (line 15), `id: selectionTapInterceptor` (line 466), `property bool _addingPage: false` (line 16) |
| `qml/components/WidgetConfigSheet.qml` | `closeConfig()` and `isOpen` readonly property | VERIFIED | `readonly property bool isOpen: configDialog.visible` (line 16), `function closeConfig()` (line 18) |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `HomeMenu.qml` | `WidgetGridModel.cpp` | `setWidgetSelected(bool)` call on selection change | VERIFIED | `selectWidget()` calls `setWidgetSelected(true)` (line 83); `deselectWidget()` calls `setWidgetSelected(false)` (line 90) |
| `HomeMenu.qml` | `HomeMenu.qml` | `requestContextMenu()` triggers `selectWidget()` instead of `editMode` | VERIFIED | `requestContextMenu()` calls `homeScreen.selectWidget(model.instanceId)` (line 383) |
| `HomeMenu.qml` | `HomeMenu.qml` | `selectionTapInterceptor` MouseArea catches taps to deselect ALL delegates | VERIFIED | `enabled: homeScreen.selectedInstanceId !== ""` — no `!delegateItem.isSelected` exclusion (line 469) |
| `src/main.cpp` | `WidgetGridModel.cpp` | All 6 navigation actions call `setWidgetSelected(false)` | VERIFIED | Pattern `setWidgetSelected.*false` matches 6 times in main.cpp |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| SEL-01 | 25-01-PLAN.md | User can long-press a widget to lift it (scale up + shadow visual feedback) | SATISFIED | `liftShadow`, `innerContent.scale = 1.05` in `onPressAndHold`; `Behavior on scale` animation (line 345) |
| SEL-02 | 25-01-PLAN.md | User can drag a lifted widget to reposition it on the grid with snap-to-cell | SATISFIED | `initiateDrag()` callable from selectionTapInterceptor on selected widget; `WidgetGridModel` remap gate defers until `setWidgetSelected(false)` |
| SEL-03 | 25-01-PLAN.md | User can release a long-pressed widget without dragging to enter selected state (border/glow indicator) | SATISFIED | Accent border at `visible: delegateItem.isSelected` (line 514); scale resets to 1.0 on release |
| SEL-04 | 25-01-PLAN.md | User can tap empty space or clock-home navbar button to deselect a widget and revert all UI | SATISFIED | Background MouseArea + selectionTapInterceptor + `navbar.clock.tap` all call deselect |
| CLN-01 | 25-01-PLAN.md | Global edit mode removed (no editMode flag, no all-widgets-edit-simultaneously) | SATISFIED | `grep editMode HomeMenu.qml` returns only the comment "replaces global editMode" — no property, no binding |
| CLN-04 | 25-01-PLAN.md | Replace global edit mode inactivity timer with per-widget selection auto-deselect timeout | SATISFIED | `selectionTimer` with `interval: 10000` replaces `inactivityTimer`; no `inactivityTimer` in codebase |

CLN-02 (FABs removed) — Phase 27 — Pending, correctly out of scope.
CLN-03 (badge buttons removed) — Phase 26 — Pending, correctly out of scope.

No orphaned requirements: all Phase 25 requirement IDs (SEL-01/02/03/04, CLN-01, CLN-04) are accounted for.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `qml/applications/home/HomeMenu.qml` | 178 | `/* Phase 27: context menu */` in empty `onPressAndHold` | Info | Background long-press is intentionally a no-op in Phase 25. Context menu is Phase 27's deliverable. Not a blocker. |

No blockers or warnings found.

### Human Verification Required

#### 1. Long-press lift animation on Pi

**Test:** On the Pi's touch display, long-press a widget on the home screen.
**Expected:** Widget visually scales up slightly (1.05x) with a faint drop shadow appearing during the hold. On release (without dragging), scale returns to 1.0, shadow disappears, and an accent border appears around the widget.
**Why human:** Scale animation and shadow rendering can only be verified on the actual Pi display with Qt's compositing.

#### 2. Tap intercept on launcher widgets

**Test:** While a non-launcher widget is selected (accent border showing), tap the AA Launcher widget.
**Expected:** Only deselection occurs — AA does NOT launch, the home screen remains visible, selection clears.
**Why human:** Verifying that the interceptor swallows the action requires interactive testing on device; the code path is correct but the gesture routing depends on Qt's hit-testing at runtime.

#### 3. 10-second auto-deselect

**Test:** Long-press to select a widget, then leave it alone for 10 seconds.
**Expected:** Selection clears automatically (accent border disappears) after 10 seconds of no interaction.
**Why human:** Timer behavior is testable in code, but verifying it doesn't interfere with the config sheet being open requires interactive testing.

#### 4. Drag-from-selected on Pi touch

**Test:** Long-press a widget to select it (accent border shows), then long-press it again.
**Expected:** Widget lifts (scale + shadow) and can be dragged to a new grid position that snaps to cells.
**Why human:** Two-step interaction (select then drag) requires touch input on device.

### Gaps Summary

No gaps. All 13 observable truths verified, all 5 artifacts confirmed substantive and wired, all 4 key links confirmed, all 6 requirement IDs satisfied, no blocker anti-patterns.

Build result: 100% tests passed (88/88). No `setEditMode`, `editMode_`, `editMode` property, `inactivityTimer`, or `exitEditMode` references remain in the modified files.

---

_Verified: 2026-03-21_
_Verifier: Claude (gsd-verifier)_
