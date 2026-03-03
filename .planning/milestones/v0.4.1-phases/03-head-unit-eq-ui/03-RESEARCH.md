# Phase 3: EQ Dual-Access & Shell Polish - Research

**Researched:** 2026-03-02
**Domain:** Qt 6 QML shell styling, NavStrip navigation, modal behavior, touch UX
**Confidence:** HIGH

## Summary

This phase has two distinct work streams: (1) extending the automotive-minimal aesthetic to the shell components (NavStrip, TopBar, launcher tiles) that Phase 1 did not restyle, and (2) adding a NavStrip EQ shortcut that shares state with the existing AudioSettings > Equalizer path.

The shell restyling is straightforward QML work -- NavStrip buttons need press feedback (scale/opacity pattern already established in Phase 1), TopBar needs minor styling alignment, and launcher tiles already have press feedback via Tile.qml but need visual refinement. The EQ dual-access requires a navigation mechanism to open the EQ page from the NavStrip without going through Settings > Audio first.

The BT device "Forget" button (UX-01) is currently a small text label with only -8px margin expansion -- inadequate for a car touchscreen. The modal dismiss behavior (UX-02) should work via Qt's default `closePolicy` (CloseOnPressOutside) but needs verification/explicit setting.

**Primary recommendation:** Add an EQ shortcut button to the NavStrip that navigates to Settings (app 6), pushes the Audio page, then pushes the EQ page onto the SettingsMenu StackView -- reusing the existing EqSettings component with no duplication. For shell polish, apply the Phase 1 press feedback pattern (scale 0.95-0.97, opacity 0.85, animDurationFast) to all NavStrip buttons.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- EQ lives as a settings sub-page, not a standalone plugin -- no NavStrip icon (NOTE: SUPERSEDED by v0.4.3 requirements -- UX-04 explicitly requires NavStrip shortcut icon for EQ)
- Entry point: a tap-through row in AudioSettings ("Equalizer >" showing current preset name)
- Navigates to a dedicated EQ page within the Settings navigation flow (back button returns to AudioSettings)
- All 10 vertical sliders in a single full-width horizontal row
- Stream selector (SegmentedButton) and preset picker above the slider area
- Custom vertical slider component (already built: EqBandSlider.qml)
- Gain range: +/-12 dB with 0.5 dB step size
- Floating dB value label above thumb while dragging
- 0 dB reference line across all slider tracks
- Double-tap to reset band to 0 dB
- Band frequency labels below each slider
- Preset picker: FullScreenPicker (bottom sheet) with bundled presets first, user presets below divider
- Per-stream bypass with BYPASS badge, sliders dim to ~40% opacity when bypassed

### Claude's Discretion
- Exact spacing and margins within the EQ page (use UiMetrics)
- Slider thumb shape and size (must meet UiMetrics.touchMin)
- Animation timing for slider snap-to-preset and bypass dimming
- Exact color treatment for bypass badge and dimmed state
- Save dialog styling and keyboard behavior
- Swipe-to-delete animation and threshold distance
- NavStrip EQ button icon choice and placement
- Shell styling details (TopBar margins, launcher tile refinements)

### Deferred Ideas (OUT OF SCOPE)
None from CONTEXT.md.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| UX-01 | BT device forget action has clearly visible, adequately-sized touch target | Current "Forget" is small text with -8px margins. Replace with icon button or pill-shaped button meeting UiMetrics.touchMin. See Architecture Patterns: BT Forget Button. |
| UX-02 | Modal pickers can be dismissed by tapping outside modal area | Qt 6 Dialog default closePolicy includes CloseOnPressOutside. Add explicit `closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside` to all Dialogs for clarity and to prevent regressions. |
| UX-04 | EQ accessible via Audio settings AND NavStrip shortcut icon (dual-access, consistent state) | NavStrip EQ button navigates to Settings > Audio > EQ via StackView push chain. Single EqSettings instance, state from EqualizerService C++ singleton. See Architecture Patterns: EQ Dual-Access. |
| ICON-03 | NavStrip buttons use consistent icon sizing and press feedback | Apply Phase 1 press feedback pattern (scale/opacity Behaviors) to all NavStrip Rectangle buttons. Standardize icon sizing to `parent.height * 0.5`. |
| ICON-04 | Launcher tiles use appropriately-sized icons with automotive-minimal visual style | Tile.qml already has press feedback. May need icon size increase, background color tweaks, or border treatment for automotive-minimal consistency. |
| NAV-01 | NavStrip buttons have consistent automotive-minimal styling with press feedback | NavStrip buttons are bare Rectangles with no press feedback. Add scale/opacity Behavior animations matching Phase 1 pattern. Add subtle border-radius, consistent padding. |
| NAV-02 | TopBar styling updated to match automotive-minimal aesthetic | TopBar is minimal (title + clock). Add subtle bottom border/divider, ensure font styling matches Phase 1 conventions. Minor work. |
| NAV-03 | Launcher grid tiles restyled with automotive-minimal aesthetic and press feedback | LauncherMenu uses Tile.qml which already has press feedback (0.95 scale). May need spacing, background, or icon refinements. Evaluate against settings tiles for consistency. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6.4/6.8 QML | 6.4+ | All UI components | Project standard, dual-platform |
| ThemeService | in-tree | Colors, day/night mode | Phase 1 established all theme properties |
| UiMetrics | in-tree | Responsive sizing, animation constants | Phase 1 established all sizing constants |
| EqualizerService | in-tree | EQ state management | Already has full QML-friendly API (gainsAsList, int-based methods) |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| MaterialIcon.qml | in-tree | Icon rendering | All icon display in NavStrip, buttons |
| SegmentedButton.qml | in-tree | Multi-option selector | EQ stream selector (already in use) |
| FullScreenPicker.qml | in-tree | Bottom-sheet picker | EQ preset picker (already in use) |
| EqBandSlider.qml | in-tree | Vertical EQ band slider | EQ page (already built and working) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| NavStrip StackView push chain for EQ | Separate EQ Loader outside Settings | Would duplicate EQ component, break settings back-button flow. Push chain is simpler. |
| Explicit closePolicy on each Dialog | Relying on Qt default | Explicit is defensive -- prevents regression if a future Dialog accidentally overrides |

## Architecture Patterns

### EQ Dual-Access Navigation

**What:** NavStrip EQ button opens EQ through the Settings StackView, reaching the same EqSettings page as Audio > Equalizer.

**When to use:** When EQ shortcut is tapped from NavStrip.

**Approach:** The NavStrip EQ button should:
1. Clear any active plugin (`PluginModel.setActivePlugin("")`)
2. Navigate to Settings (`ApplicationController.navigateTo(6)`)
3. Push Audio page onto SettingsMenu's StackView
4. Push EQ page onto Audio's inner StackView (if nested) OR push EqSettings directly onto the settings StackView

**Key insight:** EqSettings.qml is already built and uses `EqualizerService` (a C++ singleton exposed to QML root context). Opening it from two paths shows identical state because the state lives in C++, not in QML properties. No shared Loader or state synchronization needed.

**Implementation pattern:**
```qml
// In NavStrip.qml -- EQ shortcut button
Rectangle {
    // ... styling ...
    MouseArea {
        anchors.fill: parent
        onClicked: {
            PluginModel.setActivePlugin("")
            ApplicationController.navigateTo(6)  // Switch to Settings
            // SettingsMenu needs a function to deep-navigate to EQ
            settingsView.openEqDirect()
        }
    }
}
```

The SettingsMenu.qml needs a new `openEqDirect()` function that pushes Audio then EQ onto its StackView. This also requires the signal path from NavStrip through Shell to SettingsMenu (similar to existing `settingsResetRequested` signal).

**Navigation back:** When user taps Back from EQ opened via NavStrip, they go to Audio settings (depth 2), then Settings grid (depth 1). This is correct behavior -- the NavStrip shortcut is a convenience entry point, not a separate navigation context.

### Press Feedback Pattern (Phase 1 Established)

**What:** Scale + opacity animation on press for all interactive elements.

**Pattern:**
```qml
// For large items (tiles, full-width buttons)
scale: mouseArea.pressed ? 0.95 : 1.0
opacity: mouseArea.pressed ? 0.85 : 1.0

// For small items (list rows, nav buttons)
scale: mouseArea.pressed ? 0.97 : 1.0
opacity: mouseArea.pressed ? 0.85 : 1.0

// Always with these Behaviors
Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
```

### BT Forget Button

**What:** Replace tiny "Forget" text with an adequately-sized touch target.

**Current state (ConnectionSettings.qml line 136-146):**
```qml
Text {
    text: "Forget"
    font.pixelSize: UiMetrics.fontSmall
    color: "#cc4444"
    MouseArea {
        anchors.fill: parent
        anchors.margins: -8  // Only 8px expansion -- too small
        onClicked: { ... }
    }
}
```

**Recommended replacement:** A pill-shaped button with minimum `UiMetrics.touchMin` height and adequate width:
```qml
Rectangle {
    Layout.preferredWidth: forgetText.implicitWidth + UiMetrics.gap * 2
    Layout.preferredHeight: UiMetrics.touchMin
    radius: UiMetrics.touchMin / 2  // pill shape
    color: "transparent"
    border.color: "#cc4444"
    border.width: 1

    // Press feedback
    scale: forgetArea.pressed ? 0.95 : 1.0
    opacity: forgetArea.pressed ? 0.85 : 1.0
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    Text {
        id: forgetText
        anchors.centerIn: parent
        text: "Forget"
        font.pixelSize: UiMetrics.fontSmall
        color: "#cc4444"
    }

    MouseArea {
        id: forgetArea
        anchors.fill: parent
        onClicked: { ... }
    }
}
```

Alternative: delete icon button (trash can) with `UiMetrics.touchMin x UiMetrics.touchMin` size. Either works -- pill button is more discoverable.

### Modal Dismiss Pattern

**What:** Ensure all Dialog/Popup components dismiss on outside tap.

**Qt 6 default behavior:** `Popup.closePolicy` defaults to `Popup.CloseOnEscape | Popup.CloseOnPressOutside`. Modal dialogs with `dim: true` show a dim overlay, and tapping that overlay triggers close.

**Recommendation:** Add explicit `closePolicy` to all Dialogs for defensive coding:
```qml
Dialog {
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // ...
}
```

Affected components:
- `FullScreenPicker.qml` (pickerDialog)
- `EqSettings.qml` (presetPickerDialog, savePresetDialog)
- `PairingDialog.qml`
- `ExitDialog` (in NavStrip)
- Any other Dialog instances

### Anti-Patterns to Avoid
- **Duplicating EqSettings for NavStrip access:** Do NOT create a second EQ component. The EqualizerService singleton guarantees shared state -- just navigate to the existing page.
- **Hardcoded pixel sizes:** All dimensions through UiMetrics. Phase 1 established this rule; phase 3 must not regress.
- **layer.enabled effects:** Banned per project pitfalls -- GPU competition with video decode on Pi 4.
- **Breaking NavStrip EVIOCGRAB behavior:** During AA, touch is grabbed by evdev. NavStrip visual changes must not interfere with the evdev hit zone logic in EvdevTouchReader.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| EQ state management | Custom QML state sync | EqualizerService C++ singleton | Already built, already exposed to QML, signals auto-propagate |
| Bottom-sheet picker | New picker component | FullScreenPicker.qml | Already styled, already handles model-driven and options-driven modes |
| Press feedback | Per-component animations | Copy Phase 1 pattern (scale/opacity Behavior) | Consistent UX, proven on Pi |
| Icon rendering | Custom icon loading | MaterialIcon.qml | Already supports variable weight, graceful Qt 6.4 fallback |

## Common Pitfalls

### Pitfall 1: NavStrip EQ Navigation Timing
**What goes wrong:** Calling `navigateTo(6)` and then immediately trying to push onto SettingsMenu's StackView fails because SettingsMenu hasn't rendered yet.
**Why it happens:** `navigateTo()` changes `currentApplication` which triggers `visible` on SettingsMenu, but QML rendering is deferred.
**How to avoid:** Use `Qt.callLater()` for the StackView push, or have SettingsMenu check a "pending deep navigation" property on `Component.onCompleted` / `onVisibleChanged`.
**Warning signs:** EQ page doesn't appear when tapping NavStrip shortcut, but works from Audio settings.

### Pitfall 2: StackView Depth After NavStrip EQ
**What goes wrong:** After opening EQ via NavStrip, the Settings back button and the NavStrip settings button don't behave correctly.
**Why it happens:** The StackView depth is 3 (grid > audio > eq) but the user didn't navigate through those intermediate pages.
**How to avoid:** Ensure `navigateBack()` handler in SettingsMenu properly restores titles at each depth. Test the full back-button chain: EQ > Audio > Grid.

### Pitfall 3: SettingsMenu Reset vs EQ Deep Navigate
**What goes wrong:** `SettingsMenu.resetToGrid()` pops all StackView items to root. If this fires while navigating to EQ, the EQ push gets lost.
**Why it happens:** `onVisibleChanged` in SettingsMenu calls `resetToGrid()`. When NavStrip EQ navigates to settings, `visible` becomes true, triggering reset.
**How to avoid:** Add a guard: don't reset on visible if a deep navigation is pending. Or remove the `onVisibleChanged` reset and only reset on explicit settings button tap (which already has its own reset via `settingsResetRequested` signal).
**Warning signs:** NavStrip EQ button shows the settings tile grid instead of EQ.

### Pitfall 4: BT Forget Confirmation Missing
**What goes wrong:** User accidentally taps "Forget" and loses a paired device.
**Why it happens:** No confirmation dialog -- immediate action on tap.
**How to avoid:** Consider adding a simple confirmation (but note: the requirement only says "clearly visible, adequately-sized touch target" -- not confirmation). If adding confirmation, use the existing Dialog pattern.

## Code Examples

### NavStrip Button with Press Feedback
```qml
// Refactored NavStrip button pattern
Rectangle {
    Layout.fillHeight: true
    Layout.preferredWidth: navStrip.height * 1.2
    color: isActive ? ThemeService.highlightColor : "transparent"
    radius: UiMetrics.radius

    // Press feedback (new)
    scale: btnArea.pressed ? 0.95 : 1.0
    opacity: btnArea.pressed ? 0.85 : 1.0
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    MaterialIcon {
        anchors.centerIn: parent
        icon: "\ue429"  // equalizer
        size: parent.height * 0.5
        color: isActive ? ThemeService.highlightFontColor
                        : ThemeService.normalFontColor
    }

    MouseArea {
        id: btnArea
        anchors.fill: parent
        onClicked: { /* navigation logic */ }
    }
}
```

### Deep Navigation to EQ from NavStrip
```qml
// In SettingsMenu.qml
function openEqDirect() {
    // Reset to root first, then push audio > eq
    if (settingsStack.depth > 1) {
        settingsStack.pop(null)
    }
    settingsStack.push(audioPage)
    ApplicationController.setTitle("Settings > Audio")
    // Defer EQ push to next frame to ensure Audio page is loaded
    Qt.callLater(function() {
        var audioItem = settingsStack.currentItem
        if (audioItem) {
            // AudioSettings has an inner StackView or we push EQ onto settingsStack
            settingsStack.push(eqDirectComponent)
            ApplicationController.setTitle("Settings > Audio > Equalizer")
        }
    })
}

Component {
    id: eqDirectComponent
    EqSettings {}
}
```

**Alternative (simpler):** Push EQ directly onto settingsStack at depth 2, skipping the Audio page. Back would go to settings grid. This is simpler but loses the "Audio > Equalizer" breadcrumb context.

### Explicit Modal Dismiss
```qml
// Add to every Dialog in the project
Dialog {
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // ... rest of dialog
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| No press feedback on NavStrip | Phase 1 established pattern | Phase 1 (v0.4.3) | NavStrip is the last un-styled component |
| EQ only via Settings > Audio | Dual-access via NavStrip + Settings | Phase 3 (this phase) | Quick EQ access for car use |
| Small text "Forget" button | Pill-shaped touch target | Phase 3 (this phase) | Adequate for in-car touch |

## Open Questions

1. **SettingsMenu onVisibleChanged reset vs deep navigation**
   - What we know: `onVisibleChanged` calls `resetToGrid()` which would fight NavStrip EQ navigation
   - What's unclear: Best approach -- guard the reset, or remove it entirely (the NavStrip settings button already handles reset explicitly via `settingsResetRequested`)
   - Recommendation: Remove `onVisibleChanged` reset. The `settingsResetRequested` signal from NavStrip settings button handles the user-facing "tap settings to go back to grid" case. The `onVisibleChanged` was defensive but conflicts with deep navigation.

2. **EQ NavStrip button position and icon**
   - What we know: NavStrip has Home, plugin buttons, spacer, back, day/night, settings
   - What's unclear: Where the EQ button should go (before spacer? after day/night? replace something?)
   - Recommendation: Place EQ button between the plugin buttons and the spacer, on the left side. Use `\ue429` (equalizer icon). This groups "go-to" buttons on the left, utility buttons on the right.

3. **Launcher tile restyling scope**
   - What we know: LauncherMenu uses Tile.qml which already has press feedback (0.95 scale)
   - What's unclear: How much visual change is needed beyond what Tile.qml already provides
   - Recommendation: Evaluate current launcher appearance against settings tiles. May only need spacing/grid adjustments and ensuring icon sizes use UiMetrics consistently. The Tile.qml itself may already satisfy ICON-04 after Phase 1 changes.

## Sources

### Primary (HIGH confidence)
- Project codebase: NavStrip.qml, TopBar.qml, LauncherMenu.qml, Tile.qml, EqSettings.qml, EqBandSlider.qml, SettingsMenu.qml, ConnectionSettings.qml, ApplicationController.hpp
- Phase 1 research and implementation: press feedback pattern, UiMetrics constants, ThemeService properties
- EqualizerService.hpp: full QML-friendly API already implemented

### Secondary (MEDIUM confidence)
- Qt 6 Popup.closePolicy default behavior (CloseOnEscape | CloseOnPressOutside for modal popups) -- verified from Qt documentation knowledge

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - all components are in-tree, already working
- Architecture: HIGH - patterns established in Phase 1, EQ service fully built
- Pitfalls: HIGH - navigation timing and StackView depth are well-understood from Phase 2 work
- EQ dual-access: MEDIUM - the SettingsMenu onVisibleChanged conflict needs careful handling during implementation

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable -- all in-tree components, no external dependencies)
