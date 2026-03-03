# Phase 2: UiMetrics Foundation + Touch Pipeline - Context

**Gathered:** 2026-03-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Rework UiMetrics to produce correct sizing at any resolution (unclamped dual-axis scaling, new tokens, pixel floors), and make the C++ touch pipeline derive sidebar hit zones from display config instead of hardcoded 1024x600 values. Display dimensions auto-detected from actual window geometry.

This phase does NOT audit individual QML files for hardcoded values (Phase 3) or handle runtime resize/auto-detection UX (Phase 5). It builds the foundation that those phases depend on.

</domain>

<decisions>
## Implementation Decisions

### Scale formula (SCALE-01, SCALE-02, SCALE-03)
- Reference baseline: 1024x600 = scale 1.0 (today's display looks identical after this change)
- Dual-axis scaling: `scaleH = windowWidth / 1024`, `scaleV = windowHeight / 600`
- Layout tokens use `min(scaleH, scaleV)` — guarantees no overflow
- Font tokens use geometric mean `sqrt(scaleH * scaleV)` — slightly larger on ultrawide for readability
- Dimension source: QML `ApplicationWindow.width` / `ApplicationWindow.height` (not `Screen.*` which is unreliable at Wayland init)
- Fully unclamped — no 0.9-1.35 range restriction
- Log a warning at startup if computed scale is outside 0.7-2.0 range (helps debug unusual displays, doesn't restrict)

### Font pixel floors (SCALE-04)
- ALL font tokens get pixel floors, not just small ones
- Tiered minimums: fontTiny/fontSmall = 12px, fontBody = 14px, fontTitle = 16px, fontHeading = 18px (Claude to finalize exact values)
- Floors are configurable via `ui.fontFloor` config key — users can lower them if they want smaller text on high-DPI or raise them for readability
- Individual token overrides (ui.tokens.fontBody) bypass floors entirely (user always wins per Phase 1 precedence)

### New UiMetrics tokens (SCALE-05)
- Add at minimum: trackThick, trackThin, knobSize (for sliders/scrollbars)
- Claude to audit codebase for additional hardcoded dimensions that should become tokens
- New tokens follow Phase 1 pattern: overridable via `ui.tokens.*`, registered in YamlConfig initDefaults()
- Token override values are absolute pixels (no multiplication), same as Phase 1

### Sidebar touch zones (TOUCH-01)
- EvdevTouchReader sidebar hit zones must derive from display config, not hardcoded 1024x600 magic values
- Current hardcoded values: 100px/80px for horizontal sub-zones, 0.70/0.75 ratios for vertical sub-zones
- Claude's discretion on whether these come from UiMetrics tokens (shared with QML sidebar visuals) or separate C++ config — considering the AA overscan handling and the separate UI/touch interfaces between AA video and Prodigy

### Display config detection (TOUCH-02)
- Auto-detect display dimensions from actual window geometry at startup
- display.width/height config becomes a fallback only used if detection fails
- Detected values are in-memory only — not written back to config.yaml (config stays clean with only intentional user values)
- EvdevTouchReader receives dynamic display dimension updates (not just at construction time) — similar to existing setAAResolution() pattern. Future-proofs for Phase 5 runtime adaptation.

### Claude's Discretion
- Full list of new UiMetrics tokens beyond the three specified (based on codebase audit)
- Whether sidebar hit zones use UiMetrics tokens or separate C++ config values (considering AA overscan + dual UI/touch interface complexity)
- Exact pixel floor values for each font tier
- How ApplicationWindow dimensions are wired into UiMetrics (property binding, signal, or context property)
- C++ API for dynamic display dimension updates to EvdevTouchReader

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `UiMetrics.qml` (pragma Singleton): Already has autoScale, globalScale, fontScale, _tok() helper. Phase 2 modifies autoScale computation and adds font floor logic.
- `ConfigService.value()`: QML-accessible config reads. Already used by UiMetrics for ui.scale/ui.fontScale.
- `YamlConfig::initDefaults()`: Where new ui.fontFloor and ui.tokens.* defaults get registered.
- `EvdevTouchReader::setAAResolution()`: Existing pattern for thread-safe dimension updates via atomics — can be extended for display dimensions.

### Established Patterns
- UiMetrics tokens: `readonly property int tokenName: { var o = _tok("tokenName"); return isNaN(o) ? Math.round(BASE * scale) : o; }`
- Config defaults in `initDefaults()` follow `category.subcategory.value` naming
- EvdevTouchReader uses atomic stores for cross-thread updates (main thread → reader thread)

### Integration Points
- `qml/controls/UiMetrics.qml` — modify autoScale to use window dims + dual-axis, add font floors, add new tokens
- `src/core/YamlConfig.cpp` initDefaults() — register ui.fontFloor default, new token defaults
- `src/core/aa/EvdevTouchReader.hpp/cpp` — add setDisplayDimensions() for dynamic updates, derive sidebar zones from config
- `src/plugins/android_auto/AndroidAutoPlugin.cpp` — change display dim source from config to detected window geometry
- `src/core/aa/ServiceDiscoveryBuilder.cpp` — uses displayWidth()/displayHeight() for AA protocol negotiation

</code_context>

<specifics>
## Specific Ideas

- Phase 1 precedence chain must be preserved: token override > fontScale > globalScale > autoScale
- The `ui.fontFloor` key should accept a single integer (applied to smallest font) or per-token floors (Claude to decide format)
- Startup logging should show: detected window dims, computed scaleH/scaleV, effective layout scale, effective font scale, any applied config overrides, any font floor activations

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 02-uimetrics-foundation-touch-pipeline*
*Context gathered: 2026-03-03*
