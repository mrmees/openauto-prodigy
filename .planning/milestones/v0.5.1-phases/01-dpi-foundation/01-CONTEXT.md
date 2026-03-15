# Phase 1: DPI Foundation - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

UI sizing is derived from the physical screen size, not just pixel resolution. Installer detects screen size via EDID, UiMetrics computes DPI-based scaling, and the value persists in config. Requirements: DPI-01 through DPI-04.

</domain>

<decisions>
## Implementation Decisions

### EDID Detection
- Parse raw EDID from `/sys/class/drm/*/edid` — extract physical dimensions from bytes 21-22 (width_mm x height_mm)
- No extra packages needed — works on any Linux with DRM
- Convert mm dimensions to diagonal inches for storage
- Happens during hardware detection step in installer (alongside WiFi cap probing)

### Installer Flow
- If EDID succeeds: show detected size with accept/override prompt — "Detected screen: 7.0" (154x86mm) / Press Enter to accept, or type a different size in inches:"
- If EDID fails or user skips: default to 7" (standard double-DIN)
- Input is numeric entry in inches, accepts decimals (e.g., 10.1)
- Installer writes only `display.screen_size` (inches) to config — pixel resolution is auto-detected by the app at runtime

### Config Structure
- New config key: `display.screen_size` — diagonal inches as float (e.g., `7.0`)
- No pixel resolution stored in config — app reads window dimensions from QQuickWindow at runtime
- DPI computed at runtime: diagonal_pixels / screen_size_inches

### UiMetrics Math
- Replace current resolution-ratio baseline entirely — no more hardcoded 1024/600 divisor
- DPI-based: compute actual DPI from (diagonal_pixels / diagonal_inches), scale = actual_DPI / 160
- Reference DPI is 160 PPI (Android mdpi standard) — a 7" 1024x600 screen will be ~1.06x instead of exactly 1.0, which is acceptable
- Preserve dual-axis approach: min(H,V) for layout overflow safety, sqrt(H*V) for balanced font readability — DPI replaces the baseline denominator, not the dual-axis logic
- UiMetrics updates live when physical screen size config changes (reactive QML bindings already exist, just needs DPI input as reactive property)

### Display Settings UI
- Screen size is NOT editable in settings — set once at install, edit YAML to change
- Read-only informational display at top of Display settings page (first item, before brightness)
- Shows both size and computed DPI: "Screen: 7.0" / 170 PPI"
- DPI value updates live if config changes

### Claude's Discretion
- Exact EDID parsing implementation details (error handling for malformed EDID)
- How DisplayInfo exposes physical size to QML (new property or new service)
- Config key naming beyond `display.screen_size`
- Read-only display styling in settings

</decisions>

<specifics>
## Specific Ideas

- The installer prompt should feel natural in the existing TUI flow — not a separate "display setup" step, just part of hardware detection
- 160 PPI reference aligns with Android's density-independent pixel system, which is fitting for an Android Auto head unit
- Users who care about precision can edit the YAML; settings UI is for visibility, not control

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `UiMetrics.qml`: Singleton with ~50 computed properties, dual-axis scaling, per-token overrides via ConfigService — core of the scaling system, needs formula change not rewrite
- `DisplayInfo.hpp/cpp`: Simple C++ width/height bridge — needs physical size property added
- `ConfigService`: Already reads `ui.scale` and `ui.fontScale` — needs `display.screen_size` support
- Installer TUI: Step infrastructure, spinners, prompts all in place — EDID probe fits in existing hardware detection step

### Established Patterns
- Config values read via `ConfigService.value("path.to.key")` in QML
- UiMetrics reads config values reactively and recomputes all tokens
- Hardware detection in installer uses bash tool parsing (e.g., `iw phy` for WiFi caps)

### Integration Points
- `install.sh` Step 5 (hardware detection): Add EDID probe, screen size prompt, write to config
- `UiMetrics.qml`: Replace `scaleH = windowWidth / 1024.0` with DPI-based computation
- `DisplayInfo.hpp`: Add `screenSizeInches` Q_PROPERTY (read from config, exposed to QML)
- `DisplaySettings.qml`: Add read-only screen info row at top of page
- `config/defaults.yaml` or `initDefaults()`: Set default `display.screen_size: 7.0`

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-dpi-foundation*
*Context gathered: 2026-03-08*
