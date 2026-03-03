# Phase 1: Visual Foundation - Research

**Researched:** 2026-03-02
**Domain:** QML animation, theming, variable fonts, automotive UI controls
**Confidence:** HIGH

## Summary

Phase 1 reskins all existing controls with an automotive-minimal aesthetic, adds press feedback animations, smooth StackView page transitions, animated day/night theme switching, icons on settings rows, and variable font support for MaterialIcon. The entire phase operates within QML and the ThemeService C++ class -- no new pages, no restructuring.

The codebase already has strong foundations: UiMetrics singleton for all sizing, ThemeService with 13 color Q_PROPERTYs, MaterialIcon wrapping a variable font (with FILL, GRAD, opsz, wght axes), and StackView in SettingsMenu. The work is additive -- extending existing patterns rather than introducing new architectural concepts.

**Primary recommendation:** Work control-by-control through the existing `qml/controls/` directory adding press feedback and styling, then layer on StackView transitions, theme animation behaviors, and MaterialIcon variable font support. ThemeService and UiMetrics additions should come first since every other change depends on them.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Automotive minimal: dark, high-contrast, full-width rows, generous vertical spacing
- Subtle section dividers between groups (not cards, not borders around groups)
- No cards, no drop shadows, no rounded containers around groups
- Clean separation via spacing and divider lines only
- All interactive controls get visible press feedback: tiles, buttons, toggles, sliders, list items, picker rows
- Scale + opacity approach (not Material ripple -- avoids fragment shader complexity)
- Press response within 80ms
- Material Symbols Outlined on every settings row and category-related element
- Minimal whitespace around icon glyphs -- slightly oversize font relative to visual target
- MaterialIcon component gets `weight` and `opticalSize` props for variable font axes (Qt 6.8)
- Qt 6.4 fallback: don't set `font.variableAxes`, font renders fine without it
- StackView push/pop: slide + fade using `OpacityAnimator` + `XAnimator` (render-thread)
- Duration: 150ms with `Easing.OutCubic` (fast start, gentle deceleration)
- Do NOT use root-level `anchors` on StackView items (prevents position-based animations)
- All sub-animations in a ParallelAnimation must be Animator types (don't mix with NumberAnimation)
- Theme color changes animate via `Behavior on color { ColorAnimation }` on themed backgrounds
- `dividerColor` -- dedicated ThemeService property for section dividers
- `pressedColor` -- touch feedback overlay color
- UiMetrics additions: `animDuration` (150), `animDurationFast` (80), tile sizing properties
- NO `layer.enabled`, NO Qt Quick Controls Material/Universal style, NO Qt Quick Effects/MultiEffect, NO ripple effects, NO SVG icons, NO `Qt.labs` imports

### Claude's Discretion
- Exact press scale factor (0.95 vs 0.97) -- test on Pi, adjust
- Divider thickness, opacity, indentation
- Settings row layout details (icon column width, value alignment)
- Which specific Material Symbols icon codepoint per setting
- Loading skeleton design if needed
- Error state handling

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| VIS-01 | All interactive controls show visible press feedback (scale or opacity change) | Scale+opacity pattern via `Behavior on scale` and `Behavior on opacity` with 80ms duration; applied per-control on MouseArea `pressed` state |
| VIS-02 | Settings page transitions use smooth slide + fade animations (render-thread Animators, <=150ms) | StackView `pushEnter`/`pushExit`/`popEnter`/`popExit` with `ParallelAnimation` containing `OpacityAnimator` + `XAnimator`; all Animator types for render-thread execution |
| VIS-03 | Automotive-minimal aesthetic applied globally -- dark, high-contrast, full-width rows, subtle dividers, generous spacing | Restyle all controls in `qml/controls/` using ThemeService colors and UiMetrics spacing; dividers via new `dividerColor` property |
| VIS-04 | Day/night theme transitions animate smoothly (color interpolation, no instant flash) | `Behavior on color { ColorAnimation { duration: 300 } }` on all themed Rectangle backgrounds; ThemeService already emits `colorsChanged` signal |
| VIS-05 | ThemeService provides `dividerColor` and `pressedColor` properties | Add 2 Q_PROPERTYs following existing pattern: YAML key -> `activeColor()` -> getter -> QML binding; add keys to all theme YAML files |
| VIS-06 | UiMetrics extended with animation duration constants and category tile sizing | Add `animDuration` (150), `animDurationFast` (80), and tile sizing `readonly property int` values to `UiMetrics.qml` |
| ICON-01 | All settings rows and category tiles display contextual Material Symbols icons with minimal whitespace | Controls that don't already have an `icon` property get one; icon rendered slightly oversized relative to visual target for minimal whitespace |
| ICON-02 | MaterialIcon supports weight and optical size (variable font on Qt 6.8, fallback on 6.4) | `font.variableAxes` introduced in Qt 6.7; use Qt version check in QML to conditionally set axes; Qt 6.4 ignores `variableAxes` if not set |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt Quick | 6.4 / 6.8 | QML UI framework | Project target; dual-platform build |
| Qt Quick Controls | 6.4 / 6.8 | StackView, Switch, Slider, Dialog | Already used throughout codebase |
| Material Symbols Outlined | Variable (.ttf) | Icon font | Already in resources; has wght/opsz/FILL/GRAD axes |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ThemeService (C++) | In-tree | Color management + day/night | All color bindings |
| UiMetrics (QML singleton) | In-tree | Scale-aware sizing | All dimensions and durations |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Scale+opacity press | Material ripple | Ripple requires fragment shaders -- BANNED per decisions |
| Behavior on color | Transition on state | Behavior is simpler, auto-animates any property change |
| Animator types | NumberAnimation | NumberAnimation runs on main thread -- jank risk on Pi 4 |

## Architecture Patterns

### Recommended Approach
No new files or directories needed. All work modifies existing files:

```
src/core/services/ThemeService.hpp   # +2 Q_PROPERTYs
src/core/services/ThemeService.cpp   # +2 getters (one-line each)
config/themes/*/theme.yaml           # +2 color keys per theme
qml/controls/UiMetrics.qml           # +animation + tile sizing props
qml/controls/MaterialIcon.qml        # +weight/opticalSize with version guard
qml/controls/Tile.qml                # +press feedback, restyle
qml/controls/SettingsToggle.qml      # +icon prop, press feedback, restyle
qml/controls/SettingsSlider.qml      # +icon prop, restyle
qml/controls/SettingsListItem.qml    # +press feedback, restyle divider
qml/controls/SegmentedButton.qml     # +icon prop, press feedback
qml/controls/FullScreenPicker.qml    # +press feedback on rows
qml/controls/SectionHeader.qml       # +use dividerColor
qml/controls/EqBandSlider.qml        # +use dividerColor for ref line
qml/applications/settings/SettingsMenu.qml  # +StackView transitions
```

### Pattern 1: Press Feedback via Scale+Opacity
**What:** Animate `scale` and `opacity` on press, driven by MouseArea `pressed` property
**When to use:** Every interactive control (tiles, buttons, list items, picker rows)
**Example:**
```qml
// Inside any interactive control with a MouseArea
Rectangle {
    id: root
    scale: mouseArea.pressed ? 0.97 : 1.0
    opacity: mouseArea.pressed ? 0.85 : 1.0

    Behavior on scale {
        NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic }
    }
    Behavior on opacity {
        NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
```
**Note:** `Behavior` + `NumberAnimation` is already proven on Pi 4 -- used in EqBandSlider (80ms), Sidebar (80ms), NotificationArea. Using `NumberAnimation` (not `ScaleAnimator`/`OpacityAnimator`) here because `Behavior` blocks work with regular animations and the duration is so short (80ms) that render-thread offloading is unnecessary. The `Animator` types are for the longer StackView transitions where jank matters more.

### Pattern 2: StackView Render-Thread Transitions
**What:** Custom push/pop transitions using Animator types that run on the scene graph render thread
**When to use:** SettingsMenu StackView page navigation
**Example:**
```qml
// Source: Qt 6 StackView docs + Animator docs
StackView {
    id: settingsStack
    anchors.fill: parent
    initialItem: settingsList

    pushEnter: Transition {
        ParallelAnimation {
            OpacityAnimator { from: 0; to: 1; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
            XAnimator { from: settingsStack.width * 0.3; to: 0; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
        }
    }
    pushExit: Transition {
        ParallelAnimation {
            OpacityAnimator { from: 1; to: 0; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
            XAnimator { from: 0; to: -settingsStack.width * 0.3; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
        }
    }
    popEnter: Transition {
        ParallelAnimation {
            OpacityAnimator { from: 0; to: 1; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
            XAnimator { from: -settingsStack.width * 0.3; to: 0; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
        }
    }
    popExit: Transition {
        ParallelAnimation {
            OpacityAnimator { from: 1; to: 0; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
            XAnimator { from: 0; to: settingsStack.width * 0.3; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
        }
    }
}
```
**Critical:** All sub-animations inside a `ParallelAnimation` must be Animator types -- if even one is a regular `NumberAnimation`, the entire container falls back to main thread execution. Also: StackView items must NOT use root-level `anchors` (prevents position-based animations).

### Pattern 3: Theme Color Animation
**What:** Smooth color interpolation when day/night mode changes
**When to use:** All `Rectangle` elements that use ThemeService color bindings
**Example:**
```qml
Rectangle {
    color: ThemeService.backgroundColor

    Behavior on color {
        ColorAnimation { duration: 300; easing.type: Easing.InOutQuad }
    }
}
```
**Note:** Only needed on `Rectangle` `color` properties. `Text` `color` changes are instant and don't need animation (text color pops are fine; background color pops are jarring).

### Pattern 4: MaterialIcon Variable Font Axes (Qt 6.8 with 6.4 fallback)
**What:** Set `wght` and `opsz` axes on the Material Symbols variable font
**When to use:** MaterialIcon component
**Example:**
```qml
Text {
    id: root
    property string icon: ""
    property int size: 24
    property int weight: 400    // 100-700 (thin to bold)
    property int opticalSize: 24  // 20-48

    font.family: materialFont.name
    font.pixelSize: size
    text: icon

    // font.variableAxes was introduced in Qt 6.7
    // Qt 6.4 simply ignores this block if variableAxes doesn't exist
    Component.onCompleted: {
        if (root.font.variableAxes !== undefined) {
            root.font.variableAxes = {
                "wght": root.weight,
                "opsz": root.opticalSize
            }
        }
    }
}
```
**Alternative approach (cleaner, if `variableAxes` property just doesn't exist in Qt 6.4):**
```qml
// Qt 6.4 will ignore unknown properties in font{} if set conditionally
// But safer: use Qt.platform.version or a C++ version flag
```
**Recommended:** Use a simple `try` or version-guard approach. The safest is to check `typeof font.variableAxes` at component completion. On Qt 6.4, the property simply won't exist, so the check fails gracefully and the font renders with its default axis values (wght=400, opsz=24), which is perfectly fine.

### Anti-Patterns to Avoid
- **`layer.enabled: true`:** Creates GPU texture allocation that competes with video decode on Pi 4. BANNED.
- **Mixing Animator and NumberAnimation in ParallelAnimation:** If one sub-animation is NumberAnimation, the entire group runs on the main thread. Use only Animator types together.
- **Root `anchors` on StackView items:** Prevents XAnimator position-based transitions. Use `Layout` or manual positioning instead.
- **`Qt.labs.*` imports:** Experimental, may break between Qt versions.
- **Hardcoded pixel sizes:** Always use `UiMetrics.*` properties.
- **Hardcoded colors:** Always use `ThemeService.*` properties.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Press feedback | Custom touch tracking + manual animation | `Behavior on scale/opacity` + MouseArea `pressed` | Proven on Pi 4, 2 lines of QML each |
| Page transitions | Manual opacity/position management | StackView `pushEnter`/`popExit` with Animator types | Qt handles lifecycle, cleanup, back-stack |
| Color animation | Manual interpolation timer | `Behavior on color { ColorAnimation }` | Built-in, handles all color spaces correctly |
| Icon glyphs | SVG files per icon | Material Symbols font codepoints | One font file, instant rasterization, variable axes |
| Scale-aware sizing | Per-device pixel values | UiMetrics singleton | Already handles 0.9-1.35 scale range |

**Key insight:** Every animation pattern needed in this phase has a one-to-two-line QML idiom. The complexity is in applying it consistently across ~12 controls, not in the technique itself.

## Common Pitfalls

### Pitfall 1: StackView Items with Root Anchors
**What goes wrong:** `XAnimator` silently does nothing because `anchors` overrides `x` positioning.
**Why it happens:** Most sub-pages use `anchors.fill: parent` at the root level.
**How to avoid:** Wrap page content in an inner Item that fills parent. The root item must have no anchors so StackView can animate its `x` and `opacity`.
**Warning signs:** Page appears instantly instead of sliding.

### Pitfall 2: Behavior on color Performance
**What goes wrong:** Adding `Behavior on color` to every single Rectangle creates hundreds of animation objects.
**Why it happens:** Overzealous application of theme animation.
**How to avoid:** Only animate the "large surface" backgrounds -- main backgrounds, control backgrounds, section headers. Small dividers and tiny elements can change instantly.
**Warning signs:** Theme toggle feels sluggish on Pi 4.

### Pitfall 3: Press Feedback on Sliders
**What goes wrong:** Applying scale animation to the entire SettingsSlider row makes the slider thumb jump when pressed.
**Why it happens:** Scale transform applies to all children including the active drag area.
**How to avoid:** For sliders, only animate the label/track background, not the interactive thumb. Or skip press feedback on sliders entirely (the thumb drag IS the feedback).
**Warning signs:** Slider feels broken after adding press feedback.

### Pitfall 4: font.variableAxes on Qt 6.4
**What goes wrong:** QML error or binding failure on Qt 6.4 where `font.variableAxes` doesn't exist.
**Why it happens:** `font.variableAxes` was introduced in Qt 6.7.
**How to avoid:** Use `Component.onCompleted` with a guard check rather than inline property binding. Never declare `font.variableAxes: {...}` directly.
**Warning signs:** Console warnings on Ubuntu 24.04 (Qt 6.4) builds.

### Pitfall 5: Animator Duration of 0
**What goes wrong:** `Animator` types with duration 0 can cause visual artifacts.
**Why it happens:** Property value only updates after animation completes, so duration 0 might delay the update by a frame.
**How to avoid:** Always use at least 1ms duration, or don't use Animator for instant transitions.
**Warning signs:** Visual flicker on StackView transitions.

### Pitfall 6: Theme YAML Missing New Keys
**What goes wrong:** `dividerColor`/`pressedColor` return default QColor (black) on themes that lack the new keys.
**Why it happens:** Only adding keys to one theme file and forgetting others.
**How to avoid:** Update ALL theme YAML files (check `config/themes/*/theme.yaml` and `tests/data/themes/`). Provide sensible fallback defaults in C++ getter.
**Warning signs:** Dividers invisible or wrong color on non-default themes.

## Code Examples

### ThemeService C++ Addition (VIS-05)
```cpp
// ThemeService.hpp - add to Q_PROPERTY declarations:
Q_PROPERTY(QColor dividerColor READ dividerColor NOTIFY colorsChanged)
Q_PROPERTY(QColor pressedColor READ pressedColor NOTIFY colorsChanged)

// Add getters:
QColor dividerColor() const { return activeColor("divider"); }
QColor pressedColor() const { return activeColor("pressed"); }
```

### Theme YAML Addition
```yaml
# Add to both day: and night: sections in every theme
day:
  # ... existing colors ...
  divider: "#ffffff26"      # white at ~15% opacity (or use hex8)
  pressed: "#ffffff1a"      # white at ~10% opacity
night:
  # ... existing colors ...
  divider: "#ffffff1a"      # dimmer for night
  pressed: "#ffffff0d"      # dimmer for night
```
**Note:** Qt's QColor handles 8-digit hex (#RRGGBBAA). If YAML parsing strips the alpha, use a separate opacity in QML: `color: ThemeService.dividerColor` and set alpha via `Qt.rgba()`.

### UiMetrics Addition (VIS-06)
```qml
// Add to UiMetrics.qml
readonly property int animDuration: 150       // standard transition (ms)
readonly property int animDurationFast: 80    // quick feedback (ms)

// Tile sizing for category grid (used in Phase 2, defined here)
readonly property int tileW: Math.round(160 * scale)
readonly property int tileH: Math.round(140 * scale)
readonly property int tileIconSize: Math.round(48 * scale)
```

### Settings Row with Icon (ICON-01)
```qml
// Pattern for controls that currently lack an icon prop (SettingsToggle, SettingsSlider)
RowLayout {
    anchors.fill: parent
    anchors.leftMargin: UiMetrics.marginRow
    anchors.rightMargin: UiMetrics.marginRow
    spacing: UiMetrics.gap

    // NEW: Icon column
    MaterialIcon {
        icon: root.icon
        size: UiMetrics.iconSize
        color: ThemeService.normalFontColor
        visible: root.icon !== ""
        Layout.preferredWidth: UiMetrics.iconSize
    }

    Text {
        text: root.label
        font.pixelSize: UiMetrics.fontBody
        color: ThemeService.normalFontColor
        Layout.fillWidth: true
    }
    // ... rest of control ...
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Static font axes | `font.variableAxes` in QML | Qt 6.7 (2024) | Can set wght/opsz/FILL/GRAD per-instance |
| PropertyAnimation for transitions | Animator types (XAnimator, OpacityAnimator) | Qt 5.x+ (stable) | Render-thread execution, smoother on constrained hardware |
| Manual color interpolation | `Behavior on color { ColorAnimation }` | Long-standing | Built-in, handles color space correctly |

**Deprecated/outdated:**
- `loadFromModule()` -- not available in Qt 6.4, don't use
- `Qt.labs.platform` -- experimental, banned per project decisions

## Open Questions

1. **QColor with alpha from YAML**
   - What we know: Qt's QColor supports `#RRGGBBAA` format
   - What's unclear: Whether the YAML parsing in `ThemeService::loadThemeFile()` correctly handles 8-digit hex. Current theme files only use 6-digit hex.
   - Recommendation: Check the YAML color parsing code. If it strips alpha, either fix the parser or use fully opaque colors and apply opacity in QML (which is the current pattern -- see `SectionHeader.qml` line 30: `opacity: 0.3`).

2. **Press Scale Factor Tuning**
   - What we know: 0.95-0.97 range is typical for automotive UIs
   - What's unclear: Which feels best on the 1024x600 DFRobot screen at arm's length in a car
   - Recommendation: Start with 0.97 (subtle). Matt can tune on Pi.

3. **StackView Item Root Anchors**
   - What we know: All current settings sub-pages likely use root `anchors.fill: parent`
   - What's unclear: Exact structure of each page's root element
   - Recommendation: Audit all 8 settings sub-pages before adding transitions. May need to wrap content.

## Sources

### Primary (HIGH confidence)
- Qt 6 official docs: [StackView QML Type](https://doc.qt.io/qt-6/qml-qtquick-controls-stackview.html) -- transition properties, anchors constraint
- Qt 6 official docs: [Animator QML Type](https://doc.qt.io/qt-6/qml-qtquick-animator.html) -- render-thread execution, mixing rules
- Qt 6 official docs: [XAnimator](https://doc.qt.io/qt-6/qml-qtquick-xanimator.html), [OpacityAnimator](https://doc.qt.io/qt-6/qml-qtquick-opacityanimator.html)
- Qt blog: [Future text improvements in Qt 6.7](https://www.qt.io/blog/text-improvements-in-qt-6.7) -- `font.variableAxes` introduction
- In-tree codebase: All controls, ThemeService, UiMetrics inspected directly
- Font analysis: `fontTools` confirmed MaterialSymbolsOutlined.ttf has FILL, GRAD, opsz, wght variable axes

### Secondary (MEDIUM confidence)
- WebSearch: `font.variableAxes` introduced in Qt 6.7 (verified via Qt docs reference)
- WebSearch: Animator mixing rules in ParallelAnimation (verified via Qt docs)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - all in-tree, inspected directly
- Architecture: HIGH - patterns verified against Qt 6 official docs and existing codebase usage
- Pitfalls: HIGH - derived from Qt docs constraints and existing project gotchas (CLAUDE.md)

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable -- Qt 6 APIs are mature, no fast-moving dependencies)
