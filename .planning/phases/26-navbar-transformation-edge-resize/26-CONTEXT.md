# Phase 26: Navbar Transformation & Edge Resize - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

When a widget is selected, navbar volume/brightness controls transform into settings gear/trash buttons. Tapping gear opens widget config sheet, tapping trash deletes the widget. 4-edge resize handles replace the old corner resize handle. All tiny badge buttons (X delete, gear config, corner resize) are removed. Empty pages auto-delete when their last widget is removed via trash.

Gated substep: navbar transformation must be verified working BEFORE resize handle implementation begins.

</domain>

<decisions>
## Implementation Decisions

### Navbar icon swap
- Instant icon swap — no animation, no crossfade. Research recommended this to avoid zone registration timing issues.
- Settings = gear icon (\ue8b8), Delete = trash icon (\ue872). Standard Material Symbols.
- Gear replaces volume (driver-side control), trash replaces brightness (passenger-side control). LHD/RHD awareness maintained.
- Side controls are TAP ONLY in widget mode — no hold-progress feedback, no shortHold/longHold gestures on gear/trash.
- Clock center control shows the selected widget's display name instead of the time during widget selection. Provides context for what gear/trash will act on.

### Navbar action routing (per Codex architecture recommendation)
- Add `widgetInteractionMode` Q_PROPERTY to NavbarController (C++). Bound from QML: `NavbarController.widgetInteractionMode = (selectedInstanceId !== "")`.
- `controlRole()` in NavbarController becomes mode-aware: when widgetInteractionMode is true, driver-side returns `gear`, passenger-side returns `trash`, center stays `clock`.
- ActionRegistry naturally dispatches `navbar.gear.tap` / `navbar.trash.tap` — no re-registration needed.
- NavbarController emits `widgetConfigRequested()` / `widgetDeleteRequested()` signals when widget mode is active and side control tap resolves.
- HomeMenu.qml listens for these signals and handles the action using its own `selectedInstanceId`:
  - Config: reads widget metadata from WidgetGridModel, opens configSheet.openConfig()
  - Delete: guards against singleton, calls WidgetGridModel.removeWidget(), then deselectWidget()
- selectedInstanceId stays QML-only — NOT exposed to C++. NavbarController does NOT need to know which widget is selected.

### Edge resize handles
- Edge bars centered on each edge of the selected widget (like Android resize handles). Visible, clear affordance.
- All 4 edges independently draggable — dragging any edge changes the widget boundary in that direction (column/row + span change simultaneously).
- Resize ghost rectangle (dashed border, green valid / red collision) preserved from existing implementation. Driven by edge handles instead of corner handle.
- Limit flash animation (tertiary color brief flash) preserved from existing implementation.
- Touch targets extend BOTH inward and outward from the handle bar for maximum grabbability on 7" screen.
- Min/max constraints enforced per widget descriptor. Collision detection blocks resize when overlapping another widget.

### Badge removal (CLN-03)
- Remove X delete badge (top-right) — replaced by navbar trash button
- Remove gear config badge (top-left) — replaced by navbar gear button
- Remove corner resize handle (bottom-right) — replaced by 4-edge handles
- Resize ghost and limit flash kept as-is — same visual language, new trigger

### Empty page auto-delete (PGM-04)
- When trash button removes the last widget on a page, that page is automatically deleted
- Same logic as existing empty-page cleanup but triggered by navbar trash removal, not edit mode exit

### Claude's Discretion
- Exact edge bar dimensions (width, length, corner radius)
- Touch target extension distances (inward and outward from edge)
- How to handle resize when dragging left/top edges (position + span change atomically — may need new WidgetGridModel method)
- Whether edge handles need z-ordering relative to selectionTapInterceptor
- Widget display name formatting in navbar center (truncation, font size)
- How NavbarController suppresses hold gestures in widget mode (skip dispatchAction for non-tap gestures, or just don't register shortHold/longHold for gear/trash roles)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase requirements
- `.planning/REQUIREMENTS.md` — NAV-01..05, RSZ-01..04, CLN-03, PGM-04 define this phase's scope
- `.planning/ROADMAP.md` — Phase 26 success criteria (8 items) + gated substep

### Research
- `.planning/research/SUMMARY.md` — Synthesis including navbar transformation and resize pitfalls
- `.planning/research/PITFALLS.md` — Navbar zone registration timing, 4-edge resize touch target sizing, SwipeView conflicts
- `.planning/research/ARCHITECTURE.md` — NavbarController integration points, new vs modified components

### Prior phase context
- `.planning/phases/25-selection-model-interaction-foundation/25-CONTEXT.md` — Selection model decisions (selectedInstanceId, deselect triggers, widgetDeselectedFromCpp signal)
- `.planning/phases/07-edit-mode/07-CONTEXT.md` — Original resize handle decisions (ghost rectangle, limit flash)

### Key source files
- `src/ui/NavbarController.hpp` / `.cpp` — Gesture dispatch, controlRole(), dispatchAction(), LHD/RHD awareness
- `qml/components/Navbar.qml` — Control role mapping (control0Icon, control2Icon), icon resolution
- `qml/components/NavbarControl.qml` — iconText property, touch handling, hold progress feedback, clock display
- `qml/applications/home/HomeMenu.qml` — Selection state, resize ghost, badge overlays, drag overlay, configSheet
- `qml/components/WidgetConfigSheet.qml` — openConfig() API, isOpen property, closeConfig()
- `src/ui/WidgetGridModel.hpp` / `.cpp` — resizeWidget(), canPlace(), widget metadata access
- `src/main.cpp` — ActionRegistry registrations (navbar.*.tap, app.openSettings, app.launchPlugin)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `NavbarController::dispatchAction()` — Already builds `navbar.{role}.{gesture}` action IDs. Adding `gear`/`trash` roles just extends the existing pattern.
- `NavbarController::controlRole()` — Private method mapping controlIndex → role string. Needs mode-aware branch.
- `WidgetGridModel::resizeWidget()` — Existing resize with collision detection. May need extension for anchor-edge resize (position + span change).
- Resize ghost rectangle in HomeMenu.qml — Reusable visual, just needs new trigger from edge handles instead of corner handle.
- `configSheet.openConfig()` — Existing API for widget configuration.
- `WidgetGridModel::removeWidget()` — Existing delete with singleton guard.
- Empty page cleanup in `deselectWidget()` — Already implemented, triggered on deselect.

### Established Patterns
- ActionRegistry dispatch: `actionRegistry->registerAction("navbar.gear.tap", ...)` — same pattern as existing navbar actions
- LHD/RHD control mapping: `controlIndex 0 = driver side, 2 = passenger side` — gear/trash follow same convention
- Q_PROPERTY with NOTIFY for QML binding: used throughout NavbarController
- Press feedback: `_holdProgress` fill-from-bottom — disabled for gear/trash (tap-only)

### Integration Points
- `NavbarController` ← `HomeMenu.qml`: widgetInteractionMode property binding
- `NavbarController` → `HomeMenu.qml`: widgetConfigRequested() / widgetDeleteRequested() signals
- `Navbar.qml` control icons: driven by controlRole(), needs to query NavbarController for current icon per role
- `HomeMenu.qml` resize ghost: currently driven by bottom-right corner handle — needs rewiring to edge handle drag state
- `NavbarControl.qml` clock display: needs conditional display of widget name when widgetInteractionMode is true

</code_context>

<specifics>
## Specific Ideas

- Android homescreen reference: when you select a widget, the action bar transforms to show contextual actions. Navbar is our action bar equivalent.
- Widget name in center navbar control gives the user clear context about what they're configuring/deleting — important at arm's length in a car.
- Edge bars (like Android resize handles) are the most recognizable affordance for resize.
- Codex recommended keeping selectedInstanceId QML-only and routing config/delete via signals — avoids the split-brain problem identified in Phase 25.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 26-navbar-transformation-edge-resize*
*Context gathered: 2026-03-21*
