# Phase 23: Widget Visual Refinement - Context

**Gathered:** 2026-03-20
**Status:** Ready for planning

<domain>
## Phase Boundary

Interactive live review of all widgets at all valid span combinations on both target displays (1024x600 DFRobot 7" and 800x480 Pi Official 7"). User provides feedback per widget, Claude edits QML, preview tool shows changes immediately. One widget at a time until polished.

</domain>

<decisions>
## Implementation Decisions

### Review process
- One widget at a time — each widget is "done" before moving to next
- Both displays checked equally (not primary/secondary)
- Widget order: Clock (3 styles) → Date → Weather → Battery → Theme Cycle → Companion Status → AA Focus Toggle → AA Status → Now Playing → Nav Turn

### Quality standard
- "Polished + balanced" — no clipping, text readable at arm's length, good spacing, visual weight feels right, nothing cramped or sparse
- Subjective judgment by user — "you'll know it when you see it"
- Both 1024x600 and 800x480 must pass

### Workflow
- Preview tool running on dev VM with display presets, span controls, state simulation
- User views widget in preview tool at each span combination
- Claude edits QML files based on feedback
- Preview reloads automatically (QML is runtime-loaded)
- No build/deploy cycle needed during review — single Pi deploy at end (Phase 24)

### Claude's Discretion
- Exact pixel adjustments and font sizing within user's feedback direction
- How to implement specific layout fixes (anchors, Layout properties, margins)
- Whether to use fontSizeMode: Text.Fit vs fixed sizes for specific elements

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Widget files (all will be reviewed/modified)
- `qml/widgets/ClockWidget.qml` — 3 styles (digital/analog/minimal), time-only after Phase 22
- `qml/widgets/DateWidget.qml` — Column-driven breakpoints, ordinal suffixes, US/intl config
- `qml/widgets/WeatherWidget.qml` — 3 responsive breakpoints, GPS lifecycle
- `qml/widgets/BatteryWidget.qml` — Companion battery data display
- `qml/widgets/ThemeCycleWidget.qml` — Tap-to-advance theme switcher
- `qml/widgets/CompanionStatusWidget.qml` — Connected/disconnected, expanded detail
- `qml/widgets/AAFocusToggleWidget.qml` — Projection state control

### Widget system references
- `CLAUDE.md` — UiMetrics tokens, MaterialIcon codepoints, widget gotchas
- `docs/widget-developer-guide.md` — Widget contract, breakpoint patterns

### Existing widgets from earlier milestones
- `qml/applications/home/widgets/AAStatusWidget.qml` — AA connection status
- `qml/applications/home/widgets/NowPlayingWidget.qml` — Unified AA/BT media
- `qml/applications/home/widgets/NavigationTurnWidget.qml` — AA turn-by-turn

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `NormalText` / `SpecialText` / `MaterialIcon` controls for all text/icon rendering
- `UiMetrics` tokens: fontHeading, fontTitle, fontBody, fontSmall, fontTiny, iconSize, spacing, radius
- `ThemeService` color tokens: onSurface, onSurfaceVariant, primary, surfaceContainer, etc.
- Widget preview tool at `build-preview/widget-preview` with display presets + state simulation

### Established Patterns
- `fontSizeMode: Text.Fit` with `minimumPixelSize` for responsive text scaling
- Column-driven breakpoints via `colSpan` (row affects vertical scaling only)
- `anchors.fill: parent` + `anchors.margins: UiMetrics.spacing` for standard insets

### Integration Points
- All widget QML files are runtime-loaded — edits in source take effect immediately in preview
- Preview tool symlinks NormalText/MaterialIcon/SpecialText into widgets dir at launch

</code_context>

<specifics>
## Specific Ideas

- This is a hands-on review session, not a code-generation phase
- User drives the feedback, Claude makes targeted QML edits
- Each widget should look intentional at every valid span — not just "fits" but "belongs"

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 23-widget-visual-refinement*
*Context gathered: 2026-03-20*
