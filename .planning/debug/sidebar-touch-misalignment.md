---
status: diagnosed
trigger: "sidebar touch zones misaligned - volume bar bottom triggers home button"
created: 2026-03-03T00:00:00Z
updated: 2026-03-03T00:00:00Z
---

## Current Focus

hypothesis: Evdev hit zones use hardcoded 70%/75%/25% Y splits that don't match QML sidebar's actual ColumnLayout proportions
test: Compare evdev zone boundaries vs QML layout math
expecting: Zone boundaries diverge, especially at volume/home boundary
next_action: Report findings

## Symptoms

expected: Touching bottom of volume bar adjusts volume
actual: Touching near bottom of volume bar triggers home button instead
errors: none
reproduction: During AA session with sidebar enabled, touch bottom portion of volume slider
started: Unknown - may be pre-existing or related to phase 5 display config changes

## Evidence

- timestamp: 2026-03-03
  checked: EvdevTouchReader::setSidebar() vertical sidebar zone math
  found: |
    Hardcoded splits for vertical sidebar (left/right):
      sidebarVolY0_ = 0
      sidebarVolY1_ = displayHeight_ * 0.70 * evdevPerPixelY
      sidebarHomeY0_ = displayHeight_ * 0.75 * evdevPerPixelY
      sidebarHomeY1_ = screenHeight_
    This means: volume = top 70%, gap = 5%, home = bottom 25%
  implication: These are arbitrary percentages, not derived from the actual QML layout

- timestamp: 2026-03-03
  checked: Sidebar.qml ColumnLayout for vertical sidebar
  found: |
    QML vertical sidebar layout (ColumnLayout with anchors.margins and spacing):
      1. MaterialIcon (volume icon) - fixed size ~24*scale px
      2. Item (volume bar) - Layout.fillHeight: true (takes ALL remaining space)
      3. Text (volume %) - fixed size, tiny font
      4. Rectangle (1px separator)
      5. Item (home button) - Layout.preferredHeight: UiMetrics.touchMin
    The volume bar fills remaining space AFTER fixed elements are subtracted.
    The home button is UiMetrics.touchMin tall (a touch-friendly minimum size).
  implication: |
    The actual home button occupies roughly touchMin/sidebarHeight of the sidebar,
    NOT 25%. With a typical sidebar height of ~552px (600 - topbar 48),
    and touchMin likely ~44px, the home button is about 8% of the sidebar height.
    But evdev allocates 25% to home zone - 3x too much.

- timestamp: 2026-03-03
  checked: Phase 5 display config changes impact
  found: |
    EvdevTouchReader gets displayWidth_/displayHeight_ from:
    1. Constructor args (from DisplayInfo or defaults 1024x600)
    2. setDisplayDimensions() from DisplayInfo::windowSizeChanged signal
    Phase 5 removed YamlConfig display width/height methods, but EvdevTouchReader
    never used YamlConfig for display dims - it uses DisplayInfo.
    The sidebar pixel width comes from config ("video.sidebar.width", default 150).
  implication: Phase 5 changes are NOT the cause. This is a pre-existing design issue.

- timestamp: 2026-03-03
  checked: Full-screen vs windowed sidebar height
  found: |
    During AA, fullscreenMode is likely true (AA plugin requests fullscreen).
    If fullscreen, TopBar and NavStrip have height 0, so the sidebar fills the
    full 600px display height. The evdev zones use full displayHeight_ (600).
    But even in non-fullscreen mode (TopBar 8% + NavStrip 12% = 20% overhead),
    the evdev zones still use full displayHeight_ - they don't account for
    TopBar/NavStrip taking space, because in fullscreen those are hidden.
  implication: |
    In fullscreen (which is the AA case), displayHeight_ = 600 is correct.
    The zone math uses the full 600px which matches.

## Resolution

root_cause: |
  The evdev sidebar sub-zone boundaries are hardcoded percentages (70%/5%/25%)
  that don't match the actual QML Sidebar.qml layout proportions.

  In QML, the vertical sidebar uses a ColumnLayout where:
  - Volume icon: ~24px (fixed)
  - Volume bar: fills remaining space (the vast majority)
  - Volume % text: ~fontTiny height (fixed)
  - 1px separator
  - Home button: UiMetrics.touchMin (~44px, fixed)
  - Plus margins (UiMetrics.spacing) and inter-item spacing

  For a 600px sidebar height with ~8px margins and ~8px spacing:
  - Fixed overhead: ~24 + ~14 + 1 + 44 + 16 (margins) + 32 (spacing*4) = ~131px
  - Volume bar: ~469px
  - Home button: ~44px

  So the real split is roughly: volume ~78%, fixed elements ~14%, home ~7%.

  But evdev allocates: volume 70%, gap 5%, home 25%.

  The 25% home zone starts at 75% of display height (Y=450 in a 600px display),
  while the actual home button starts at about 600 - 8(margin) - 44(touchMin) = 548px.
  That's a ~98px overlap where touches intended for the volume bar (between Y=450
  and Y=548) incorrectly trigger the home button.

fix: empty
verification: empty
files_changed: []
