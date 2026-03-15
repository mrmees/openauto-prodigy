# Phase 2: Clock & Scale Control - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Make the navbar clock readable at automotive glance distance, add 12h/24h format toggle, and provide a user-facing UI scale stepper. Requirements: CLK-01, CLK-02, CLK-03, DPI-05.

</domain>

<decisions>
## Implementation Decisions

### Clock Sizing
- Clock auto-sizes to fill the available navbar control area (~80% of height), leaving small padding
- Bold/semi-bold font weight for glanceability in daylight — thicker strokes than regular text
- Same bold weight for both horizontal and vertical navbar orientations

### Clock Format (12h)
- Bare time only — no AM/PM, no dot, no indicator. Just "12:34"
- Solid colon, no blinking
- Default format is 12h (US-centric default)

### 24h Toggle
- Lives in Display settings under a new "Clock" section header, between General and Navbar sections
- Config key: `display.clock_24h` (boolean, default false)
- Toggle updates navbar clock immediately (live, no restart required)
- Clock Timer already ticks every second — picks up new format on next tick

### Scale Stepper
- Located directly under the read-only Screen info row, before the General section (no separate section header)
- ±0.1 increments via [-] and [+] buttons
- Range: 0.5 to 2.0
- Live scaling — UI resizes immediately as you tap +/-
- Safety revert: 10-second countdown if scale goes outside comfortable range. Tap "Keep this size" to confirm, otherwise reverts
- Reset button (↺) next to stepper, resets to 1.0
- Config key: existing `ui.scale` (already wired to UiMetrics.globalScale)

### Vertical Navbar Clock
- Keep stacked digit approach (current pattern), use smaller font than horizontal
- Include colon between hours and minutes: stack is 1, 2, :, 3, 4 (5 characters)
- Same bold weight as horizontal clock

### Claude's Discretion
- Exact font size calculation for "fill available space" behavior
- Safety revert threshold (all changes? only extreme values?)
- Transition animation when scale changes (if any)
- Vertical clock font size relative to navbar width

</decisions>

<specifics>
## Specific Ideas

- The safety revert is inspired by Windows monitor resolution change confirmation — "Keep these settings? Reverting in 10 seconds..."
- Clock should feel like a car stereo display — bold, clean, no clutter
- Scale stepper should be immediately usable without explanation — [-] 1.0 [+] is self-evident

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `NavbarControl.qml`: Already has clock display with Timer (1s interval), horizontal and vertical modes, format string `Qt.formatTime(new Date(), "h:mm AP")`
- `UiMetrics.qml`: `globalScale` already reads `ConfigService.value("ui.scale")`, all tokens multiply by `scale` (which includes `globalScale`). Stepper just writes to config.
- `SettingsToggle`: Existing component for boolean config toggles — reuse for 24h toggle
- `DisplaySettings.qml`: Target file for both clock section and scale stepper additions

### Established Patterns
- Config changes via `ConfigService.setValue()` trigger reactive QML bindings
- Settings controls bind to `configPath` property for auto-read/write
- `SectionHeader` component for section dividers in settings pages

### Integration Points
- `NavbarControl.qml` line 69: Format string change from `"h:mm AP"` to conditional 24h format
- `UiMetrics.qml` line 6: `_cfgScale` already reads `ConfigService.value("ui.scale")` — stepper writes here
- `DisplaySettings.qml`: Add scale stepper after Screen info row, add Clock section before Navbar section
- `YamlConfig.cpp`: Add `display.clock_24h` default (false)

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-clock-scale-control*
*Context gathered: 2026-03-08*
