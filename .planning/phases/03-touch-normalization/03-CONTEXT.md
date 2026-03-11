# Phase 3: Touch Normalization - Context

**Gathered:** 2026-03-11
**Status:** Ready for planning

<domain>
## Phase Boundary

Make every settings sub-page comfortable to use by touch in a car. Consistent alternating-row list styling, touch-friendly control sizing via UiMetrics, and readable text at arm's length. No new settings capabilities — this is a visual/interaction normalization pass.

</domain>

<decisions>
## Implementation Decisions

### Row styling
- Alternating-row backgrounds on all sub-pages (surfaceContainer / surfaceContainerHigh), matching the main settings menu pattern
- New reusable `SettingsRow` QML component — takes index prop, computes even/odd color, provides consistent height/padding/press-feedback. Controls nest inside it.
- SectionHeaders reset the alternation counter (first row after a header is always surfaceContainer)
- Minimize unnecessary section headers — if a page only has one logical group, skip the header entirely
- Nested controls (e.g. Debug codec hw/sw toggle, decoder dropdown) flatten into individual rows with slight indentation. Sub-rows hidden when parent is disabled (e.g. decoder options hidden when codec is off).

### Row height & touch targets
- All sub-page rows use `UiMetrics.rowH` (single consistent height — no tall/short variants)
- Main settings menu keeps its current custom delegate and `tileH * 0.55` height (not converted to SettingsRow)

### Action items
- Action buttons (Close App, AA Protocol Test) converted to tappable rows with label + chevron
- Debug AA test buttons become a row that opens a sub-page with the button grid

### Picker display
- FullScreenPicker rows show current value inline + chevron (e.g. "Resolution    720p  >")
- No need to open the picker just to see what's currently set

### Text readability
- fontBody minimum on all settings pages — no fontCaption, no fontSmall
- No subtitle/helper text — every control is a self-explanatory labeled row. If a control needs explanation, the label itself should be clear enough.
- Removed text includes: "Configured at install time", "Test outbound commands (requires active AA connection)", and similar helper strings
- SectionHeader styling: Claude's discretion

### Page structure
- Sub-pages stay as Flickable + ColumnLayout (not ListView) — mixed control types per page, no recycling benefit at ~15 items max
- Each existing control wrapped in SettingsRow component

### Scope
- All 9 sub-pages get the full treatment (AA, Display, Audio, Bluetooth, Theme, Companion, System, Information, Debug)
- EQ is exempt — specialized touch UI with band sliders, leave as-is
- Main settings menu keeps its current custom delegate

### Claude's Discretion
- SectionHeader visual styling (size, weight, color — whatever works as a divider)
- Exact SettingsRow component API (what props beyond index)
- Whether any pages need SectionHeaders at all vs being one flat list
- Press feedback style on SettingsRow

</decisions>

<specifics>
## Specific Ideas

- "Disable lower level settings that are not available based on higher selections" — codec sub-rows (decoder, decoder name) hidden when parent codec toggle is off
- "Remove subtitles — we shouldn't need them if we're giving everything a line item" — no helper text, labels must be self-explanatory

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `SettingsToggle`: config-path-driven on/off — already uses UiMetrics, just needs SettingsRow wrapper
- `SettingsSlider`: debounced slider with config persistence — needs to fit in rowH height
- `FullScreenPicker`: modal bottom-sheet selector — needs current value display in row
- `ReadOnlyField`: label + value — already row-shaped, may just need color/height alignment
- `SegmentedButton`: multi-option toggle — needs to fit in rowH
- `SectionHeader`: divider — will reset alternation counter
- `SettingsListItem`: existing row component with shadow/press feedback — reference for SettingsRow design but different purpose (navigation vs. settings control container)

### Established Patterns
- Config-driven controls via `configPath` string — auto-load/save
- StackView navigation for sub-page drill-down (used for AA test buttons sub-page)
- UiMetrics tokens for all sizing — rowH, fontBody, marginPage, spacing, gap, iconSize
- ThemeService M3 color roles — surfaceContainer, surfaceContainerHigh, onSurface, onSurfaceVariant

### Integration Points
- Each sub-page QML file gets modified — wrap controls in SettingsRow
- New `qml/controls/SettingsRow.qml` component created
- `src/CMakeLists.txt` needs SettingsRow registered
- Debug page needs a new AA Protocol Test sub-page component (or inline StackView)

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-touch-normalization*
*Context gathered: 2026-03-11*
