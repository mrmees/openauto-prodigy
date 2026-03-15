# Phase 3: Theme Color System - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

All UI elements draw from a complete, consistent semantic color palette aligned with the 16 Material Design tokens that Android Auto sends on the wire. ThemeService properties are renamed to match AA token names, all hardcoded QML colors are eliminated, existing themes are migrated, and a "Connected Device" dynamic theme receives live colors from the phone. Requirements: THM-01, THM-02, THM-03.

</domain>

<decisions>
## Implementation Decisions

### Color Palette: AA Wire Token Alignment
- Semantic color system matches the 16 tokens AA sends via UiConfigRequest (msg 0x8011): primary, on_surface, surface, surface_variant, surface_container_low, inverse_surface, inverse_on_surface, outline, outline_variant, background, text_primary, text_secondary, red, on_red, yellow, on_yellow
- ThemeService accepts all 46 known token keys by string (future-proofing) but only depends on the 16 confirmed wire tokens
- Colors beyond the 16 (scrim, pressed state, success green, etc.) are derived from the 16 base tokens in ThemeService -- not stored in YAML
- Wire token names used directly: `red`/`onRed` not `error`/`onError`, `yellow`/`onYellow` not `warning`/`onWarning`

### Property Rename
- All 16 existing ThemeService Q_PROPERTYs renamed to AA token names (e.g., backgroundColor -> background, highlightColor -> primary, normalFontColor -> textPrimary)
- Clean break -- old property names stop working, no alias layer
- All QML references updated in the same pass
- Mapping documented in release notes for custom theme authors

### Connected Device Theme
- New theme option: "Connected Device" -- HU chrome adopts the phone's Material You palette when AA is connected
- Minimal YAML theme file with metadata + default fallback palette (copy of default theme colors)
- When AA sends UiConfigRequest tokens, ThemeService writes them directly into the Connected Device theme YAML file
- Theme file IS the cache -- persists last-received colors across launches/disconnects
- On next launch with no phone connected, loads the last-received colors from the theme file
- First-ever launch (no cached colors) falls back to the default palette in the theme YAML

### Hardcode Elimination
- All 59 hardcoded hex values across 12 QML files eliminated in this phase
- Every color reference uses ThemeService properties after migration
- GestureOverlay (17 hardcodes), IncomingCallOverlay (5), PairingDialog (3), ConnectionSettings (3), CompanionSettings (7), NotificationArea (5), ExitDialog (1), and others all migrated
- Overlay backgrounds, scrim, pressed states, status indicators all derive from the 16 base tokens
- GestureOverlay and other overlays adapt to theme (not fixed dark style)

### Theme YAML Structure
- Flat key-value pairs using AA token names: `primary: "#e94560"`, `on_surface: "#ffffff"`, etc.
- Day/night sections preserved (existing pattern)
- Only the 16 base tokens in YAML -- derived colors computed by ThemeService at runtime
- All 4 bundled themes (default, AMOLED, Ocean, Ember) migrated to new token names with full palettes
- Connected Device theme added as 5th bundled theme

### Backwards Compatibility
- Clean break on property names -- no migration logic for old custom themes
- Old user theme overrides in ~/.openauto/themes/ will have unrecognized keys (silently ignored, falls back to defaults)
- Release notes document old-to-new key mapping
- Installer not modified for cleanup -- users update their overrides manually

### Web Config Panel
- Flask web config panel theme routes updated to use new AA token names
- Keeps web panel consistent with ThemeService property names

### Claude's Discretion
- Exact derivation formulas for non-base colors (scrim opacity, pressed state dimming, etc.)
- Old-to-new property mapping decisions where the mapping isn't 1:1
- UiConfigRequest parsing implementation details (protobuf deserialization, ARGB uint32 -> QColor conversion)
- Test coverage approach for the color system migration
- How to handle the UiConfigRequest on the video channel (new handler method or extension of existing)
- Whether to send UiConfigRequest proactively on AA connect or wait for phone to request

</decisions>

<specifics>
## Specific Ideas

- The 16 AA wire tokens ARE the semantic color system -- don't invent roles AA doesn't provide
- "Connected Device" theme makes the whole experience feel unified with the phone's Material You palette
- Writing phone colors directly to the theme YAML file is elegant -- the file doubles as persistent cache
- Proto definition exists: `oaa/video/UiConfigRequestMessage.proto` (msg 0x8011, HU->Phone, gold confidence)
- APK analysis identified 46 total token keys (jrt.java) but only 16 appear on the wire today
- Phase 4 (Visual Depth) will build on this palette for shadow/gradient/bevel effects

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ThemeService.hpp/cpp`: 16 Q_PROPERTYs with day/night YAML loading, `activeColor(key)` pattern, `colorsChanged` signal -- needs property rename + AA token ingestion
- `UiConfigRequestMessage.proto`: Wire format for color tokens (string key + uint32 ARGB value), HU->Phone on video channel
- 4 bundled theme YAMLs (`config/themes/`): default, AMOLED, Ocean, Ember -- all need key migration
- `ConfigService`: YAML read/write with deep merge -- used for persisting Connected Device colors

### Established Patterns
- Theme colors accessed via `ThemeService.propertyName` in QML (direct Q_PROPERTY binding)
- Day/night switching via `setNightMode(bool)` with `colorsChanged` signal
- Theme YAML structure: `id`, `name`, `font_family`, `day: { ... }`, `night: { ... }`
- Unknown YAML keys silently ignored (graceful degradation for old custom themes)

### Integration Points
- `ThemeService.hpp/cpp` -- rename properties, add AA token ingestion, add derived color computation
- `config/themes/*.yaml` -- migrate all 4 themes + add Connected Device theme
- 12 QML files with hardcoded colors -- full sweep to ThemeService references
- `web-config/` -- update theme routes to new token names
- Video channel handler -- parse UiConfigRequest and forward tokens to ThemeService
- `AndroidAutoOrchestrator` or `AndroidAutoService` -- wire UiConfigRequest to ThemeService

</code_context>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 03-theme-color-system*
*Context gathered: 2026-03-08*
