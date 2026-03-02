# Phase 3: Head Unit EQ UI - Research

**Researched:** 2026-03-01
**Domain:** Qt 6 QML UI, touch-friendly controls, C++/QML interop
**Confidence:** HIGH

## Summary

This phase is a pure QML UI build on top of a fully-implemented C++ service layer (EqualizerService). The core challenge is building a custom vertical slider control that works well with finger input at 1024x600, wiring it to the existing `EqualizerService` Q_INVOKABLE API, and integrating into the existing Settings StackView navigation.

The main technical risk is the `StreamId` enum — it's a bare `enum class` not registered with Qt's meta-object system, so QML cannot access its values directly. This requires either adding `Q_NAMESPACE`/`Q_ENUM_NS` registration or using integer constants on the QML side. A secondary concern is that `std::array<float, 10>` returned by `gainsForStream()` may not convert cleanly to a QML-accessible JavaScript array — this needs a `QVariantList` wrapper or per-band `gain(stream, band)` calls.

**Primary recommendation:** Use integer constants (0/1/2) for stream IDs in QML, add a `Q_INVOKABLE QVariantList gainsAsList(int stream)` helper to EqualizerService, and build a custom `EqBandSlider.qml` control with a vertically-oriented touch area. All other controls (SegmentedButton, FullScreenPicker, SectionHeader) are reusable as-is.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- EQ lives as a settings sub-page, not a standalone plugin — no NavStrip icon
- Entry point: a tap-through row in AudioSettings ("Equalizer >" showing current preset name)
- Navigates to a dedicated EQ page within the Settings navigation flow (back button returns to AudioSettings)
- All 10 vertical sliders in a single full-width horizontal row
- Stream selector (SegmentedButton) and preset picker above the slider area
- Custom vertical slider component (SettingsSlider is horizontal — needs new EqBandSlider control)
- Gain range: +/-12 dB with 0.5 dB step size (49 positions per slider)
- Floating dB value label above the thumb while dragging, disappears on release
- Subtle 0 dB reference line drawn horizontally across all 10 slider tracks
- Double-tap a slider to reset that individual band to 0 dB
- Band frequency labels below each slider (31, 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16k)
- Stream selector: SegmentedButton with 3 segments (Media, Nav, Phone) — reuse existing control
- Preset picker: FullScreenPicker (bottom sheet) — bundled presets listed first, then user presets below a divider with section header
- Checkmark on active preset in the picker
- Save button (icon) on EQ page — tap opens a name dialog with text input, saves current slider positions as user preset
- Delete user presets via swipe-to-delete gesture in the preset picker (bundled presets not swipeable)
- When sliders are manually adjusted away from any preset, preset label shows "Custom"
- Per-stream bypass (matches IEqualizerService API) — each stream has independent bypass state
- Bypass indicator: BYPASS badge/button in the header bar area, sliders dim to ~40% opacity when bypassed
- Sliders remain draggable when bypassed (can configure curve before enabling)
- No separate "Reset to Flat" button — user picks the Flat preset from the picker instead

### Claude's Discretion
- Exact spacing and margins within the EQ page (use UiMetrics)
- Slider thumb shape and size (must meet UiMetrics.touchMin)
- Animation timing for slider snap-to-preset and bypass dimming
- Exact color treatment for bypass badge and dimmed state
- Save dialog styling and keyboard behavior
- Swipe-to-delete animation and threshold distance

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| UI-01 | User can adjust 10 EQ band gains via vertical sliders on the head unit touchscreen | Custom EqBandSlider control, EqualizerService.setGain() per band, layout analysis shows ~98px per slider is viable at 1024x600 |
| UI-02 | User can select presets from a picker in the EQ settings view | Reuse FullScreenPicker with combined bundled+user preset model, swipe-to-delete for user presets |
| UI-03 | User can switch between media, navigation, and phone EQ profiles via stream selector | Reuse SegmentedButton, integer stream IDs (0/1/2) since StreamId enum is not QML-registered |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt Quick Controls 2 | 6.4+ | Slider base, Dialog, Overlay | Already in project, provides native touch support |
| QML Animations | 6.4+ | Slider snap, bypass dimming, swipe-to-delete | Built-in, GPU-accelerated on Pi |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| EqualizerService (C++) | existing | All EQ operations | Every slider move, preset change, bypass toggle |
| ThemeService (C++) | existing | Colors for all UI elements | Every visual element |
| UiMetrics (QML singleton) | existing | Responsive sizing | Every dimension and spacing |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom vertical Slider | Qt Slider with `orientation: Qt.Vertical` | Qt's built-in vertical Slider exists but has limited styling; custom control gives full control over thumb, track, and touch area — worth it for car UI |
| Integer stream IDs in QML | Q_NAMESPACE + Q_ENUM_NS registration | Proper enum registration is cleaner but requires C++ changes; integers are simpler for a 3-value enum and match existing patterns in the codebase |
| Per-band gain() calls | gainsAsList() helper | Helper is one call vs 10; use helper for initial load / preset snap, per-band for slider moves |

## Architecture Patterns

### Recommended File Structure
```
qml/controls/EqBandSlider.qml           # New custom vertical slider control
qml/applications/settings/EqSettings.qml # New EQ settings page
```

Plus modifications to:
```
qml/applications/settings/AudioSettings.qml  # Add "Equalizer >" row
qml/applications/settings/SettingsMenu.qml   # Register EQ page in navigation
src/core/services/EqualizerService.hpp/cpp    # Add QML-friendly helpers
src/CMakeLists.txt                            # Register new QML files
```

### Pattern 1: Settings Sub-Page Navigation (StackView)
**What:** Settings uses a StackView. Sub-pages are pushed as Components. Back button pops.
**When to use:** EQ page navigated from AudioSettings, not from the top-level settings list.
**How it works in this codebase:**
```
SettingsMenu.qml
  └─ StackView (settingsStack)
       ├─ settingsList (initial)
       └─ audioPage → AudioSettings.qml
            └─ (needs to push) eqPage → EqSettings.qml
```
**Critical detail:** AudioSettings is loaded BY SettingsMenu's StackView. For EQ to be a sub-sub-page, AudioSettings needs access to that same StackView. Two approaches:
1. Pass `settingsStack` as a property to AudioSettings (cleaner)
2. Use `StackView.view` attached property from within AudioSettings to access the parent StackView

The existing pattern uses approach 1 implicitly — when SettingsMenu pushes AudioSettings, it becomes a child of the StackView. AudioSettings can navigate via `StackView.view.push(eqComponent)`.

### Pattern 2: EqualizerService QML Binding
**What:** EqualizerService is already a QML context property. All Q_INVOKABLE methods are callable from QML.
**Key detail:** `StreamId` is `enum class StreamId` in namespace `oap` — NOT inside a QObject, NOT registered with Q_ENUM or Q_NAMESPACE/Q_ENUM_NS. QML cannot reference `StreamId.Media`. Must use integer values: 0=Media, 1=Navigation, 2=Phone.

**gainsForStream() return type issue:** `std::array<float, 10>` is NOT automatically converted to a JS array by Qt. Q_INVOKABLE methods returning `std::array` will likely produce an opaque object in QML. Need a QVariantList wrapper:
```cpp
Q_INVOKABLE QVariantList gainsAsList(int streamIndex) const;
```

**Alternative:** Call `gain(stream, band)` 10 times individually. Works but is verbose for preset snap animation where all 10 sliders update at once.

### Pattern 3: Custom Vertical Slider (EqBandSlider)
**What:** A finger-friendly vertical slider for a single EQ band.
**Layout math at 1024x600 (scale=1.0):**
- Page margins: 2 * 20px = 40px
- Available width: 984px for 10 sliders
- Per-slider column: ~98px (well above touchMin of 56px)
- Top control bar (stream selector + preset + bypass): ~80px (1 rowH)
- Settings title bar: ~56px (headerH)
- Frequency labels below sliders: ~24px
- Available slider height: 600 - 56 - 80 - 24 - margins ≈ 400px
- 49 steps in 400px = ~8px per step — coarse enough for finger dragging

**Implementation approach:**
- Rectangle track with a draggable thumb (Rectangle or custom shape)
- MouseArea covering the full slider column width (not just the track)
- Map Y position to dB value: top = +12dB, bottom = -12dB
- Snap to 0.5 dB steps
- Floating label: show on press, hide on release (opacity animation)
- Double-tap: Timer-based detection or TapHandler with tapCount

### Pattern 4: Preset Picker with Sections
**What:** FullScreenPicker bottom sheet with bundled and user presets separated by a divider.
**Existing FullScreenPicker limitations:** Currently supports a flat list. Need to extend for:
- Section dividers (SectionHeader between bundled and user presets)
- Swipe-to-delete on user preset items only
- Checkmark on active preset

**Options:**
1. Extend FullScreenPicker to support sections — risk: makes the generic control more complex
2. Build an EQ-specific preset picker dialog — keeps FullScreenPicker simple, duplicates some code
3. Use FullScreenPicker's model-driven mode with a custom model that includes section headers as special rows

**Recommendation:** Option 3 — create a `PresetListModel` (QAbstractListModel in C++) or just assemble the list in QML JS from `bundledPresetNames()` + `userPresetNames()` with a separator. Given the small list size (8 bundled + N user), a JS approach is fine and avoids a new C++ class.

Actually, looking more carefully at FullScreenPicker, it uses a flat `ListView` delegate. Swipe-to-delete requires adding horizontal drag handling to individual delegates. This is custom enough that a dedicated EQ preset picker (a Dialog with custom delegates) is cleaner than shoehorning it into FullScreenPicker.

### Anti-Patterns to Avoid
- **Hardcoded pixel sizes:** Every dimension must use UiMetrics. The CLAUDE.md explicitly says "DO NOT hardcode pixel sizes in QML."
- **Accessing StreamId enum from QML:** Will silently fail or produce undefined. Use integers 0/1/2.
- **Calling gainsForStream() directly in QML:** std::array<float,10> won't auto-convert. Use a QVariantList wrapper or individual gain() calls.
- **Debouncing slider moves in QML:** EqualizerService already has a 2-second debounce timer for config saves. Don't add another debounce layer. Call setGain() on every slider move for immediate audio feedback; persistence is already handled.
- **Using SettingsSlider for vertical bands:** It's horizontal with a label layout. Build a purpose-built EqBandSlider instead.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Stream switching | Custom toggle/radio | SegmentedButton.qml | Already exists, proven, themed |
| Bottom sheet dialog | Custom popup positioning | Dialog with `parent: Overlay.overlay` | Pattern used by FullScreenPicker, handles modal dimming |
| Responsive sizing | Manual breakpoints | UiMetrics singleton | Scale factor already computed, all dimensions derived |
| Config persistence | Manual YAML writes | EqualizerService handles it | 2-second debounce + shutdown flush already implemented |
| Preset list data | QAbstractListModel | QML JS array from bundledPresetNames() + userPresetNames() | List is small (<20 items), model overhead not justified |

**Key insight:** Phase 2 built the entire service layer. This phase should contain zero business logic — it's a pure view layer that calls Q_INVOKABLE methods.

## Common Pitfalls

### Pitfall 1: StreamId Enum Not Accessible in QML
**What goes wrong:** QML code like `EqualizerService.setGain(StreamId.Media, 0, 3.0)` silently passes `undefined` for the stream parameter.
**Why it happens:** `StreamId` is a bare `enum class` in namespace `oap`, not inside a QObject, and has no `Q_ENUM_NS` registration.
**How to avoid:** Use integer literals in QML: `EqualizerService.setGain(0, 0, 3.0)` for Media. Define JS constants at the top of EqSettings.qml: `readonly property int streamMedia: 0`, etc.
**Warning signs:** EQ changes not affecting the expected stream, or setGain silently doing nothing.

### Pitfall 2: std::array Return Type in QML
**What goes wrong:** `EqualizerService.gainsForStream(0)` returns an opaque object, not a JS array. Indexing it returns undefined.
**Why it happens:** Qt's meta-type system doesn't auto-convert `std::array<float, N>` to `QVariantList`.
**How to avoid:** Add a `Q_INVOKABLE QVariantList gainsAsList(int stream)` method, or use individual `gain(stream, band)` calls.
**Warning signs:** Sliders all showing 0 on page load or preset switch.

### Pitfall 3: StackView Sub-Sub-Page Navigation
**What goes wrong:** Pressing back from EQ page goes all the way back to settings list instead of AudioSettings.
**Why it happens:** If EQ is pushed onto a separate StackView or the back handler doesn't account for depth > 2.
**How to avoid:** Push EQ page onto the SAME settingsStack that AudioSettings lives in. The existing `onBackRequested` handler in SettingsMenu already does `settingsStack.pop()` when depth > 1 — this naturally handles any depth. Set title to "Settings > Audio > Equalizer".
**Warning signs:** Back button behavior inconsistency.

### Pitfall 4: Slider Touch Area Too Small
**What goes wrong:** Users can't reliably grab individual sliders — touches register on adjacent bands.
**Why it happens:** 10 sliders in 984px = ~98px each, but if the touchable area is just the thumb (e.g., 20px wide), precision is needed.
**How to avoid:** Make the entire column width the touch area, not just the thumb. The MouseArea should fill the full ~98px column. Map the Y coordinate to dB value regardless of X position within the column.
**Warning signs:** Users complaining about accidentally hitting wrong bands.

### Pitfall 5: Double-Tap Detection vs Drag
**What goes wrong:** Double-tap to reset interferes with drag-to-adjust. A quick drag is misinterpreted as a double-tap.
**Why it happens:** MouseArea processes both tap and drag events.
**How to avoid:** Use separate input handlers — TapHandler for double-tap (requires `tapCount: 2`) and DragHandler or MouseArea `onPositionChanged` for dragging. Or track movement distance: if the finger moves more than a threshold during press, it's a drag, not a tap.
**Warning signs:** Sliders resetting to 0 during fast adjustments.

### Pitfall 6: Bypass Signal Not Emitted
**What goes wrong:** Toggling bypass doesn't update the QML UI (sliders don't dim).
**Why it happens:** `setBypassed()` currently just calls `engine.setBypassed()` — there's no `bypassChanged` signal on EqualizerService. The bypass state is only on the engine, not exposed as a Q_PROPERTY.
**How to avoid:** Add `bypassedChanged(int stream)` signal and emit it from `setBypassed()`. Or add per-stream bypass Q_PROPERTYs analogous to the existing preset Q_PROPERTYs.
**Warning signs:** Bypass badge toggling but sliders not dimming, or vice versa.

### Pitfall 7: gainsChanged Signal Fires Per-Band During Preset Apply
**What goes wrong:** Applying a preset fires `gainsChanged` once (good), but if the UI handler refreshes all 10 sliders on each signal, and setGain is called per-band, you get 10 refreshes.
**Why it happens:** `applyPreset()` calls `engine.setAllGains()` and emits `gainsChanged` once. But if the QML connects to gainsChanged and reloads all sliders, and the initial load uses individual gain() calls, the pattern is fine. Just don't call setGain() 10 times from QML to apply a preset — use applyPreset().
**How to avoid:** Always use `applyPreset()` for preset selection, never loop `setGain()`.

## Code Examples

### EqBandSlider.qml — Vertical Slider Control
```qml
// Conceptual structure — not a copy-paste implementation
Item {
    id: root
    property int bandIndex: 0
    property real gainValue: 0.0  // -12.0 to +12.0
    property string freqLabel: ""
    property bool bypassed: false

    signal gainChanged(real dB)
    signal resetRequested()

    // Full column is the touch area
    MouseArea {
        anchors.fill: parent
        property bool isDragging: false

        onPressed: { isDragging = false; /* show floating label */ }
        onPositionChanged: {
            isDragging = true
            // Map mouse.y to dB: top = +12, bottom = -12
            var normalized = 1.0 - (mouse.y - trackTop) / trackHeight
            var dB = -12.0 + normalized * 24.0
            dB = Math.round(dB / 0.5) * 0.5  // snap to 0.5 dB
            dB = Math.max(-12.0, Math.min(12.0, dB))
            root.gainValue = dB
            root.gainChanged(dB)
        }
        onReleased: { /* hide floating label */ }
        onDoubleClicked: root.resetRequested()
    }

    // Track, thumb, 0dB line, frequency label, floating label
    // All dimensions via UiMetrics
}
```

### Calling EqualizerService from QML
```qml
// Stream constants (since StreamId enum isn't QML-registered)
readonly property int streamMedia: 0
readonly property int streamNavigation: 1
readonly property int streamPhone: 2

property int currentStream: streamMedia

// Set a single band gain (immediate audio effect)
EqualizerService.setGain(currentStream, bandIndex, newGainDb)

// Apply a preset (updates all bands at once)
EqualizerService.applyPreset(currentStream, "Rock")

// Read current gains for a stream (need QVariantList helper)
var gains = EqualizerService.gainsAsList(currentStream)
for (var i = 0; i < 10; i++) {
    sliderRepeater.itemAt(i).gainValue = gains[i]
}

// Toggle bypass
EqualizerService.setBypassed(currentStream, !EqualizerService.isBypassed(currentStream))
```

### StackView Sub-Page Push from AudioSettings
```qml
// In AudioSettings.qml — add tap-through row
SettingsListItem {
    icon: "\ue050"   // equalizer icon
    label: "Equalizer"
    // Show current preset name as subtitle/description
    onClicked: {
        // Access parent StackView via attached property
        StackView.view.push(eqSettingsComponent)
        ApplicationController.setTitle("Settings > Audio > Equalizer")
    }
}

Component {
    id: eqSettingsComponent
    EqSettings {}
}
```

### Swipe-to-Delete Delegate Pattern
```qml
// Inside preset picker delegate
Item {
    id: delegateRoot
    clip: true

    Rectangle {
        id: contentRow
        x: 0
        // Horizontal drag for user presets only
        Behavior on x { NumberAnimation { duration: 150 } }
    }

    // Red delete background revealed by swipe
    Rectangle {
        visible: isUserPreset
        anchors.right: parent.right
        color: "red"
        // MaterialIcon trash can
    }

    MouseArea {
        drag.target: isUserPreset ? contentRow : undefined
        drag.axis: Drag.XAxis
        drag.minimumX: -deleteThreshold
        drag.maximumX: 0
        onReleased: {
            if (contentRow.x < -deleteThreshold / 2) {
                EqualizerService.deleteUserPreset(presetName)
            } else {
                contentRow.x = 0  // snap back
            }
        }
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Qt Quick Controls 1 Slider | Qt Quick Controls 2 Slider | Qt 5.7+ | Better touch, themeable, no widget dependency |
| C++ QAbstractListModel for small lists | QML JS arrays with Q_INVOKABLE | Always valid for <50 items | Less boilerplate, no C++ model class needed |
| loadFromModule() | qt_add_qml_module + QML_FILES | Qt 6.2+ | This project uses qt_add_qml_module (required for Qt 6.4 compat) |

**Deprecated/outdated:**
- `loadFromModule()` not available in Qt 6.4 (project must support 6.4)

## Open Questions

1. **StackView.view attached property in Qt 6.4**
   - What we know: This is standard Qt Quick Controls 2 API. Should work in both 6.4 and 6.8.
   - What's unclear: Whether AudioSettings (loaded as a Component pushed by SettingsMenu) can reliably access `StackView.view` to push further sub-pages.
   - Recommendation: If `StackView.view` doesn't work, pass `settingsStack` as a property when pushing AudioSettings. LOW risk — test during implementation.

2. **Virtual keyboard for save preset name dialog**
   - What we know: The Pi runs labwc (Wayland compositor). Qt's virtual keyboard module (QtVirtualKeyboard) may or may not be installed.
   - What's unclear: Whether a TextInput in a Dialog will trigger a virtual keyboard on the Pi. If not, the user has no way to type a preset name.
   - Recommendation: Check if QtVirtualKeyboard is available on Pi. If not, consider a simpler naming approach (auto-generated names with optional rename) or include VK in install script deps. This can be tested during implementation.

3. **gainsChanged signal parameter type in QML**
   - What we know: `gainsChanged(StreamId stream)` signal uses the unregistered `StreamId` enum.
   - What's unclear: Whether QML can receive and compare this parameter. It may arrive as an integer (which would work) or be undefined.
   - Recommendation: Test, and if needed, change signal to `gainsChanged(int stream)` for QML compatibility. LOW risk fix.

## C++ Changes Required

Small but critical additions to `EqualizerService`:

1. **`gainsAsList(int stream)` Q_INVOKABLE** — returns `QVariantList` of 10 floats for a stream
2. **`bypassedChanged(int stream)` signal** — emitted from `setBypassed()` so QML can react
3. **Integer overloads or casts** — Q_INVOKABLE methods taking `StreamId` may work via implicit int cast from QML, but should be verified. If not, add `int`-parameter overloads.
4. **Consider `bypassed` Q_PROPERTY per stream** — analogous to existing `mediaPreset`/`navigationPreset`/`phonePreset` Q_PROPERTYs

## Sources

### Primary (HIGH confidence)
- **Codebase inspection** — EqualizerService.hpp/cpp, IEqualizerService.hpp, SettingsMenu.qml, AudioSettings.qml, FullScreenPicker.qml, SegmentedButton.qml, UiMetrics.qml, SettingsSlider.qml, SettingsListItem.qml, main.cpp
- **Qt 6 documentation** — Q_INVOKABLE, StackView, MouseArea, TapHandler patterns from training knowledge (HIGH confidence for stable Qt APIs)

### Secondary (MEDIUM confidence)
- **std::array QML interop** — Based on Qt meta-type system behavior; `std::array` is not a Q_DECLARE_METATYPE-registered type by default, won't auto-convert to JS array. Verified by pattern: project uses `QStringList` (which IS auto-converted) for other list returns.
- **StreamId enum QML accessibility** — Confirmed by codebase: no Q_NAMESPACE/Q_ENUM_NS anywhere for StreamId. Other enums in the project (CallState, ConnectionState) ARE registered via Q_ENUM inside QObject classes, confirming the pattern is deliberate where it exists and missing where it doesn't.

### Tertiary (LOW confidence)
- **Virtual keyboard on Pi** — Needs runtime verification. QtVirtualKeyboard availability depends on RPi OS Trixie packages.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components are existing project patterns, no new libraries needed
- Architecture: HIGH — StackView navigation, QML controls, C++/QML interop are well-understood from codebase inspection
- Pitfalls: HIGH — StreamId/std::array issues identified from direct code analysis, not speculation

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable domain — Qt 6 QML patterns don't change fast)
