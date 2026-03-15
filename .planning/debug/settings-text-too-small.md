---
status: diagnosed
trigger: "Settings text too small and difficult to read on 1024x600 display"
created: 2026-03-11T00:00:00Z
updated: 2026-03-11T00:00:00Z
---

## Current Focus

hypothesis: Base font sizes in UiMetrics are designed for phone-density viewing distances, not automotive arm-length use
test: Calculate physical text sizes and compare to automotive readability guidelines
expecting: Font sizes below ~3mm cap height at arm's length
next_action: Report findings

## Symptoms

expected: Text across settings pages should be comfortably readable at arm's length in a car
actual: Text is too small and difficult to read
errors: None (visual/UX issue)
reproduction: Open any settings page on the 1024x600 DFRobot display
started: Has likely always been this way (base sizes were set for general-purpose use)

## Eliminated

(none)

## Evidence

- timestamp: 2026-03-11
  checked: UiMetrics.qml font token definitions and scaling math
  found: |
    DPI-based scaling: 1024x600 @ 7.0" = 169.5 DPI, scale factor = 1.060
    Base sizes and actual pixel outputs:
      fontTiny:    14 * 1.06 = 15px (2.2mm / ~1.6mm cap)
      fontSmall:   16 * 1.06 = 17px (2.5mm / ~1.8mm cap)
      fontBody:    20 * 1.06 = 21px (3.1mm / ~2.2mm cap)
      fontTitle:   22 * 1.06 = 23px (3.4mm / ~2.4mm cap)
      fontHeading: 28 * 1.06 = 30px (4.5mm / ~3.1mm cap)
  implication: Scale factor is nearly 1.0 -- the 7" 1024x600 display is close to the 160dpi reference, so scaling provides almost no size boost. The base sizes themselves are the problem.

- timestamp: 2026-03-11
  checked: Which font tokens settings controls use
  found: |
    fontSmall (17px) used by:
      - NormalText.qml (line 5)
      - SpecialText.qml (line 5)
      - SectionHeader.qml (line 18) -- section divider labels
      - SettingsSlider.qml (line 79) -- slider numeric value display
      - DebugSettings.qml (lines 104, 137) -- debug detail text
      - EqSettings.qml (line 78) -- EQ labels
    fontBody (21px) used by:
      - SettingsSlider.qml (line 56) -- slider label
      - SettingsToggle.qml (line 31) -- toggle label
      - SettingsListItem.qml (line 78) -- list item label
      - ReadOnlyField.qml (lines 31, 38) -- label and value
      - FullScreenPicker.qml (lines 123, 137, 299) -- picker label and value
      - DisplaySettings.qml (line 61) -- "UI Scale" label
      - Most ConnectionSettings, CompanionSettings, DebugSettings labels
    fontTitle (23px) used by:
      - FullScreenPicker dialog header (line 196)
      - DisplaySettings scale value (line 117)
  implication: The most common settings text is fontBody (21px / 2.2mm cap) and fontSmall (17px / 1.8mm cap). Both are below automotive minimum readability.

- timestamp: 2026-03-11
  checked: Hardcoded font sizes in settings QML
  found: |
    SettingsMenu.qml line 151: font.pixelSize: Math.round(parent.height * 0.45)
      This is the main settings category list -- calculates to ~53px, which is actually fine.
    AndroidAutoMenu.qml line 59: font.pixelSize: 10 (hardcoded, not using UiMetrics)
    No other hardcoded sizes in settings pages -- all use UiMetrics tokens properly.
  implication: Settings pages correctly use UiMetrics tokens. The problem is the token values themselves.

- timestamp: 2026-03-11
  checked: Automotive readability guidelines vs actual sizes
  found: |
    At arm's length (~60-80cm), minimum readable cap height is ~3mm.
    Comfortable reading requires ~4-5mm cap height.
    Current settings text:
      fontSmall: 1.8mm cap -- well below minimum (used for section headers, secondary text)
      fontBody:  2.2mm cap -- below minimum (used for ALL primary settings labels)
      fontTitle: 2.4mm cap -- below minimum (used for dialog headers)
      fontHeading: 3.1mm cap -- at minimum (barely used in settings)
  implication: Every text element commonly used in settings is below automotive readability minimums.

## Resolution

root_cause: |
  The UiMetrics base font sizes (fontTiny=14, fontSmall=16, fontBody=20, fontTitle=22, fontHeading=28)
  are calibrated for phone-density viewing distances (~30cm). On a 7-inch 1024x600 car display viewed
  at arm's length (60-80cm), the DPI-based scaling factor is only 1.06x (since 169.5 DPI is close to
  the 160 DPI reference), resulting in nearly no size increase. The physical cap heights of all commonly
  used settings text (fontSmall=1.8mm, fontBody=2.2mm) are below the ~3mm minimum for automotive
  readability at arm's length.

  The scaling SYSTEM works correctly -- the problem is the base values fed into it.

fix: (not applied -- diagnosis only)
verification: (not applied)
files_changed: []
