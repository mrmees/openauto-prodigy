# Phase 25: Selection Model & Interaction Foundation - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace global edit mode with per-widget long-press selection. Users long-press a single widget to lift/drag/select it — no more all-widgets-edit-simultaneously. Remove the global editMode flag and inactivity timer, replacing them with a per-widget selectedInstanceId and auto-deselect timeout. FABs and badge buttons stay temporarily (removed in Phases 26-27 when their replacements ship).

</domain>

<decisions>
## Implementation Decisions

### Long-press behavior
- Long-press ALWAYS selects the widget, regardless of whether it has interactive content (Now Playing controls, AA Focus toggle, etc.)
- Tap = widget's own action; hold = management. Clear separation, no ambiguity.
- Long-press threshold stays at 500ms (carried from Phase 07)
- Widget lifts with scale up (~5%) + drop shadow visual feedback
- Other widgets stay completely static — no jiggle, no dim
- Drag activation: if finger moves beyond threshold while holding, transition to drag mode
- Drag follows Phase 07 conventions: ~50% opacity, snap-to-grid on release, placeholder at original position

### Selection visuals (after release without drag)
- Selected widget shows primary-colored accent border (reuses existing pattern)
- Full dotted grid lines visible across the entire page (same as current edit mode)
- Widget stays at normal scale in selected state (lift + scale is only during press-and-hold)
- Only ONE widget can be selected at a time

### Deselect triggers
- Tap empty grid space → deselect
- Tap any other widget → deselect ONLY (does NOT fire the tapped widget's normal action — two-step: first tap deselects, second tap activates widget)
- Clock-home navbar button → deselect (existing `navbar.clock.tap` action routes through ActionRegistry — add deselect before home navigation)
- Auto-deselect timeout: 10 seconds of inactivity
- AA fullscreen activation → force deselect AND dismiss any open overlays (config sheet, picker) to prevent EVIOCGRAB zombie-overlay (touch stolen by evdev while QML overlay is visible)
- SwipeView interactive=false while any widget is selected (prevents accidental page changes)
- Since SwipeView is locked during selection, page swipe cannot be a deselect method

### Cleanup scope
- Remove `editMode` boolean flag from HomeMenu.qml
- Remove `WidgetGridModel.setEditMode()` — replace with selection-aware remap gate (defer remap while selectedInstanceId is non-empty, apply on deselect)
- Remove 10s inactivity timer — replace with 10s auto-deselect timer on the selection state
- Keep FABs temporarily (CLN-02 is Phase 27)
- Keep badge buttons temporarily (CLN-03 is Phase 26)
- Keep WidgetConfigSheet and gear badge intact — Phase 26 replaces the trigger with navbar gear button
- Reuse existing visual components: dotted grid lines, drag placeholder, drop highlight — same visuals, new trigger (selection instead of editMode)
- Remove the tap-empty-space-to-exit-edit-mode handler — replace with tap-to-deselect

### Long-press forwarding on interactive widgets
- Two mechanisms exist and BOTH must work:
  1. **Non-interactive widgets** (Clock, Date, Battery, etc.): The delegate's z:-1 MouseArea in HomeMenu.qml catches long-press directly — no widget-level forwarding needed since these widgets have no competing MouseAreas
  2. **Interactive widgets** (Now Playing, AA Focus, Weather, Companion Status): These have their own MouseAreas that steal the press. They MUST explicitly forward long-press via `onPressAndHold` → `requestContextMenu()` at matching 500ms threshold. This is the proven pattern already used in the codebase (see AAFocusToggleWidget.qml, NowPlayingWidget.qml, WeatherWidget.qml).
- Long-press detection must NOT be moved above widget content (z > 0) — this would steal all taps from widget controls
- Any current or future widget with its own MouseArea that omits `requestContextMenu()` forwarding will be unselectable — this is an implementation requirement for all interactive widgets
- `requestContextMenu()` currently triggers the old context menu; this phase repurposes it to trigger selection instead

### State ownership
- `selectedInstanceId` is a QML string property on `HomeMenu.qml` — NOT a C++ class
- C++ interaction state machine is explicitly out of scope (per REQUIREMENTS.md Out of Scope)
- WidgetGridModel gets a `setWidgetSelected(bool)` method as a thin remap gate only — no selection logic in C++
- If QML-only state causes gesture races during implementation, document the issue for Phase 26 but do not introduce a C++ controller in this phase

### Claude's Discretion
- Exact scale factor for lift animation (suggested ~1.05)
- Drop shadow implementation approach (simple Rectangle shadow vs MultiEffect — Pi GPU constraint)
- Drag activation distance threshold (pixels before drag mode activates)
- Accent border width for selected state
- Auto-deselect timer restart behavior (which interactions reset the timer)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase requirements
- `.planning/REQUIREMENTS.md` — SEL-01..04, CLN-01, CLN-04 define this phase's scope
- `.planning/ROADMAP.md` — Phase 25 success criteria (9 items)

### Research
- `.planning/research/SUMMARY.md` — Synthesis of stack/features/architecture/pitfalls
- `.planning/research/PITFALLS.md` — Touch event layering risks, SwipeView gesture stealing, selection state management
- `.planning/research/ARCHITECTURE.md` — Integration points, component modification list

### Prior phase context (relevant decisions)
- `.planning/phases/07-edit-mode/07-CONTEXT.md` — Original edit mode decisions (drag, resize, FAB, grid lines). This phase REPLACES Phase 07's global edit mode.
- `.planning/phases/09-widget-descriptor-grid-foundation/09-CONTEXT.md` — Grid math, remap deferral, DisplayInfo/WidgetGridModel split

### Key source files
- `qml/applications/home/HomeMenu.qml` — Main homescreen: editMode, drag, resize, FABs, grid lines, SwipeView
- `qml/components/Navbar.qml` — Navbar controls (volume/clock/brightness) — Phase 26 transforms these
- `src/ui/NavbarController.cpp` — Gesture dispatch (dispatchAction via ActionRegistry)
- `src/ui/WidgetGridModel.cpp` — Grid state, setEditMode() remap gate, placement CRUD
- `qml/components/WidgetHost.qml` — Widget container with long-press detection at z:-1

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `HomeMenu.editMode` property and all its dependent visuals (grid lines, drag overlay, drop highlight, drag placeholder) — refactored from editMode-triggered to selection-triggered
- `WidgetGridModel.moveWidget()`, `canPlace()`, collision detection — all reusable as-is
- `inactivityTimer` — refactored from edit mode exit to auto-deselect
- `exitEditMode()` cleanup logic — adapted to `clearSelection()` function
- Accent border from edit mode delegate styling — reused for selected state
- `dragOverlay` MouseArea for global drag capture — reused with selection-based activation

### Established Patterns
- Long-press detection: MouseArea at z:-1 with pressAndHoldInterval: 500
- Drag: delegateItem.dragging flag, global dragOverlay MouseArea, snap-to-grid math
- Press feedback: scale+opacity animation (used throughout settings and controls)
- Qt.callLater for batched state updates (grid dims, page cleanup)
- WidgetGridModel dimension remap deferral via editMode_ flag — needs adaptation to selection state

### Integration Points
- `WidgetGridModel.setEditMode(bool)` → replace with `setWidgetSelected(bool)` or equivalent remap gate
- `SwipeView.interactive` binding: change from `!editMode` to `selectedInstanceId === ""`
- `PluginModel.onActivePluginChanged` → force deselect instead of force exitEditMode
- NavbarController clock-home tap action → add deselect signal (or route through ActionRegistry)

</code_context>

<specifics>
## Specific Ideas

- Android homescreen as reference — long-press lifts individual widget, not a global mode
- Drop shadow should convey "lifted above the surface" — but watch Pi GPU constraints (MultiEffect caused freezes in v0.6.2 FullScreenPicker)
- "Tap = widget action, hold = management" is the core principle — no exceptions for interactive widgets

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 25-selection-model-interaction-foundation*
*Context gathered: 2026-03-21*
