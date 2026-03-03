---
status: complete
phase: 02-uimetrics-foundation-touch-pipeline
source: [02-01-SUMMARY.md, 02-02-SUMMARY.md]
started: 2026-03-03T07:00:00Z
updated: 2026-03-03T08:30:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Startup log shows scale computation chain
expected: After launching the app on the Pi, the console/log output should show the detected window dimensions (1024x600), scaleH, scaleV, layout scale (min), and font scale (geometric mean). No "Screen.*" references — dimensions come from the actual window.
result: pass

### 2. UI scales correctly at native 1024x600
expected: At the Pi's native 1024x600, the UI should look essentially the same as before (scale ~1.0). Text is readable, nav strip icons are properly sized, nothing is clipped or overflowing.
result: pass

### 3. UI scales down at 800x480
expected: If you resize the window to 800x480 (or set a smaller resolution), UI elements should be visibly smaller than at 1024x600. Text should still be readable (font floors prevent illegibility) — body text should not go below ~14px even at small scales.
result: pass

### 4. Sidebar touch zones align with visual sidebar
expected: During an AA session, tapping sidebar buttons (home, volume, etc.) should hit the correct targets. The touch zones should match the visual sidebar width — no offset or misalignment at the Pi's display resolution.
result: issue
reported: "Only works after restarting the program - changing the sidebar enable/disable toggle and then returning to android auto does not update the change. Sidebar draws over AA window (margins not recalculated), touch zones not updated (taps on sidebar go to AA instead of being intercepted)."
severity: major

### 5. AA connection and video work normally
expected: Android Auto connects wirelessly as before. Video displays correctly, touch input works (tap, scroll), audio plays. No regression from the scaling/touch changes.
result: pass

## Summary

total: 5
passed: 4
issues: 1
pending: 0
skipped: 0

## Gaps

- truth: "Sidebar touch zones and AA video margins update dynamically when sidebar is toggled on/off mid-session"
  status: deferred
  reason: "User reported: Only works after restarting the program - changing the sidebar enable/disable toggle and then returning to android auto does not update the change. Sidebar draws over AA window (margins not recalculated), touch zones not updated (taps on sidebar go to AA instead of being intercepted)."
  severity: major
  test: 4
  deferred_to: "wishlist — live sidebar toggle reactivity (margins + touch zones mid-session)"
