# Phase 3: QML Hardcoded Island Audit - Context

**Gathered:** 2026-03-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace all hardcoded pixel values across ~20 QML files (~175+ instances) with UiMetrics tokens. Zero hardcoded pixel values remain in QML (excluding intentional debug overlays). This phase does NOT change layout behavior or grid flow (Phase 4) or add runtime adaptation (Phase 5). It tokenizes what exists.

</domain>

<decisions>
## Implementation Decisions

### Token inventory strategy
- Minimal new tokens — add only what's needed to eliminate hardcoded values (~6-8 new tokens)
- Reuse existing tokens where close enough (e.g., spacing uses padS/padM/padL/padXL)
- New tokens follow Phase 1 pattern: overridable via `ui.tokens.*`, registered in YamlConfig initDefaults()
- All new tokens added upfront in Plan 1 before any file edits — Plans 2 and 3 just consume them

### Audit batching (3 plans by UI layer)
- **Plan 1:** New UiMetrics tokens + shell chrome (TopBar, BottomBar/NavStrip, Shell)
- **Plan 2:** Plugin views (PhoneView, BtAudioView, GestureOverlay, IncomingCallOverlay)
- **Plan 3:** Controls & dialogs (Tile, PairingDialog, NormalText, SpecialText, HomeMenu, remaining files)
- Final plan includes grep-based verification confirming zero hardcoded pixel literals remain

### Proportional scaling with safety floors
- Album art, call buttons, overlay buttons, icons — all scale proportionally with UiMetrics
- Call buttons and overlay buttons have touch-target floors (never below touchMin)
- Icons use existing UiMetrics.iconSize token — Material Symbols scales via font metrics
- GestureOverlay scales proportionally but button height never drops below touchMin (44px)

### Claude's Discretion
- Button token strategy: one generic touchMin-based approach vs separate touchMin + touchLarge tokens
- Divider height: always 1px or scale with resolution (based on target resolution range)
- Radius strategy: standardize to a 3-4 tier design scale vs tokenize existing values (consider: 8/9/11/12 differences don't matter at car distance)
- Spacing standardization: hybrid approach — fix obvious oddities (87px), keep reasonable values, snap to 4px grid where sensible
- Whether pad tokens cover spacing needs or separate concept warranted
- Status indicators and progress bar heights: dedicated tokens vs reuse of existing small spacing tokens
- Album art sizing approach: proportional to content area vs fixed token with UiMetrics scaling
- Call button size floor value (e.g., 56px minimum)
- Whether settings sub-views (AudioSettings, DisplaySettings, etc.) are included in full or deprioritized
- Exact list of new tokens beyond the categories identified

</decisions>

<specifics>
## Specific Ideas

- UiMetrics already has padS (4), padM (8), padL (16), padXL (24) — spacing should reuse these rather than adding a parallel naming scheme
- Phase 1 precedence chain preserved: token override > fontScale > globalScale > autoScale
- Final verification: grep for `pixelSize: [0-9]`, `width: [0-9]`, `height: [0-9]`, `radius: [0-9]`, `spacing: [0-9]`, `Margin: [0-9]` patterns — pass means zero matches outside debug overlays

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `UiMetrics.qml` (pragma Singleton): ~15 existing tokens including padS/M/L/XL, fontBody/Small/Title/Heading, iconSize, touchMin, headerH, rowH, tileW/tileH, trackThick/trackThin/knobSize
- `_tok()` helper: config override pattern — `var o = _tok("tokenName"); return isNaN(o) ? Math.round(BASE * scale) : o;`
- `NormalText.qml` / `SpecialText.qml`: text controls that should reference UiMetrics font tokens (currently hardcoded)
- `ConfigService.value()`: QML-accessible config reads for token overrides

### Established Patterns
- All tokens scale via `Math.round(BASE * layoutScale)` or `Math.round(BASE * fontScale)` depending on type
- Font tokens have pixel floors via `Math.max(floor, computed)` (Phase 2)
- Token overrides bypass scaling entirely — absolute pixel values (Phase 1)

### Integration Points
- `qml/controls/UiMetrics.qml` — add new tokens in Plan 1
- `src/core/YamlConfig.cpp` initDefaults() — register new token defaults
- Every QML file with hardcoded values — the bulk of the work

### Files with most hardcoded values (worst offenders)
- `GestureOverlay.qml` — 20+ magic numbers (fonts, radius, spacing, button dims)
- `PhoneView.qml` — 15+ values (fonts 12/14/18/24/28, status dot, margins)
- `IncomingCallOverlay.qml` — fonts, 72x72 buttons, spacing
- `BtAudioView.qml` — 15+ values (album art 200x200, fonts, progress bar, spacing including 87px)
- `TopBar.qml` / `BottomBar.qml` — margin/spacing hardcoded (8, 12, 16, 20)

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 03-qml-hardcoded-island-audit*
*Context gathered: 2026-03-03*
