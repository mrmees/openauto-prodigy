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
- Gear replaces volume position, trash replaces brightness position. Since volume/brightness positions are already LHD/RHD-aware (volume = driver side), gear naturally follows: gear is always on the driver side, trash on passenger side.
- Side controls are TAP ONLY in widget mode — no hold-progress feedback, no shortHold/longHold gestures on gear/trash. NavbarController must suppress hold timers and progress feedback when widgetInteractionMode is true (existing `handlePress()` starts hold timers unconditionally — needs a guard).
- Clock center control shows the selected widget's display name instead of the time during widget selection. Tap still routes to `navbar.clock.tap` (deselect + go home). If display name is too long, truncate with ellipsis.

### Navbar mode binding and drag reversion (NAV-05)
- `widgetInteractionMode` binding: `selectedInstanceId !== "" && draggingInstanceId === ""`. Navbar reverts to volume/brightness during active drag (not just on deselect). This satisfies NAV-05 "navbar reverts on drag start."
- On deselect (any trigger): widgetInteractionMode goes false, navbar reverts instantly.
- On auto-deselect timeout: same — navbar reverts.

### Navbar action routing (per Codex architecture recommendation)
- Add `widgetInteractionMode` Q_PROPERTY to NavbarController (C++). Bound from QML via Binding element.
- `controlRole()` in NavbarController becomes mode-aware: when widgetInteractionMode is true, volume-position returns `gear`, brightness-position returns `trash`, center stays `clock`.
- ActionRegistry naturally dispatches `navbar.gear.tap` / `navbar.trash.tap` via existing `dispatchAction()` — no re-registration needed. Register these two actions in main.cpp.
- `navbar.gear.tap` action emits a `widgetConfigRequested()` signal on NavbarController. `navbar.trash.tap` action emits a `widgetDeleteRequested()` signal on NavbarController.
- HomeMenu.qml listens for these signals via Connections block and handles the action using its own `selectedInstanceId`:
  - Config: reads widget metadata via new `WidgetGridModel.widgetMeta(instanceId)` Q_INVOKABLE (returns widgetId, displayName, hasConfigSchema, isSingleton), then opens configSheet.openConfig()
  - Delete: guards against singleton, calls WidgetGridModel.removeWidget(), then deselectWidget()
- selectedInstanceId stays QML-only — NOT exposed to C++.

### Singleton and no-config edge cases
- **Singleton widget selected + trash tapped:** Trash icon shows but is visually dimmed/disabled. Tapping does nothing (removeWidget already guards against singletons in C++, but the icon should reflect this).
- **No-config widget selected + gear tapped:** Gear icon shows but is visually dimmed/disabled. Tapping does nothing.
- To support dimming: NavbarController or Navbar.qml needs to know whether the current selection is singleton/has-config. Options: expose via `widgetMeta()` result in QML, bind dimmed state from HomeMenu to navbar via properties.

### Widget metadata helper (new)
- Add `Q_INVOKABLE QVariantMap widgetMeta(const QString& instanceId) const` to WidgetGridModel.
- Returns: `{ widgetId, displayName, iconName, hasConfigSchema, isSingleton }`.
- Used by HomeMenu.qml for: center navbar text (displayName), gear enabled state (hasConfigSchema), trash enabled state (!isSingleton), config sheet opening (widgetId for schema lookup).

### Edge resize handles
- Edge bars centered on each edge of the selected widget (like Android resize handles). Visible, clear affordance.
- All 4 edges independently draggable — dragging any edge changes the widget boundary in that direction (column/row + span change simultaneously).
- Resize ghost rectangle (dashed border, green valid / red collision) preserved from existing implementation. Driven by edge handles instead of corner handle.
- Limit flash animation (tertiary color brief flash) preserved from existing implementation.
- Touch targets extend BOTH inward and outward from the handle bar for maximum grabbability on 7" screen.
- Min/max constraints enforced per widget descriptor. Collision detection blocks resize when overlapping another widget.
- Edge resize handles must sit ABOVE the selectionTapInterceptor (z > 15) so they receive touch events. The interceptor covers the whole widget; handles need to punch through.

### Edge resize API (locked — NOT Claude's Discretion)
- Current `resizeWidget(instanceId, colSpan, rowSpan)` only changes span, NOT position. Left/top edge resize requires atomic position + span change.
- Add new `Q_INVOKABLE bool resizeWidgetFromEdge(const QString& instanceId, int newCol, int newRow, int newColSpan, int newRowSpan)` to WidgetGridModel. This validates the combined position+span against grid bounds, min/max constraints, and collision detection, then applies atomically.
- The existing `resizeWidget()` can remain for backward compat or be replaced — planner decides.

### Badge removal (CLN-03)
- Remove X delete badge (top-right) — replaced by navbar trash button
- Remove gear config badge (top-left) — replaced by navbar gear button
- Remove corner resize handle (bottom-right) — replaced by 4-edge handles
- Resize ghost and limit flash kept as-is — same visual language, new trigger

### Empty page auto-delete (PGM-04)
- When trash button removes the last widget on a page, that page should be auto-deleted.
- Current `deselectWidget()` cleanup skips the current page. For PGM-04, the trash action should: remove widget → check if page is now empty → if so, navigate to previous page, then remove the empty page. This must happen BEFORE deselectWidget() runs its cleanup (which would skip current page).
- Alternatively: modify deselectWidget() cleanup to NOT skip current page when the deletion was explicit (via trash). Use a flag or separate cleanup path.

### Claude's Discretion
- Exact edge bar dimensions (width, length, corner radius)
- Touch target extension distances (inward and outward from edge)
- Edge handle z-ordering value (must be > 15 to sit above selectionTapInterceptor)
- Widget display name font size in navbar center
- How `handlePress()` guard works for tap-only mode (early return before timer start, or separate code path)
- Whether to add a `widgetDisplayName` property to NavbarController or handle the name display entirely in QML

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
- `src/ui/NavbarController.hpp` / `.cpp` — Gesture dispatch, controlRole(), dispatchAction(), handlePress(), LHD/RHD awareness
- `qml/components/Navbar.qml` — Control role mapping (control0Icon, control2Icon), icon resolution
- `qml/components/NavbarControl.qml` — iconText property, touch handling, hold progress feedback, clock display
- `qml/applications/home/HomeMenu.qml` — Selection state, resize ghost, badge overlays, drag overlay, configSheet, selectionTapInterceptor (z:15)
- `qml/components/WidgetConfigSheet.qml` — openConfig() API, isOpen property, closeConfig()
- `src/ui/WidgetGridModel.hpp` / `.cpp` — resizeWidget(), canPlace(), removeWidget(), widget metadata access
- `src/main.cpp` — ActionRegistry registrations (navbar.*.tap, app.openSettings, app.launchPlugin)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `NavbarController::dispatchAction()` — Already builds `navbar.{role}.{gesture}` action IDs. Adding `gear`/`trash` roles just extends the existing pattern.
- `NavbarController::controlRole()` — Private method mapping controlIndex → role string. Needs mode-aware branch.
- `WidgetGridModel::resizeWidget()` — Existing resize with collision detection. Span-only — needs `resizeWidgetFromEdge()` for position+span changes.
- Resize ghost rectangle in HomeMenu.qml — Reusable visual, just needs new trigger from edge handles instead of corner handle.
- `configSheet.openConfig()` — Existing API for widget configuration (needs instanceId, widgetId, displayName, iconName).
- `WidgetGridModel::removeWidget()` — Existing delete with singleton guard (returns false for singletons).
- Empty page cleanup in `deselectWidget()` — Already implemented, skips current page. Needs adaptation for PGM-04.

### Established Patterns
- ActionRegistry dispatch: `actionRegistry->registerAction("navbar.gear.tap", ...)` — same pattern as existing navbar actions
- LHD/RHD control mapping: `controlIndex 0 = driver side, 2 = passenger side` — gear/trash follow same convention (volume position = gear, brightness position = trash)
- Q_PROPERTY with NOTIFY for QML binding: used throughout NavbarController
- Press feedback: `_holdProgress` fill-from-bottom — must be suppressed for gear/trash (tap-only)
- `handlePress()` unconditionally starts hold timers — needs widgetInteractionMode guard

### Integration Points
- `NavbarController` ← `HomeMenu.qml`: widgetInteractionMode property binding (`selectedInstanceId !== "" && draggingInstanceId === ""`)
- `NavbarController` → `HomeMenu.qml`: widgetConfigRequested() / widgetDeleteRequested() signals
- `NavbarController` → `Navbar.qml`: controlRole() drives icon selection per mode
- `Navbar.qml` / `NavbarControl.qml`: icon swap + clock/widget-name conditional + gear/trash dimming for singleton/no-config
- `HomeMenu.qml` → `WidgetGridModel`: new widgetMeta() for metadata lookup, new resizeWidgetFromEdge() for edge resize
- `HomeMenu.qml` resize ghost: currently driven by bottom-right corner handle — needs rewiring to edge handle drag state
- Edge handle z-ordering: must be > 15 (selectionTapInterceptor z-level) to receive touch events

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
