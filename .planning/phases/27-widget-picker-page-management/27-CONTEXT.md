# Phase 27: Widget Picker & Page Management - Context

**Gathered:** 2026-03-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Long-press on empty grid space shows a menu with "Add Widget" and "Add Page" options. "Add Widget" opens a bottom-sheet widget picker with categorized scrollable list; tapping a widget auto-places it at the first available cell. "Add Page" creates a new page and navigates to it. All FABs removed (add widget, add page, delete page) — replaced by long-press empty menu and picker.

</domain>

<decisions>
## Implementation Decisions

### Long-press empty space menu (PGM-01, PGM-02, PGM-03)
- Long-press on empty grid area (500ms, matching widget long-press threshold) shows a contextual menu
- Menu contains exactly 2 options: "Add Widget" and "Add Page"
- "Add Widget" opens the bottom-sheet widget picker
- "Add Page" creates a new page via `WidgetGridModel.addPage()` and navigates to it
- Menu appears near the touch point (popup style, not full-screen overlay)
- **Gesture arbitration:** Empty-space long-press menu is ONLY available when no widget is selected (`selectedInstanceId === ""`). During active selection, drag, or resize, long-press on empty space does nothing (deselect is handled by tap, not long-press). This prevents state conflicts with selection, navbar widget mode, and auto-deselect timer.
- Menu is dismissed by: tap-outside, selecting either option, AA fullscreen activation, or page change
- `_addingPage` guard is NOT needed for add-page from this menu — nothing is selected when the menu fires, so there's no deselect side-effect to guard against

### Bottom sheet widget picker (PKR-01, PKR-02, PKR-03)
- Replaces the current inline `pickerOverlay` panel in HomeMenu.qml (the horizontal card strip at bottom)
- NOTE: `WidgetPicker.qml` is a separate unused component — NOT the currently shipped picker. The live picker is embedded inline in HomeMenu.qml as `pickerOverlay`.
- New picker slides up from bottom as a bottom sheet (like WidgetConfigSheet pattern — Dialog-based)
- Categorized scrollable list — widgets grouped by category with headers
- Each widget card shows: icon + name + description (existing WidgetPickerModel already exposes these roles)
- Tapping a widget auto-places it at first available grid cell via `WidgetGridModel.findFirstAvailableCell()` then `placeWidget()` — existing auto-place logic
- **PKR-03 clarification:** `WidgetPickerModel.filterByAvailableSpace()` only checks descriptor minimum size against grid dimensions — it does NOT check actual page occupancy. Real availability is checked at placement time by `findFirstAvailableCell()`. If `findFirstAvailableCell()` returns empty, show "No space available" feedback and don't close the picker. The existing filter is sufficient for PKR-03 (hiding widgets too large for the grid), but placement failure must be handled gracefully.
- **"No Widget" entry suppression:** `WidgetPickerModel.filterByAvailableSpace()` currently prepends a synthetic "No Widget" entry (empty widgetId). This is WRONG for the add-widget flow — passing empty widgetId to `placeWidget()` has no guard. Must either: suppress the "No Widget" entry when used for add-widget, or add a new filter method, or guard `placeWidget()` against empty widgetId.
- After successful placement, picker closes

### Overlay lifecycle (no-selection cleanup)
- With Phase 27, the picker and popup menu can be open while NO widget is selected. Current cleanup paths only fire when `selectedInstanceId !== ""`:
  - `deselectWidget()` closes `pickerOverlay` — won't fire if nothing is selected
  - AA fullscreen only deselects if `selectedInstanceId !== ""` — picker stays open
  - `selectionTimer` only runs when `selectedInstanceId !== ""` — no auto-dismiss
- **Required new cleanup paths:**
  - AA fullscreen activation must dismiss popup menu AND picker even when no widget is selected
  - Page change must dismiss popup menu
  - Navigating away from home (settings, plugin launch) must dismiss popup menu AND picker
  - The `widgetDeselectedFromCpp` signal path already covers navigation-away cases (Phase 25 fix), but only when something is selected. Need a parallel path for no-selection overlay dismissal.
- **selectionTimer binding:** Currently references `pickerOverlay.visible`. When pickerOverlay is replaced with the new bottom sheet, update the binding to reference the new component's visibility/open state.

### FAB removal (CLN-02)
- Remove add-widget FAB (bottom-right) — replaced by long-press empty → "Add Widget" → picker
- Remove add-page FAB (above add-widget) — replaced by long-press empty → "Add Page"
- Remove delete-page FAB (bottom-left, red) — already functionally replaced by PGM-04 (trash auto-deletes empty pages in Phase 26)
- Remove the delete-page confirmation dialog (no longer needed with auto-delete)
- Remove inline `pickerOverlay` — replaced by new bottom-sheet picker component

### Claude's Discretion
- Exact popup menu visual style (pill buttons, card with divider, or simple list)
- Bottom sheet height (half-screen, 60%, or adaptive based on widget count)
- Category sort order in the picker
- Whether to animate the popup menu appearance (fade, scale, or instant)
- Exact card layout within the bottom sheet (grid vs list)
- How to suppress "No Widget" entry (filter method parameter, separate method, or placeWidget guard)
- Whether to refactor WidgetPicker.qml into the new bottom sheet or build fresh (WidgetPicker.qml has reusable `rebuildCategories()` logic but is currently unused)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase requirements
- `.planning/REQUIREMENTS.md` — PKR-01..03, PGM-01..03, CLN-02 define this phase's scope
- `.planning/ROADMAP.md` — Phase 27 success criteria (5 items)

### Prior phase context
- `.planning/phases/25-selection-model-interaction-foundation/25-CONTEXT.md` — Selection model, widgetDeselectedFromCpp signal, overlay cleanup paths
- `.planning/phases/26-navbar-transformation-edge-resize/26-CONTEXT.md` — PGM-04 empty page auto-delete via trash (already shipped)

### Key source files
- `qml/applications/home/HomeMenu.qml` — Inline pickerOverlay (the LIVE picker, not WidgetPicker.qml), FABs, empty-space long-press stub, _addingPage guard, selectionTimer binding, deselectWidget() cleanup
- `qml/components/WidgetPicker.qml` — UNUSED categorized card component with `rebuildCategories()` — reference for category logic only
- `qml/components/WidgetConfigSheet.qml` — Bottom sheet Dialog pattern reference
- `src/ui/WidgetPickerModel.hpp` / `.cpp` — filterByAvailableSpace() (prepends "No Widget" entry), category/role data
- `src/ui/WidgetGridModel.hpp` / `.cpp` — addPage(), findFirstAvailableCell(), placeWidget() (no empty-widgetId guard)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `WidgetPickerModel` — Has `filterByAvailableSpace()`, category sorting, DisplayNameRole/IconNameRole/CategoryRole/DescriptionRole. NOTE: prepends "No Widget" entry that must be suppressed for add-widget flow.
- `WidgetGridModel.findFirstAvailableCell()` — Existing auto-place logic (page-occupancy-aware)
- `WidgetGridModel.placeWidget()` — Existing widget placement (no empty-widgetId guard — caller must validate)
- `WidgetGridModel.addPage()` — Existing page creation with proper index management
- `WidgetConfigSheet.qml` — Bottom sheet pattern using QML `Dialog` type (reference for picker redesign)
- `WidgetPicker.qml` — UNUSED but has reusable `rebuildCategories()` category-extraction logic

### Established Patterns
- Bottom sheet: `Dialog` type with `anchors.bottom: parent.bottom`, slide-up animation
- Category grouping: `rebuildCategories()` scans model, extracts unique categories, builds sections
- Auto-place: `findFirstAvailableCell(colSpan, rowSpan)` returns `{col, row}` or empty map
- Long-press on empty space: existing MouseArea at background z-level with `pressAndHoldInterval: 500` — currently a stub comment `/* Phase 27: context menu */`

### Integration Points
- Empty-space long-press stub in HomeMenu.qml — needs implementation with selection-state guard
- Inline `pickerOverlay` in HomeMenu.qml — needs complete replacement with bottom-sheet component
- FAB section in HomeMenu.qml — all 3 FABs + delete dialog need removal
- `selectionTimer.running` binding references `pickerOverlay.visible` — needs update to new picker component
- `deselectWidget()` closes `pickerOverlay` — needs update to close new picker
- AA fullscreen handler — needs expansion to dismiss picker/menu even when nothing is selected
- `widgetDeselectedFromCpp` path — only fires when selected; parallel no-selection cleanup needed for navigation-away

</code_context>

<specifics>
## Specific Ideas

- Android home screen reference: long-press empty space → "Widgets" / "Wallpaper" / "Settings" popup. Our version is simpler: just "Add Widget" / "Add Page".
- The inline pickerOverlay is the real picker to replace — WidgetPicker.qml is dead code with useful category logic.
- Auto-place on tap is the proven pattern from all prior phases — no drag-to-place.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 27-widget-picker-page-management*
*Context gathered: 2026-03-21*
