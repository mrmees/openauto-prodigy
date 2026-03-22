# Phase 07: Edit Mode - Context

**Gathered:** 2026-03-12
**Status:** Ready for planning

<domain>
## Phase Boundary

Touch-driven edit mode for customizing home screen widget layout. Users can reposition, resize, add, and remove widgets through long-press entry, drag interactions, and a floating action button. Replaces the current per-widget-only context menu with a screen-wide edit state. Multi-page support is Phase 08.

</domain>

<decisions>
## Implementation Decisions

### Edit mode entry
- Long-press ANYWHERE on the grid (empty cell or widget) enters edit mode
- All widgets simultaneously show accent borders and resize handles
- Replaces the current behavior where long-press opens per-widget context menu outside edit mode

### Edit mode visual state
- Accent border (primary color) on all widgets when edit mode is active
- Small resize handle at bottom-right corner of each widget
- 'X' removal badge at top-right corner of each widget
- '+' FAB in bottom-right corner for adding widgets
- Subtle dotted grid lines showing cell boundaries (visible throughout edit mode)

### Edit mode exit
- Tap any empty grid area exits edit mode
- 10-second inactivity timeout as safety net
- Auto-exits when AA fullscreen activates (EVIOCGRAB steals touch)
- No explicit "Done" button needed

### Context menu in edit mode
- Tapping a widget in edit mode opens the existing context menu (change widget type, adjust opacity, clear)
- Short hold (~200ms) then drag initiates move — distinguishes tap (context menu) from drag (reposition)

### Drag to reposition
- Short hold on widget (~200ms) before drag activates — prevents accidental drags from taps
- Dragged widget follows finger at ~50% opacity, slightly scaled up
- Original position shows empty outlined placeholder
- Valid target cells highlight in primary/accent color as you drag over them
- Occupied/invalid cells flash red
- On release over invalid target, widget snaps back to original position
- On valid drop, widget snaps to nearest cell — layout writes are atomic (on commit, not during drag)

### Resize interaction
- Single resize handle at bottom-right corner only
- Ghost dashed/outlined rectangle shows proposed new size, snapping to grid cells while dragging
- Bidirectional — drag outward to grow, inward to shrink
- Ghost stops responding at min/max limits (brief color flash on boundary hit)
- Ghost turns red if size would overlap another widget
- Widget stays at original size until handle is released

### Widget catalog (add)
- '+' FAB in bottom-right corner, visible only during edit mode
- Tapping FAB opens the existing bottom-sheet picker
- After selecting a widget, it auto-places in the first available grid spot at default size (2x2, clamped)
- User can then drag to reposition if needed

### Widget removal
- 'X' circle badge at top-right corner of every widget during edit mode
- Tap 'X' to remove immediately — no confirmation dialog
- Widget can be re-added from catalog

### Full grid feedback
- Toast message at bottom: "No space available — remove a widget first"
- Picker shows empty state / nothing when no widgets can fit

### Claude's Discretion
- Exact accent border width and color intensity
- FAB size and corner offset
- 'X' badge size and visual treatment
- Toast duration and styling
- Grid line dot spacing and opacity
- Drag threshold distance before activation
- Placeholder outline style (dashed vs dotted)
- Resize handle visual design (corner triangle, circle, etc.)
- MouseArea+Drag vs DragHandler implementation (Qt 6.4 compat — researcher resolves)

</decisions>

<specifics>
## Specific Ideas

- Android/iOS home screen edit mode as reference — long-press enters, all widgets become editable simultaneously
- Ghost rectangle for resize preview (not live resize) — flagged in STATE.md as important for Pi performance
- Auto-place after catalog selection rather than tap-to-place — better for car touchscreen where precision tapping is harder

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `WidgetGridModel`: Already has `moveWidget()`, `resizeWidget()`, `removeWidget()`, `canPlace()`, occupancy tracking — backend is complete
- `WidgetPickerModel`: Has `filterByAvailableSpace()` for catalog filtering
- `WidgetDescriptor`: Declares `minCols/minRows/maxCols/maxRows` constraints per widget
- `WidgetRegistry::widgetsFittingSpace()`: Returns widgets that can fit given available columns/rows
- `HomeMenu.qml`: Current long-press + context menu + picker overlay implementation — will be heavily modified
- `WidgetHost.qml`: Has `longPressed()` signal and `requestContextMenu()` — needs edit mode awareness
- `ThemeService`: M3 color roles available (primary, error, scrim, surfaceContainer variants)
- `UiMetrics`: All sizing tokens available (touchMin, radius, spacing, icon sizes)

### Established Patterns
- Overlay pattern: scrim Rectangle + floating content (used by contextMenuOverlay, pickerOverlay)
- Glass card pattern: surfaceContainer background with configurable opacity
- Long-press detection: MouseArea at z:-1 with pressAndHoldInterval: 500
- Press feedback: scale+opacity animation on touch (used throughout settings)
- Toast: No existing toast component — will need to create one

### Integration Points
- `EvdevTouchReader`: EVIOCGRAB signal needed to trigger edit mode auto-exit
- `ApplicationController`: May need `isEditMode` property for AA fullscreen coordination
- `WidgetGridModel.placementsChanged()`: Signal exists — persist on this after commit
- EQ slider drag pattern (`Drag.XAxis` in EqSettings.qml): Reference for drag implementation in QML

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 07-edit-mode*
*Context gathered: 2026-03-12*
