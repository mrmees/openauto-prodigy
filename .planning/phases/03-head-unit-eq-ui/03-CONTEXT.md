# Phase 3: Head Unit EQ UI - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Touch-friendly QML interface for the 10-band equalizer on the head unit touchscreen (1024x600). Users can adjust band gains via vertical sliders, pick presets, switch between stream profiles (media/nav/phone), toggle bypass, and save/delete user presets. All DSP and service logic exists from Phases 1-2 — this phase is purely the visual control layer on top of EqualizerService.

</domain>

<decisions>
## Implementation Decisions

### Layout & Navigation
- EQ lives as a settings sub-page, not a standalone plugin — no NavStrip icon
- Entry point: a tap-through row in AudioSettings ("Equalizer >" showing current preset name)
- Navigates to a dedicated EQ page within the Settings navigation flow (back button returns to AudioSettings)
- All 10 vertical sliders in a single full-width horizontal row
- Stream selector (SegmentedButton) and preset picker above the slider area

### Slider Interaction
- Custom vertical slider component (SettingsSlider is horizontal — needs new EqBandSlider control)
- Gain range: +/-12 dB with 0.5 dB step size (49 positions per slider)
- Floating dB value label above the thumb while dragging, disappears on release
- Subtle 0 dB reference line drawn horizontally across all 10 slider tracks
- Double-tap a slider to reset that individual band to 0 dB
- Band frequency labels below each slider (31, 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16k)

### Preset & Stream Controls
- Stream selector: SegmentedButton with 3 segments (Media, Nav, Phone) — reuse existing control
- Preset picker: FullScreenPicker (bottom sheet) — bundled presets listed first, then user presets below a divider with section header
- Checkmark on active preset in the picker
- Save button (icon) on EQ page — tap opens a name dialog with text input, saves current slider positions as user preset
- Delete user presets via swipe-to-delete gesture in the preset picker (bundled presets not swipeable)
- When sliders are manually adjusted away from any preset, preset label shows "Custom"

### Bypass & Reset
- Per-stream bypass (matches IEqualizerService API) — each stream has independent bypass state
- Bypass indicator: BYPASS badge/button in the header bar area, sliders dim to ~40% opacity when bypassed
- Sliders remain draggable when bypassed (can configure curve before enabling)
- No separate "Reset to Flat" button — user picks the Flat preset from the picker instead (keeps UI cleaner)

### Claude's Discretion
- Exact spacing and margins within the EQ page (use UiMetrics)
- Slider thumb shape and size (must meet UiMetrics.touchMin)
- Animation timing for slider snap-to-preset and bypass dimming
- Exact color treatment for bypass badge and dimmed state
- Save dialog styling and keyboard behavior
- Swipe-to-delete animation and threshold distance

</decisions>

<specifics>
## Specific Ideas

- Stream selector + preset picker + bypass controls in a top control bar; sliders take remaining vertical space below
- The EQ page should feel like a mixing console — functional, not decorative
- Preset picker follows the same bottom-sheet pattern as the codec/device pickers elsewhere in settings

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- **SegmentedButton.qml**: 3-option selector for Media/Nav/Phone stream switching — ready to use
- **FullScreenPicker.qml**: Bottom-sheet picker with model-driven or options-driven modes — use for preset selection
- **SettingsToggle.qml**: On/off switch — could be used for bypass if preferred over badge
- **UiMetrics.qml**: Responsive sizing singleton — all dimensions must use this (touchMin, rowH, fontBody, etc.)
- **SectionHeader.qml**: Section divider text — use for "User Presets" divider in picker
- **MaterialIcon.qml**: Font-based icons — use for save, bypass, back buttons
- **IEqualizerService**: Full API for all EQ operations (gain get/set, preset apply/save/delete, bypass, stream-specific)

### Established Patterns
- Settings sub-pages use Flickable with ColumnLayout, anchored margins via UiMetrics
- Config persistence via ConfigService.setValue() + ConfigService.save() with debounce timers
- All font sizes, spacing, colors via ThemeService and UiMetrics — no hardcoded pixels
- QAbstractListModel pattern for pickers (AudioDeviceModel, CodecCapabilityModel)
- ApplicationController.navigateBack() for settings navigation stack

### Integration Points
- **AudioSettings.qml**: Add "Equalizer" row that navigates to EQ page
- **EqualizerService (C++)**: Expose to QML context — needs Q_INVOKABLE methods and Q_PROPERTY bindings
- **ApplicationController**: Register EQ page in settings navigation stack
- **main.cpp**: EqualizerService already created — expose to QML engine root context
- **CMakeLists.txt**: Add new QML files and any new C++ model classes

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-head-unit-eq-ui*
*Context gathered: 2026-03-01*
