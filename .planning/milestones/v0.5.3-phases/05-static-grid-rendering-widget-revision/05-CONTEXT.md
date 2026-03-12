# Phase 05: Static Grid Rendering & Widget Revision - Context

**Gathered:** 2026-03-12
**Status:** Ready for planning

<domain>
## Phase Boundary

Home screen renders widgets positioned on the cell-based grid with correct sizing using a Repeater + manual positioning pattern over WidgetGridModel. Existing widgets (Clock, AA Status, Now Playing) are revised to use pixel-based breakpoints instead of the isMainPane boolean, adapting content to variable grid cell dimensions. Edit mode interaction is Phase 07; content widgets are Phase 06.

</domain>

<decisions>
## Implementation Decisions

### Widget Content Tiers — Clock
- 1x1 (smallest): Large centered time only, bold weight, auto-sized to fill the cell. No AM/PM — respects existing 24h toggle.
- 2x1 (wide): Time + date (e.g. "March 12")
- 2x2+ (full): Time + date + day of week (e.g. "Wednesday") — three distinct lines, each scaling with available space

### Widget Content Tiers — AA Status
- 1x1 (smallest): Single connection state icon filling the cell. Green phone-link when connected, grey/muted when disconnected. Tapping launches AA.
- 2x1+ (wide): Icon + status text (e.g. "Connected", "Tap to connect")
- No animation — static icon that changes color with state

### Widget Content Tiers — Now Playing
- 2x1 (compact strip): Track title (truncated if needed) + play/pause button. Artist is cut at this size.
- 3x2 (full layout): Title, artist, prev/play/next controls — full media card

### Empty Grid Appearance
- **Normal use:** Pure wallpaper — empty cells show nothing. Widgets float on the wallpaper like glass cards on a desktop.
- **Edit mode (Phase 07):** Subtle cell outlines appear so users can see where to drop widgets. Normal use stays clean.
- **Zero-widget state:** Just wallpaper + launcher dock. No hint text, no prompts. Clean and intentional.

### Default Layout
- **Blank canvas on fresh install** — no default widgets placed
- Default layout will be revisited after widget redesign is complete (post Phase 06/07) when all widgets can be evaluated at their final sizes
- GRID-06 requirement deferred to later in the milestone

### Glass Card Styling
- **Corner radius:** Fixed (UiMetrics.radius) regardless of widget size — consistent look across 1x1 through 6x4
- **Border:** None — drop the 1px outlineVariant border. Glass background opacity alone defines widget bounds.
- **Default opacity:** 25% surfaceContainer (unchanged from v0.5.2). Per-widget opacity remains adjustable via edit mode.
- **Gutters:** Medium (~8px, UiMetrics.spacing) between adjacent widget cards. Applied as half-spacing margins on each cell (4px per side = 8px between neighbors).

### Claude's Discretion
- Exact pixel breakpoint thresholds (research suggests ~250px width for "show text", ~200px height for "show full")
- GridCell inline delegate vs separate QML file
- Math.floor vs Math.round for cell dimension computation
- WidgetPlacementModel C++ class cleanup timing (can defer past this phase)
- Font sizing ratios within breakpoint tiers

</decisions>

<specifics>
## Specific Ideas

- Clock at 1x1 should feel bold and glanceable — the time IS the widget at that size
- Glass cards without borders = cleaner, more modern feel. If readability suffers on certain wallpapers, per-widget opacity is the lever (not borders).
- Empty home screen should feel intentional, not broken — like a fresh phone home screen before you add widgets
- GRID-06 (default layout) explicitly punted — blank canvas until all widgets are finalized

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- **WidgetGridModel** (`src/ui/WidgetGridModel.hpp`): Complete from Phase 04 — exposes column, row, colSpan, rowSpan, qmlComponent, opacity, visible, instanceId roles
- **WidgetHost.qml** (`qml/components/WidgetHost.qml`): Glass card + Loader pattern. Needs adaptation: remove paneId/WidgetPlacementModel refs, remove border rectangle, accept opacity from model
- **UiMetrics.qml**: Has spacing (8px at 1.0 scale), radius, marginPage, fontHeading — all needed for grid rendering
- **DisplayInfo**: Exposes gridColumns/gridRows as Q_PROPERTYs from Phase 04
- **LauncherDock.qml**: Fixed height (~UiMetrics.tileH * 0.5 + spacing). Grid container must reserve space for it.

### Established Patterns
- **QT_QML_SKIP_CACHEGEN**: Required on all QML files loaded via Loader (known gotcha). Existing widgets already have this.
- **UiMetrics-relative sizing**: Zero hardcoded pixel values in QML — all sizes derive from UiMetrics tokens
- **ColumnLayout + Layout.fillHeight**: Used for stacking grid area above dock

### Integration Points
- **HomeMenu.qml**: Complete rewrite — 3-pane layout replaced with Repeater over WidgetGridModel
- **main.cpp**: WidgetGridModel already exposed as QML context property. WidgetPlacementModel context property already removed (Phase 04-02).
- **ClockWidget.qml / AAStatusWidget.qml / NowPlayingWidget.qml**: Replace isMainPane/paneSize checks with pixel-dimension breakpoints
- **WidgetContextMenu / WidgetPicker Loaders**: Remove from HomeMenu (edit mode features, Phase 07 reintroduces)

</code_context>

<deferred>
## Deferred Ideas

- **Default widget layout (GRID-06):** Revisit after Phase 06/07 when all widgets exist at final sizes — pick a default arrangement then
- **Edit mode grid hints:** Phase 07 will implement subtle cell outlines visible only during edit mode

</deferred>

---

*Phase: 05-static-grid-rendering-widget-revision*
*Context gathered: 2026-03-12*
