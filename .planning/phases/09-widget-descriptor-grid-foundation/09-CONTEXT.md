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
- Free-form string field on WidgetDescriptor — not a fixed enum
- Plugins declare any category string in their manifest (e.g., "Status", "Media", "Navigation", "Vehicle")
- Picker groups widgets by unique category values — categories grow organically as plugins register widgets
- No "Utility" catchall — uncategorized widgets fall into an "Other" group
- Built-in categories for shipped widgets: Status (Clock, AA Status), Media (Now Playing), Navigation (Nav Turn-by-Turn)

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
- **Physical sizing when trustworthy** — use `display.screen_size` (inches) → DPI; cellSide derived as proportion of screen diagonal in pixels
- **DPI sanity range: 80-300** — outside this range, fall back to pixel heuristic targeting the same square-cell band
- **Pixel heuristic fallback** — produces equivalent square cells when DPI is missing or implausible
- **Both cols and rows derive freely** — cols = floor(usableW / cellSide), rows = floor(usableH / cellSide); neither dimension is anchored or capped
- **Centered grid with even gutters** — grid frame does not stretch to fill; leftover space becomes symmetric margins (offsetX, offsetY)
- **Runtime recompute** — resize triggers live grid spec recalculation
- **`home.gridDensityBias`** config: -1 | 0 | +1 (roomy / standard / dense) — nudges target cell size up or down, not a column count override; controlled escape hatch for weird screens

### Grid math responsibility split
- **DisplayInfo.cpp** computes full grid spec: gridColumns, gridRows, gridCellSide, gridOffsetX, gridOffsetY, gridWidth, gridHeight
- **HomeMenu.qml** renders from grid spec directly — no `pageView.width / cols` division; uses cellSide + offsets from DisplayInfo
- **WidgetGridModel.cpp** owns logical placements in integer grid units only — no pixel awareness; receives cols/rows and applies remap rule when they change
- **UiMetrics.qml** remains general UI scaling — widget-grid geometry does NOT depend on global UI scale except as fallback input when physical sizing is unavailable

### Grid migration on dimension change
- **Proportional position remap** — when computed grid dims differ from saved dims, scale positions proportionally (col 3 of 6 → col 4 of 8); overlaps resolved by nudging
- **Spans stay original size** — a 2x2 widget stays 2x2 on a bigger grid; cells are physically bigger so content scales naturally via UiMetrics
- **Store source grid dims in YAML** — save grid_cols/grid_rows alongside placements (not an abstract version number); on load, if current computed dims differ from saved, trigger remap, update saved dims, save
- **No migration code per-version** — the saved dims ARE the version; remap is a single generic algorithm

### Dock height removal
- **Remove 56px deduction now** in Phase 09 — DisplayInfo computes grid over full screen height
- **Dock overlaps grid via z-order** — dock renders at z=10 on top of grid at z=0
- **Bottom-row widgets partially obscured** by dock until Phase 10 removes it — acceptable transitional state
- **No grid-side awareness of dock** — grid doesn't know or care about the dock; Phase 10 deletes dock and the bottom row is fully revealed

### Claude's Discretion
- Exact diagonal divisor constant for cell sizing (researcher tests values across target displays)
- DPI fallback heuristic formula
- Remap nudge algorithm for resolving overlaps after proportional remapping
- Picker card sizing, spacing, and scroll behavior within the grouped layout
- Category sort order in picker (alphabetical vs predefined)
- gridDensityBias effect magnitude (how much cellSide changes per step)

</decisions>

<specifics>
## Specific Ideas

- Codex design session established the physical sizing model: diagonal-proportional cells, DPI sanity gating, density bias control, strict responsibility split between DisplayInfo/HomeMenu/WidgetGridModel/UiMetrics
- "I want to shoot for square grid cells" — no rectangular cells, ever
- "A 7" screen may be good with a 1/2" grid, but a 10" or 12" screen that starts to feel like micromanaging" — grid density should feel appropriate for screen size, not maximize cell count
- gridDensityBias is explicitly NOT a column count control — it's a nudge on cell size

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `DisplayInfo.cpp`: Already computes gridColumns/gridRows via updateGridDimensions() — needs rewrite to full grid spec with square cells + offsets
- `UiMetrics.qml`: Has `_dpi` computed from EDID + screen_size config — DPI value available for physical sizing path
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
- `DisplayInfo` → `WidgetGridModel`: dimension change signal triggers remap
- `DisplayInfo` → `HomeMenu.qml`: grid spec (cellSide, offsets) drives rendering
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
