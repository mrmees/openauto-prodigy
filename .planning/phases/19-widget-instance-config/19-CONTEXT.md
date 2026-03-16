# Phase 19: Widget Instance Config - Context

**Gathered:** 2026-03-16
**Status:** Ready for planning

<domain>
## Phase Boundary

Extend the widget contract so any widget can declare typed configuration options, users can set them per-placement via a host-rendered config sheet, and config persists across restarts and grid remaps. This is generic infrastructure — no widget-specific config UI in this phase.

</domain>

<decisions>
## Implementation Decisions

### Config UI trigger
- **Edit mode is the single management surface** — move, resize, remove, and now configure
- Show a gear/settings icon only in edit mode, only for widgets that declare `configSchema`
- Widgets without config: no gear, no placeholder, no disabled button
- Tapping gear opens host-owned config bottom sheet
- Do NOT overload long-press with a context menu popup — long-press stays as edit mode entry
- Do NOT use tap-on-body-in-edit-mode for config — too easy to trigger accidentally during drag

### Config sheet UI
- **Bottom sheet** sliding up from bottom, matching the existing widget picker pattern
- Widget icon + name in header
- Body renders schema-driven controls only (toggles, dropdowns, sliders based on field type)
- Dismiss via outside tap, explicit close, or swipe down
- **Changes apply immediately** — no Save button in v1. Fits the small structured-field model.
- Touch-friendly sizing for 1024x600 display — no giant forms

### Config schema design
- **New `configSchema` field on `WidgetDescriptor`** — separate from `defaultConfig`
- `configSchema` defines editable fields and how the host renders them (key, label, type, options/range, default)
- `defaultConfig` remains the source of default values (existing field, currently unused)
- **Effective widget config** = persisted per-instance values overlaid on `defaultConfig`
- **v1 field types only:** `enum/select`, `bool/toggle`, `int/range`
- No custom widget-defined config editors
- No arbitrary key-value blobs without schema
- Host validates against schema before persisting — widgets only see sanitized effective config

### Config persistence + remap
- Config persists per widget placement/instance in YAML alongside geometry
- Grid remap (auto-snap) only affects geometry/placement — config values are untouched
- **Stable instance IDs across remaps** — no heuristic merge by widgetId
- Widget removal + re-add: gets fresh defaults, not restored config (copy/restore is future work)
- Config survives app restart via YAML round-trip

### Claude's Discretion
- Exact C++ struct/class for config schema field definitions
- How configSchema is populated (compile-time in widget registration, or runtime)
- YAML serialization format for per-instance config within grid placements
- Bottom sheet QML implementation details (animation, sizing, scroll behavior)
- How WidgetInstanceContext exposes effective config to QML (property, method, or model)

### Not At Claude's Discretion
- Schema-driven host-rendered config (not per-widget custom editors)
- Gear icon only in edit mode, only for widgets with configSchema
- Bottom sheet (not dialog, not full-screen)
- Immediate apply (not staged Save)
- Stable instance IDs across remap (not heuristic merge)
- Three field types only: enum, bool, int/range
- Removal + re-add gets defaults (not restored previous config)

</decisions>

<canonical_refs>
## Canonical References

### Widget contract
- `src/core/widget/WidgetTypes.hpp` — WidgetDescriptor (line 14), GridPlacement (line 36), defaultConfig field (line 23)
- `src/ui/WidgetGridModel.hpp` + `.cpp` — Placement model, roles, setWidgetOpacity pattern
- `src/ui/WidgetInstanceContext.hpp` + `.cpp` — QML-side context injection (cellWidth, spans, providers)
- `src/core/widget/WidgetContextFactory.hpp` — Creates WidgetInstanceContext per placement

### YAML persistence
- `src/core/YamlConfig.cpp` lines 821-862 — Grid placement serialization (instance_id, widget_id, col, row, spans, opacity, page)
- `src/core/YamlConfig.cpp` lines 718-805 — Legacy pane-based config map serialization (reference for how config was previously round-tripped)

### Edit mode UI
- `qml/applications/home/HomeMenu.qml` — Edit mode entry (line 338), remove badge (line 430), resize handle, FAB buttons
- `qml/controls/FullScreenPicker.qml` — Bottom sheet pattern reference (widget picker uses this)

### Widget registration
- `src/main.cpp` — Widget descriptor registration with WidgetRegistry
- `src/core/widget/WidgetRegistry.hpp` — Widget discovery and descriptor storage

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `WidgetDescriptor.defaultConfig` (QVariantMap) — exists, unused. Ready to serve as default values source.
- `WidgetGridModel.setWidgetOpacity()` — pattern for per-instance mutable data. Config set/get will follow the same model.
- `FullScreenPicker.qml` — bottom sheet pattern with slide-up animation, outside-tap dismiss. Config sheet can reuse this.
- Legacy `WidgetPlacement.config` (QVariantMap) — existed in pane system, was not carried to GridPlacement. Shows prior art for the YAML round-trip.

### Established Patterns
- Widget registration in `main.cpp` populates WidgetDescriptor fields at compile time
- `WidgetInstanceContext` provides properties to QML via Binding-based injection through `WidgetContextFactory`
- Edit mode toggles visibility of control overlays (remove badge, resize handle) via `editMode` boolean

### Integration Points
- `GridPlacement` struct needs `QVariantMap config` field added
- `YamlConfig::gridPlacements()` and `setGridPlacements()` need config map serialization
- `WidgetGridModel` needs config read/write methods + new role for QML
- `WidgetInstanceContext` needs effective config property (defaults merged with instance overrides)
- `HomeMenu.qml` edit mode overlay needs gear icon conditionally shown
- New `WidgetConfigSheet.qml` bottom sheet component renders schema-driven controls

</code_context>

<specifics>
## Specific Ideas

- "Priority for this phase: generic reusable config infrastructure, not widget-specific custom UI"
- "Schema and values are different things. defaultConfig should not become a weird dual-purpose container."
- "Apply immediately in v1. That keeps the flow simpler and fits the small structured-field model."
- "Widgets with no schema get no gear, no placeholder, no disabled button."
- Config sheet should feel like a lightweight edit task, not a full settings page

</specifics>

<deferred>
## Deferred Ideas

- Widget config copy/restore on removal + re-add — future phase if users want it
- Custom widget-defined config editors (beyond host-rendered schema) — only if structured types prove insufficient
- Additional field types (color picker, text input, file selector) — only when a widget needs them
- Opacity slider in config sheet — could move existing opacity to the config sheet but not required for v1

</deferred>

---

*Phase: 19-widget-instance-config*
*Context gathered: 2026-03-16*
