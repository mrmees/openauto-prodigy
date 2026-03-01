---
status: diagnosed
phase: 02-theme-display
source: [02-05-SUMMARY.md, ce44c14 commit]
started: 2026-03-01T21:30:00Z
updated: 2026-03-01T21:50:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Wallpaper Display with Theme Default
expected: On the home/launcher screen, a wallpaper image is visible behind the UI, matching the active theme.
result: pass

### 2. Wallpaper Picker Options
expected: In Settings > Display, the wallpaper picker shows "Theme Default", "None", plus any custom images from ~/.openauto/wallpapers/ (e.g., "Midnight Glow", "Ocean Blue", "Ember Warmth" if sample wallpapers were installed).
result: issue
reported: "dropped a .jpg into .openauto/wallpapers, it didn't show up until the app was restarted"
severity: minor

### 3. Theme Default Wallpaper Follows Theme
expected: With wallpaper set to "Theme Default", switching themes (e.g., Default → Ocean) changes the wallpaper to match the new theme automatically.
result: pass

### 4. Custom Wallpaper Persists Across Theme Switch
expected: Select a custom wallpaper (e.g., "Ocean Blue" from the picker). Then switch to a different theme. The custom wallpaper stays — it does NOT change to the new theme's wallpaper.
result: issue
reported: "fail - wallpaper selection did not persist"
severity: major

### 5. None Wallpaper
expected: Setting wallpaper to "None" removes the wallpaper entirely, showing solid background color only.
result: pass

### 6. Gesture Overlay Opens and Stays
expected: 3-finger tap opens the quick controls overlay. The overlay stays open — it does NOT dismiss immediately when you lift your fingers. It stays until you press Close.
result: pass

### 7. Gesture Overlay Auto-Dismiss
expected: If you open the gesture overlay and don't touch anything, it auto-dismisses after about 15 seconds.
result: pass

### 8. Gesture Overlay Input Guard
expected: After 3-finger tap opens the overlay, the buttons/sliders are briefly unresponsive (~half second) so finger-lift doesn't accidentally trigger them. After the brief delay, all controls work normally.
result: pass

## Summary

total: 8
passed: 6
issues: 2
pending: 0
skipped: 0

## Gaps

- truth: "Wallpaper picker updates dynamically when new images added to ~/.openauto/wallpapers/"
  status: failed
  reason: "User reported: dropped a .jpg into .openauto/wallpapers, it didn't show up until the app was restarted"
  severity: minor
  test: 2
  root_cause: "buildWallpaperList() only called once at startup from scanThemeDirectories(). No re-scan mechanism exists."
  artifacts:
    - path: "src/core/services/ThemeService.cpp"
      issue: "buildWallpaperList() has exactly one call site (line 120), no public refresh method"
  missing:
    - "Add Q_INVOKABLE refreshWallpapers() that calls buildWallpaperList(), invoke from QML when wallpaper picker opens"
  debug_session: ".planning/debug/wallpaper-picker-no-dynamic-update.md"

- truth: "Custom wallpaper selection persists when switching themes"
  status: failed
  reason: "User reported: fail - wallpaper selection did not persist"
  severity: major
  test: 4
  root_cause: "display.wallpaper_override is missing from YamlConfig defaults tree. setValueByPath() validates against defaults and silently rejects unknown keys. Config writes for this key fail silently."
  artifacts:
    - path: "src/core/YamlConfig.cpp"
      issue: "Line 22: defaults tree has no display.wallpaper_override entry"
    - path: "src/core/YamlConfig.cpp"
      issue: "Lines 711-718: setValueByPath rejects paths not in defaults, returns false silently"
  missing:
    - "Add display.wallpaper_override to YamlConfig defaults (empty string default)"
  debug_session: ".planning/debug/wallpaper-not-persisting-on-theme-switch.md"
