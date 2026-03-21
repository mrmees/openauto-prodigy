# Phase 22: Date Widget & Clock Cleanup - Context

**Gathered:** 2026-03-20
**Status:** Ready for planning

<domain>
## Phase Boundary

Split date display out of the clock widget into a standalone date widget. Clock widget becomes time-only across all 3 styles (digital, analog, minimal). Date widget has its own breakpoints, styling, and per-instance config.

</domain>

<decisions>
## Implementation Decisions

### Date widget content breakpoints (column-driven)
- **1x1**: Day of month with ordinal suffix ("28th") — large, bold, hero element
- **2x1**: Short day-of-week + ordinal date ("Mon 21st")
- **3x1**: Long day-of-week + short month + ordinal date ("Tuesday, Aug 10th")
- **4x1+**: Long day-of-week + long month + ordinal date ("Monday, December 31st")
- **Height ≥ 2**: Scale up the column-gated content and center vertically (e.g., 5x2 renders the 2x1 layout scaled up)
- Breakpoints are driven by colSpan only; rowSpan only affects vertical scaling/centering

### Date widget visual style
- Bold + prominent "desk calendar" feel
- Day number is the hero element — largest, heaviest weight
- Supporting text (month, day-of-week) is lighter weight and smaller
- At 1x1, the ordinal date dominates the entire cell

### Date widget config
- Per-instance config with a single enum: date order
- Options: "US" (March 20) vs "International" (20 March)
- Affects all breakpoints — the order of month vs day swaps
- ConfigSchemaField type: Enum with 2 values

### Clock cleanup
- Clean strip: remove ALL date/day code from all 3 clock styles
- Remove `showDate` and `showDay` properties entirely
- Remove `currentDate` and `currentDay` properties and their Timer assignments
- Digital: remove the date and day NormalText rows
- Analog: remove the dateLabel NormalText below the canvas
- Minimal: already time-only, no changes needed
- Clock descriptor: remove "format" references to date display from description
- Clock widget should fill its space with time display — no empty gaps where date was

### Claude's Discretion
- Exact ordinal suffix implementation (JS function for "st", "nd", "rd", "th")
- Font sizing ratios between hero day number and supporting text
- How to handle the US vs international format at each breakpoint (label order)
- Date widget descriptor metadata (icon codepoint, category, default size)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Widget system
- `src/main.cpp` lines 485-508 — Clock widget descriptor registration pattern (copy for date widget)
- `qml/widgets/ClockWidget.qml` — Current clock implementation to be modified (date removal)
- `src/core/plugin/WidgetTypes.hpp` — WidgetDescriptor struct, ConfigSchemaField types

### Widget contract
- `docs/widget-developer-guide.md` — Widget contract: widgetContext injection, breakpoints, effectiveConfig
- `CLAUDE.md` — Widget QML resource path gotcha (QT_QML_SKIP_CACHEGEN), MaterialIcon codepoints

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ClockWidget.qml` Timer pattern: can reuse for date updates (1-minute interval sufficient for date)
- `NormalText` / `MaterialIcon` controls for all text rendering
- `ConfigSchemaField::Enum` type for the date order config
- Widget descriptor registration pattern in `main.cpp` — straightforward copy+modify

### Established Patterns
- Column-driven breakpoints via `colSpan` property from `widgetContext`
- `effectiveConfig` for per-instance config access in QML
- `fontSizeMode: Text.Fit` with `minimumPixelSize` for responsive text
- Loader + Component switching for style variants (not needed here — single style)

### Integration Points
- New widget descriptor registered in `main.cpp` alongside existing widgets
- New `DateWidget.qml` added to `src/CMakeLists.txt` QML module
- Clock widget modified in-place (removing date code, adjusting layouts)

</code_context>

<specifics>
## Specific Ideas

- 1x1 should feel like tearing off a desk calendar page — the number is the whole widget
- At larger sizes, day-of-week is useful context ("is it Monday or Tuesday?")
- Ordinal suffixes (1st, 2nd, 3rd, 4th...) add polish over plain numbers

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 22-date-widget-clock-cleanup*
*Context gathered: 2026-03-20*
