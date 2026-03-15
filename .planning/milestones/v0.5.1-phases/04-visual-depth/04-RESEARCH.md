# Phase 4: Visual Depth - Research

**Researched:** 2026-03-10
**Domain:** Qt 6 QML visual effects, M3 elevation system, automotive UI polish
**Confidence:** HIGH

## Summary

This phase adds Material Design 3-inspired depth and shadow effects to all interactive elements in the UI. The primary technical enabler is Qt's `MultiEffect` type (available since Qt 6.5, via `import QtQuick.Effects`), which provides GPU-accelerated drop shadows. The dev VM must be upgraded from Qt 6.4 to Qt 6.8 first, as MultiEffect is not available in Qt 6.4. The Pi already runs Qt 6.8.2 with `qml6-module-qtquick-effects` installed.

The codebase audit found 28+ button/interactive patterns across 14 QML files that use ad-hoc `pressed ? color1 : color2` feedback. These will be consolidated into 3 reusable M3 button components (ElevatedButton, FilledButton, OutlinedButton) that encapsulate shadow, press animation, and state layer behavior. The navbar shadow replacement (gradient fade instead of 1px border) is a simpler change isolated to `Navbar.qml`.

**Primary recommendation:** Upgrade dev VM Qt first (Wave 0), then build reusable M3 button components with MultiEffect shadows, then migrate all existing button patterns to use them, then handle navbar gradient last.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Upgrade dev VM (claude-dev) Qt from 6.4 to 6.8 before implementing depth effects
- M3 elevation shadow (drop shadow beneath buttons) at Level 2 (medium) intensity
- Shadow color follows M3 spec with proper day/night mode switching
- Surface tint overlay at higher elevations (subtle primary-color tint per M3)
- Automotive-tuned -- M3 levels as starting point, adjusted for 1024x600 glanceability
- Scale down (0.95) + shadow reduction on press (shadow shrinks/disappears, button sinks)
- Remove existing opacity dim (0.85) -- scale + shadow is enough feedback
- Keep existing scale animation values (0.95 for Tiles, 0.97 for smaller items)
- Color change to primaryContainer on press remains (M3 standard, already in place)
- Gradient fade (4-8dp) from navbar edge into content area, replacing current 1px barShadow border
- barShadow border removed entirely, gradient fade is the sole separation treatment
- Gradient skipped when AA is active (black navbar blends with phone's dark status bar)
- Gradient adapts to theme (derived from navbar background color with opacity)
- All tappable elements get depth: Tiles, SettingsListItems, dialog buttons, SegmentedButtons, FullScreenPicker rows
- Popup panels (NavbarPopup, power menu, GestureOverlay) at higher M3 elevation (Level 3-4)
- Dialogs (ExitDialog, PairingDialog) at M3 Level 3 with scrim + elevated surface
- Create reusable QML components: ElevatedButton.qml, FilledButton.qml, OutlinedButton.qml
- Built-in shadow, press behavior, state layers, and elevation management
- All existing button patterns migrate to these components
- State layers (semi-transparent onSurface overlay at 8-12% opacity) for touch feedback
- M3-inspired with automotive tuning (not strict spec adherence)
- Fewer distinct elevation levels than M3 spec if some are indistinguishable on hardware

### Claude's Discretion
- Exact shadow blur radius and offset values (tuned for 1024x600)
- Which M3 elevation levels to collapse if they're visually identical at target DPI
- Surface tint opacity values (perceptible but not garish)
- State layer implementation approach (Rectangle overlay vs shader)
- Qt 6.8 installation method on dev VM (PPA vs aqtinstall vs source)
- Whether to keep any #if QT_VERSION guards or clean-break to Qt 6.8 minimum

### Deferred Ideas (OUT OF SCOPE)
- Ripple animation (expanding circle from touch point) -- future phase
- Animated elevation transitions (smooth shadow growth on focus) -- can add after base system works
- Per-component elevation customization via config -- overkill for v1.0
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| STY-01 | Buttons have subtle 3D depth effect (shadow, bevel, or gradient) that responds to press state | MultiEffect shadow on reusable M3 button components; shadow reduces on press; state layer overlay for feedback |
| STY-02 | Taskbar/navbar has subtle depth treatment distinguishing it from content area | Gradient Rectangle fading from navbar edge color to transparent, replacing existing barShadow 1px border |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt Quick Effects (MultiEffect) | 6.8.2 | GPU-accelerated drop shadows and blur | Official Qt replacement for deprecated DropShadow; combines blur+shadow in single shader pass |
| Qt Quick (Rectangle + gradient) | 6.8.2 | Navbar gradient fade | No extra dependency; simple gradient Rectangle is more performant than a shader effect for edge fade |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| aqtinstall | latest | Install Qt 6.8 on dev VM | One-time prerequisite; `pip install aqtinstall` then `aqt install-qt linux desktop 6.8.2 gcc_64` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| MultiEffect | RectangularShadow | Better performance for rectangles, but requires Qt 6.9+ (Pi has 6.8.2) -- not available |
| MultiEffect | Manual Rectangle shadow (offset dark rect behind element) | No blur, looks cheap -- not suitable for M3 elevation |
| MultiEffect | Qt5Compat.GraphicalEffects DropShadow | Deprecated compatibility module, worse performance than MultiEffect |

**Installation (dev VM upgrade):**
```bash
pip install aqtinstall
aqt install-qt linux desktop 6.8.2 gcc_64 --outputdir ~/Qt
# Then update CMake to point to ~/Qt/6.8.2/gcc_64/
```

**CMake addition needed:**
```cmake
# No new find_package component needed -- MultiEffect is part of QtQuick
# But the QML runtime needs qml6-module-qtquick-effects installed
# Pi already has it; dev VM gets it via aqtinstall
```

## Architecture Patterns

### Recommended Project Structure
```
qml/
├── controls/
│   ├── ElevatedButton.qml    # NEW: M3 elevated button (shadow + press)
│   ├── FilledButton.qml      # NEW: M3 filled button (solid bg + shadow + press)
│   ├── OutlinedButton.qml    # NEW: M3 outlined button (border + press)
│   ├── Tile.qml              # REFACTOR: delegate to FilledButton internally
│   ├── SettingsListItem.qml  # REFACTOR: add shadow, remove opacity dim
│   ├── SegmentedButton.qml   # REFACTOR: add shadow, remove opacity dim
│   └── FullScreenPicker.qml  # REFACTOR: remove opacity dim, add press shadow
├── components/
│   ├── Navbar.qml            # MODIFY: gradient fade replaces barShadow
│   ├── ExitDialog.qml        # REFACTOR: use M3 button components
│   ├── PairingDialog.qml     # REFACTOR: use M3 button components
│   ├── GestureOverlay.qml    # REFACTOR: use M3 button components, panel elevation
│   └── NavbarPopup.qml       # MODIFY: panel elevation treatment
```

### Pattern 1: MultiEffect Shadow on Button Component
**What:** Wrap each interactive element with a MultiEffect that provides a blurred drop shadow beneath it.
**When to use:** All button/tile/interactive components.
**Example:**
```qml
// Source: https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html
import QtQuick
import QtQuick.Effects

Item {
    id: root
    property bool pressed: false
    property int elevation: 2  // M3 level

    // The visual content
    Rectangle {
        id: content
        anchors.fill: parent
        radius: UiMetrics.radiusSmall
        color: ThemeService.surfaceContainerLow
        // ... button content here
    }

    // Shadow effect
    MultiEffect {
        source: content
        anchors.fill: content
        shadowEnabled: true
        shadowColor: ThemeService.shadow
        shadowOpacity: root.pressed ? 0.1 : 0.3
        shadowBlur: root.pressed ? 0.15 : 0.4
        shadowVerticalOffset: root.pressed ? 1 : 3
        shadowHorizontalOffset: 0
        shadowScale: 1.0

        Behavior on shadowOpacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
        Behavior on shadowBlur { NumberAnimation { duration: UiMetrics.animDurationFast } }
        Behavior on shadowVerticalOffset { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }
}
```

### Pattern 2: State Layer Overlay
**What:** Semi-transparent rectangle overlay that appears on press/hover for M3 state feedback.
**When to use:** All interactive elements, as supplement to shadow reduction.
**Recommendation:** Use a simple Rectangle overlay (not a shader). Rectangle is simpler, more maintainable, and performant enough for an overlay.
```qml
// State layer: semi-transparent onSurface overlay on press
Rectangle {
    anchors.fill: parent
    radius: parent.radius
    color: ThemeService.onSurface
    opacity: mouseArea.pressed ? 0.10 : 0.0
    Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
}
```

### Pattern 3: Navbar Gradient Fade
**What:** A gradient Rectangle that fades from the navbar background color to transparent.
**When to use:** Along the content-facing edge of the navbar.
```qml
Rectangle {
    // Positioned just outside the navbar content-facing edge
    width: parent.width  // for top/bottom navbar
    height: Math.round(6 * UiMetrics.scale)  // 4-8dp
    gradient: Gradient {
        GradientStop { position: 0.0; color: navbar.barBg }
        GradientStop { position: 1.0; color: "transparent" }
    }
    visible: !navbar.aaActive  // Skip during AA
}
```

### Pattern 4: Elevation-Based Surface Tint
**What:** Higher-elevation surfaces get a subtle primary color tint per M3 spec.
**When to use:** Popup panels, dialogs, gesture overlay -- Level 3-4 surfaces.
```qml
// Surface tint: blend surface color with primary at low opacity
readonly property color tintedSurface: Qt.rgba(
    ThemeService.surfaceContainerHigh.r * 0.95 + ThemeService.primary.r * 0.05,
    ThemeService.surfaceContainerHigh.g * 0.95 + ThemeService.primary.g * 0.05,
    ThemeService.surfaceContainerHigh.b * 0.95 + ThemeService.primary.b * 0.05,
    1.0
)
```

### Anti-Patterns to Avoid
- **MultiEffect on animated sources:** "Avoid using source items which animate, so blurring doesn't need to be regenerated in every frame." The button content itself is static; only the shadow properties animate (which is fine).
- **Excessive blurMax:** Keep `blurMax` small (default is fine). Larger values waste GPU. Use `blurMultiplier` to increase apparent blur instead.
- **Shadow on every frame:** Don't put MultiEffect on items that redraw every frame (like video). Only use on static/quasi-static UI elements.
- **Nested MultiEffects:** Don't stack MultiEffect inside MultiEffect. Each one is a full-screen shader pass.

## M3 Elevation System (Automotive-Tuned)

### Recommended Collapsed Levels

M3 defines 6 levels (0-5). For a 1024x600 screen at arm's length, collapse to 4 distinguishable levels:

| App Level | M3 Levels | Shadow Blur | Y-Offset | Opacity | Surface Tint | Used By |
|-----------|-----------|-------------|----------|---------|--------------|---------|
| 0 (Flat) | M3 Level 0 | 0 | 0 | 0 | None | Disabled elements, backgrounds |
| 1 (Resting) | M3 Level 1-2 | 0.35 | 2px | 0.25 | None | Buttons, tiles, list items |
| 2 (Raised) | M3 Level 3 | 0.50 | 4px | 0.30 | 3-5% primary | Popups, dialogs, gesture overlay |
| 3 (Floating) | M3 Level 4-5 | 0.70 | 6px | 0.35 | 5-8% primary | (reserved for future use) |

**Rationale for collapsing:** M3 Levels 1 and 2 differ by ~1dp shadow -- indistinguishable at 1024x600 at arm's length. Levels 4 and 5 are similarly close. Collapsing to 4 levels keeps the hierarchy clear without wasting visual bandwidth.

### Press State Transition
| Property | Resting (Level 1) | Pressed |
|----------|-------------------|---------|
| Shadow blur | 0.35 | 0.15 |
| Shadow Y-offset | 2px | 1px |
| Shadow opacity | 0.25 | 0.10 |
| Scale | 1.0 | 0.95 (tiles) / 0.97 (smaller) |
| State layer | 0% opacity | 10% onSurface |
| Opacity (old) | 1.0 | 1.0 (removed -- no more 0.85 dim) |

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Blurred drop shadows | Stacked semi-transparent offset Rectangles | MultiEffect with shadowEnabled | Rectangles can't blur; they create hard-edged fake shadows that look amateur |
| Shadow color for day/night | Hardcoded rgba values | ThemeService.shadow (already exists as Q_PROPERTY) | Automatic day/night switching |
| Surface tint calculation | Manual RGB math in every component | Helper function or property in UiMetrics/ThemeService | Avoid duplicating tint math; one source of truth |

## Common Pitfalls

### Pitfall 1: MultiEffect Padding Clipping
**What goes wrong:** Shadow gets clipped at the item boundary because MultiEffect needs extra space for the shadow to render.
**Why it happens:** MultiEffect renders into the source item's bounding box by default.
**How to avoid:** Set `autoPaddingEnabled: true` (default) and ensure the MultiEffect item has enough space. If shadow is clipped, add explicit `paddingRect` that accounts for shadowVerticalOffset + blur radius.
**Warning signs:** Shadow appears cut off on one side, or shadow disappears at edges.

### Pitfall 2: Performance on Raspberry Pi 4
**What goes wrong:** Too many MultiEffect instances cause frame drops on the Pi's VideoCore VI GPU.
**Why it happens:** Each MultiEffect is a shader pass with texture sampling.
**How to avoid:** Limit MultiEffect to visible items only. For list views (SettingsListItem, FullScreenPicker delegates), use `visible` binding or clip. The launcher grid shows ~6 tiles max, so 6 MultiEffect instances is fine. Settings list may show ~8 items, also fine.
**Warning signs:** Dropped frames when scrolling settings lists or opening dialogs with many buttons.

### Pitfall 3: MultiEffect on Qt Controls Button
**What goes wrong:** Qt Controls `Button` has its own rendering pipeline. Wrapping it with MultiEffect may produce unexpected results.
**Why it happens:** Button's `background` is a separate item from its `contentItem`.
**How to avoid:** Don't use MultiEffect on Qt Controls Button. Instead, build custom button components from scratch using `Item` + `Rectangle` + `MouseArea` + `MultiEffect`. The existing codebase already follows this pattern (custom backgrounds on Buttons, or plain Rectangle+MouseArea).

### Pitfall 4: Removing Opacity Dim Breaks Disabled State
**What goes wrong:** Removing the `opacity: pressed ? 0.85 : 1.0` pattern also removes the disabled state visual (opacity 0.5).
**Why it happens:** Some components use opacity for both press feedback AND disabled indication (e.g., Tile.qml line 17: `tileEnabled ? 1.0 : 0.5`).
**How to avoid:** When removing opacity dim for press, preserve the disabled state opacity. Separate the two concerns: press feedback via shadow+state layer, disabled via reduced opacity.

### Pitfall 5: Shadow Hue in Dark Mode
**What goes wrong:** Black shadows on dark surfaces are invisible.
**Why it happens:** M3 dark theme surfaces are already dark; a black shadow has no contrast.
**How to avoid:** Use `ThemeService.shadow` which can be tuned per theme. In dark mode, shadows may need slightly lighter color or the shadow can be replaced with a subtle border/outline. Alternatively, rely more on surface tint differentiation in dark mode (M3's actual approach -- dark mode uses tint, not shadow, for elevation).

### Pitfall 6: aqtinstall Qt Version vs System Qt
**What goes wrong:** CMake picks up system Qt 6.4 instead of aqtinstall Qt 6.8.
**Why it happens:** System Qt is in the default search path.
**How to avoid:** Set `CMAKE_PREFIX_PATH` explicitly to the aqtinstall path: `cmake -DCMAKE_PREFIX_PATH=~/Qt/6.8.2/gcc_64 ..`

## Qt Version Upgrade: Dev VM Strategy

### Recommended Approach: aqtinstall (Claude's Discretion)
**Why aqtinstall over PPA or source:**
- PPAs don't exist for Qt 6.8 on Ubuntu 24.04
- Source compilation takes 2+ hours and requires extensive build deps
- aqtinstall downloads pre-built binaries in ~2 minutes
- Non-destructive: installs alongside system Qt, doesn't break existing packages

**Installation steps:**
```bash
pip install aqtinstall
aqt install-qt linux desktop 6.8.2 gcc_64 --outputdir ~/Qt
# Verify:
~/Qt/6.8.2/gcc_64/bin/qmake6 --version
```

**Build configuration change:**
```bash
# Old (implicit system Qt):
cmake ..
# New (explicit Qt 6.8):
cmake -DCMAKE_PREFIX_PATH=~/Qt/6.8.2/gcc_64 ..
```

### QT_VERSION Guards: Clean Break (Claude's Discretion)
**Recommendation:** Remove all `#if QT_VERSION` guards and set Qt 6.8 as minimum.

**Rationale:**
- 8 existing guards, all in the video pipeline (VideoFramePool, VideoDecoder, DmaBufVideoBuffer)
- They exist solely for dev VM compatibility with Qt 6.4
- Once dev VM runs 6.8, the guards serve no purpose
- The Pi (target platform) already runs 6.8.2
- Removing guards simplifies the codebase and eliminates dead code paths

**CMakeLists.txt change:**
```cmake
# Add minimum version enforcement:
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Network DBus)
```

## Codebase Audit: Migration Targets

### Elements Using opacity dim (to remove)
| File | Pattern | Scale | Notes |
|------|---------|-------|-------|
| Tile.qml:17 | `opacity: pressed ? 0.85 : (enabled ? 1.0 : 0.5)` | 0.95 | Keep disabled opacity |
| SettingsListItem.qml:16 | `opacity: pressed ? 0.85 : 1.0` | 0.97 | Remove entirely |
| SegmentedButton.qml:69 | `opacity: pressed ? 0.85 : 1.0` | 0.97 | Remove entirely |
| FullScreenPicker.qml:67,195 | `opacity: pressed ? 0.85 : 1.0` | 0.97 | Remove from both row and delegate |
| ConnectionSettings.qml:144 | `opacity: pressed ? 0.85 : 1.0` | 0.95 | Remove, add shadow |

### Elements Using `parent.pressed ? color1 : color2` (to standardize)
| File | Count | Component Type |
|------|-------|---------------|
| ExitDialog.qml | 3 | Qt Controls Button with custom background |
| GestureOverlay.qml | 3 | Qt Controls Button with custom background |
| SystemSettings.qml | 7 | Qt Controls Button with custom background |
| AboutSettings.qml | 1 | Qt Controls Button with custom background |
| PhoneView.qml | 5 | Qt Controls Button with custom background |
| IncomingCallOverlay.qml | 2 | Qt Controls Button with custom background |
| BtAudioView.qml | 3 | Qt Controls Button with custom background |
| NavbarPopup.qml | 1 | inline Rectangle + MouseArea (power menu) |
| Navbar.qml (power menu) | 1 | inline Rectangle + MouseArea |

### Navbar Shadow (Navbar.qml lines 43-48)
Current: `Rectangle { id: shadow; color: navbar.barShadowColor; radius: 0 }` (1px border effect)
Target: Gradient fade Rectangle (4-8dp), positioned on the content-facing edge, adapting to edge position.

### Popup/Dialog Elevation Targets
| Component | Current | Target |
|-----------|---------|--------|
| NavbarPopup | `surfaceContainerHigh` + 1px border | Level 2 shadow + surface tint |
| Power menu (in Navbar.qml) | `surfaceContainerHigh` + 1px border | Level 2 shadow + surface tint |
| GestureOverlay panel | `surfaceContainerHighest` @ 87% + 2px border | Level 2 shadow + surface tint |
| ExitDialog | `surfaceContainerHigh` + 1px border | Level 2 shadow + surface tint |
| PairingDialog | `surfaceContainerHigh` + no border | Level 2 shadow + surface tint |

## Code Examples

### ElevatedButton.qml (Skeleton)
```qml
import QtQuick
import QtQuick.Effects

Item {
    id: root

    property string text: ""
    property string icon: ""
    property color buttonColor: ThemeService.surfaceContainerLow
    property color pressedColor: ThemeService.primaryContainer
    property color textColor: ThemeService.onSurface
    property real buttonScale: 0.97
    property bool buttonEnabled: true

    signal clicked()

    implicitWidth: UiMetrics.overlayBtnW
    implicitHeight: UiMetrics.overlayBtnH

    scale: mouseArea.pressed && buttonEnabled ? buttonScale : 1.0
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    // Shadow
    MultiEffect {
        source: bg
        anchors.fill: bg
        shadowEnabled: true
        shadowColor: ThemeService.shadow
        shadowOpacity: mouseArea.pressed ? 0.10 : 0.25
        shadowBlur: mouseArea.pressed ? 0.15 : 0.35
        shadowVerticalOffset: mouseArea.pressed ? 1 : 2
        shadowHorizontalOffset: 0
        Behavior on shadowOpacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
        Behavior on shadowBlur { NumberAnimation { duration: UiMetrics.animDurationFast } }
        Behavior on shadowVerticalOffset { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: UiMetrics.radiusSmall
        color: mouseArea.pressed ? root.pressedColor : root.buttonColor
        opacity: root.buttonEnabled ? 1.0 : 0.5

        // State layer
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: ThemeService.onSurface
            opacity: mouseArea.pressed ? 0.10 : 0.0
            Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
        }

        // Content goes here (text, icon, etc.)
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        cursorShape: root.buttonEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (root.buttonEnabled) root.clicked()
    }
}
```

### Navbar Gradient Fade (Edge-Aware)
```qml
// Replace shadow Rectangle (lines 43-48 in Navbar.qml)
Rectangle {
    id: navGradient
    visible: !navbar.aaActive
    z: -1  // Behind navbar content

    // Gradient direction depends on edge position
    gradient: Gradient {
        orientation: navbar.isVertical ? Gradient.Horizontal : Gradient.Vertical
        GradientStop {
            position: 0.0
            color: Qt.rgba(navbar.barBg.r, navbar.barBg.g, navbar.barBg.b, 0.6)
        }
        GradientStop { position: 1.0; color: "transparent" }
    }

    // Position on content-facing edge
    states: [
        State {
            when: navbar.edge === "bottom"
            PropertyChanges { target: navGradient; width: navbar.width; height: Math.round(6 * UiMetrics.scale) }
            AnchorChanges { target: navGradient; anchors.bottom: navbar.top; anchors.left: navbar.left }
        }
        // ... similar for top, left, right
    ]
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Qt5Compat DropShadow | MultiEffect (Qt 6.5+) | Qt 6.5 (2023) | Single-shader multi-effect, better perf |
| MultiEffect for rectangles | RectangularShadow (Qt 6.9+) | Qt 6.9 (2025) | SDF-based, much faster -- but NOT available on Qt 6.8 |
| Opacity dim on press | Scale + shadow reduction + state layer | M3 spec | More physical, better depth language |

**Deprecated/outdated:**
- `Qt5Compat.GraphicalEffects.DropShadow`: Compatibility module, slower than MultiEffect
- `QGraphicsDropShadowEffect`: Qt Widgets API, not applicable to QML
- Hardcoded shadow colors: Use ThemeService.shadow for day/night awareness

## Open Questions

1. **MultiEffect performance on Pi 4 with ~15 simultaneous instances**
   - What we know: Pi 4 GPU (VideoCore VI) handles basic Qt rendering fine at 60fps. MultiEffect is a shader pass per instance.
   - What's unclear: Real-world impact of 6-8 shadowed tiles + 2-3 popup panels simultaneously. The launcher grid is the worst case.
   - Recommendation: Implement and test on Pi. If perf is an issue, reduce blurMax or fall back to simple offset Rectangle shadows for list items.

2. **aqtinstall module availability**
   - What we know: `aqt install-qt linux desktop 6.8.2 gcc_64` installs base Qt. MultiEffect needs the Effects QML module.
   - What's unclear: Whether the base installation includes QtQuick.Effects or if a module flag (`-m`) is needed.
   - Recommendation: Try base install first; if `import QtQuick.Effects` fails, add the module.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest + Qt Test (C++) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build && ctest --output-on-failure -R theme` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| STY-01 | Buttons have shadow that responds to press | manual-only | Visual verification on Pi | N/A |
| STY-02 | Navbar has depth treatment | manual-only | Visual verification on Pi | N/A |

**Manual-only justification:** Both requirements are purely visual. Shadow rendering correctness depends on GPU output. No programmatic assertion can verify "subtle 3D effect" -- it must be evaluated by human eye on target hardware.

### Sampling Rate
- **Per task commit:** `cd build && ctest --output-on-failure` (existing tests still pass)
- **Per wave merge:** Full suite + visual check on Pi
- **Phase gate:** Full suite green + Pi visual verification before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] Qt 6.8 installation on dev VM -- prerequisite for any implementation
- [ ] Verify `import QtQuick.Effects` works in both dev VM build and Pi build
- [ ] Verify existing 65 tests pass after Qt 6.8 upgrade on dev VM

## Sources

### Primary (HIGH confidence)
- [MultiEffect QML Type docs (Qt 6.10.2, API stable since 6.5)](https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html) -- shadow properties, import, performance notes
- [RectangularShadow docs](https://doc.qt.io/qt-6/qml-qtquick-effects-rectangularshadow.html) -- confirmed Qt 6.9+ only, NOT available for this phase
- [RectangularShadow announcement blog](https://www.qt.io/blog/rectangularshadow-fast-rectangle-shadows-in-qt-6.9) -- performance comparison, SDF approach
- [Debian Trixie qt6-base-dev](https://packages.debian.org/trixie/qt6-base-dev) -- confirmed Qt 6.8.2 on Pi
- Pi SSH verification -- `qml6-module-qtquick-effects` 6.8.2 installed

### Secondary (MEDIUM confidence)
- [aqtinstall docs](https://aqtinstall.readthedocs.io/en/latest/getting_started.html) -- installation method for dev VM
- [M3 elevation CSS values](https://studioncreations.com/blog/material-design-3-box-shadow-css-values/) -- shadow blur/offset reference values
- [Qt forum: MultiEffect for shadow](https://forum.qt.io/topic/145150/qt-6-5-multieffect-for-shadow) -- community usage patterns

### Tertiary (LOW confidence)
- M3 elevation level to component mapping -- M3 docs were not fully parseable; level assignments based on general M3 knowledge

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - MultiEffect verified available on Qt 6.8, confirmed on Pi
- Architecture: HIGH - Patterns derived from official Qt docs + existing codebase audit
- Pitfalls: HIGH - Based on official docs warnings + practical Pi hardware constraints
- M3 elevation values: MEDIUM - CSS values sourced from community reference; will need visual tuning on hardware

**Research date:** 2026-03-10
**Valid until:** 2026-04-10 (stable -- Qt 6.8 API is frozen, M3 spec is mature)
