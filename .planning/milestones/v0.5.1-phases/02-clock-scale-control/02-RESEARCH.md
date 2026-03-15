# Phase 2: Clock & Scale Control - Research

**Researched:** 2026-03-08
**Domain:** Qt/QML UI controls, reactive config binding, font sizing
**Confidence:** HIGH

## Summary

This phase is entirely within the existing codebase's established patterns. The clock changes are straightforward QML text formatting and font sizing in `NavbarControl.qml`. The scale stepper is a new QML control in `DisplaySettings.qml` that writes to the existing `ui.scale` config key. The 24h toggle reuses the existing `SettingsToggle` component.

The one significant technical gotcha is the **reactivity gap in UiMetrics**: the `_cfgScale` property is bound via `ConfigService.value("ui.scale")` which is a one-time Q_INVOKABLE call, not a reactive property binding. When the stepper writes a new `ui.scale` value, `UiMetrics` will NOT automatically update. This must be solved with a `Connections` block listening to `ConfigService.configChanged` signal.

**Primary recommendation:** Fix UiMetrics reactivity first, then layer on the clock and stepper UI changes. Everything else uses established patterns.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Clock auto-sizes to fill available navbar control area (~80% of height), bold/semi-bold weight
- 12h format shows bare time only -- no AM/PM, no dot, no indicator. Just "12:34"
- Solid colon, no blinking. Default format is 12h
- 24h toggle lives in Display settings under a new "Clock" section header, between General and Navbar sections
- Config key: `display.clock_24h` (boolean, default false)
- Toggle updates navbar clock immediately (live, no restart)
- Scale stepper located directly under Screen info row, before General section (no section header)
- Plus/minus 0.1 increments via [-] and [+] buttons, range 0.5 to 2.0
- Live scaling -- UI resizes immediately as you tap +/-
- Safety revert: 10-second countdown if scale goes outside comfortable range
- Reset button (circular arrow) next to stepper, resets to 1.0
- Config key: existing `ui.scale` (already wired to UiMetrics.globalScale)
- Vertical navbar clock: keep stacked digit approach, include colon (1, 2, :, 3, 4), same bold weight, smaller font than horizontal

### Claude's Discretion
- Exact font size calculation for "fill available space" behavior
- Safety revert threshold (all changes? only extreme values?)
- Transition animation when scale changes (if any)
- Vertical clock font size relative to navbar width

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CLK-01 | Clock display is larger and more readable at automotive glance distance | Font sizing approach documented -- fill 80% of control height with bold weight |
| CLK-02 | AM/PM designation removed from clock display | Format string change from `"h:mm AP"` to `"h:mm"` in NavbarControl.qml |
| CLK-03 | User can toggle between 12h and 24h clock format in settings | New `display.clock_24h` config key + SettingsToggle in DisplaySettings.qml |
| DPI-05 | User can adjust UI scale via stepper control in Display settings | Stepper writes to `ui.scale`, UiMetrics reactivity fix required for live updates |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6.4/6.8 | 6.4 (dev) / 6.8 (Pi) | UI framework | Already in use |
| QML | (bundled) | Declarative UI | Already in use |

### Supporting
No new libraries needed. Everything uses existing QML components and ConfigService.

## Architecture Patterns

### Pattern 1: UiMetrics Reactivity Fix
**What:** `UiMetrics._cfgScale` is currently bound as `ConfigService.value("ui.scale")` -- a one-time Q_INVOKABLE call. It does NOT re-evaluate when the config changes.
**When to use:** Required for live scale updates.
**Solution:**
```qml
// In UiMetrics.qml, add a Connections block:
Connections {
    target: ConfigService
    function onConfigChanged(path, value) {
        if (path === "ui.scale") _cfgScale = value
        else if (path === "ui.fontScale") _cfgFontScale = value
    }
}
```
**CRITICAL:** The `_cfgScale` and `_cfgFontScale` properties must change from `readonly` to writable (`property var`) for this signal handler to update them. The initial value can still come from `ConfigService.value()` in `Component.onCompleted` or inline.

### Pattern 2: Clock Font Auto-Sizing
**What:** Clock text fills ~80% of the available control height.
**How:** In horizontal mode, the clock control is `parent.height` of the navbar (= `UiMetrics.navbarThick`, default ~56px scaled). Target font size = `Math.round(parent.height * 0.8)`. In vertical mode, use width-relative sizing (smaller, since characters stack).
**Example:**
```qml
// Horizontal clock
Text {
    font.pixelSize: Math.round(root.height * 0.75)
    font.weight: Font.DemiBold
    // ...
}
```
The 0.75 multiplier accounts for font metrics (ascenders/descenders take space beyond the em square), achieving visual ~80% fill.

### Pattern 3: Clock Format String
**What:** Qt `formatTime` format strings for 12h/24h.
**Example:**
```qml
var fmt = ConfigService.value("display.clock_24h") === true ? "HH:mm" : "h:mm"
clockHoriz.text = Qt.formatTime(new Date(), fmt)
```
- `"h:mm"` = 12h without AM/PM (e.g., "2:34")
- `"HH:mm"` = 24h with leading zero (e.g., "14:34")
- `"H:mm"` = 24h without leading zero (e.g., "2:34" at 2am)

Decision needed: use `"HH:mm"` (with leading zero) for 24h -- it's the standard automotive format.

### Pattern 4: Scale Stepper Custom Control
**What:** A new QML control with [-] value [+] layout.
**Structure:** Not an existing control -- must be built inline or as a new component. Similar to `SettingsSlider` but with discrete buttons.
```qml
RowLayout {
    // [-] button
    Rectangle { /* touchable button with "-" text */ }
    // Current value display
    Text { text: currentScale.toFixed(1) }
    // [+] button
    Rectangle { /* touchable button with "+" text */ }
    // Reset button
    Rectangle { /* circular arrow icon */ }
}
```

### Pattern 5: Safety Revert Dialog
**What:** Countdown timer that reverts scale to previous value if user doesn't confirm.
**Recommendation for threshold:** Trigger on ANY scale change (not just extreme values). The Windows monitor resolution pattern is well-understood -- always ask. Simpler logic, and protects against any accidentally unusable scale.
**Implementation:** Store `previousScale` before applying change. Show overlay with countdown. If timer expires, restore `previousScale`.

### Anti-Patterns to Avoid
- **Don't use Behavior animation on scale changes:** Animating every UiMetrics-derived property simultaneously would be expensive and visually chaotic. Scale changes should be instant (re-layout on next frame).
- **Don't create a separate "apply" step for scale:** The decision is "live + safety revert", not "preview + apply".

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Boolean toggle | Custom switch | `SettingsToggle.qml` | Already exists, binds to configPath |
| Section dividers | Custom layout | `SectionHeader.qml` | Already exists |
| Config persistence | Manual YAML write | `ConfigService.setValue() + save()` | Already exists with signal emission |

## Common Pitfalls

### Pitfall 1: UiMetrics Doesn't Update Live
**What goes wrong:** Stepper writes `ui.scale` via ConfigService, but UiMetrics never re-reads the value. UI stays at old scale.
**Why it happens:** `ConfigService.value()` is Q_INVOKABLE (imperative call), not a Q_PROPERTY (reactive binding). QML only calls it once at property initialization.
**How to avoid:** Add `Connections { target: ConfigService; function onConfigChanged(...) }` in UiMetrics.qml.
**Warning signs:** Scale stepper seems to "do nothing" until app restart.

### Pitfall 2: Config Schema Validation Rejects New Key
**What goes wrong:** Writing `display.clock_24h` via `ConfigService.setValue()` silently fails.
**Why it happens:** `YamlConfig::setValueByPath()` validates against the defaults tree. If `display.clock_24h` isn't in `initDefaults()`, the write is rejected.
**How to avoid:** Add `root_["display"]["clock_24h"] = false;` to `YamlConfig::initDefaults()`.
**Warning signs:** Toggle appears to work but value doesn't persist across restart.

### Pitfall 3: Clock Format Not Reactive to Config Change
**What goes wrong:** User toggles 24h format, but clock keeps showing old format until next timer tick (up to 1 second delay).
**Why it happens:** Timer fires every 1s. If format change happens right after a tick, there's up to 1s latency.
**How to avoid:** This is actually acceptable -- 1 second max delay is fine for a settings toggle. But if immediate update is desired, listen for `ConfigService.configChanged` on `display.clock_24h` and force a re-render.

### Pitfall 4: Scale 0 vs Scale 1.0 Semantics
**What goes wrong:** UiMetrics treats `ui.scale = 0` as "not set, use 1.0". If the stepper writes `0`, it means "auto" not "zero scale".
**Why it happens:** The config default is `0` (meaning "use auto-derived DPI scale only"). The stepper range is 0.5-2.0, so this shouldn't trigger, but be aware of the edge.
**How to avoid:** Stepper range starts at 0.5. Resetting sets to `0` (meaning "auto/default"), not `1.0`. Wait -- the CONTEXT.md says reset to 1.0. Need to decide: does reset mean "auto" (write 0) or "explicit 1.0" (write 1.0)? Since the stepper shows a numeric value and user expects "1.0 = normal size", **reset should write 1.0**, not 0.

### Pitfall 5: test_config_key_coverage Must Include New Key
**What goes wrong:** Tests fail because new `display.clock_24h` key isn't in the coverage test's key list.
**How to avoid:** Add `"display.clock_24h"` to the `testAllRuntimeKeys()` key list.

## Code Examples

### Existing Code: NavbarControl Clock (current, to be modified)
```qml
// File: qml/components/NavbarControl.qml, line 56-79
// Current: uses UiMetrics.fontBody for clock, includes AM/PM
Text {
    font.pixelSize: UiMetrics.fontBody  // ~20px -- too small
    // ...
    onTriggered: {
        var timeStr = Qt.formatTime(new Date(), "h:mm AP")  // includes AM/PM
    }
}
```

### Existing Code: UiMetrics Scale Chain
```qml
// File: qml/controls/UiMetrics.qml
// _cfgScale (line 6) -> globalScale (line 10) -> scale (line 43) -> all tokens
readonly property var _cfgScale: ConfigService.value("ui.scale")  // ONE-TIME CALL
readonly property real globalScale: { /* ... */ return Number(v); }
readonly property real scale: autoScale * globalScale  // everything derives from this
```

### Existing Code: ConfigService Signal
```cpp
// File: src/core/services/ConfigService.hpp, line 25
signals:
    void configChanged(const QString& path, const QVariant& value);
// Emitted by setValue() -- available in QML via Connections
```

### Existing Code: DisplaySettings Layout Order
```qml
// File: qml/applications/settings/DisplaySettings.qml
// Current order: ReadOnlyField("Screen") -> SectionHeader("General") -> ...
// Target order: ReadOnlyField("Screen") -> [NEW: Scale Stepper] -> SectionHeader("General") -> ... -> [NEW: SectionHeader("Clock")] -> [NEW: SettingsToggle 24h] -> SectionHeader("Navbar") -> ...
```

### Existing Code: Navbar Control Sizing
```qml
// File: qml/components/Navbar.qml
// Horizontal: clock control is width/2 x height (full navbar height)
// Vertical: clock control is width x height/2
NavbarControl { width: parent.width / 2; height: parent.height }  // horiz
NavbarControl { width: parent.width; height: parent.height / 2 }  // vert
```

## State of the Art

No external changes relevant. Everything is internal QML/Qt patterns that haven't changed between Qt 6.4 and 6.8.

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `UiMetrics.fontBody` for clock | Fill-to-fit font sizing | This phase | Clock becomes readable |
| One-time config read in UiMetrics | Reactive via `configChanged` signal | This phase | Live scale updates work |

## Open Questions

1. **Reset button: write 0 (auto) or 1.0 (explicit)?**
   - What we know: Config default is 0 (meaning "not set, use DPI auto"). CONTEXT.md says "resets to 1.0".
   - Recommendation: Write 1.0 per CONTEXT.md. User expectation is "1.0 = normal". If user wants pure DPI-auto, they can edit YAML.

2. **24h format leading zero?**
   - What we know: `"HH:mm"` = leading zero (02:34), `"H:mm"` = no leading zero (2:34).
   - Recommendation: Use `"HH:mm"` -- standard for 24h automotive clocks. For 12h, use `"h:mm"` (no leading zero) per convention.

3. **Vertical clock font sizing?**
   - What we know: Characters stack vertically. Width is `UiMetrics.navbarThick` (~56px scaled). Height is `parent.height / 2` (~300px on 600px display).
   - Recommendation: Use `Math.round(root.width * 0.65)` for each character. With 5 stacked characters (1, 2, :, 3, 4) plus spacing, this should fit in the available height. May need runtime tuning.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (QTest) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && ctest --output-on-failure -R test_config` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CLK-01 | Clock font is larger than fontBody | manual-only | Visual inspection on Pi | N/A |
| CLK-02 | No AM/PM in clock display | manual-only | Visual inspection on Pi | N/A |
| CLK-03 | `display.clock_24h` config key readable/writable | unit | `cd build && ctest --output-on-failure -R test_config_key_coverage` | Needs key added |
| DPI-05 | `ui.scale` config key writable + configChanged emitted | unit | `cd build && ctest --output-on-failure -R test_config_service` | Existing covers setValue |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure -R "test_config|test_yaml"`
- **Per wave merge:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] Add `"display.clock_24h"` to `test_config_key_coverage.cpp` `testAllRuntimeKeys()` key list
- [ ] Add `display.clock_24h` default to `YamlConfig::initDefaults()`

## Sources

### Primary (HIGH confidence)
- Direct code inspection of `qml/controls/UiMetrics.qml` -- confirmed one-time binding issue
- Direct code inspection of `src/core/services/ConfigService.cpp` -- confirmed `configChanged` signal exists
- Direct code inspection of `qml/components/NavbarControl.qml` -- confirmed current font sizing and format
- Direct code inspection of `src/core/YamlConfig.cpp` -- confirmed schema validation in `setValueByPath`
- Direct code inspection of `qml/applications/settings/DisplaySettings.qml` -- confirmed layout order
- Direct code inspection of `tests/test_config_key_coverage.cpp` -- confirmed key list maintenance

### Secondary (MEDIUM confidence)
- Qt documentation on `Qt.formatTime` format strings (well-established, stable API)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, all existing codebase patterns
- Architecture: HIGH -- all patterns verified by reading actual source code
- Pitfalls: HIGH -- reactivity gap and schema validation confirmed by code inspection

**Research date:** 2026-03-08
**Valid until:** 2026-04-08 (stable -- internal codebase patterns)
