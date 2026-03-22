# Phase 09: Widget Descriptor & Grid Foundation - Context

**Gathered:** 2026-03-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Enrich WidgetDescriptor with manifest metadata (category, description, icon) and display it in the widget picker. Replace fixed-pixel grid math with DPI-based physical sizing that produces square cells with consistent density across all target displays. Add YAML grid migration infrastructure so changing grid dimensions preserves existing widget placements. Remove hardcoded dock height deduction from grid math.

</domain>

<decisions>
## Implementation Decisions

### Widget categories
- Canonical category ID (lowercase, singular) + display label — not pure free-form strings
- Built-in IDs: `status` ("Status"), `media` ("Media"), `navigation` ("Navigation"), `launcher` ("Launcher")
- Plugins should use existing IDs when they fit; can register new IDs when they don't (e.g., `vehicle` for OBD)
- Prevents fragmentation from case/plurality/wording disagreements ("Status" vs "status" vs "System Status")
- Picker groups widgets by category ID — categories grow as plugins register, but stay canonical
- Uncategorized widgets fall into an "Other" group

### Widget description
- Short tagline format, 2-5 words max (not full sentences)
- Fits on one line under the widget name in picker cards
- Examples: Clock → "Current time", AA Status → "Connection status", Now Playing → "Track info & controls", Nav Turn → "Turn-by-turn directions"

### Picker display
- Grouped cards layout: category headers above horizontal card rows
- Each card shows icon + name + short description
- Categories with a single widget still get a header for consistency
- Replaces current flat horizontal card strip

### Physical cell sizing (from Codex design session)
- **Square cells always** — single cellSide value, no separate width/height
- **DPI from QScreen, not window size** — physical sizing uses `QScreen` physical dimensions (panel-level truth), not app window rect; DPI is a display property, not a window property
- **Physical sizing when trustworthy** — use QScreen physical DPI; cellSide derived as proportion of screen diagonal in pixels
- **DPI sanity range: 80-300** — outside this range, fall back to pixel heuristic targeting the same square-cell band
- **`display.screen_size` config is a hard override** — if set, it overrides QScreen physical dimensions unconditionally; this is the calibration path for panels that report plausible-but-wrong sizes (many cheap touch panels misreport within 80-300 DPI range); already exists in config, just needs to remain authoritative
- **DPI priority cascade:** `display.screen_size` config (user override) → QScreen physical dimensions → pixel heuristic fallback
- **Windowed mode** — when window is not fullscreen, physical sizing may not apply (window rect != panel rect); use pixel heuristic fallback in windowed/development mode
- **Pixel heuristic fallback** — produces equivalent square cells when DPI is missing, implausible, or in windowed mode
- **Both cols and rows derive freely** — cols = floor(usableW / cellSide), rows = floor(usableH / cellSide); neither dimension is anchored or capped
- **Floor minimums: 3 cols / 2 rows** — prevents nonsensical grids on tiny windows or extreme aspect ratios
- **Centered grid with even gutters** — grid frame does not stretch to fill; leftover space becomes symmetric margins (offsetX, offsetY)
- **Runtime recompute** — resize triggers live grid spec recalculation
- **`home.gridDensityBias`** config: -1 | 0 | +1 (roomy / standard / dense) — nudges target cell size up or down, not a column count override; controlled escape hatch for weird screens

### Grid math responsibility split
- **DisplayInfo.cpp** computes **cellSide only** from DPI/diagonal (the physical sizing math) and exposes it as a Q_PROPERTY; does NOT compute cols/rows/offsets (it doesn't know the actual available rect after navbar, page indicators, margins)
- **HomeMenu.qml** (or a QML helper) owns the **grid frame** — computes cols, rows, offsetX, offsetY from its actual available rect + cellSide from DisplayInfo; this is where navbar/dock/indicator space is accounted for
- **WidgetGridModel.cpp** owns logical placements in integer grid units only — no pixel awareness; receives cols/rows from QML and applies remap rule when they change
- **Boot sequence:** Model loads base placements + saved dims from YAML at startup but does NOT derive live placements yet; model waits for QML to provide current cols/rows (after layout completes and available rect is known); first remap produces live placements; no provisional layout rendered before QML dims arrive — grid is invisible until the first real layout is computed (avoids first-frame junk and duplicate remaps)
- **UiMetrics.qml** remains general UI scaling — widget-grid geometry does NOT depend on global UI scale except as fallback input when physical sizing is unavailable

### Grid migration on dimension change
- **Proportional position remap** — when computed grid dims differ from saved dims, scale positions proportionally (col 3 of 6 → col 4 of 8); overlaps resolved by nudging
- **Spans stay original size** — a 2x2 widget stays 2x2 on a bigger grid; cells are physically bigger so content scales naturally via UiMetrics
- **Shrink failure policy** — when shrinking, clamp position to fit within bounds; if clamping creates overlap, spill widget to next page (auto-create page if needed); if widget min span exceeds grid dimensions entirely (e.g., 4-wide widget on 3-col grid), mark hidden with warning — never silently delete
- **Store source grid dims in YAML** — save grid_cols/grid_rows alongside placements (not an abstract version number); on load, if current computed dims differ from saved, trigger remap
- **Base snapshot is source of truth** — YAML placements are the canonical layout; every remap re-derives live placements from this base snapshot, never from current mutated state; prevents drift from repeated resize cycles (small → large → small accumulating nudges/spills)
- **Remap is runtime-only by default** — remapped layout displays live but does NOT auto-persist to YAML; only persists when user explicitly edits (moves/resizes/adds a widget in edit mode), which updates both live state AND the YAML base snapshot with current grid dims
- **No migration code per-version** — the saved dims ARE the version; remap is a single generic algorithm

### Dock height removal
- **Remove 56px deduction now** in Phase 09 — grid math no longer knows about dock
- **Dock overlaps grid via z-order** — dock renders at z=10 on top of grid at z=0
- **Bottom-row widgets partially obscured** by dock until Phase 10 removes it — acceptable transitional state
- **No grid-side awareness of dock** — grid doesn't know or care about the dock; bottom-row overlap is a temporary visual artifact, not a grid logic concern; Phase 10 deletes dock and the bottom row is fully revealed

### Claude's Discretion
- Exact diagonal divisor constant for cell sizing (researcher tests values across target displays)
- DPI fallback heuristic formula for windowed/non-fullscreen mode
- Remap nudge algorithm for resolving overlaps after proportional remapping
- Page spill strategy when shrink-remap can't fit widgets on current page
- Picker card sizing, spacing, and scroll behavior within the grouped layout
- Category sort order in picker (alphabetical vs predefined)
- gridDensityBias effect magnitude (how much cellSide changes per step)
- Whether QML grid frame logic lives in HomeMenu.qml directly or a dedicated GridFrame helper

</decisions>

<specifics>
## Specific Ideas

- Codex design session established the physical sizing model: diagonal-proportional cells, DPI sanity gating, density bias control
- Codex review identified key refinements: grid frame ownership belongs in QML (not DisplayInfo), remap must be runtime-only (not auto-persisted), DPI must come from QScreen (not window rect), shrink needs explicit failure policy
- "I want to shoot for square grid cells" — no rectangular cells, ever
- "A 7" screen may be good with a 1/2" grid, but a 10" or 12" screen that starts to feel like micromanaging" — grid density should feel appropriate for screen size, not maximize cell count
- gridDensityBias is explicitly NOT a column count control — it's a nudge on cell size
- Widget pixel breakpoints (ClockWidget, NowPlayingWidget) are explicitly Phase 11 scope (WF-04) — they will be inconsistent during Phase 09-10, and that's the intended phase boundary

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `DisplayInfo.cpp`: Already computes gridColumns/gridRows via updateGridDimensions() — needs rewrite to compute cellSide only (physical sizing); cols/rows/offsets move to QML
- `UiMetrics.qml`: Has `_dpi` computed from EDID + screen_size config — DPI value available for physical sizing path; should use QScreen for panel-level DPI
- `WidgetPickerModel`: Exposes DisplayNameRole/IconNameRole — needs CategoryRole/DescriptionRole additions
- `WidgetPicker.qml`: Horizontal card strip — needs rewrite to grouped-by-category layout
- `WidgetGridModel::setGridDimensions()`: Has clamping logic for dimension changes — needs proportional remap algorithm instead of clamp-and-hide
- `YamlConfig::gridPlacements()/setGridPlacements()`: YAML schema v3 serialization — needs grid_cols/grid_rows fields

### Established Patterns
- Widget QML files require `QT_QML_SKIP_CACHEGEN TRUE` in CMake
- WidgetDescriptor structs registered in main.cpp with size constraints
- UiMetrics tokens used throughout QML for all sizing (no hardcoded pixels)
- MaterialIcon uses `icon:` property with codepoints

### Integration Points
- `DisplayInfo` → `HomeMenu.qml`: cellSide Q_PROPERTY drives QML grid frame computation
- `HomeMenu.qml` → `WidgetGridModel`: passes computed cols/rows; dimension change triggers remap
- `WidgetDescriptor` → `WidgetPickerModel`: new fields exposed as model roles
- `YamlConfig`: grid_cols/grid_rows persisted alongside placements for migration detection
- `Shell.qml`: dock z-order change (dock above grid, not beside it)

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 09-widget-descriptor-grid-foundation*
*Context gathered: 2026-03-14*
