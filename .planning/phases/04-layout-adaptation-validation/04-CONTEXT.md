# Phase 4: Layout Adaptation + Validation - Context

**Gathered:** 2026-03-03
**Status:** Ready for planning

<domain>
## Phase Boundary

Grid layouts adapt to available space without overflow or clipping, validated at all target resolutions. This phase fixes layout overflow issues discovered when UiMetrics scales below 1024x600, adds defensive guards, closes AUDIT-10, and validates the full UI at multiple resolutions. Does NOT add runtime adaptation (Phase 5) or user-facing scale options (future milestone).

</domain>

<decisions>
## Implementation Decisions

### Launcher grid overflow (LAYOUT-01)
- Use BOTH dynamic column count AND container-derived tile width
- Container-derived tiles: tiles fill available width evenly (bigger screen = bigger tiles, no wasted space)
- Column count derived from available container width (not hardcoded `columns: 4`)
- Vertical scroll when tiles exceed visible area (10+ plugins expected long-term)
- Replace `GridLayout` with scrollable `GridView` or `Flow` in a `Flickable`
- At 800x480: grid naturally reduces to 3 columns with wider tiles instead of overflowing

### Settings tile grid (LAYOUT-02)
- Math checks out at 800x480: 3 cols * 219px + 2 * 19px = 695px fits in 800px container
- Apply same container-derived approach for consistency (even though it doesn't overflow today)
- Settings has fixed 6 tiles — no scroll needed, but grid should still derive from container width

### EQ slider minimum guard (LAYOUT-03)
- Add minimum track height guard (`Math.max(trackHeight, minimumTrackH)`)
- Works fine at 800x480 (295px track) — guard is defensive for extreme window sizes
- Claude's discretion on exact minimum value

### AUDIT-10 completion
- Close out the zero-hardcoded-pixels grep verification during Phase 4 validation sweep
- Final grep pass confirms zero `pixelSize: [0-9]`, `width: [0-9]`, `height: [0-9]` literals in QML (excluding debug overlays)

### Validation approach
- Create a reusable validation script that launches the app at target resolutions
- Matt does visual inspection (no automated screenshots needed)
- Test resolutions: 800x480, 1024x600, 1280x720 (required) PLUS edge cases: ultrawide (1920x480), portrait (480x800), tiny (480x272)
- Pass criteria: no visual clipping, overflow, or illegible text at any resolution
- Script uses Qt window size env vars or flags to set resolution on dev VM

### Claude's Discretion
- Exact column count breakpoint formula (e.g., `Math.floor(availableWidth / (preferredTileW + gap))`)
- Whether to use `GridView` (model-based, scrollable) vs `Flow` in `Flickable` for the launcher
- Minimum EQ track height value (e.g., 60px or 80px)
- Settings grid: whether to add container-derived sizing or leave as-is since it already fits
- Validation script implementation details (shell script vs QML test file)

</decisions>

<specifics>
## Specific Ideas

- Launcher currently uses `GridLayout` with `columns: 4` and `anchors.centerIn: parent` — at 800px the grid is 933px wide, clipped ~66px on each side
- Settings uses `GridLayout` with `columns: 3` — fits at 800x480 (695px) with 105px to spare
- EQ `trackHeight` is `height - labelAreaH - spacing*2` with no floor guard — goes negative at extreme sizes
- The `Tile.qml` component itself is clean (proportional icon, tokenized font) — only the parent grid layout needs fixing
- Phase 3 CONTEXT.md established: `_tok()` pattern for overridable tokens, `Math.round(BASE * scale)` for proportional sizing

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `UiMetrics.qml`: `tileW` (base 280), `tileH` (base 200), `gridGap` (base 24) — all scale with `layoutScale`
- `Tile.qml`: clean proportional component, icon scales as `Math.min(width, height) * 0.35`, font is `fontHeading`
- `EqBandSlider.qml`: `trackHeight` computed from parent height minus label area — just needs a `Math.max` guard

### Established Patterns
- All layout tokens scale via `Math.round(BASE * layoutScale)` (Phase 2)
- Token overrides bypass scaling — absolute pixel values (Phase 1)
- `autoScale = min(scaleH, scaleV)` unclamped (Phase 2)

### Integration Points
- `qml/applications/launcher/LauncherMenu.qml` — replace `GridLayout` with container-aware grid
- `qml/applications/settings/SettingsMenu.qml` — apply container-derived sizing for consistency
- `qml/controls/EqBandSlider.qml` — add `Math.max` guard on `trackHeight`
- `qml/controls/UiMetrics.qml` — may need new token for minimum EQ track height

### Key Math at 800x480 (autoScale = 0.781)
- `tileW = 219`, `tileH = 156`, `gridGap = 19`
- 4-col launcher: 933px (overflows 800px by 133px)
- 3-col settings: 695px (fits in 800px)
- EQ track height: ~295px (fine, just needs floor guard)

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 04-layout-adaptation-validation*
*Context gathered: 2026-03-03*
