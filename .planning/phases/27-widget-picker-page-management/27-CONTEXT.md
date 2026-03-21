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
- "Add Page" creates a new page via `WidgetGridModel.addPage()` and navigates to it using the existing `_addingPage` guard pattern (prevents deselect side-effects from page change)
- Menu appears near the touch point (popup style, not full-screen overlay)

### Bottom sheet widget picker (PKR-01, PKR-02, PKR-03)
- Replaces the current horizontal card strip overlay (`WidgetPicker.qml` + `pickerOverlay` in HomeMenu.qml)
- Slides up from bottom as a bottom sheet (like WidgetConfigSheet pattern)
- Categorized scrollable list — widgets grouped by category with headers
- Each widget card shows: icon + name + description (existing WidgetPickerModel already exposes these roles)
- Tapping a widget auto-places it at first available grid cell via `WidgetGridModel.findFirstAvailableCell()` then `placeWidget()` — existing auto-place logic
- Picker filters to widgets that fit available space via existing `WidgetPickerModel.filterByAvailableSpace()`
- After successful placement, picker closes
- If no space available, show "No space available" message in the picker

### FAB removal (CLN-02)
- Remove add-widget FAB (bottom-right) — replaced by long-press empty → "Add Widget" → picker
- Remove add-page FAB (above add-widget) — replaced by long-press empty → "Add Page"
- Remove delete-page FAB (bottom-left, red) — already functionally replaced by PGM-04 (trash auto-deletes empty pages in Phase 26)
- Remove the delete-page confirmation dialog (no longer needed with auto-delete)
- Remove `pickerOverlay` toggle logic — picker now opened from empty-space menu, not FAB

### Claude's Discretion
- Exact popup menu visual style (pill buttons, card with divider, or simple list)
- Bottom sheet height (half-screen, 60%, or adaptive based on widget count)
- Category sort order in the picker
- Whether to animate the popup menu appearance (fade, scale, or instant)
- Whether the empty-space menu should dismiss on tap-outside (likely yes)
- Exact card layout within the bottom sheet (grid vs list)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase requirements
- `.planning/REQUIREMENTS.md` — PKR-01..03, PGM-01..03, CLN-02 define this phase's scope
- `.planning/ROADMAP.md` — Phase 27 success criteria (5 items)

### Prior phase context
- `.planning/phases/25-selection-model-interaction-foundation/25-CONTEXT.md` — Selection model, _addingPage guard pattern
- `.planning/phases/26-navbar-transformation-edge-resize/26-CONTEXT.md` — PGM-04 empty page auto-delete via trash (already shipped)

### Key source files
- `qml/applications/home/HomeMenu.qml` — Current pickerOverlay, FABs, empty-space long-press handler, _addingPage guard
- `qml/components/WidgetPicker.qml` — Current horizontal card strip picker (to be replaced/refactored)
- `qml/components/WidgetConfigSheet.qml` — Bottom sheet pattern reference (Dialog-based)
- `src/ui/WidgetPickerModel.hpp` / `.cpp` — filterByAvailableSpace(), category/role data
- `src/ui/WidgetGridModel.hpp` / `.cpp` — addPage(), findFirstAvailableCell(), placeWidget()

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `WidgetPickerModel` — Already has `filterByAvailableSpace()`, category sorting, DisplayNameRole/IconNameRole/CategoryRole/DescriptionRole
- `WidgetGridModel.findFirstAvailableCell()` — Existing auto-place logic
- `WidgetGridModel.placeWidget()` — Existing widget placement
- `WidgetGridModel.addPage()` — Existing page creation with proper index management
- `WidgetConfigSheet.qml` — Bottom sheet pattern using QML `Dialog` type (reference for picker redesign)
- `_addingPage` guard in HomeMenu.qml — Prevents deselect side-effects from programmatic page changes
- `WidgetPicker.qml` — Existing categorized card layout with `rebuildCategories()` — reusable category logic

### Established Patterns
- Bottom sheet: `Dialog` type with `anchors.bottom: parent.bottom`, slide-up animation
- Category grouping: `rebuildCategories()` scans model, extracts unique categories, builds sections
- Auto-place: `findFirstAvailableCell(colSpan, rowSpan)` returns `{col, row}` or empty map
- Long-press on empty space: existing MouseArea at background z-level with `pressAndHoldInterval: 500`

### Integration Points
- Current empty-space long-press handler in HomeMenu.qml — currently a no-op comment (`/* Phase 27: context menu */`), needs implementation
- `pickerOverlay.visible` — currently gated on selection state, needs replacement with bottom sheet
- FAB section in HomeMenu.qml — all 3 FABs + delete dialog need removal
- `selectionTimer.running` binding references `pickerOverlay.visible` — needs update when picker changes

</code_context>

<specifics>
## Specific Ideas

- Android home screen reference: long-press empty space → "Widgets" / "Wallpaper" / "Settings" popup. Our version is simpler: just "Add Widget" / "Add Page".
- The current WidgetPicker categorized layout works well — the main change is moving it from an in-grid overlay to a proper bottom sheet.
- Auto-place on tap is the proven pattern from all prior phases — no drag-to-place.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 27-widget-picker-page-management*
*Context gathered: 2026-03-21*
