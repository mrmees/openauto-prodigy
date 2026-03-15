# Phase 04: Grid Data Model & Persistence - Context

**Gathered:** 2026-03-12
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace the pane-based WidgetPlacementModel with a cell-based grid model. The grid stores widget placements as (col, row, colSpan, rowSpan) with occupancy tracking and collision detection. YAML persistence and resolution portability are included. Config migration from v0.5.2 is explicitly descoped — no current users.

</domain>

<decisions>
## Implementation Decisions

### Grid Density
- **6x4 grid** (6 columns × 4 rows) for the 1024x600 display
- Grid density **scales with display size** via UiMetrics — other resolutions get proportional grids (e.g. 800x480 → 5x3)
- Launcher dock **floats below** the grid area (independent, not consuming grid rows)
- **Visible gaps** between grid cells (using UiMetrics.gridGap) — glass card aesthetic with breathing room

### Config Migration
- **Dropped entirely** — no current users to migrate
- If old v0.5.2 pane-based config is detected, ignore it and write fresh grid defaults
- GRID-07 requirement descoped from this milestone

### Resolution Portability
- **Clamp to edges** when saved layout doesn't fit current grid dimensions (e.g. widget at col 5 on a 5-column grid → col 4)
- Overlap conflicts resolved by **first-placed wins** — late-comers get hidden (removed from visible grid, kept in config for when space returns)
- Build clamping logic into the model **now** (not deferred)

### Widget Size Constraints
- **Flexible min/max ranges** — widgets declare wide (minCols, minRows, maxCols, maxRows)
- Each WidgetDescriptor includes **defaultCols/defaultRows** for initial placement size
- **Full-width spanning allowed** — a widget can span all 6 columns on 1024x600
- Replaces the binary Main/Sub size enum entirely

### Background Layers
- **Both grid-cell and widget-level backgrounds** — grid cell provides default glass card (WidgetHost), widgets can also render their own internal backgrounds
- Grid-cell glass card opacity adjustable per-cell (can be set to 0% to hide)
- Widgets that want custom shapes (circular bg, transparent, etc.) handle it internally

### Phase 06 Widget Stubs
- **Pre-register** nav turn-by-turn and unified now playing widget descriptors now
- IDs, names, icons, and size constraints defined; QML component URL left empty or placeholder
- Phase 06 fills in the actual QML implementation

### Claude's Discretion
- Exact grid density formula (how UiMetrics derives columns/rows from display dimensions)
- Occupancy tracking algorithm implementation
- YAML config schema structure for grid placements
- Default widget layout arrangement for fresh installs
- WidgetPickerModel changes (filter by grid space available vs old size enum)

</decisions>

<specifics>
## Specific Ideas

- Target grid sizes: 800x480 → ~5x3, 1024x600 → 6x4, larger displays scale up proportionally
- Clock widget: min 1x1, max 6x4, default 2x2
- AA Status widget: min 1x1, max 3x2, default 2x1
- Now Playing widget: min 2x1, max 6x2, default 3x2
- Nav Turn widget (stub): min 2x1, max 4x2, default 3x1
- Unified Now Playing (stub): min 2x1, max 6x2, default 3x2

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- **WidgetPlacementModel** (`src/ui/WidgetPlacementModel.hpp`): Being replaced, but signal patterns (placementsChanged, auto-save) carry over
- **WidgetDescriptor** (`src/core/widget/WidgetTypes.hpp`): Needs min/max/default cols/rows fields added; drop WidgetSize enum
- **WidgetRegistry** (`src/core/widget/WidgetRegistry.hpp`): Descriptor registration unchanged, filter method needs update (size enum → grid space check)
- **WidgetInstanceContext** (`src/ui/WidgetInstanceContext.hpp`): Adapt from paneSize enum to grid cell dimensions
- **YamlConfig** (`src/core/YamlConfig.hpp`): Deep merge + auto-save pattern reused; schema version bump needed
- **UiMetrics** (`qml/controls/UiMetrics.qml`): Already has `gridGap`, `DisplayInfo.windowWidth/Height` — add column/row count derivation

### Established Patterns
- **Auto-save on change**: WidgetPlacementModel::placementsChanged → YamlConfig::save (wired in main.cpp)
- **QAbstractListModel**: WidgetPlacementModel already is one — new grid model should follow same pattern
- **Config defaults gate**: All new config keys need `initDefaults()` entries or `setValueByPath` silently fails

### Integration Points
- **HomeMenu.qml**: Currently renders 3 fixed WidgetHost items — Phase 05 will convert to Repeater + manual positioning
- **WidgetHost.qml**: Glass card + Loader pattern reusable; needs grid cell dimensions instead of pane ID
- **main.cpp lines 370-397**: Widget registration — add Phase 06 stubs here
- **WidgetPickerModel**: Filter needs to change from size enum to "fits in available grid space"

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-grid-data-model*
*Context gathered: 2026-03-12*
