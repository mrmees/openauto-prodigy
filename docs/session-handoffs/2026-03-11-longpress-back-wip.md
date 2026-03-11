# Session Handoff: Long-Press-to-Go-Back (WIP)

## Date
2026-03-11

## Goal
Add long-press (500ms) anywhere on settings pages to navigate back. Visual feedback via expanding ripple with back arrow icon during the hold period.

## What Was Built
- `goBack()` function in SettingsMenu.qml (shared by hardware back button and gesture)
- `blocksBackHoldAt(x, y)` hit-test function — walks item tree parent chain looking for `blocksBackHold: true` markers
- `blocksBackHold` markers on 6 controls: FullScreenPicker, SettingsToggle, SettingsSlider, SettingsListItem, SegmentedButton (always true), SettingsRow (true when interactive)
- Expanding ripple Rectangle with MaterialIcon back arrow, centered on press point
- Split TapHandler: touch (Pi) and mouse (desktop dev)

## What's NOT Working
**TapHandler `onPressedChanged` never fires on the Pi touchscreen.** The long-press detection doesn't trigger at all. Tried:

1. ~~DragHandler on StackView~~ — competed with Flickable vertical scrolling
2. ~~Left-edge MouseArea swipe zone~~ — felt unnatural in a car
3. ~~BackHoldArea (MouseArea at z:-1)~~ — works on empty background but not on rows with full-area controls (FullScreenPicker etc.)
4. ~~TapHandler with WithinBounds~~ — doesn't fire when MouseAreas grab the pointer
5. ~~TapHandler with DragThreshold + split touch/mouse~~ — CURRENT STATE, still not firing on Pi

## Codex's Analysis (conversation thread: 019cdd0f-2d44-7b11-b710-18daf849f459)
- `target: null` is fine
- Parenting under settingsMenu is fine
- Suggested `acceptedButtons: Qt.NoButton` for touch handler to avoid synthetic-mouse conflicts
- Suggested explicit `dragThreshold` for touch jitter tolerance
- These fixes were applied but it's STILL not working — needs further debugging

## Key Question
**Why does TapHandler with DragThreshold mode never fire `onPressedChanged` on the Pi?** Possible causes to investigate:
- StackView or Flickable grabbing the pointer before TapHandler gets it
- Touch events not reaching settingsMenu (the TapHandler's parent)
- Qt 6.8 behavior difference with passive grabs vs Qt 6.4
- Need to add debug logging: `onGrabChanged: function(transition, point) { console.log("grab", transition) }`

## Alternative Approaches Not Yet Tried
- **Add `onPressAndHold` directly to FullScreenPicker's rowMouseArea** (and other full-row controls) — invasive but guaranteed to work since those MouseAreas DO receive events
- **Overlay MouseArea that accepts press + Timer** — accept press to track hold, but propagate clicks via `propagateComposedEvents`. Risk: may interfere with controls
- **C++ event filter** — install on the QML window, detect long-press at the Qt event level before QML dispatches

## Files Modified This Session
- `qml/applications/settings/SettingsMenu.qml` — main changes (TapHandler, ripple, goBack, blocksBackHoldAt)
- `qml/controls/BackHoldArea.qml` — created then made obsolete (still registered in CMake but unused)
- `qml/controls/SettingsRow.qml` — added `blocksBackHold: interactive`
- `qml/controls/FullScreenPicker.qml` — added `blocksBackHold: true`
- `qml/controls/SettingsToggle.qml` — added `blocksBackHold: true`
- `qml/controls/SettingsSlider.qml` — added `blocksBackHold: true`
- `qml/controls/SettingsListItem.qml` — added `blocksBackHold: true`
- `qml/controls/SegmentedButton.qml` — added `blocksBackHold: true`
- `src/CMakeLists.txt` — registered BackHoldArea.qml (can remove if abandoned)
- `docs/plans/2026-03-11-swipe-back-settings.md` — original design doc (outdated, was for swipe)

## Branch
`dev/0.5.2` — all commits pushed

## Also In Progress
- Phase 03 gap closure plan 03-03 (font size increase) is at checkpoint — waiting for Matt to verify text sizing on Pi after the UiMetrics font bump. Commit `32787fc`. Resume executor agent `a66869c5f321e0920` with "approved" or issues.
