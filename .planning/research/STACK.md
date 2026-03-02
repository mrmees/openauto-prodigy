# Technology Stack: v0.4.3 UI Redesign & Settings Reorganization

**Project:** OpenAuto Prodigy
**Researched:** 2026-03-02
**Scope:** NEW capabilities needed for automotive-minimal visual refresh. Does NOT cover existing validated stack (Qt 6.8, PipeWire, BlueZ, etc.).

## Recommended Stack Additions

### Animation & Transitions (QML built-in -- no new dependencies)

| Technology | Module | Purpose | Why |
|------------|--------|---------|-----|
| `OpacityAnimator` | QtQuick | Fade transitions for page/view changes | Runs on scene graph render thread, not UI thread. Stays smooth even when UI thread is busy (e.g., during BT scan or config writes). Available since Qt 5.2. |
| `XAnimator` / `YAnimator` | QtQuick | Slide transitions for StackView push/pop | Same render-thread advantage. Use for settings category drill-down animations. |
| `Behavior on <property>` | QtQuick | Implicit animations on color, opacity, scale changes | Already used in codebase (NotificationArea, EqBandSlider, Sidebar). Extend pattern to all interactive controls for press/hover feedback. |
| `ColorAnimation` | QtQuick | Theme-aware color transitions on state changes | Already used in EqSettings. Apply to Tile hover, NavStrip active state, toggle switches. |
| `StackView` transitions | QtQuick.Controls | Custom `pushEnter`/`pushExit`/`popEnter`/`popExit` | StackView already used in SettingsMenu but with default (instant) transitions. Add slide + fade for polished category navigation. |

**Critical constraint:** Do NOT use `anchors` on root-level items pushed to StackView -- anchors prevent position-based transition animations. Use `Layout` or explicit `width`/`height` binding instead. (Source: Qt 6 StackView docs.)

**Performance rule:** Keep all animation durations between 80-200ms. Automotive UIs must feel instant. 150ms is the sweet spot for "smooth but not sluggish." The Pi 4 GPU handles these fine under labwc/Wayland -- the codebase already proves this with Sidebar animations at 80ms.

### Animator Types vs Regular Animations -- When to Use Which

| Use Case | Type | Why |
|----------|------|-----|
| Page transitions (StackView) | `OpacityAnimator` + `XAnimator` | Render thread; stays smooth during model updates |
| Control feedback (hover, press) | `Behavior` + `NumberAnimation` | Simpler syntax; UI thread is fine for these tiny animations |
| Color changes (theme, active state) | `Behavior` + `ColorAnimation` | ColorAnimation handles QColor interpolation correctly |
| List item add/remove | `ViewTransition` + `NumberAnimation` | Built-in GridView/ListView transition system |

**Caveat:** When mixing Animator types in `ParallelAnimation`, ALL sub-animations must be Animator types for render-thread optimization to apply. Mixing `NumberAnimation` with `OpacityAnimator` in the same parallel group defeats the purpose. (Source: Qt 6 Animator docs.)

### Icon System (Material Symbols -- already in tree)

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| Material Symbols Outlined (variable font) | Current (in `resources/fonts/`) | All UI iconography | Already bundled at 10MB. Variable font supports optical size axis (20-48dp), weight axis (100-700), and grade axis. |

**Current state:** `MaterialIcon.qml` wraps the font as a `Text` element with `icon` (codepoint) and `size` properties. This works but doesn't leverage the variable font axes.

**Recommended enhancement -- `font.variableAxes`:**

Qt 6.7+ added `font.variableAxes` (QFont::setVariableAxis). On the Pi (Qt 6.8), set optical size to match icon pixel size for crisper rendering at any scale:

```qml
// Enhanced MaterialIcon.qml
Text {
    property string icon: ""
    property int size: 24
    property int weight: 400  // 100-700
    property int opticalSize: -1  // auto = match size

    font.family: materialFont.name
    font.pixelSize: size
    font.variableAxes: {
        var axes = {}
        axes["opsz"] = (opticalSize > 0) ? opticalSize : Math.min(48, Math.max(20, size))
        axes["wght"] = weight
        return axes
    }
    text: icon
}
```

**Qt 6.4 compatibility:** `font.variableAxes` does not exist in Qt 6.4. Don't set it on the dev VM -- the font still renders fine, just without optical size optimization. Since `font.variableAxes` is a QML property, not setting it is a no-op. No conditional compilation needed. Use a safe runtime check:

```qml
Component.onCompleted: {
    if (font.hasOwnProperty("variableAxes")) {
        font.variableAxes = { "opsz": Math.min(48, Math.max(20, size)), "wght": weight }
    }
}
```

**Whitespace around glyphs:** Material Symbols glyphs are drawn within a square em-box. At `font.pixelSize: 36`, the glyph is inscribed in a 36x36 box with some internal padding (~10-15% on each side). To get icons with minimal visual whitespace:
- Slightly oversize the `font.pixelSize` relative to the desired visual size (e.g., `size: UiMetrics.iconSize * 1.15`)
- The variable font's `opsz` axis helps: higher optical sizes use thicker strokes that fill more of the glyph box
- Do NOT use `clip: true` with a smaller bounding rect -- can cut off glyphs unpredictably

**Font size consideration:** The 10MB variable font is bundled in the Qt resource system (compiled into the binary). This adds ~10MB to binary size. For a car head unit with 4GB RAM this is fine. Do NOT switch to the static font (295KB) because you lose the optical size axis that makes icons crisp at UiMetrics-scaled sizes.

### Settings Category Grid (QML built-in)

| Technology | Module | Purpose | Why |
|------------|--------|---------|-----|
| `Grid` in `Flickable` | QtQuick | Top-level settings category tiles | 6 categories in a 2x3 or 3x2 grid. Static layout, not model-driven. |

**Recommendation:** Use a static `Grid` inside a `Flickable`, NOT `GridView`. The settings categories are fixed (Android Auto, Display, Audio, Connectivity, Companion, System/About) -- there's no dynamic model. A `Grid` is simpler, avoids delegate recycling complexity, and gives direct layout control for a fixed 6-item grid on 1024x600.

### Touch Feedback (QML built-in)

| Pattern | Implementation | Purpose |
|---------|----------------|---------|
| Press scale | `Behavior on scale { NumberAnimation { duration: 80 } }` + `scale: mouseArea.pressed ? 0.95 : 1.0` | Subtle press feedback on tiles and buttons |
| Press opacity | `Behavior on opacity { NumberAnimation { duration: 80 } }` + opacity dim on press | Alternative to scale for list items |
| Ripple effect | NOT recommended | Material ripple requires fragment shader. Scale + opacity achieves 90% of the effect with 10% of the code. |

### StackView Transition Recipes

For the settings category drill-down (top-level grid -> category detail -> sub-setting):

```qml
StackView {
    id: settingsStack

    pushEnter: Transition {
        ParallelAnimation {
            OpacityAnimator { from: 0; to: 1; duration: 150 }
            XAnimator { from: 60; to: 0; duration: 150; easing.type: Easing.OutCubic }
        }
    }
    pushExit: Transition {
        OpacityAnimator { from: 1; to: 0; duration: 150 }
    }
    popEnter: Transition {
        ParallelAnimation {
            OpacityAnimator { from: 0; to: 1; duration: 150 }
            XAnimator { from: -60; to: 0; duration: 150; easing.type: Easing.OutCubic }
        }
    }
    popExit: Transition {
        ParallelAnimation {
            OpacityAnimator { from: 1; to: 0; duration: 150 }
            XAnimator { from: 0; to: 60; duration: 150; easing.type: Easing.OutCubic }
        }
    }
}
```

**Why `Easing.OutCubic`:** Fast start, gentle deceleration. Feels responsive. Avoid `Easing.InOutQuad` for page transitions -- the slow start feels laggy in a car UI where every tap should feel immediate.

### New UiMetrics Properties Needed

The existing `UiMetrics` singleton needs a few additions for the redesign:

| Property | Value | Purpose |
|----------|-------|---------|
| `tileWidth` | `Math.round(140 * scale)` | Settings category tile size |
| `tileHeight` | `Math.round(120 * scale)` | Settings category tile size |
| `iconLarge` | `Math.round(48 * scale)` | Category tile icons (larger than current `iconSize: 36`) |
| `animDuration` | `150` | Centralized animation duration constant |
| `animDurationFast` | `80` | Quick feedback animations |

### New ThemeService Properties Needed

| Property | Purpose | Why |
|----------|---------|-----|
| `dividerColor` | Section dividers, list separators | Currently using `descriptionFontColor` at 0.15 opacity inline -- a dedicated property is cleaner and theme-consistent |
| `pressedColor` | Touch feedback overlay color | Avoid hardcoding alpha-modified highlight colors in QML |

`highlightFontColor` already exists and covers active/selected text on highlighted backgrounds.

**Note:** Adding ThemeService properties requires C++ changes (Q_PROPERTY + getter + theme YAML key). Keep additions minimal -- the two above cover the redesign needs.

## What NOT to Add

| Technology | Why Avoid |
|------------|-----------|
| Qt Quick Controls Material style | Pulls in entire Material theme engine. Heavy, fights ThemeService, adds startup time. Custom controls already exist. |
| Qt Quick Controls Universal style | Same problem. Use "Basic" (default) style and theme through ThemeService. |
| Qt Quick Effects / MultiEffect | Blur, shadow, glow effects. GPU-intensive, marginal benefit on 1024x600. Drop shadows can be faked with offset semi-transparent rectangles. |
| Qt Quick Particles | Completely unnecessary for a car head unit. |
| QtGraphicalEffects | Removed in Qt 6. Don't even look at it. |
| SVG icons | Material Symbols font already in tree. SVG adds per-icon rendering overhead. Font glyphs are rasterized once. |
| Lottie animations | JSON-based animations. Overkill. `Behavior` + `NumberAnimation` covers everything needed. |
| Qt Quick 3D | No. |
| Additional icon fonts (FontAwesome, etc.) | Material Symbols Outlined covers everything. Second font wastes memory and creates inconsistency. |
| `Qt.labs` imports | Experimental, may break between minor versions. Everything needed is in stable QtQuick / QtQuick.Controls. |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Page transitions | `OpacityAnimator` + `XAnimator` | `PropertyAnimation` on x/opacity | PropertyAnimation runs on UI thread; Animator runs on render thread. Matters when settings page has complex content loading. |
| Icon rendering | Variable font with `font.variableAxes` | Static font (295KB) | Loses optical size axis. Icons look slightly blurry at non-24px sizes. 10MB cost is irrelevant on Pi 4. |
| Settings grid | Static `Grid` in `Flickable` | `GridView` with `ListModel` | GridView adds delegate recycling overhead for 6 fixed items. Simpler code wins. |
| Touch feedback | Scale + opacity `Behavior` | Material ripple shader | Ripple requires fragment shader. Complex to implement and test. Scale + opacity achieves 90% of the effect with 10% of the code. |
| Color transitions | `ColorAnimation` in `Behavior` | `PropertyAnimation` targeting color | `ColorAnimation` handles color space interpolation correctly (no gray flash in RGB lerp). |
| Settings navigation | `StackView` with custom transitions | `Loader` + manual visibility | StackView already in use, handles back-stack automatically, transition properties are built-in. Switching to Loader would be a regression. |

## Integration Points

### With ThemeService
- All new animations use ThemeService colors as from/to values
- New `dividerColor` and `pressedColor` properties follow existing pattern (YAML key -> Q_PROPERTY -> QML binding)
- Day/night mode switch should animate color transitions globally (add `Behavior on color` to themed Rectangle backgrounds for smooth day/night transition)

### With UiMetrics
- All new sizing properties follow existing `Math.round(N * scale)` pattern
- Animation durations are NOT scaled -- 150ms is 150ms regardless of display size (timing perception doesn't scale with pixels)
- Tile grid dimensions adapt to available space via UiMetrics, not hardcoded

### With Existing Controls
- `SettingsListItem`, `SettingsToggle`, `SettingsSlider`, `SegmentedButton` need touch feedback additions (press scale/opacity)
- `Tile.qml` already has hover color change via ThemeService -- add press scale animation
- `SectionHeader` stays unchanged (visual divider, not interactive)
- `MaterialIcon.qml` gets `weight` and `opticalSize` properties (backward compatible -- defaults to current behavior)

## Installation

No new packages required. Everything is built into Qt Quick / Qt Quick Controls, which are already dependencies.

```bash
# No changes to install.sh or CMakeLists.txt for animation/transition support
# These are all QML-side changes using existing Qt modules:
#   - QtQuick (Animator, Behavior, NumberAnimation, ColorAnimation)
#   - QtQuick.Controls (StackView transitions)
```

The only C++ changes are:
1. 2 new Q_PROPERTY additions to ThemeService (`dividerColor`, `pressedColor`)
2. Corresponding YAML theme keys and getters
3. UiMetrics additions are QML-only (it's a QML singleton) -- no C++ needed

## Qt 6.4 / 6.8 Compatibility Notes

| Feature | Qt 6.4 (dev VM) | Qt 6.8 (Pi) | Mitigation |
|---------|-----------------|--------------|------------|
| `OpacityAnimator`, `XAnimator`, `YAnimator` | Yes | Yes | Available since Qt 5.2 |
| `StackView` transition properties | Yes | Yes | Available since Qt Quick Controls 2 (Qt 5.7) |
| `Behavior`, `NumberAnimation`, `ColorAnimation` | Yes | Yes | Core QtQuick since Qt 5.0 |
| `font.variableAxes` | NO | Yes | Don't set it on 6.4; font renders fine without it. Runtime check only. |
| `Easing.OutCubic` | Yes | Yes | All easing curves available in both |
| `Grid` / `GridView` | Yes | Yes | Core QtQuick |
| `ViewTransition` | Yes | Yes | Available since Qt Quick 2.0 |

**Only `font.variableAxes` is 6.8-specific.** Everything else works on both platforms with zero conditional code.

## Sources

- [Qt 6 StackView documentation](https://doc.qt.io/qt-6/qml-qtquick-controls-stackview.html) -- transition properties, anchor caveat (HIGH confidence)
- [Qt 6 Animator type documentation](https://doc.qt.io/qt-6/qml-qtquick-animator.html) -- render thread execution model, available subtypes, ParallelAnimation caveat (HIGH confidence)
- [Qt 6 Behavior type documentation](https://doc.qt.io/qt-6/qml-qtquick-behavior.html) -- implicit animations, single-behavior limitation (HIGH confidence)
- [Material Symbols developer guide](https://developers.google.com/fonts/docs/material_symbols) -- variable font axes, optical size range 20-48dp, static vs variable tradeoffs (HIGH confidence)
- [Qt 6 GridView documentation](https://doc.qt.io/qt-6/qml-qtquick-gridview.html) -- ViewTransition for populate/add/remove animations (HIGH confidence)
- [Qt 6 OpacityAnimator](https://doc.qt.io/qt-6/qml-qtquick-opacityanimator.html) -- scene graph thread execution details (HIGH confidence)
- Existing codebase patterns in `qml/controls/EqBandSlider.qml`, `qml/applications/android_auto/Sidebar.qml`, `qml/components/NotificationArea.qml` -- confirmed Behavior + NumberAnimation works on Pi 4 at 80-200ms durations (HIGH confidence, direct evidence)

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Animation types & performance | HIGH | Qt docs verified, codebase already uses these patterns successfully on Pi 4 |
| StackView transitions | HIGH | Qt docs confirmed, properties available in both Qt 6.4 and 6.8 |
| Material Symbols variable axes | HIGH | Google developer docs verified, `font.variableAxes` confirmed Qt 6.7+ |
| Qt 6.4 compatibility | HIGH | All recommended types except `font.variableAxes` pre-date Qt 6.0 |
| Touch feedback patterns | MEDIUM | Based on common QML patterns and automotive UI conventions, not empirically tested at 1024x600 yet |
| Icon whitespace behavior | MEDIUM | Google docs confirm em-box model but exact padding percentages are approximate |
