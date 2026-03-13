# Phase 08: Multi-Page - Context

**Gathered:** 2026-03-13
**Status:** Ready for planning

<domain>
## Phase Boundary

Users can organize widgets across multiple swipeable home screen pages. Horizontal swipe navigation between pages, page indicator dots, page creation/deletion, lazy instantiation for performance. Launcher dock remains fixed across all pages. Does NOT include a dedicated system widgets page (future milestone).

</domain>

<decisions>
## Implementation Decisions

### Page navigation feel
- Horizontal swipe between pages with smooth glide animation (~350ms)
- Rubber band overscroll at edges (elastic snap-back at first/last page)
- Standard filled/unfilled dot indicators centered between grid and dock
- Active dot = primary/accent color, inactive = muted
- Page indicator dots always visible, even with a single page

### Page management
- Explicit "add page" button in edit mode (FAB, separate from add-widget FAB) — no auto-create on overflow
- No maximum page limit — user can create as many as they want
- Empty pages auto-cleaned on edit mode exit
- Explicit "delete page" button also available in edit mode — removes page and all its widgets
- Delete-page requires confirmation dialog ("Delete page and N widgets?")
- Page 1 cannot be deleted (always at least one page)

### Edit mode + pages
- Edit mode is page-scoped — only affects the currently visible page
- No cross-page navigation during edit mode (exit edit, swipe, long-press to edit another page)
- Swipe gesture disabled during edit mode (prevents conflict with drag-to-reposition)
- Add-page FAB only visible during edit mode
- Full page behavior: toast "No space available — remove a widget first" (same as Phase 07)

### Config schema
- No migration needed (alpha, no current users)
- Add `page` field to GridPlacement (schema v3)
- Fresh install: 2 blank pages

### Claude's Discretion
- SwipeView vs Flickable vs custom page container implementation
- Add-page FAB placement relative to add-widget FAB
- Delete-page button/FAB placement and styling
- Confirmation dialog visual treatment
- Dot indicator sizing (visible but appropriately small for 7" screen)
- Lazy instantiation strategy for non-visible pages
- Page transition easing curve
- Whether dots are tappable for page navigation in normal mode (not required)

</decisions>

<specifics>
## Specific Ideas

- "The natural extension is going to be eliminating the navbar for a hardcoded, undeleteable page of dedicated prodigy widgets" — this drives the always-visible dots decision
- "We'll populate the second page with system widgets and remove the default AA/phone/media/settings bar in a future milestone"
- Page dots should be visible but not necessarily large enough to be reliable touch targets on a 7" screen

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `WidgetGridModel`: Has all grid operations (place/move/resize/remove/canPlace/occupancy) — needs page-scoping
- `GridPlacement` struct: Needs `page` field addition
- `YamlConfig`: Has `gridPlacements()`/`setGridPlacements()` — needs page-aware schema v3
- `HomeMenu.qml`: Current single-page grid + edit mode — needs SwipeView/page container wrapper
- `LauncherDock.qml`: Fixed height ColumnLayout child — needs to stay outside page container
- Edit mode infrastructure: long-press entry, FABs, drag overlay (z:200), 10s timeout — all page-scoped

### Established Patterns
- Overlay pattern: scrim Rectangle + floating content (contextMenuOverlay, pickerOverlay)
- Glass card pattern: surfaceContainer background with configurable opacity
- UiMetrics: All sizing tokens available (no hardcoded pixels)
- ThemeService: M3 color roles for active/inactive dot colors
- Atomic config writes: `placementsChanged` signal triggers YAML save

### Integration Points
- `WidgetGridModel` needs page filtering (current model is flat list — needs active page concept)
- `main.cpp`: Model instantiation and QML context property registration
- `EvdevTouchReader`: EVIOCGRAB coordination — edit mode auto-exit already handled
- `ApplicationController`: May need current page tracking for state persistence
- Edit mode timeout (10s) already exists — no changes needed for page-scoped behavior

</code_context>

<deferred>
## Deferred Ideas

- Dedicated system widgets page replacing the navbar — future milestone
- Page reordering via drag on page indicator dots (already in REQUIREMENTS.md as POLISH-03)
- Drag widgets between pages (explicitly out of scope in REQUIREMENTS.md)

</deferred>

---

*Phase: 08-multi-page*
*Context gathered: 2026-03-13*
