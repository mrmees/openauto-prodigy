# Phase 4: Visual Depth - Context

**Gathered:** 2026-03-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Buttons and navbar get subtle physical depth that makes the UI feel polished. M3-inspired elevation system with automotive tuning. Includes Qt 6.8 upgrade on dev VM as a prerequisite. No new functionality -- purely visual polish and component standardization.

</domain>

<decisions>
## Implementation Decisions

### Prerequisites
- Upgrade dev VM (claude-dev) Qt from 6.4 to 6.8 before implementing depth effects
- This enables MultiEffect for proper blurred shadows (not available in Qt 6.4)
- Eliminates existing Qt version compatibility workarounds

### Button Depth Style
- M3 elevation shadow (drop shadow beneath buttons) at Level 2 (medium) intensity
- Shadow color follows M3 spec with proper day/night mode switching
- Surface tint overlay at higher elevations (subtle primary-color tint per M3)
- Automotive-tuned -- M3 levels as starting point, adjusted for 1024x600 glanceability

### Press Feedback
- Scale down (0.95) + shadow reduction on press (shadow shrinks/disappears, button sinks)
- Remove existing opacity dim (0.85) -- scale + shadow is enough feedback
- Keep existing scale animation values (0.95 for Tiles, 0.97 for smaller items)
- Color change to primaryContainer on press remains (M3 standard, already in place)

### Navbar Separation
- Gradient fade (4-8dp) from navbar edge into content area, replacing current 1px barShadow border
- barShadow border removed entirely, gradient fade is the sole separation treatment
- Gradient skipped when AA is active (black navbar blends with phone's dark status bar)
- Gradient adapts to theme (derived from navbar background color with opacity)

### Scope
- All tappable elements get depth: Tiles, SettingsListItems, dialog buttons, SegmentedButtons, FullScreenPicker rows
- Popup panels (NavbarPopup, power menu, GestureOverlay) at higher M3 elevation (Level 3-4)
- Dialogs (ExitDialog, PairingDialog) at M3 Level 3 with scrim + elevated surface
- Consistent treatment across the entire interactive surface

### M3 Button Components
- Create reusable QML components: ElevatedButton.qml, FilledButton.qml, OutlinedButton.qml
- Built-in shadow, press behavior, state layers, and elevation management
- All existing button patterns migrate to these components
- State layers (semi-transparent onSurface overlay at 8-12% opacity) for touch feedback
- Replaces ad-hoc color-swap press feedback in some elements

### M3 Compliance
- M3-inspired with automotive tuning (not strict spec adherence)
- Elevation levels used but values tuned for car screen readability
- Surface tint at higher elevations (popups/dialogs subtly tinted with primary)
- State layers (press overlay) included in this phase
- Fewer distinct elevation levels than M3 spec if some are indistinguishable on hardware

### Claude's Discretion
- Exact shadow blur radius and offset values (tuned for 1024x600)
- Which M3 elevation levels to collapse if they're visually identical at target DPI
- Surface tint opacity values (perceptible but not garish)
- State layer implementation approach (Rectangle overlay vs shader)
- Qt 6.8 installation method on dev VM (PPA vs aqtinstall vs source)
- Whether to keep any #if QT_VERSION guards or clean-break to Qt 6.8 minimum

</decisions>

<specifics>
## Specific Ideas

- M3-inspired, not M3-strict -- automotive glanceability trumps spec purity
- "Medium (Level 2)" shadows chosen because subtle Level 1 may not register at arm's length in a car
- Gradient fade for navbar chosen over drop shadow for a softer automotive feel
- Opacity dim removed because it makes buttons look "ghosted" which doesn't fit M3 elevation language
- Reusable M3 button components chosen for long-term maintainability and consistency

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Tile.qml`: Primary launcher button -- scale 0.95, opacity 0.85, primaryContainer color swap. Migration target for FilledButton.
- `SettingsListItem.qml`: Settings row -- scale 0.97, opacity 0.85. Migration target for ElevatedButton or list-specific variant.
- `SegmentedButton.qml`: Multi-option toggle -- scale 0.97, opacity 0.85, secondaryContainer selected state.
- `FullScreenPicker.qml`: Picker rows -- scale 0.97, opacity 0.85.
- `ThemeService`: Already has barShadow, scrim, all 34 M3 color roles, primaryContainer/onPrimaryContainer for press states.
- `UiMetrics`: Scale-aware sizing (radiusSmall, spacing, animDurationFast). Shadow offsets should use UiMetrics scaling.

### Established Patterns
- Press feedback: `mouseArea.pressed ? 0.95 : 1.0` with `Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast } }`
- Color swap on press: `parent.pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainerLow`
- Qt logging categories: `qCDebug(lcAA)` for protocol, similar pattern for UI debugging
- Navbar edge states: 4-position anchoring (top/bottom/left/right) with AnchorChanges

### Integration Points
- `Navbar.qml`: Replace barShadow Rectangle (line 43-48) with gradient fade element
- `Tile.qml`: Remove opacity animation, add shadow layer, migrate to FilledButton pattern
- `SettingsListItem.qml`: Remove opacity, add shadow, potentially wrap in ElevatedButton
- Dialog buttons in ExitDialog, PairingDialog, GestureOverlay, SystemSettings, AboutSettings: migrate to M3 button components
- `ThemeService.cpp`: May need new Q_PROPERTYs for surface tint color or elevation-derived colors
- All QML files with `pressed ? ... : ...` patterns: audit and migrate to component-based approach

</code_context>

<deferred>
## Deferred Ideas

- Ripple animation (expanding circle from touch point) -- visual flair beyond state layers, future phase
- Animated elevation transitions (smooth shadow growth on focus) -- can add after base system works
- Per-component elevation customization via config -- overkill for v1.0

</deferred>

---

*Phase: 04-visual-depth*
*Context gathered: 2026-03-09*
