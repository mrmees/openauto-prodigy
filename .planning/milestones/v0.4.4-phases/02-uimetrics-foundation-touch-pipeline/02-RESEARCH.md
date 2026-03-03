# Phase 2: UiMetrics Foundation + Touch Pipeline - Research

**Researched:** 2026-03-03
**Domain:** Qt QML singleton scaling, evdev touch coordinate mapping, dual-axis UI scaling
**Confidence:** HIGH

## Summary

This phase modifies two existing subsystems: the `UiMetrics.qml` singleton (scaling/token engine) and the `EvdevTouchReader` C++ class (sidebar touch zone computation). Both are well-understood internal code with clear modification points. No new libraries are needed.

The main technical challenge is wiring ApplicationWindow dimensions into a QML singleton that currently uses `Screen.width`/`Screen.height`. QML singletons cannot directly reference items in the component tree, so the window dimensions must be injected from outside -- either via a C++ context property or by having `main.qml` push values into the singleton after window creation.

**Primary recommendation:** Use a C++ context property (e.g., `DisplayInfo` QObject with `windowWidth`/`windowHeight` Q_PROPERTYs) set from main.cpp after the root window is created. This gives UiMetrics reactive bindings and also provides the display dimensions to C++ code (EvdevTouchReader, ServiceDiscoveryBuilder) from a single source of truth.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Reference baseline: 1024x600 = scale 1.0 (today's display looks identical after this change)
- Dual-axis scaling: `scaleH = windowWidth / 1024`, `scaleV = windowHeight / 600`
- Layout tokens use `min(scaleH, scaleV)` -- guarantees no overflow
- Font tokens use geometric mean `sqrt(scaleH * scaleV)` -- slightly larger on ultrawide for readability
- Dimension source: QML `ApplicationWindow.width` / `ApplicationWindow.height` (not `Screen.*` which is unreliable at Wayland init)
- Fully unclamped -- no 0.9-1.35 range restriction
- Log a warning at startup if computed scale is outside 0.7-2.0 range (helps debug unusual displays, doesn't restrict)
- ALL font tokens get pixel floors, not just small ones
- Tiered minimums: fontTiny/fontSmall = 12px, fontBody = 14px, fontTitle = 16px, fontHeading = 18px (Claude to finalize exact values)
- Floors are configurable via `ui.fontFloor` config key -- users can lower them if they want smaller text on high-DPI or raise them for readability
- Individual token overrides (ui.tokens.fontBody) bypass floors entirely (user always wins per Phase 1 precedence)
- New tokens at minimum: trackThick, trackThin, knobSize (for sliders/scrollbars)
- New tokens follow Phase 1 pattern: overridable via `ui.tokens.*`, registered in YamlConfig initDefaults()
- Token override values are absolute pixels (no multiplication), same as Phase 1
- EvdevTouchReader sidebar hit zones must derive from display config, not hardcoded 1024x600 magic values
- Auto-detect display dimensions from actual window geometry at startup
- display.width/height config becomes a fallback only used if detection fails
- Detected values are in-memory only -- not written back to config.yaml
- EvdevTouchReader receives dynamic display dimension updates (not just at construction time)

### Claude's Discretion
- Full list of new UiMetrics tokens beyond the three specified (based on codebase audit)
- Whether sidebar hit zones use UiMetrics tokens or separate C++ config values (considering AA overscan + dual UI/touch interface complexity)
- Exact pixel floor values for each font tier
- How ApplicationWindow dimensions are wired into UiMetrics (property binding, signal, or context property)
- C++ API for dynamic display dimension updates to EvdevTouchReader

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SCALE-01 | UiMetrics scale factor is unclamped -- derives freely from actual screen dimensions | Remove Math.max/Math.min clamp in autoScale. Currently `Math.max(0.9, Math.min(1.35, ...))` on line 25 of UiMetrics.qml |
| SCALE-02 | UiMetrics uses dual-axis scaling -- min(scaleH, scaleV) for layout, geometric mean for fonts | Replace single-axis `shortest / 600` with dual formulas. See Architecture section for exact formulas |
| SCALE-03 | UiMetrics derives scale from window dimensions (not Screen.*) | Wire ApplicationWindow.width/height into singleton via C++ DisplayInfo context property |
| SCALE-04 | Font tokens have pixel floors (min 12px for smallest) to guarantee automotive legibility | Add Math.max() wrapper around font computations with configurable floor values |
| SCALE-05 | New UiMetrics tokens added for currently-missing dimensions (trackThick, trackThin, knobSize) | Codebase audit found hardcoded slider track (6px), knob (22px/18px), and sidebar icon dimensions |
| TOUCH-01 | EvdevTouchReader sidebar hit zones derived from display config, not hardcoded 1024x600 magic values | Four hardcoded values in setSidebar(): 100.0f, 80.0f, 0.70f, 0.75f. Replace with proportional calculations |
| TOUCH-02 | YamlConfig display dimensions auto-detected from actual screen or validated against it | Create DisplayInfo C++ object that reads window geometry post-show, falls back to config |
</phase_requirements>

## Architecture Patterns

### Key Technical Challenge: Singleton Window Access

QML singletons (pragma Singleton) exist outside the component tree. They **cannot** use `parent`, `Window`, or `ApplicationWindow` attached properties. The current `Screen.width`/`Screen.height` works only because `Screen` is a global attached type, but it reports the monitor resolution, not the actual window size (and is unreliable at Wayland startup).

**Recommended approach: C++ DisplayInfo bridge**

Create a lightweight C++ QObject (`DisplayInfo`) that:
1. Is constructed in `main.cpp`
2. Has `windowWidth` and `windowHeight` Q_PROPERTYs with NOTIFY signals
3. Is exposed as a root context property: `engine.rootContext()->setContextProperty("DisplayInfo", displayInfo)`
4. Gets updated when the root window is created and on window resize

UiMetrics.qml then binds: `readonly property int _winW: DisplayInfo.windowWidth` -- this is reactive and works from a singleton.

```cpp
// src/ui/DisplayInfo.hpp
class DisplayInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowSizeChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowSizeChanged)
public:
    explicit DisplayInfo(QObject* parent = nullptr);
    int windowWidth() const { return windowWidth_; }
    int windowHeight() const { return windowHeight_; }
    void setWindowSize(int w, int h);
signals:
    void windowSizeChanged();
private:
    int windowWidth_ = 1024;  // safe defaults
    int windowHeight_ = 600;
};
```

In `main.cpp`, after `engine.load()`:
```cpp
auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
auto updateSize = [displayInfo, rootWindow]() {
    displayInfo->setWindowSize(rootWindow->width(), rootWindow->height());
};
QObject::connect(rootWindow, &QQuickWindow::widthChanged, displayInfo, updateSize);
QObject::connect(rootWindow, &QQuickWindow::heightChanged, displayInfo, updateSize);
updateSize();  // initial values
```

**Why not alternative approaches:**
- **Config property in QML**: Would require `main.qml` to `UiMetrics.windowWidth = root.width` -- this works but creates a dependency cycle (singleton set from a consumer). Also requires UiMetrics properties to be writable, violating the readonly pattern.
- **C++ function call from QML**: `UiMetrics` would need to import and call a C++ service -- doable but the property binding approach is more Qt-idiomatic.

### UiMetrics Scale Formula Changes

Current (line 23-26 of UiMetrics.qml):
```javascript
readonly property real autoScale: {
    var shortest = Math.min(Screen.width, Screen.height);
    return Math.max(0.9, Math.min(1.35, shortest / 600));
}
```

New:
```javascript
// Raw axis scales (0 = not yet detected, use 1.0)
readonly property int _winW: DisplayInfo.windowWidth
readonly property int _winH: DisplayInfo.windowHeight
readonly property real scaleH: _winW > 0 ? _winW / 1024.0 : 1.0
readonly property real scaleV: _winH > 0 ? _winH / 600.0 : 1.0

// Layout scale: min guarantees no overflow
readonly property real autoScale: Math.min(scaleH, scaleV)

// Font scale: geometric mean for balanced readability
readonly property real _autoFontScale: Math.sqrt(scaleH * scaleV)

// Warning at startup (via Component.onCompleted)
// if autoScale < 0.7 || autoScale > 2.0 -> console.warn
```

The `scale` and `_fontScaleTotal` computations change to use separate bases:
```javascript
readonly property real scale: autoScale * globalScale          // layout
readonly property real _fontScaleTotal: _autoFontScale * globalScale * fontScale  // fonts
```

### Font Pixel Floors

Floors apply after all scaling but before token override check:
```javascript
readonly property int fontBody: {
    var o = _tok("fontBody");
    if (!isNaN(o)) return o;  // override bypasses floor
    var scaled = Math.round(20 * _fontScaleTotal);
    return Math.max(scaled, _floor("fontBody", 14));
}

function _floor(tokenName, defaultFloor) {
    // Check per-token floor from config: ui.fontFloor.fontBody
    var perToken = ConfigService.value("ui.fontFloor." + tokenName);
    if (perToken !== undefined && perToken !== null && Number(perToken) > 0)
        return Number(perToken);
    // Check global floor
    var global = ConfigService.value("ui.fontFloor");
    if (global !== undefined && global !== null && Number(global) > 0)
        return Number(global);
    return defaultFloor;
}
```

**Recommended floor values:**
| Token | Base px | Floor | Rationale |
|-------|---------|-------|-----------|
| fontTiny | 14 | 10 | Smallest text, still readable at arm's length |
| fontSmall | 16 | 12 | Secondary UI text |
| fontBody | 20 | 14 | Primary content, must be glanceable |
| fontTitle | 22 | 16 | Section headers |
| fontHeading | 28 | 18 | Page headings, big impact at small scales |

The `ui.fontFloor` config key should support either:
- Single integer: `ui.fontFloor: 10` (uniform floor for all tokens)
- Per-token map: `ui.fontFloor: { fontBody: 14, fontSmall: 12 }`

The `_floor()` helper checks per-token first, then falls back to global.

### New Token Audit Results

Scanning the codebase for hardcoded pixel values that should become tokens:

**Confirmed new tokens (from CONTEXT.md + audit):**

| Token | Base px | Source | Used in |
|-------|---------|--------|---------|
| `trackThick` | 6 | Slider track height | BottomBar.qml (line 39), Sidebar.qml (lines 47, 112), GestureOverlay.qml |
| `trackThin` | 1 | Separator lines | Sidebar.qml (line 75, 140) |
| `knobSize` | 22 | Slider handle diameter | BottomBar.qml (lines 56-57) |
| `knobSizeSmall` | 18 | Sidebar volume knob | Sidebar.qml (lines 62, 127) |

These 4 tokens are directly needed for Phase 3 QML audit work. The existing tokens already cover most other hardcoded dimensions once the audit replaces them.

**Not recommended as tokens:**
- Sidebar margins (8px/6px) -- these are spacing values, `UiMetrics.spacing` already covers them
- Border widths (1px) -- these are intentionally 1 physical pixel, scaling them looks wrong
- Animation durations -- already non-scaled tokens in UiMetrics

### Sidebar Touch Zone Architecture

**Recommendation: Keep sidebar hit zones as proportional C++ calculations, NOT shared UiMetrics tokens.**

Rationale:
1. The QML Sidebar and C++ EvdevTouchReader operate in completely different coordinate spaces (QML pixels vs evdev 0-4095 range). Sharing a token value doesn't simplify anything -- both sides still need independent coordinate transforms.
2. The sidebar QML uses `Layout.fillHeight` / `Layout.fillWidth` with proportional fills -- exact pixel widths of sub-zones aren't known in QML and don't need to be. The C++ needs fixed ratios (70%/75% for volume/home split), not pixel values.
3. AA overscan/crop introduces additional transforms in the C++ path that have no QML equivalent.

**Concrete changes to EvdevTouchReader::setSidebar():**

Replace the 4 hardcoded magic numbers:
```cpp
// OLD (hardcoded to ~1024x600 assumptions):
sidebarVolX1_ = (displayWidth_ - 100.0f) * evdevPerPixelX;
sidebarHomeX0_ = (displayWidth_ - 80.0f) * evdevPerPixelX;
sidebarVolY1_ = displayHeight_ * 0.70f * evdevPerPixelY;
sidebarHomeY0_ = displayHeight_ * 0.75f * evdevPerPixelY;

// NEW (proportional):
// Horizontal: home button is ~sidebarWidth at the end, volume fills the rest
float homePx = static_cast<float>(sidebarPixelWidth_);
sidebarVolX1_ = (displayWidth_ - homePx * 1.0f) * evdevPerPixelX;
sidebarHomeX0_ = (displayWidth_ - homePx * 0.8f) * evdevPerPixelX;

// Vertical: same ratios, just driven by display dimensions (not hardcoded)
sidebarVolY1_ = displayHeight_ * 0.70f * evdevPerPixelY;  // ratio is OK
sidebarHomeY0_ = displayHeight_ * 0.75f * evdevPerPixelY;  // ratio is OK
```

The vertical ratios (0.70/0.75) are actually fine as ratios -- they're proportional already. The problem is the horizontal sub-zones use absolute pixel values (100px, 80px) that assume a specific display width. These should be derived from the sidebar width.

### Display Dimension Auto-Detection

The `DisplayInfo` C++ object serves dual duty:
1. **QML side**: Feeds UiMetrics with reactive window dimensions
2. **C++ side**: Provides detected dimensions to AndroidAutoPlugin (for EvdevTouchReader) and ServiceDiscoveryBuilder (for AA protocol negotiation)

Flow:
```
Window created -> main.cpp reads rootWindow->width()/height()
                -> DisplayInfo.setWindowSize(w, h)
                -> QML: UiMetrics recomputes all tokens via property binding
                -> C++: AndroidAutoPlugin reads DisplayInfo for EvdevTouchReader
```

**EvdevTouchReader dynamic updates** follow the existing `setAAResolution()` pattern:
```cpp
void EvdevTouchReader::setDisplayDimensions(int w, int h) {
    pendingDisplayWidth_.store(w, std::memory_order_relaxed);
    pendingDisplayHeight_.store(h, std::memory_order_release);
}
```

Consumed in `processSync()` alongside the existing AA resolution check.

### Config Fallback Logic

```
Effective display dims = window geometry (if > 0) ?? config display.width/height ?? 1024x600
```

The detected values are NOT written back to config.yaml. The `display.width`/`display.height` config keys become "fallback hint" values used only if auto-detection fails (e.g., running headless, testing on dev VM where window size isn't meaningful).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Reactive window dimensions in QML singleton | Global JS variable or timer polling | C++ QObject with Q_PROPERTY + NOTIFY | Qt property bindings propagate automatically, no polling needed |
| Cross-thread dimension updates | Mutex-protected shared state | Atomic stores + consume in event loop | Matches existing setAAResolution() pattern, proven lock-free |

## Common Pitfalls

### Pitfall 1: QML Singleton Initialization Order
**What goes wrong:** UiMetrics.qml reads `DisplayInfo.windowWidth` before `DisplayInfo` is set as a context property, getting `undefined`.
**Why it happens:** QML singletons are created lazily on first access, but if other QML components reference UiMetrics during engine.load() (before the root window exists), `DisplayInfo` may not have valid dimensions yet.
**How to avoid:** Initialize `DisplayInfo` with sensible defaults (1024x600) BEFORE `engine.load()`. Set it as a context property before loading QML. Update with real window dimensions after root window exists. The initial 1024x600 defaults mean the first frame renders correctly at the reference resolution.
**Warning signs:** UI elements at wrong size on first frame, then jumping.

### Pitfall 2: Scale Multiplication Stacking
**What goes wrong:** After changing autoScale to dual-axis, the combined `scale = autoScale * globalScale` may produce unexpected values if someone also set `ui.scale` in config.
**Why it happens:** Phase 1 precedence chain: `token override > fontScale > globalScale > autoScale`. If autoScale changes dramatically (e.g., from 1.0 to 0.78 at 800x480), a `globalScale: 1.2` that previously produced 1.2x now produces 0.94x.
**How to avoid:** Log the full computation chain at startup: detected dims -> scaleH/scaleV -> autoScale -> globalScale -> final scale. Makes debugging trivial.
**Warning signs:** Elements visibly smaller than expected after this phase.

### Pitfall 3: Font Floor vs Token Override Precedence
**What goes wrong:** A user sets `ui.tokens.fontBody: 10` (intentionally small) but the floor overrides it back to 14px.
**Why it happens:** Floor check runs after token override check.
**How to avoid:** The CONTEXT.md decision is explicit: "Individual token overrides bypass floors entirely." The code must check `_tok()` FIRST and return early before floor logic.

### Pitfall 4: EvdevTouchReader Dimensions Stale at Construction
**What goes wrong:** EvdevTouchReader is constructed in `AndroidAutoPlugin::initialize()` with config-based display dimensions (1024x600 default), but the real window might be 800x480.
**Why it happens:** The plugin initializes before the QML engine is fully loaded and the window is shown.
**How to avoid:** Use `setDisplayDimensions()` to push real dimensions after the window is created, same pattern as `setAAResolution()`. The reader starts with config defaults and updates once real geometry is known.

### Pitfall 5: Window Size 0x0 During Initialization
**What goes wrong:** On some compositors, `QQuickWindow::width()`/`height()` returns 0 before the window is actually shown.
**How to avoid:** Only push dimensions when both width and height are > 0. Use `QQuickWindow::widthChanged`/`heightChanged` signals to catch the real values once the window is mapped.

## Code Examples

### UiMetrics.qml -- Full Revised autoScale + Font Floor Pattern
```javascript
// Source: existing UiMetrics.qml pattern + CONTEXT.md decisions
pragma Singleton
import QtQuick

QtObject {
    // --- Window dimensions from C++ bridge ---
    readonly property int _winW: DisplayInfo ? DisplayInfo.windowWidth : 1024
    readonly property int _winH: DisplayInfo ? DisplayInfo.windowHeight : 600

    // --- Dual-axis scales ---
    readonly property real scaleH: _winW / 1024.0
    readonly property real scaleV: _winH / 600.0
    readonly property real autoScale: Math.min(scaleH, scaleV)
    readonly property real _autoFontScale: Math.sqrt(scaleH * scaleV)

    // --- Config overrides (Phase 1 pattern, unchanged) ---
    readonly property var _cfgScale: ConfigService.value("ui.scale")
    readonly property var _cfgFontScale: ConfigService.value("ui.fontScale")
    readonly property real globalScale: { /* ... same as Phase 1 ... */ }
    readonly property real fontScale: { /* ... same as Phase 1 ... */ }

    // --- Combined scales ---
    readonly property real scale: autoScale * globalScale
    readonly property real _fontScaleTotal: _autoFontScale * globalScale * fontScale

    // --- Font floor helper ---
    function _floor(tokenName, defaultFloor) {
        var perToken = ConfigService.value("ui.fontFloor." + tokenName);
        if (perToken !== undefined && perToken !== null && Number(perToken) > 0)
            return Number(perToken);
        var global = ConfigService.value("ui.fontFloor");
        if (global !== undefined && global !== null && Number(global) > 0)
            return Number(global);
        return defaultFloor;
    }

    // Font with floor (override bypasses floor)
    readonly property int fontBody: {
        var o = _tok("fontBody");
        if (!isNaN(o)) return o;
        return Math.max(Math.round(20 * _fontScaleTotal), _floor("fontBody", 14));
    }

    // New slider/scrollbar tokens
    readonly property int trackThick: { var o = _tok("trackThick"); return isNaN(o) ? Math.round(6 * scale) : o; }
    readonly property int trackThin: { var o = _tok("trackThin"); return isNaN(o) ? Math.max(Math.round(1 * scale), 1) : o; }
    readonly property int knobSize: { var o = _tok("knobSize"); return isNaN(o) ? Math.round(22 * scale) : o; }
    readonly property int knobSizeSmall: { var o = _tok("knobSizeSmall"); return isNaN(o) ? Math.round(18 * scale) : o; }

    // Startup logging
    Component.onCompleted: {
        console.log("UiMetrics: window=" + _winW + "x" + _winH
                   + " scaleH=" + scaleH.toFixed(3) + " scaleV=" + scaleV.toFixed(3)
                   + " layout=" + scale.toFixed(3) + " font=" + _fontScaleTotal.toFixed(3));
        if (autoScale < 0.7 || autoScale > 2.0)
            console.warn("UiMetrics: autoScale " + autoScale.toFixed(3)
                       + " is outside normal range (0.7-2.0)");
    }
}
```

### EvdevTouchReader -- setDisplayDimensions() Pattern
```cpp
// Source: mirrors existing setAAResolution() pattern
void EvdevTouchReader::setDisplayDimensions(int w, int h) {
    pendingDisplayWidth_.store(w, std::memory_order_relaxed);
    pendingDisplayHeight_.store(h, std::memory_order_release);
    qCDebug(lcAA) << "Pending display dimension update:" << w << "x" << h;
}

// In processSync(), after existing AA resolution check:
int newDisplayH = pendingDisplayHeight_.load(std::memory_order_acquire);
if (newDisplayH > 0) {
    int newDisplayW = pendingDisplayWidth_.load(std::memory_order_relaxed);
    displayWidth_ = newDisplayW;
    displayHeight_ = newDisplayH;
    pendingDisplayWidth_.store(0, std::memory_order_relaxed);
    pendingDisplayHeight_.store(0, std::memory_order_relaxed);
    // Recompute sidebar zones and letterbox with new display dims
    if (sidebarEnabled_)
        setSidebar(sidebarEnabled_, sidebarPixelWidth_, sidebarPosition_);
    computeLetterbox();
    qCDebug(lcAA) << "Applied display dimension update:" << displayWidth_ << "x" << displayHeight_;
}
```

### main.cpp -- DisplayInfo Wiring
```cpp
// Source: project patterns from existing main.cpp
auto* displayInfo = new DisplayInfo(&app);

// Set defaults from config (fallback if window detection fails)
displayInfo->setWindowSize(yamlConfig->displayWidth(), yamlConfig->displayHeight());

// Expose to QML BEFORE engine.load()
engine.rootContext()->setContextProperty("DisplayInfo", displayInfo);

// ... engine.load(url) ...

// After window exists, wire real dimensions
auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
if (rootWindow) {
    auto updateSize = [displayInfo, rootWindow]() {
        int w = rootWindow->width(), h = rootWindow->height();
        if (w > 0 && h > 0)
            displayInfo->setWindowSize(w, h);
    };
    QObject::connect(rootWindow, &QQuickWindow::widthChanged, displayInfo, updateSize);
    QObject::connect(rootWindow, &QQuickWindow::heightChanged, displayInfo, updateSize);
    updateSize();
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `Screen.width/height` for scale | Window geometry for scale | This phase | Reliable at Wayland init, responds to resize |
| Single-axis scale (shortest edge / 600) | Dual-axis: min(h,v) for layout, geomean for fonts | This phase | Correct proportions at non-16:9 ratios |
| Clamped 0.9-1.35 range | Unclamped with warning outside 0.7-2.0 | This phase | Supports 800x480 and larger displays |
| Hardcoded sidebar touch zones (100px/80px) | Proportional to sidebar width | This phase | Works at any display resolution |

## Open Questions

1. **fontFloor config format: single value or map?**
   - What we know: CONTEXT.md says "a single integer or per-token floors"
   - Recommendation: Support both. `_floor()` helper checks `ui.fontFloor.{tokenName}` first, then `ui.fontFloor` as scalar fallback. YamlConfig initDefaults registers `ui.fontFloor: 0` (disabled by default -- built-in floors are the defaults in the _floor() function's defaultFloor parameter).

2. **Should fontTiny become overridable?**
   - What we know: Currently fontTiny is NOT overridable (no `_tok()` check). All other font tokens are.
   - Recommendation: Make it overridable for consistency. It's getting a floor, so it should also support override.

3. **ServiceDiscoveryBuilder display dimensions**
   - What we know: `ServiceDiscoveryBuilder` reads `yamlConfig_->displayWidth()/displayHeight()` for AA margin calculations. These should ideally use detected dimensions too.
   - Recommendation: Pass the `DisplayInfo` object to the orchestrator/builder so it can use detected dimensions. This is a clean change since the builder already receives yamlConfig.

## Sources

### Primary (HIGH confidence)
- `qml/controls/UiMetrics.qml` -- current singleton implementation, all token patterns
- `src/core/aa/EvdevTouchReader.cpp` -- sidebar touch zone calculations (lines 137-175)
- `src/core/aa/EvdevTouchReader.hpp` -- setAAResolution() atomic update pattern
- `src/plugins/android_auto/AndroidAutoPlugin.cpp` -- display dimension flow (lines 73-80)
- `src/core/YamlConfig.cpp` -- initDefaults() registration pattern (lines 134-148)
- `qml/applications/android_auto/Sidebar.qml` -- sidebar visual layout, hardcoded pixel values
- `qml/components/BottomBar.qml` -- slider hardcoded dimensions (track 6px, knob 22px)
- `src/main.cpp` -- QML engine setup, context property pattern, root window access

### Secondary (MEDIUM confidence)
- Qt 6 documentation: QML Singletons cannot access component tree (verified against Qt docs pattern)
- Qt 6 documentation: QQuickWindow width/height signals for resize detection

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all changes are to existing internal code, no new dependencies
- Architecture: HIGH -- DisplayInfo bridge pattern is well-understood Qt idiom, verified against codebase patterns
- Pitfalls: HIGH -- all identified from direct code reading of the actual files being modified

**Research date:** 2026-03-03
**Valid until:** 2026-04-03 (stable -- internal codebase, no external dependency changes)
