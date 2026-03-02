# Phase 1: Visual Foundation - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Restyle all existing controls and shell components with an automotive-minimal aesthetic. Add press feedback, smooth transitions, icons on settings rows, and animated day/night switching. This phase touches existing controls and components only — no new settings pages or restructuring (that's Phase 2).

</domain>

<decisions>
## Implementation Decisions

### Visual Style
- Automotive minimal: dark, high-contrast, full-width rows, generous vertical spacing
- Subtle section dividers between groups (not cards, not borders around groups)
- No cards, no drop shadows, no rounded containers around groups
- Clean separation via spacing and divider lines only

### Press Feedback
- All interactive controls get visible press feedback: tiles, buttons, toggles, sliders, list items, picker rows
- Scale + opacity approach (not Material ripple — avoids fragment shader complexity)
- Press response within 80ms

### Icons
- Material Symbols Outlined on every settings row and category-related element
- Minimal whitespace around icon glyphs — slightly oversize font relative to visual target
- MaterialIcon component gets `weight` and `opticalSize` props for variable font axes (Qt 6.8)
- Qt 6.4 fallback: don't set `font.variableAxes`, font renders fine without it
- Icon selection is Claude's discretion — pick contextually appropriate icons per setting

### Page Transitions
- StackView push/pop: slide + fade using `OpacityAnimator` + `XAnimator` (render-thread)
- Duration: 150ms with `Easing.OutCubic` (fast start, gentle deceleration)
- Do NOT use root-level `anchors` on StackView items (prevents position-based animations)
- All sub-animations in a ParallelAnimation must be Animator types (don't mix with NumberAnimation)

### Day/Night Transitions
- Theme color changes animate via `Behavior on color { ColorAnimation }` on themed backgrounds
- No instant flash when toggling day/night mode

### ThemeService C++ Additions
- `dividerColor` — dedicated property for section dividers (currently inline `descriptionFontColor` at 0.15 opacity)
- `pressedColor` — touch feedback overlay color
- Both follow existing pattern: YAML theme key → Q_PROPERTY → QML binding

### UiMetrics Additions
- `animDuration`: 150 (standard transition)
- `animDurationFast`: 80 (quick feedback)
- Tile sizing properties for settings category grid (used in Phase 2, defined here)

### What NOT to Add
- No `layer.enabled` (GPU texture allocation competes with video decode on Pi 4)
- No Qt Quick Controls Material or Universal style (fights ThemeService)
- No Qt Quick Effects / MultiEffect (GPU-intensive, marginal benefit)
- No ripple effects (fragment shader complexity for 10% better feel)
- No SVG icons (font glyphs rasterize once, SVG renders per-icon)
- No `Qt.labs` imports (experimental, may break between versions)

### Claude's Discretion
- Exact press scale factor (0.95 vs 0.97) — test on Pi, adjust
- Divider thickness, opacity, indentation
- Settings row layout details (icon column width, value alignment)
- Which specific Material Symbols icon codepoint per setting
- Loading skeleton design if needed
- Error state handling

</decisions>

<specifics>
## Specific Ideas

- "Automotive minimal" — Tesla/CarPlay settings vibe. Not flashy, not decorated. Dark background, high-contrast text, big touch rows, minimal chrome.
- Icons with minimal whitespace — user specifically called this out. Icons should feel dense and readable, not floating in padding.
- Everything visible gets the treatment — not just settings pages. NavStrip, TopBar, launcher, modals all get automotive-minimal styling in this milestone (though launcher/NavStrip restyle is Phase 3, the foundation controls apply here).

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `UiMetrics` singleton: all sizing already computed from scale factor — add new props following same pattern
- `ThemeService` Q_PROPERTYs: 12+ existing color properties, `dividerColor`/`pressedColor` follow same pattern
- `MaterialIcon.qml`: wraps font as Text element, needs `weight`/`opticalSize` props added
- `Behavior` + `NumberAnimation`: already used in EqBandSlider (80ms), Sidebar (80ms), NotificationArea — proven on Pi 4

### Established Patterns
- All controls use `UiMetrics.*` for sizing (never hardcoded pixels)
- All colors come from `ThemeService.*` properties (never hardcoded colors)
- Controls are in `qml/controls/` — SettingsToggle, SettingsSlider, SegmentedButton, FullScreenPicker, SectionHeader, SettingsListItem, Tile, MaterialIcon
- StackView already used in SettingsMenu with default (instant) transitions

### Integration Points
- `ThemeService.cpp` / `ThemeService.hpp` — add 2 Q_PROPERTYs + getters + theme YAML keys
- `qml/controls/UiMetrics.qml` — add animation duration and tile sizing constants
- `qml/controls/MaterialIcon.qml` — add variable font axis support
- All existing controls in `qml/controls/` — add press feedback Behaviors
- `qml/applications/settings/SettingsMenu.qml` — add StackView transition properties
- Theme YAML files — add `divider_color` and `pressed_color` keys

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 01-visual-foundation*
*Context gathered: 2026-03-02*
