---
status: diagnosed
phase: 03-touch-normalization
source: [03-01-SUMMARY.md, 03-02-SUMMARY.md]
started: 2026-03-11T05:10:00Z
updated: 2026-03-11T06:25:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Alternating Row Backgrounds
expected: Open any settings sub-page (e.g., System or Information). Each row should alternate between two slightly different background colors (surfaceContainer / surfaceContainerHigh). After a section header, the alternation should reset (first row after header is always the lighter color).
result: pass

### 2. Row Press Feedback
expected: Tap and hold on any interactive row (e.g., Close App in System, or Delete Theme in Theme settings). The row should show visible press feedback (color change or highlight) while your finger is down.
result: pass

### 3. Touch Target Sizing
expected: All rows across all settings pages should have consistent, comfortable height for finger tapping. No tiny controls or cramped rows. Everything should feel easy to hit while the display is in the car.
result: pass

### 4. Text Readability
expected: All text on settings pages uses a readable body-size font. No tiny caption text or helper/subtitle text under controls. Labels should be readable at arm's length on the 1024x600 display.
result: issue
reported: "Text is generally too small and difficult to read."
severity: major

### 5. Close App Row (System Settings)
expected: In System settings, "Close App" should appear as a full-width tappable row with a power icon on the left and a chevron on the right — not a centered button. Tapping it should open the exit confirmation dialog.
result: pass

### 6. Flat Pickers in Rows
expected: In pages like Audio or Display, dropdown-style pickers (FullScreenPicker) should sit cleanly inside their row without their own card/shadow background. They should show the current value inline with a chevron indicator.
result: pass

### 7. AA Protocol Test Accordion (Debug Settings)
expected: In Debug settings, "AA Protocol Test" should appear as a tappable row with a connection status dot (green if connected, gray if not). Tapping the row should expand/collapse a section of test buttons below it — not navigate to a sub-page.
result: pass

### 8. Codec Sub-Row Indentation (Debug Settings)
expected: In Debug settings under Video Codecs, each codec's sub-controls (hw/sw toggle, decoder name) should be visually indented under the parent codec toggle. Sub-controls should be hidden when their parent codec is disabled.
result: pass

### 9. Bluetooth Forget Button (Connection Settings)
expected: In Connection/Bluetooth settings, the Forget button should be a simple outlined rectangle — no drop shadow, no press scale animation. It should look clean and minimal.
result: pass

## Summary

total: 9
passed: 8
issues: 1
pending: 0
skipped: 0

## Gaps

- truth: "All text on settings pages uses a readable body-size font. No tiny caption text or helper/subtitle text under controls. Labels should be readable at arm's length on the 1024x600 display."
  status: failed
  reason: "User reported: Text is generally too small and difficult to read."
  severity: major
  test: 4
  root_cause: "UiMetrics base font sizes calibrated for phone-density viewing (~30cm). On 7-inch 1024x600 at 169.5 DPI, scaling factor is only 1.06x — fontBody renders at 21px (2.2mm cap height), well below ~3mm automotive minimum."
  artifacts:
    - path: "qml/controls/UiMetrics.qml"
      issue: "Base font sizes too small for automotive use (fontBody=20, fontSmall=16, fontTiny=14)"
  missing:
    - "Increase base font sizes ~1.4x (fontBody 20→28, fontSmall 16→22, fontTiny 14→20) to hit 3mm+ cap height"
  debug_session: ".planning/debug/settings-text-too-small.md"
